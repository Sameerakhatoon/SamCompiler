#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "helpers/buffer.h"
#include "helpers/vector.h"

extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

// Owned by node.c, threaded through statements so children know
// which body / function they belong to.
extern struct node* parser_current_body;
extern struct node* parser_current_function;

// ch77: a NODE_TYPE_BLANK sentinel for empty `()`.
static struct node* parser_blank_node;

// ch55: history flags carried through nested parses.
enum {
    HISTORY_FLAG_INSIDE_UNION     = 0b00000001,
    HISTORY_FLAG_IS_UPWARD_STACK  = 0b00000010,
    HISTORY_FLAG_IS_GLOBAL_SCOPE      = 0b00000100,
    HISTORY_FLAG_INSIDE_STRUCTURE     = 0b00001000,
    HISTORY_FLAG_INSIDE_FUNCTION_BODY = 0b00010000,
    HISTORY_FLAG_IN_SWITCH_STATEMENT                  = 0b00100000,
    // ch130: tell parse_for_parentheses we're already past the
    // operator that opened these parens, so they're a grouping not
    // a function call.
    HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL   = 0b01000000,
    // An initializer is an assignment-expression: a top-level `,`
    // separates declarators (`int a = 1, b = 2;`) instead of being the
    // comma operator. Parentheses start a fresh history, so `(a, b)`
    // inside an initializer is still a comma expression.
    HISTORY_FLAG_NO_COMMA_OPERATOR                    = 0b10000000,
};

// ch94/166: parsed_switch_case is defined in compiler.h so codegen
// can iterate switch cases too. Each `case <expr>:` inside a switch
// body becomes one of these, attached to the parser_history_switch
// hanging off the active history. The index is the literal value of
// the case expression (we only allow integer-constant cases for now).

struct history_cases {
    struct vector* cases;             // vector of parsed_switch_case
    bool           has_default_case;
};

struct parser_history_switch {
    // ch186: pointer (not value) so history_down() copies share the
    // underlying case data instead of each clone allocating its own.
    struct history_cases* case_data;
};

// ch57: parser-side scope entity. Each declared variable becomes one
// of these in the parser's scope chain. Codegen reads stack_offset
// from this; the flags help the resolver decide stack vs structure-
// scoped placement.
enum {
    PARSER_SCOPE_ENTITY_ON_STACK        = 0b00000001,
    PARSER_SCOPE_ENTITY_STRUCTURE_SCOPE = 0b00000010,
};

struct parser_scope_entity {
    int          flags;
    int          stack_offset;
    struct node* node;
};

static struct parser_scope_entity* parser_new_scope_entity(struct node* node, int stack_offset, int flags){
    struct parser_scope_entity* entity = calloc(1, sizeof(struct parser_scope_entity));
    entity->node         = node;
    entity->flags        = flags;
    entity->stack_offset = stack_offset;
    return entity;
}

// Forward decl - body lives after current_process declaration.
static struct parser_scope_entity* parser_scope_last_entity_stop_global_scope(void);

// Parse history. Threaded through every parse_*() call so children can
// see flags / context their parents pushed (e.g. "we are inside an
// expression right now"). history_begin() makes a fresh one; history_down()
// clones one for a deeper call so we don't mutate the parent's flags.
struct history {
    int                          flags;
    struct parser_history_switch _switch;
    // With EXPRESSION_IS_UNARY: the node count when the operand parse
    // began, so parse_exp can tell whether the operand exists yet.
    size_t                       unary_operand_base;
};

static struct parser_history_switch parser_new_switch_statement(struct history* history);
static void                         parser_end_switch_statement(struct parser_history_switch* sw);
static void                         parser_register_case(struct history* history, struct node* case_node);

static struct history* history_begin(int flags);
static struct history* history_down(struct history* history, int flags);
static bool            parser_is_bool_object(struct datatype* dtype);
static struct node*    parser_bool_value(struct node* value);
static bool            parser_expression_type(struct node* node, struct datatype* out);
static const char*     parser_label_name(const char* label);
static void            parser_add_leaf(struct vector* leaves, size_t offset, struct datatype* type, struct node* value);
static struct node*    parser_global_initializer_node(struct token* name_token, struct vector* leaves);
static void            parse_compound_literal(struct datatype* dtype);
static bool            parse_function_pointer_declarator(struct datatype* dtype, struct token** name_out);
static struct array_brackets* parse_array_brackets(struct history* history);
static bool            parser_name_in(struct vector* names, const char* name);

static struct compile_process* current_process;
// ch97: parser-owned fixup system. Forward references to as-yet-
// undeclared structs register a fixup; parse() resolves them after
// the full pass and asserts none remain.
static struct fixup_system*    parser_fixup_sys;
static struct token*           parser_last_token;
// The token before parser_last_token: an identifier after `.` or `->`
// is a member name, never an enumerator.
static struct token*           parser_previous_token;
// Set once a _Bool type has been parsed.
static bool                    parser_saw_bool;
// Statements that belong right after the statement being parsed: the
// stores a local's brace initializer turns into. parse_body takes them.
static struct vector*          parser_pending_statements;
// Statements that belong right before it: the hidden declarations and
// stores of compound literals in it.
static struct vector*          parser_preceding_statements;
// The labels and goto targets of the function being parsed (const char*,
// already prefixed by parser_label_name).
static struct vector*          parser_function_labels;
static struct vector*          parser_function_gotos;

// Defined here so current_process is in scope.
static struct parser_scope_entity* parser_scope_last_entity_stop_global_scope(void){
    return scope_last_entity_stop_at(current_process, current_process->scope.root);
}

static void          parser_ignore_nl_or_comment(struct token* token);
static struct token* token_next(void);
static struct token* token_peek_next(void);
static void          parse_single_token_to_node(void);
static void          parse_expressionable_for_op(struct history* history, const char* op);
static void          parse_exp_normal(struct history* history);
static int           parse_exp(struct history* history);
static int           parser_get_pointer_depth(void);
static void          parser_deal_with_additional_expression(struct history* history);
static void          parse_for_parentheses(struct history* history);
static void          parse_identifier(struct history* history);
static bool          is_keyword_variable_modifier(const char* val);
static bool          parse_datatype_modifiers(struct datatype* dtype);
static void          parse_datatype_type(struct datatype* dtype);
static void          parse_datatype(struct datatype* dtype);
static void          parse_variable_function_or_struct_union(struct history* history);
static bool          parser_is_int_valid_after_datatype(struct datatype* dtype);
static void          parser_ignore_int(struct datatype* dtype);
static void          parse_expressionable_root(struct history* history);
static void          make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node);
static void          make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node);
static void          parse_variable(struct datatype* dtype, struct token* name_token, struct history* history);
static void          make_variable_list_node(struct vector* var_list_vec);
static void          expect_sym(char c);
static void          parse_keyword(struct history* history);
static void          parse_sizeof(struct history* history);
static int           parse_expressionable_single(struct history* history);
static void          parse_expressionable(struct history* history);
static int           parse_next(void);
static void          parse_body(size_t* variable_size, struct history* history);
static void          expect_op(const char* op);
static bool          token_next_is_symbol(char c);
static bool          token_next_is_operator(const char* op);
static struct token* token_peek_next(void);
static void          parse_datatype(struct datatype* dtype);
static void          parse_for_cast(void);

static struct history* history_begin(int flags){
    struct history* h = calloc(1, sizeof(struct history));
    h->flags = flags;
    return h;
}

static struct history* history_down(struct history* history, int flags){
    struct history* h = calloc(1, sizeof(struct history));
    memcpy(h, history, sizeof(struct history));
    h->flags = flags;
    return h;
}

// ch94: switch-case bookkeeping. parse_switch calls
// parser_new_switch_statement at entry; parse_case calls
// parser_register_case for each `case <expr>:` it sees; parse_switch
// calls parser_end_switch_statement at exit.
static struct parser_history_switch parser_new_switch_statement(struct history* history){
    memset(&history->_switch, 0, sizeof(history->_switch));
    // ch186: allocate case_data on the heap so the inner cases vector
    // survives history_down() copies.
    history->_switch.case_data = calloc(1, sizeof(struct history_cases));
    history->_switch.case_data->cases = vector_create(sizeof(struct parsed_switch_case));
    history->flags |= HISTORY_FLAG_IN_SWITCH_STATEMENT;
    return history->_switch;
}

static void parser_end_switch_statement(struct parser_history_switch* sw){
    (void)sw;
}

static void parser_register_case(struct history* history, struct node* case_node){
    if(!(history->flags & HISTORY_FLAG_IN_SWITCH_STATEMENT)){
        compiler_error(current_process, "case label outside a switch statement\n");
    }
    struct parsed_switch_case scase;
    scase.index = (long long)case_node->stmt._case.exp->llnum;
    struct vector* cases = history->_switch.case_data->cases;
    vector_set_peek_pointer(cases, 0);
    struct parsed_switch_case* seen = vector_peek(cases);
    while(seen){
        if(seen->index == scase.index){
            compiler_error(current_process, "Duplicate case value %lld\n", scase.index);
        }
        seen = vector_peek(cases);
    }
    vector_push(cases, &scase);
}

static void parser_ignore_nl_or_comment(struct token* token){
    while(token && token_is_nl_or_comment_or_newline_seperator(token)){
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token* token_next(void){
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    // The token after any skipped newlines and comments.
    next_token = vector_peek_no_increment(current_process->token_vec);
    if(next_token){
        current_process->pos = next_token->pos;
    }
    parser_previous_token = parser_last_token;
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token* token_peek_next(void){
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

// How a token reads in a diagnostic: 'while', ';', the number 5.
static const char* parser_token_text(struct token* token){
    static char text[64];
    if(!token){
        return "the end of the file";
    }
    switch(token->type){
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_KEYWORD:
        case TOKEN_TYPE_OPERATOR:
            snprintf(text, sizeof(text), "'%.40s'", token->sval);
            break;
        case TOKEN_TYPE_SYMBOL:
            snprintf(text, sizeof(text), "'%c'", token->cval);
            break;
        case TOKEN_TYPE_NUMBER:
            snprintf(text, sizeof(text), "the number %llu", token->llnum);
            break;
        case TOKEN_TYPE_STRING:
            snprintf(text, sizeof(text), "a string literal");
            break;
        default:
            snprintf(text, sizeof(text), "an unexpected token");
    }
    return text;
}

// Enumeration constants (C11 6.7.2.2). Each is an int constant whose
// name is visible in the scope that declared it; a variable of the same
// name declared deeper hides it (a `shadow` entry). The parser turns a
// use of the name into a number node, so nothing after the parser sees
// enumerators. A `rename` entry is a static local instead: its name
// stands for the global that holds it (parse_static_local).
struct parser_enumerator {
    const char*     name;
    struct constant value;
    int             depth;
    bool            shadow;
    const char*     rename;
};
static struct vector* parser_enumerators;

static int parser_scope_depth(void){
    int depth = 0;
    for(struct scope* s = current_process->scope.current; s; s = s->parent){
        depth++;
    }
    return depth;
}

static void parser_enumerator_push(const char* name, struct constant* value, bool shadow){
    struct parser_enumerator e = { .name = name, .depth = parser_scope_depth(), .shadow = shadow };
    if(value){
        e.value = *value;
    }
    vector_push(parser_enumerators, &e);
}

// Entries of scopes that have closed go away.
static void parser_enumerators_trim(void){
    int depth = parser_scope_depth();
    while(vector_count(parser_enumerators)){
        struct parser_enumerator* e = vector_back(parser_enumerators);
        if(e->depth <= depth){
            break;
        }
        vector_pop(parser_enumerators);
    }
}

static struct parser_enumerator* parser_enumerator_find(const char* name){
    for(int i = vector_count(parser_enumerators) - 1; i >= 0; i--){
        struct parser_enumerator* e = vector_at(parser_enumerators, i);
        if(S_EQ(e->name, name)){
            return e->shadow ? 0 : e;
        }
    }
    return 0;
}

// A variable, parameter or function named like a visible enumerator
// hides it from here on in this scope.
static void parser_enumerator_hide(const char* name){
    if(name && parser_enumerators && parser_enumerator_find(name)){
        parser_enumerator_push(name, 0, true);
    }
}

// A sub-parse that should have left one new node on the stack. If it
// left none, the caller's node_pop() would take an earlier node, which
// is how whole functions disappeared from the output.
static void parser_expect_new_node(size_t count_before, const char* what){
    if(node_count() <= count_before){
        compiler_error(current_process, "Expected %s but found %s\n", what, parser_token_text(token_peek_next()));
    }
}

static void parse_single_token_to_node(void){
    struct token* token = token_next();
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
            node_create(&(struct node){ .type = NODE_TYPE_NUMBER,     .llnum = token->llnum, .num.flags = token->num.flags });
            break;
        case TOKEN_TYPE_IDENTIFIER: {
            bool member = parser_previous_token
                && (token_is_operator(parser_previous_token, ".") || token_is_operator(parser_previous_token, "->"));
            struct parser_enumerator* e = member ? 0 : parser_enumerator_find(token->sval);
            if(e && e->rename){
                node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = e->rename });
                break;
            }
            if(e){
                node_push(constant_node(&e->value));
                break;
            }
            node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval  = token->sval  });
            break;
        }
        case TOKEN_TYPE_STRING: {
            // Adjacent string literals are one string (C11 5.1.1.2
            // translation phase 6): "ab" "cd" is "abcd". The second
            // literal used to end the statement and drop the function.
            const char* sval = token->sval;
            struct token* next = token_peek_next();
            if(next && next->type == TOKEN_TYPE_STRING){
                struct buffer* joined = buffer_create();
                for(const char* c = sval; *c; c++){
                    buffer_write(joined, *c);
                }
                while((next = token_peek_next()) && next->type == TOKEN_TYPE_STRING){
                    token_next();
                    for(const char* c = next->sval; *c; c++){
                        buffer_write(joined, *c);
                    }
                }
                buffer_write(joined, 0x00);
                sval = buffer_ptr(joined);
            }
            node_create(&(struct node){ .type = NODE_TYPE_STRING,     .sval  = sval  });
            break;
        }
        default:
            compiler_error(current_process, "This is not a single token that can be converted to a node");
    }
}

static void parse_expressionable_for_op(struct history* history, const char* op){
    parse_expressionable(history);
}

// Find which precedence group an operator belongs to. Returns the group
// index (low = high precedence) and writes the group pointer to
// *group_out. Returns -1 if the operator isn't in any group.
static int parser_get_precedence_for_operator(const char* op,
                                              struct expressionable_op_precedence_group** group_out){
    *group_out = 0;
    for(int i = 0; i < TOTAL_OPERATOR_GROUPS; i++){
        for(int b = 0; op_precedence[i].operators[b]; b++){
            const char* _op = op_precedence[i].operators[b];
            if(S_EQ(op, _op)){
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }
    return -1;
}

// "Does the left-hand operator have at-least-as-high precedence than
// the right-hand operator?" Used to decide whether a OPl (b OPr c)
// should rewrite to (a OPl b) OPr c. Right-associative ops never claim
// priority over themselves.
//
// the same operator twice used to return false here, so
// `a - b - c` stayed `a - (b - c)`. A left-to-right operator
// now has priority over itself like over any other operator of its
// group. `&&`, `||` and `,` keep the right-nested shape: grouping does
// not change their result, and the short-circuit codegen shares one
// pair of end labels down the right-hand chain.
static bool parser_left_op_has_priority(const char* op_left, const char* op_right){
    struct expressionable_op_precedence_group* group_left  = 0;
    struct expressionable_op_precedence_group* group_right = 0;

    if(S_EQ(op_left, op_right) && (S_EQ(op_left, "&&") || S_EQ(op_left, "||") || S_EQ(op_left, ","))){
        return false;
    }

    int precdence_left  = parser_get_precedence_for_operator(op_left,  &group_left);
    int precdence_right = parser_get_precedence_for_operator(op_right, &group_right);
    if(group_left->associtivity == ASSOCIATIVITY_RIGHT_TO_LEFT){
        return false;
    }
    return precdence_left <= precdence_right;
}

// Rotate so the left subtree absorbs the right's left. e.g.
//   node:  (50 * (20 + 120))           with op=* and right.op=+
//   after: ((50 * 20) + 120)
// Used by parser_reorder_expression when the parent op out-ranks the
// child op.
// ch101: hoist `node->exp.right->exp.left` up to become the new left
// child, with the original op. The remaining right-grandchild becomes
// the new right child, and the parent op is replaced by the child's
// op. Used by the extra reorder pass to flatten subscripts /
// assignments / `(...)`-then-comma sequences.
static void parser_node_move_right_left_to_left(struct node* node){
    make_exp_node(node->exp.left, node->exp.right->exp.left, node->exp.op);
    struct node* completed_node = node_pop();

    const char* new_op = node->exp.right->exp.op;
    node->exp.left  = completed_node;
    node->exp.right = node->exp.right->exp.right;
    node->exp.op    = new_op;
}

static void parser_node_shift_children_left(struct node* node){
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right->type == NODE_TYPE_EXPRESSION);

    const char*  right_op           = node->exp.right->exp.op;
    struct node* new_exp_left_node  = node->exp.left;
    struct node* new_exp_right_node = node->exp.right->exp.left;
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    struct node* new_left_operand  = node_pop();
    struct node* new_right_operand = node->exp.right->exp.right;

    node->exp.left  = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op    = right_op;
}

// A left operand that is already complete: a leaf, or a postfix
// expression (`a[i]`, `f()`, `s.a`, `p->a`), which binds tighter than
// any binary operator. Only a leaf counted here before, so
// `a[1] * 2 + i * 10` stayed a[1] * (2 + i * 10).
static bool parser_is_complete_operand(struct node* node){
    if(node->type != NODE_TYPE_EXPRESSION){
        return true;
    }
    return S_EQ(node->exp.op, "[]") || S_EQ(node->exp.op, "()")
        || is_access_operator(node->exp.op);
}

// Recursive reorder: walks the freshly-built expression subtree and
// flips ordering where the parent op deserves higher precedence.
static void parser_reorder_expression(struct node** node_out){
    struct node* node = *node_out;
    if(node->type != NODE_TYPE_EXPRESSION){
        return;
    }
    if(parser_is_complete_operand(node->exp.left)
       && node->exp.right
       && node->exp.right->type != NODE_TYPE_EXPRESSION){
        return;
    }

    // The interesting case: left is complete, right is an expression.
    // e.g. `50 * (20 + 120)` parsed top-down. If our op has higher
    // precedence than the right's op, shift left.
    if(parser_is_complete_operand(node->exp.left)
       && node->exp.right
       && node->exp.right->type == NODE_TYPE_EXPRESSION){
        const char* right_op = node->exp.right->exp.op;
        if(parser_left_op_has_priority(node->exp.op, right_op)){
            parser_node_shift_children_left(node);
            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }

    // ch101 + ch175: extra reordering. ch101 was too aggressive
    // (`||` between the array and assignment tests reordered any
    // `a[i] op b` regardless of op). ch175 tightens to:
    //   - LHS array AND RHS assignment, or
    //   - LHS `()` or `[]` AND RHS `,`.
    if((is_array_node(node->exp.left) && is_node_assignment(node->exp.right))
       || ((node_is_expression(node->exp.left, "()")
            || node_is_expression(node->exp.left, "[]"))
           && node_is_expression(node->exp.right, ","))){
        parser_node_move_right_left_to_left(node);
    }
}

// Binary expression: pop the left operand off the node stack, consume
// the operator, parse the right operand, then assemble a NODE_TYPE_EXPRESSION.
// ch29 will fold precedence-aware reordering into the trailing comment.
// ch130: `*X` chain - read all consecutive `*` operators and parse
// the operand that follows, building a single NODE_TYPE_UNARY whose
// indirection.depth carries the chain length.
static struct history* parser_unary_operand_history(void){
    struct history* history = history_begin(EXPRESSION_IS_UNARY);
    history->unary_operand_base = node_count();
    return history;
}

static void parse_for_indirection_unary(void){
    int depth = parser_get_pointer_depth();
    // ch176: flag this descent EXPRESSION_IS_UNARY so the operator
    // parser knows to stop before unrelated operators.
    size_t count_before = node_count();
    parse_expressionable(parser_unary_operand_history());
    parser_expect_new_node(count_before, "an operand after '*'");
    struct node* operand = node_pop();
    make_unary_node("*", operand);
    struct node* unary = node_pop();
    unary->unary.indirection.depth = depth;
    node_push(unary);
}

// ch130: any other unary (-, !, ~, &).
static void parse_for_normal_unary(void){
    const char* op = token_next()->sval;
    // ch176: same as the indirection path - flag the descent so the
    // operator parser knows where to stop.
    size_t count_before = node_count();
    parse_expressionable(parser_unary_operand_history());
    char what[64];
    snprintf(what, sizeof(what), "an operand after '%s'", op);
    parser_expect_new_node(count_before, what);
    struct node* operand = node_pop();
    // Unary plus leaves the value as it is; the operand stands alone.
    if(S_EQ(op, "+")){
        node_push(operand);
        return;
    }
    make_unary_node(op, operand);
}

// A prefix operator, then whatever operators follow it, with the unary
// as their left operand. The `*` path used to skip the second step, so
// in a right operand `1 + *q * 2` became (1 + *q) * 2.
static void parse_for_unary(struct history* history){
    const char* op = token_peek_next()->sval;
    if(op_is_indirection(op)){
        parse_for_indirection_unary();
    } else {
        parse_for_normal_unary();
    }
    parser_deal_with_additional_expression(history);
}

static bool parser_is_unary_operator(const char* op){
    return is_unary_operator(op);
}

// `v.a++` arrives as v . (a++) because the right side of `.` is parsed
// first and the postfix lands on `a`. Postfix ++/-- applies to the
// whole member access, so lift it: (v . a)++. Inner levels are already
// lifted, so `v.b.c++` ends as ((v.b).c)++.
static struct node* parser_hoist_postfix(struct node* exp_node){
    if(exp_node->type != NODE_TYPE_EXPRESSION || !is_access_operator(exp_node->exp.op)){
        return exp_node;
    }
    struct node* right = exp_node->exp.right;
    if(right->type != NODE_TYPE_UNARY || !(right->unary.flags & UNARY_FLAG_IS_POSTFIX)){
        return exp_node;
    }
    exp_node->exp.right  = right->unary.operand;
    right->unary.operand = exp_node;
    return right;
}

static void parse_exp_normal(struct history* history){
    struct token* op_token = token_peek_next();
    const char*   op       = op_token->sval;
    struct node*  node_left = node_peek_expressionable_or_null();
    if(!node_left){
        // ch130: no left operand - must be a unary, else hard error.
        if(!parser_is_unary_operator(op)){
            compiler_error(current_process, "The given expression has no left operand");
        }
        parse_for_unary(history);
        return;
    }

    // `x++` / `x--`: postfix, no right operand. The operator table has
    // always listed them in the top precedence group; the parser never
    // built anything for them, so `x++;` swallowed the next node (A13).
    if(is_increment_operator(op)){
        token_next();
        node_pop();
        make_unary_node(op, node_left);
        node_peek()->unary.flags |= UNARY_FLAG_IS_POSTFIX;
        return;
    }

    token_next();          // consume operator

    node_pop();            // detach the left operand
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    size_t count_before_right = node_count();

    // ch130: choose right-operand parser by what's next.
    if(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_OPERATOR){
        if(S_EQ(token_peek_next()->sval, "(")){
            parse_for_parentheses(history_down(history, history->flags | HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL));
        } else if(parser_is_unary_operator(token_peek_next()->sval)){
            parse_for_unary(history);
        } else {
            compiler_error(current_process,
                "Two operators are expected for a given expression for operator %s\n",
                token_peek_next()->sval);
        }
    } else {
        parse_expressionable_for_op(history_down(history, history->flags), op);
    }

    char what[64];
    snprintf(what, sizeof(what), "an operand after '%s'", op);
    parser_expect_new_node(count_before_right, what);
    struct node* node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    make_exp_node(node_left, node_right, op);
    struct node* exp_node = node_pop();

    // ch30: reorder for operator precedence.
    parser_reorder_expression(&exp_node);

    node_push(parser_hoist_postfix(exp_node));
}

// ch77: after a `(`...`)` is parsed, if the next token is an
// operator, keep parsing - lets `(50+20)+30` reduce as one expression
// instead of stopping after the close paren.
// The rest keeps the enclosing NO_COMMA flag: in `int a = -1, b = 2;`
// and `enum { A = -3, B }` the comma after the unary separates
// declarators, and was read as the comma operator.
static void parser_deal_with_additional_expression(struct history* history){
    struct token* t = token_peek_next();
    if(t && t->type == TOKEN_TYPE_OPERATOR){
        parse_expressionable(history_begin(history->flags & HISTORY_FLAG_NO_COMMA_OPERATOR));
    }
}

// Inside a unary operand, `*(p + 1) - 5`, the operators after the
// parentheses belong to the expression around the unary, which picks
// them up once the unary node is built (parse_for_unary). Taking them
// here made the operand (p + 1) - 5.
static void parser_deal_with_additional_expression_unless_unary(struct history* history){
    if(history->flags & EXPRESSION_IS_UNARY){
        return;
    }
    parser_deal_with_additional_expression(history);
}

// ch77: parse `(...)`. Two flavours:
//   plain `(expr)`     -> NODE_TYPE_EXPRESSION_PARENTHESES.
//   `callee(args)`     -> NODE_TYPE_EXPRESSION with op "()", left =
//                         the callee that was already on the stack,
//                         right = the inner expression (or BLANK).
static void parse_for_parentheses(struct history* history){
    expect_op("(");
    // ch93: `(int) x` - a keyword right after the open paren means
    // this is a cast expression, not a parenthesised value.
    // Only a type keyword starts a cast; `(sizeof(x) == 4)` is a
    // parenthesised expression that happens to begin with a keyword.
    if(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_KEYWORD
       && (keyword_is_datatype(token_peek_next()->sval) || is_keyword_variable_modifier(token_peek_next()->sval))){
        parse_for_cast();
        parser_deal_with_additional_expression_unless_unary(history);
        return;
    }

    struct node* left_node = 0;
    struct node* tmp_node  = node_peek_or_null();
    if(tmp_node && node_is_value_type(tmp_node)){
        left_node = tmp_node;
        node_pop();
        // `(*fp)(x)` and `(fp)(x)` call through fp: dereferencing a
        // function pointer gives the function back (C11 6.5.3.2p4).
        while(left_node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            struct node* inner = left_node->parenthesis.exp;
            if(inner->type == NODE_TYPE_UNARY && op_is_indirection(inner->unary.op)
               && inner->unary.indirection.depth == 1){
                inner = inner->unary.operand;
            }
            // The callee may also be an element or a member,
            // `(*t[0].f)(x)` and `(*q->f)(x)`, as the resolver allows.
            if(inner->type != NODE_TYPE_IDENTIFIER && inner->type != NODE_TYPE_EXPRESSION_PARENTHESES
               && !is_array_node(inner) && !is_access_node(inner)){
                break;
            }
            left_node = inner;
        }
    }

    struct node* exp_node = parser_blank_node;
    if(!token_next_is_symbol(')')){
        parse_expressionable_root(history_begin(0));
        exp_node = node_pop();
    }
    expect_sym(')');

    make_exp_parentheses_node(exp_node);

    if(left_node){
        struct node* parentheses_node = node_pop();
        make_exp_node(left_node, parentheses_node, "()");
    }

    parser_deal_with_additional_expression_unless_unary(history);
}

// ch90: `cond ? true : false`. Caller has already left `cond` on the
// node stack.
static void parse_for_tenary(struct history* history){
    struct node* condition_node = node_pop();
    expect_op("?");
    parse_expressionable_root(history_down(history, history->flags));
    struct node* true_result_node = node_pop();
    expect_sym(':');
    // The false branch is a conditional expression, so it ends at a
    // comma: `f(c ? a : b, x)` has two arguments.
    parse_expressionable_root(history_down(history, history->flags | HISTORY_FLAG_NO_COMMA_OPERATOR));
    struct node* false_result_node = node_pop();
    make_tenary_node(true_result_node, false_result_node);
    struct node* tenary_node = node_pop();
    make_exp_node(condition_node, tenary_node, "?");
}

// ch93: `(T) operand`. Opening `(` already consumed by caller.

static void parse_for_cast(void){
    struct datatype dtype = {0};
    parse_datatype(&dtype);
    // `(int (*)(int))p`
    struct token* no_name = 0;
    parse_function_pointer_declarator(&dtype, &no_name);
    // `(int[]){ 1, 2 }`: the abstract declarator's brackets.
    if(token_next_is_operator("[")){
        struct array_brackets* brackets = parse_array_brackets(history_begin(0));
        dtype.array.brackets = brackets;
        dtype.array.size     = array_brackets_calculate_size(&dtype, brackets);
        dtype.flags         |= DATATYPE_FLAG_IS_ARRAY;
    }
    expect_sym(')');
    if(token_next_is_symbol('{')){
        parse_compound_literal(&dtype);
        return;
    }
    // A cast binds like a unary operator: `(char)x + 1` casts x only.
    size_t count_before = node_count();
    parse_expressionable(parser_unary_operand_history());
    parser_expect_new_node(count_before, "an operand after the cast");
    struct node* operand_node = node_pop();
    if(parser_is_bool_object(&dtype)){
        operand_node = parser_bool_value(operand_node);
    }
    make_cast_node(&dtype, operand_node);
}

// ch92: `arr[index]`. Pops the array as the left side, eats `[`,
// parses the index, eats `]`, builds BRACKET(inner), then EXPRESSION
// with op `[]`.
static void parse_for_array(struct history* history){
    struct node* left_node = node_peek_or_null();
    if(left_node){
        node_pop();
    }
    expect_op("[");
    // The index is a whole expression of its own, also inside a unary's
    // operand (`&a[i & 1]`).
    parse_expressionable_root(history_begin(0));
    expect_sym(']');
    struct node* exp_node = node_pop();
    if(left_node && left_node->type == NODE_TYPE_STRING){
        // "abc"[i] is *("abc" + i) (C11 6.5.2.1); the resolver only
        // follows brackets on variables.
        make_exp_node(left_node, exp_node, "+");
        make_unary_node("*", node_pop());
        struct node* unary = node_pop();
        unary->unary.indirection.depth = 1;
        node_push(unary);
        return;
    }
    make_bracket_node(exp_node);
    if(left_node){
        struct node* bracket_node = node_pop();
        make_exp_node(left_node, bracket_node, "[]");
    }
}

// ch91: `a, b` sequence operator.
static void parse_for_comma(struct history* history){
    token_next();   // eat `,`
    struct node* left_node = node_pop();
    parse_expressionable_root(history);
    struct node* right_node = node_pop();
    make_exp_node(left_node, right_node, ",");
}

static int parse_exp(struct history* history){
    if(history->flags & EXPRESSION_IS_UNARY){
        bool has_operand = node_count() > history->unary_operand_base;
        // The operand itself starts with a prefix operator: `-*p`,
        // `!!x`, `++*p`. It binds to the operand only, so no
        // parser_deal_with_additional_expression here.
        if(!has_operand && parser_is_unary_operator(token_peek_next()->sval)){
            if(op_is_indirection(token_peek_next()->sval)){
                parse_for_indirection_unary();
            } else {
                parse_for_normal_unary();
            }
            return 0;
        }
        // ch176: when we're parsing a unary's operand, bail out as soon
        // as we hit an operator that isn't a continuation (access, array
        // bracket, call parens, postfix ++/--). Anything else - including
        // the `=` in `*p = ...` - is for the outer expression parser.
        if(!unary_operand_compatible(token_peek_next())){
            return -1;
        }
    }
    if(S_EQ(token_peek_next()->sval, "(")){
        parse_for_parentheses(history);
    } else if(S_EQ(token_peek_next()->sval, "[")){
        parse_for_array(history);
    } else if(S_EQ(token_peek_next()->sval, "?")){
        parse_for_tenary(history);
    } else if(S_EQ(token_peek_next()->sval, ",")){
        if(history->flags & HISTORY_FLAG_NO_COMMA_OPERATOR){
            return -1;
        }
        parse_for_comma(history);
    } else {
        parse_exp_normal(history);
    }
    return 0;
}

// g01: assert compares a token's type with TOKEN_TYPE_IDENTIFIER,
// not NODE_TYPE_IDENTIFIER. The book ships this wrong.
static void parse_identifier(struct history* history){
    assert(token_peek_next()->type == TOKEN_TYPE_IDENTIFIER);
    parse_single_token_to_node();
}

// Keywords that prefix or modify a datatype (signed/unsigned, storage
// class, const, etc.) but don't name a type themselves.
// volatile, register, inline, auto and restrict are accepted and change
// nothing: this compiler loads and stores every access (what volatile
// asks for), keeps no variable in a register, and does not inline.
// Before, they were not keywords and the declaration was lost (A13).
static bool is_keyword_variable_modifier(const char* val){
    return S_EQ(val, "unsigned") || S_EQ(val, "signed")
        || S_EQ(val, "static")   || S_EQ(val, "const")
        || S_EQ(val, "extern")   || S_EQ(val, "__ignore_typecheck__")
        || S_EQ(val, "volatile") || S_EQ(val, "register")
        || S_EQ(val, "inline")   || S_EQ(val, "auto")
        || S_EQ(val, "restrict");
}

// Consume a run of modifier keywords, OR-ing matching DATATYPE_FLAG_*s
// into dtype. Stops at the first non-modifier keyword. Returns whether
// `signed` or `unsigned` was among them.
static bool parse_datatype_modifiers(struct datatype* dtype){
    bool sign_given = false;
    struct token* token = token_peek_next();
    while(token && token->type == TOKEN_TYPE_KEYWORD){
        if(!is_keyword_variable_modifier(token->sval)){
            break;
        }

        if(S_EQ(token->sval, "signed")){
            dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
            sign_given = true;
        } else if(S_EQ(token->sval, "unsigned")){
            dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
            sign_given = true;
        } else if(S_EQ(token->sval, "static")){
            dtype->flags |= DATATYPE_FLAG_IS_STATIC;
        } else if(S_EQ(token->sval, "const")){
            dtype->flags |= DATATYPE_FLAG_IS_CONST;
        } else if(S_EQ(token->sval, "extern")){
            dtype->flags |= DATATYPE_FLAG_IS_EXTERN;
        } else if(S_EQ(token->sval, "__ignore_typecheck__")){
            dtype->flags |= DATATYPE_FLAG_IGNORE_TYPE_CHECKING;
        }

        token_next();
        token = token_peek_next();
    }
    return sign_given;
}

static bool token_next_is_operator(const char* op){
    struct token* token = token_peek_next();
    return token_is_operator(token, op);
}

static bool token_next_is_keyword(const char* keyword){
    struct token* token = token_peek_next();
    return token_is_keyword(token, keyword);
}

static bool token_next_is_symbol(char c){
    struct token* token = token_peek_next();
    return token_is_symbol(token, c);
}

// Convenience wrappers - the parser doesn't carry the process around
// as an arg, but scope.c does, so adapt.
static void parser_scope_new(void){
    scope_new(current_process, 0);
}

static void parser_scope_finish(void){
    scope_finish(current_process);
    parser_enumerators_trim();
}

// Push a parser_scope_entity (carries node + flags + offset) into
// the current scope. ch61 retypes this to take the entity, not the
// raw node.
static void parser_scope_push(struct parser_scope_entity* entity, size_t size){
    scope_push(current_process, entity, size);
}

static struct parser_scope_entity* parser_scope_last_entity(void){
    return scope_last_entity(current_process);
}

// Consume the next token and assert it's the given symbol character.
// Fatal compiler_error if not.
static void expect_sym(char c){
    struct token* next_token = token_next();
    if(!next_token || next_token->type != TOKEN_TYPE_SYMBOL || next_token->cval != c){
        compiler_error(current_process, "Expecting symbol %c however something else was provided\n", c);
    }
}

static void expect_op(const char* op){
    struct token* next_token = token_next();
    if(!next_token || next_token->type != TOKEN_TYPE_OPERATOR || !S_EQ(next_token->sval, op)){
        compiler_error(current_process, "Expecting the operator %s but something else was provided\n", op);
    }
}

static void expect_keyword(const char* keyword){
    struct token* next_token = token_next();
    if(!next_token || next_token->type != TOKEN_TYPE_KEYWORD || !S_EQ(next_token->sval, keyword)){
        compiler_error(current_process, "Expecting the keyword %s but something else was provided\n", keyword);
    }
}

// Consume the datatype keyword + (optional) secondary primitive (the
// "int" in "long int", say) and hand both back to the caller.
// `unsigned` or `signed` alone is an int (`unsigned base`).
static struct token parser_implicit_int = { .type = TOKEN_TYPE_KEYWORD, .sval = "int" };
static bool         parser_sign_given;

static void parser_get_datatype_tokens(struct token** datatype_token,
                                       struct token** datatype_secondary_token){
    struct token* next = token_peek_next();
    if(parser_sign_given && !(next && next->type == TOKEN_TYPE_KEYWORD && keyword_is_datatype(next->sval))){
        *datatype_token = &parser_implicit_int;
        return;
    }
    *datatype_token = token_next();
    struct token* next_token = token_peek_next();
    if(token_is_primitive_keyword(next_token)){
        *datatype_secondary_token = next_token;
        token_next();
    }
}

static int parser_datatype_expected_for_type_string(const char* str){
    int type = DATA_TYPE_EXPECT_PRIMITIVE;
    if(S_EQ(str, "union")){
        type = DATA_TYPE_EXPECT_UNION;
    } else if(S_EQ(str, "struct")){
        type = DATA_TYPE_EXPECT_STRUCT;
    }
    return type;
}

// Forges a unique synthetic identifier for anonymous struct / union
// bodies. ch34 uses a process-local monotonic counter; nothing else
// relies on the spelling.
static int parser_get_random_type_index(void){
    static int x = 0;
    return ++x;
}

static struct token* parser_build_random_type_name(void){
    char tmp_name[25];
    snprintf(tmp_name, sizeof tmp_name, "customtypename_%i", parser_get_random_type_index());
    char* sval = malloc(sizeof tmp_name);
    strncpy(sval, tmp_name, sizeof tmp_name);
    struct token* token = calloc(1, sizeof(struct token));
    token->type = TOKEN_TYPE_IDENTIFIER;
    token->sval = sval;
    return token;
}

// Count leading '*' operator tokens; each '*' is one level of pointer.
static int parser_get_pointer_depth(void){
    int depth = 0;
    while(token_next_is_operator("*")){
        depth++;
        token_next();
    }
    return depth;
}

static bool parser_datatype_is_secondary_allowed(int expected_type){
    return expected_type == DATA_TYPE_EXPECT_PRIMITIVE;
}

static bool parser_datatype_is_secondary_allowed_for_type(const char* type){
    return S_EQ(type, "long")  || S_EQ(type, "short")
        || S_EQ(type, "double")|| S_EQ(type, "float");
}

static void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token,
                                                             struct token* datatype_secondary_token,
                                                             struct datatype* datatype_out);

// "long int" -> primary=long, secondary=int. The secondary datatype is
// held hanging off the primary's `.secondary` field. It adds no storage:
// `long int` is a long and `short int` a short. The book added the two
// sizes, so `long long` was 16 bytes, `long int` 12 and `short int` 6;
// `long long` is sized in parser_datatype_init.
static void parser_datatype_adjust_size_for_secondary(struct datatype* datatype,
                                                      struct token* datatype_secondary_token){
    if(!datatype_secondary_token){
        return;
    }
    struct datatype* secondary = calloc(1, sizeof(struct datatype));
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, 0, secondary);
    datatype->secondary  = secondary;
    datatype->flags     |= DATATYPE_FLAG_IS_SECONDARY;
}

static void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token,
                                                             struct token* datatype_secondary_token,
                                                             struct datatype* datatype_out){
    if(!parser_datatype_is_secondary_allowed_for_type(datatype_token->sval)
       && datatype_secondary_token){
        compiler_error(current_process,
            "Your not allowed a secondary datatype here for the given datatype %s\n",
            datatype_token->sval);
    }

    if(S_EQ(datatype_token->sval, "void")){
        datatype_out->type = DATA_TYPE_VOID;
        datatype_out->size = DATA_SIZE_ZERO;
    } else if(S_EQ(datatype_token->sval, "char")){
        datatype_out->type = DATA_TYPE_CHAR;
        datatype_out->size = DATA_SIZE_BYTE;
    } else if(S_EQ(datatype_token->sval, "_Bool")){
        // A byte holding 0 or 1; conversions to it are `!= 0`
        // (parser_bool_value). parse_datatype makes it unsigned.
        datatype_out->type   = DATA_TYPE_CHAR;
        datatype_out->size   = DATA_SIZE_BYTE;
        datatype_out->flags |= DATATYPE_FLAG_IS_BOOL;
        parser_saw_bool = true;
    } else if(S_EQ(datatype_token->sval, "short")){
        datatype_out->type = DATA_TYPE_SHORT;
        datatype_out->size = DATA_SIZE_WORD;
    } else if(S_EQ(datatype_token->sval, "int")){
        datatype_out->type = DATA_TYPE_INTEGER;
        datatype_out->size = DATA_SIZE_DWORD;
    } else if(S_EQ(datatype_token->sval, "long")){
        datatype_out->type = DATA_TYPE_LONG;
        // 4 bytes on ILP32 x86, 8 on the LP64 targets.
        datatype_out->size = current_process->target->long_size;
    } else if(S_EQ(datatype_token->sval, "float")){
        datatype_out->type = DATA_TYPE_FLOAT;
        datatype_out->size = DATA_SIZE_DWORD;
    } else if(S_EQ(datatype_token->sval, "double")){
        // g02: write the type to .type, not .size. A double is 8 bytes
        // on every target (it was 4); floating point is still
        // rejected where a value would be generated.
        datatype_out->type = DATA_TYPE_DOUBLE;
        datatype_out->size = DATA_SIZE_DDWORD;
    } else {
        compiler_error(current_process, "BUG: Invalid primitive datatype\n");
    }

    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}

// Look up a previously-defined struct by name; returns body size or 0.
static size_t size_of_struct(const char* struct_name){
    struct symbol* sym = symresolver_get_symbol(current_process, struct_name);
    if(!sym){
        return 0;
    }
    assert(sym->type == SYMBOL_TYPE_NODE);
    struct node* n = sym->data;
    assert(n->type == NODE_TYPE_STRUCT);
    return n->_struct.body_n->body.size;
}

// ch99: union version of size_of_struct.
static size_t size_of_union(const char* union_name){
    struct symbol* sym = symresolver_get_symbol(current_process, union_name);
    if(!sym){
        return 0;
    }
    assert(sym->type == SYMBOL_TYPE_NODE);
    struct node* n = sym->data;
    assert(n->type == NODE_TYPE_UNION);
    return n->_union.body_n->body.size;
}

static void parser_datatype_init_type_and_size(struct token* datatype_token,
                                               struct token* datatype_secondary_token,
                                               struct datatype* datatype_out,
                                               int pointer_depth, int expected_type){
    if(!parser_datatype_is_secondary_allowed(expected_type) && datatype_secondary_token){
        compiler_error(current_process, "You provided an invalid secondary datatype\n");
    }
    switch(expected_type){
        case DATA_TYPE_EXPECT_PRIMITIVE:
            parser_datatype_init_type_and_size_for_primitive(datatype_token, datatype_secondary_token, datatype_out);
            break;
        case DATA_TYPE_EXPECT_STRUCT:
            // ch65: look up by name; size is the body's size, and we
            // remember the defining node so resolvers can walk it.
            datatype_out->type        = DATA_TYPE_STRUCT;
            datatype_out->size        = size_of_struct(datatype_token->sval);
            datatype_out->struct_node = struct_node_for_name(current_process, datatype_token->sval);
            break;
        case DATA_TYPE_EXPECT_UNION:
            // ch99: real union lookup now lands.
            datatype_out->type        = DATA_TYPE_UNION;
            datatype_out->size        = size_of_union(datatype_token->sval);
            datatype_out->struct_node = union_node_for_name(current_process, datatype_token->sval);
            break;
        default:
            compiler_error(current_process, "BUG: Unsupported datatype expectation\n");
    }

    // ch144: when the declarator has pointer stars on it, stamp
    // IS_POINTER + the recorded depth onto the datatype.
    if(pointer_depth > 0){
        datatype_out->flags        |= DATATYPE_FLAG_IS_POINTER;
        datatype_out->pointer_depth = pointer_depth;
    }
}

static void parser_datatype_init(struct token* datatype_token,
                                 struct token* datatype_secondary_token,
                                 struct datatype* datatype_out,
                                 int pointer_depth, int expected_type){
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token,
                                       datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;

    // `long long` is clamped to the target's long. On x86 that is 32
    // bits, so warn; on the LP64 targets long is already 64 bits.
    if(S_EQ(datatype_token->sval, "long")
       && datatype_secondary_token
       && S_EQ(datatype_secondary_token->sval, "long")){
        if(current_process->target->long_size < DATA_SIZE_DDWORD){
            compiler_warning(current_process,
                "Our compiler does not support 64 bit longs, therefore your long long is defaulting to 32 bits\n");
        }
        datatype_out->size = current_process->target->long_size;
    }
}

// A function pointer declarator after the base type (C11 6.7.6.3):
// `(*name)(params)`, `(*name[N])(params)` for an array of them, or the
// abstract `(*)(params)` of a cast or parameter. The parameter types are
// read and not kept: every argument is passed the same way. A name may
// also follow the abstract form, which is what typedef substitution
// makes of `typedef int (*fn)(int); fn f;`. The type becomes a pointer
// whose function_return is the base type.
static bool parse_function_pointer_declarator(struct datatype* dtype, struct token** name_out){
    if(!token_next_is_operator("(")){
        return false;
    }
    vector_save(current_process->token_vec);
    token_next();
    bool is_function_pointer = token_next_is_operator("*");
    vector_restore(current_process->token_vec);
    if(!is_function_pointer){
        return false;
    }

    expect_op("(");
    int depth = parser_get_pointer_depth();
    struct token* name = 0;
    if(token_is_identifier(token_peek_next())){
        name = token_next();
    }
    struct array_brackets* brackets = 0;
    if(token_next_is_operator("[")){
        brackets = parse_array_brackets(history_begin(0));
    }
    expect_sym(')');
    expect_op("(");
    int level = 1;
    while(level){
        struct token* token = token_next();
        if(!token){
            compiler_error(current_process, "Missing ) after the parameters of a function pointer\n");
        }
        if(token_is_operator(token, "(")){
            level++;
        } else if(token_is_symbol(token, ')')){
            level--;
        }
    }

    struct datatype* function_return = calloc(1, sizeof(struct datatype));
    *function_return = *dtype;
    dtype->flags |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_FUNCTION_POINTER;
    dtype->pointer_depth   = function_return->pointer_depth + depth;
    dtype->function_return = function_return;
    if(brackets){
        dtype->array.brackets = brackets;
        dtype->array.size     = array_brackets_calculate_size(dtype, brackets);
        dtype->flags         |= DATATYPE_FLAG_IS_ARRAY;
    }
    if(!name && token_is_identifier(token_peek_next())){
        name = token_next();
    }
    *name_out = name;
    return true;
}

// Set by parse_datatype_type when the type was `enum ...`, so a
// declaration with no declarator (`enum color { RED };`) is accepted.
static bool parser_last_datatype_was_enum;

// `enum [tag] [{ A, B = expr, C, }]`, the `enum` keyword consumed. The
// tag names nothing this compiler needs (the type is int). Each
// enumerator is the previous one plus 1, or its constant expression.
static void parse_enum_specifier(void){
    if(token_is_identifier(token_peek_next())){
        token_next();
    }
    if(!token_next_is_symbol('{')){
        return;
    }
    expect_sym('{');
    struct datatype int_type = datatype_for_int();
    struct constant value = { 0, int_type };
    bool first = true;
    while(!token_next_is_symbol('}')){
        struct token* name = token_next();
        if(!token_is_identifier(name)){
            compiler_error(current_process, "Expected an enumerator name but found %s\n", parser_token_text(name));
        }
        if(token_next_is_operator("=")){
            token_next();
            parse_expressionable_root(history_begin(HISTORY_FLAG_NO_COMMA_OPERATOR));
            if(!constant_fold(current_process, node_pop(), &value)){
                compiler_error(current_process, "The value of %s must be an integer constant expression\n", name->sval);
            }
        } else if(!first){
            value.value += 1;
        }
        value.value = constant_value_as(&value, &int_type);
        value.type  = int_type;
        parser_enumerator_push(name->sval, &value, false);
        first = false;
        if(!token_next_is_operator(",")){
            break;
        }
        token_next();
    }
    expect_sym('}');
}

static void parse_datatype_type(struct datatype* dtype){
    struct token* datatype_token            = 0;
    struct token* datatype_secondary_token  = 0;
    // Only modifiers and no type keyword (`unsigned x`, `const n`): the
    // type is int. The name was taken as the type before and failed with
    // "Invalid primitive datatype".
    static struct token implicit_int = { .type = TOKEN_TYPE_KEYWORD, .sval = "int" };
    struct token* next = token_peek_next();
    parser_last_datatype_was_enum = false;
    if(token_is_keyword(next, "enum")){
        // An enumerated type is int here (its constants all fit, and
        // gcc picks unsigned int only for values above INT_MAX).
        token_next();
        parse_enum_specifier();
        parser_last_datatype_was_enum = true;
        datatype_token = &implicit_int;
    } else if(!next || next->type != TOKEN_TYPE_KEYWORD || !keyword_is_datatype(next->sval)){
        datatype_token = &implicit_int;
    } else {
        parser_get_datatype_tokens(&datatype_token, &datatype_secondary_token);
    }
    int expected_type = parser_datatype_expected_for_type_string(datatype_token->sval);

    // For `struct` / `union`, the next token names the type (or is
    // missing, in which case we forge a synthetic identifier).
    if(datatype_is_struct_or_union_for_name(datatype_token->sval)){
        if(token_peek_next()->type == TOKEN_TYPE_IDENTIFIER){
            datatype_token = token_next();
        } else {
            datatype_token = parser_build_random_type_name();
            dtype->flags |= DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
        }
    }

    // `int**` etc.
    int pointer_depth = parser_get_pointer_depth();

    // Keep the spelling so downstream can render it.
    dtype->type_str      = datatype_token->sval;
    dtype->pointer_depth = pointer_depth;
    if(pointer_depth > 0){
        dtype->flags |= DATATYPE_FLAG_IS_POINTER;
    }

    // once. A second parser_datatype_init_type_and_size here
    // undid the `long long` clamp: sizeof(long long) was 16.
    parser_datatype_init(datatype_token, datatype_secondary_token, dtype, pointer_depth, expected_type);
}

// modifier* type modifier*
static void parse_datatype(struct datatype* dtype){
    memset(dtype, 0, sizeof(struct datatype));
    dtype->flags |= DATATYPE_FLAG_IS_SIGNED;

    bool sign_given = parse_datatype_modifiers(dtype);
    // `unsigned` or `signed` alone is an int (parser_get_datatype_tokens).
    parser_sign_given = sign_given;
    parse_datatype_type(dtype);
    parser_sign_given = false;
    sign_given |= parse_datatype_modifiers(dtype);
    // Plain char is unsigned on aarch64 and riscv64; it was
    // always signed, so `char c = 200` read back as -56 there.
    if(dtype->type == DATA_TYPE_CHAR && !sign_given && current_process->target->plain_char_unsigned){
        dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
    }
    if(dtype->flags & DATATYPE_FLAG_IS_BOOL){
        dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
    }
}

// A _Bool object, not a pointer to one or an array of them.
static bool parser_is_bool_object(struct datatype* dtype){
    return (dtype->flags & DATATYPE_FLAG_IS_BOOL)
        && !(dtype->flags & (DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_ARRAY));
}

// Converting a value to _Bool gives 0 or 1 (C11 6.3.1.2): `(value) != 0`.
// Without it `_Bool b = 2` stored 2, and `b = 256` stored 0.
static struct node* parser_bool_value(struct node* value){
    make_exp_parentheses_node(value);
    struct node* parenthesized = node_pop();
    struct constant zero = { 0, datatype_for_int() };
    make_exp_node(parenthesized, constant_node(&zero), "!=");
    return node_pop();
}

// "long int" / "float int" / "double int" are tolerated abbreviations
// where `int` is purely decorative. Anything else with a trailing
// `int` is a hard error.
static bool parser_is_int_valid_after_datatype(struct datatype* dtype){
    return dtype->type == DATA_TYPE_LONG
        || dtype->type == DATA_TYPE_SHORT
        || dtype->type == DATA_TYPE_FLOAT
        || dtype->type == DATA_TYPE_DOUBLE;
}

static void parser_ignore_int(struct datatype* dtype){
    if(!token_is_keyword(token_peek_next(), "int")){
        return;
    }
    if(!parser_is_int_valid_after_datatype(dtype)){
        compiler_error(current_process,
            "You provided a secondary \"int\" type however its not supported with this current abbrevation\n");
    }
    token_next();   // swallow the redundant "int"
}

// Parse an expression at "root level" - read it, then leave its node
// on the stack for the caller to pop. Used when a sub-grammar (like
// the RHS of `int x = E;`) needs to embed an expressionable result.
// The value parser_bool_value already made: `(x) != 0`.
static bool parser_is_bool_value(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, "!=")
        && node->exp.left->type == NODE_TYPE_EXPRESSION_PARENTHESES
        && node->exp.right->type == NODE_TYPE_NUMBER && node->exp.right->llnum == 0;
}

// Assignments to a _Bool store 0 or 1: `b = x` becomes `b = (x) != 0`
// and `b op= x` becomes `b = (b op x) != 0`. This runs on the finished
// expression, where the tree is in its final shape; while the operator
// is parsed, `s.b = 1` has only `b` as the left operand.
static void parser_convert_bool_assignments(struct node* node){
    if(!node){
        return;
    }
    switch(node->type){
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            parser_convert_bool_assignments(node->parenthesis.exp);
            return;
        case NODE_TYPE_UNARY:
            parser_convert_bool_assignments(node->unary.operand);
            return;
        case NODE_TYPE_CAST:
            parser_convert_bool_assignments(node->cast.operand);
            return;
        case NODE_TYPE_TENARY:
            parser_convert_bool_assignments(node->tenary.true_node);
            parser_convert_bool_assignments(node->tenary.false_node);
            return;
        case NODE_TYPE_EXPRESSION:
            break;
        default:
            return;
    }
    parser_convert_bool_assignments(node->exp.left);
    parser_convert_bool_assignments(node->exp.right);
    struct datatype left_type;
    if(!is_node_assignment(node) || parser_is_bool_value(node->exp.right)
       || !parser_expression_type(node->exp.left, &left_type) || !parser_is_bool_object(&left_type)){
        return;
    }
    if(!S_EQ(node->exp.op, "=")){
        // `|=` -> `|`: the operator without its trailing `=`.
        char* op = strdup(node->exp.op);
        op[strlen(op) - 1] = '\0';
        make_exp_node(node->exp.left, node->exp.right, op);
        node->exp.right = node_pop();
        node->exp.op    = "=";
    }
    node->exp.right = parser_bool_value(node->exp.right);
}

static void parse_expressionable_root(struct history* history){
    size_t count_before = node_count();
    parse_expressionable(history);
    parser_expect_new_node(count_before, "an expression");
    struct node* result_node = node_pop();
    // Only a program that declares a _Bool can assign to one.
    if(parser_saw_bool){
        parser_convert_bool_assignments(result_node);
    }
    node_push(result_node);
}

// ch97: private payload for the struct-forward-ref fixup. fix() looks
// up the named struct in the symbol table and patches the variable's
// datatype with the resolved size + struct_node pointer.
struct datatype_struct_node_fix_private {
    struct node* node;
};

static bool datatype_struct_node_fix(struct fixup* fixup){
    struct datatype_struct_node_fix_private* priv = fixup_private(fixup);
    struct datatype* dtype = &priv->node->var.type;
    dtype->type        = DATA_TYPE_STRUCT;
    dtype->size        = size_of_struct(dtype->type_str);
    dtype->struct_node = struct_node_for_name(current_process, dtype->type_str);
    if(!dtype->struct_node){
        // A pointer to a struct that is never defined is fine (C11
        // 6.2.5p22) as long as nothing reaches through it.
        if(dtype->flags & DATATYPE_FLAG_IS_POINTER){
            return true;
        }
        compiler_error(current_process, "struct %s is never defined, so %s has no size\n",
            dtype->type_str, priv->node->var.name ? priv->node->var.name : "a value");
    }
    return true;
}

static void datatype_struct_node_end(struct fixup* fixup){
    free(fixup_private(fixup));
}

static void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node){
    const char* name_str = 0;
    if(name_token){
        name_str = name_token->sval;
    }
    node_create(&(struct node){
        .type     = NODE_TYPE_VARIABLE,
        .var.name = name_str,
        .var.type = *dtype,
        .var.val  = value_node,
    });
    // ch97: if the variable references a struct we haven't seen yet,
    // register a fixup to backfill the datatype once parsing finishes.
    struct node* var_node = node_peek_or_null();
    if(var_node->var.type.type == DATA_TYPE_STRUCT && !var_node->var.type.struct_node){
        struct datatype_struct_node_fix_private* priv = calloc(1, sizeof(*priv));
        priv->node = var_node;
        fixup_register(parser_fixup_sys, &(struct fixup_config){
            .fix     = datatype_struct_node_fix,
            .end     = datatype_struct_node_end,
            .private = priv,
        });
    }
}

// ch58/73/74: stack-offset computation. Locals grow downward
// (negative). Function args grow upward starting at the function's
// stack_addition (defaults to 8 = saved EBP + return EIP).
static void parser_scope_offset_for_stack(struct node* node, struct history* history){
    struct parser_scope_entity* last_entity = parser_scope_last_entity_stop_global_scope();
    bool upward_stack = history->flags & HISTORY_FLAG_IS_UPWARD_STACK;
    int offset = -(int)variable_size(node);

    if(upward_stack){
        // ch74: anchor to the function's stack_addition; subsequent
        // args step forward by the previous arg's datatype size.
        size_t stack_addition = function_node_argument_stack_addition(parser_current_function);
        offset = (int)stack_addition;
        if(last_entity){
            offset = (int)datatype_size(&variable_node(last_entity->node)->var.type);
            // on the register-argument targets every argument
            // was spilled into its own 8-byte home-area slot, so step
            // by whole slots. x86 keeps the exact type size (its
            // output is locked).
            if(current_process->target->max_reg_args > 0){
                offset = align_value(offset, current_process->target->arg_slot_size);
            }
        }
    }

    if(last_entity){
        offset += variable_node(last_entity->node)->var.aoffset;
        if(variable_node_is_primitive(node)){
            variable_node(node)->var.padding =
                padding(upward_stack ? offset : -offset, node->var.type.size);
        }
    }

    // ch139: struct/union members in a stack frame respect the
    // body's padded flag. Tail expression also folds the per-variable
    // padding into the recorded aoffset so codegen gets the address
    // directly.
    struct node* body = variable_struct_or_union_body_node(node);
    if(body && body->body.padded){
        variable_node(node)->var.padding = padding(upward_stack ? offset : -offset, DATA_SIZE_DWORD);
    }
    variable_node(node)->var.aoffset = offset + (upward_stack
        ?  variable_node(node)->var.padding
        : -variable_node(node)->var.padding);
}

// Global vars don't live on the stack; they get offset 0 and the
// codegen will emit them into the data segment.
static void parser_scope_offset_for_global(struct node* node, struct history* history){
    (void)node; (void)history;
}

// Struct members lay out upward (low to high): each new field starts
// at the previous field's `stack_offset + size`, then padded up.
// The member starts after the previous one ends (its whole size, so an
// array counts every element), rounded up to its own alignment. This
// offset is the one member access uses (struct_offset). It used to add
// the previous member's element size, and to align only scalars.
static void parser_scope_offset_for_structure(struct node* node, struct history* history){
    (void)history;
    int offset = 0;
    // the previous member of this struct only. scope_last_entity
    // also searches the enclosing scopes, so the first member came
    // after the last global (`int g; struct s { long a; };` gave the
    // struct 4 bytes of padding plus alignment in its size).
    struct parser_scope_entity* last_entity = vector_back_ptr_or_null(current_process->scope.current->entities);
    if(last_entity){
        struct node* last = variable_node(last_entity->node);
        offset = last->var.aoffset + (int)variable_size(last);
    }
    int aligned = align_value(offset, (int)datatype_alignment(&node->var.type));
    node->var.padding = aligned - offset;
    node->var.aoffset = aligned;
}

static void parser_scope_offset(struct node* node, struct history* history){
    if(history->flags & HISTORY_FLAG_IS_GLOBAL_SCOPE){
        parser_scope_offset_for_global(node, history);
        return;
    }
    if(history->flags & HISTORY_FLAG_INSIDE_STRUCTURE){
        parser_scope_offset_for_structure(node, history);
        return;
    }
    if(history->flags & HISTORY_FLAG_INSIDE_UNION){
        // Union members all sit at offset 0; they were laid out like
        // stack locals.
        node->var.aoffset = 0;
        node->var.padding = 0;
        return;
    }
    parser_scope_offset_for_stack(node, history);
}

// Build the variable node and push it into the current scope so name
// resolution can find it later. Stack offset is recorded on the node
// itself so codegen can read it back.
static void make_variable_node_and_register(struct history* history,
                                            struct datatype* dtype,
                                            struct token* name_token,
                                            struct node* value_node){
    make_variable_node(dtype, name_token, value_node);
    struct node* var_node = node_pop();
    parser_enumerator_hide(name_token ? name_token->sval : 0);

    // Only assign a stack offset if we're inside some non-global
    // scope (a function body / struct body). Global vars get offset 0
    // and live in the data segment later.
    if(current_process->scope.current != current_process->scope.root){
        parser_scope_offset(var_node, history);
    }

    // ch61: register the variable in the current scope so subsequent
    // declarations can chain off it (offset math) and name resolution
    // can find it.
    parser_scope_push(
        parser_new_scope_entity(var_node, var_node->var.aoffset, 0),
        var_node->var.type.size);

    // ch149: also register on the resolver side so codegen can find
    // the variable by name later.
    resolver_default_new_scope_entity(current_process->resolver, var_node, var_node->var.aoffset, 0);

    node_push(var_node);
}

// Build a NODE_TYPE_VARIABLE_LIST grouping comma-separated peers
// (`int a, b, c;` -> one var_list containing three NODE_TYPE_VARIABLE).
static void make_variable_list_node(struct vector* var_list_vec){
    node_create(&(struct node){
        .type          = NODE_TYPE_VARIABLE_LIST,
        .var_list.list = var_list_vec,
    });
}

// ch44: eat one or more `[N]` brackets following a declarator name.
// Each one becomes a NODE_TYPE_BRACKET wrapping the inner expression;
// they all get collected into an array_brackets struct stored on the
// datatype's `.array.brackets`.
// `node` folded to a number, or an error naming `what` when it is not
// an integer constant expression.
static struct node* parser_fold_required(struct node* node, const char* what){
    struct constant c;
    if(!constant_fold(current_process, node, &c)){
        compiler_error(current_process, "%s must be an integer constant expression\n", what);
    }
    return constant_node(&c);
}

// A global is initialized at assembly time, so its value must be a
// constant (C11 6.7.9p4). It is folded here and converted to the
// variable's type; the backends write llnum as it is, so `int g = 3+4`
// and `-3` gave 0 and `= h` a garbage value. A string, or
// the address of something, stays as written.
static struct node* parser_global_initializer(struct datatype* dtype, struct token* name_token, struct node* value_node){
    bool is_scalar = !(dtype->flags & DATATYPE_FLAG_IS_ARRAY) && !datatype_is_struct_or_union(dtype);
    if(!is_scalar || value_node->type == NODE_TYPE_STRING){
        return value_node;
    }
    // An address (`&g`, an array or function name) is written as a
    // symbol; the backends printed llnum of the node, a garbage value.
    if((dtype->flags & DATATYPE_FLAG_IS_POINTER)
       && ((value_node->type == NODE_TYPE_UNARY && S_EQ(value_node->unary.op, "&"))
           || value_node->type == NODE_TYPE_IDENTIFIER)){
        struct vector* leaves = vector_create(sizeof(struct initializer_leaf));
        parser_add_leaf(leaves, 0, dtype, value_node);
        return parser_global_initializer_node(name_token, leaves);
    }
    struct constant c;
    if(!constant_fold(current_process, value_node, &c)){
        compiler_error(current_process, "The initializer of global %s must be a constant expression\n",
            name_token ? name_token->sval : "variable");
    }
    c.value = constant_value_as(&c, dtype);
    c.type  = *dtype;
    return constant_node(&c);
}

// A bracket node holding the number `n`.
static struct node* parser_bracket_of(size_t n){
    struct constant c = { n, datatype_for_int() };
    make_bracket_node(constant_node(&c));
    return node_pop();
}

static struct array_brackets* parse_array_brackets(struct history* history){
    struct array_brackets* brackets = array_brackets_new();
    while(token_next_is_operator("[")){
        expect_op("[");
        if(token_is_symbol(token_peek_next(), ']')){
            // `int x[];` - empty brackets, no size. An initializer gives
            // the size (parse_variable); `int m[][3]` keeps a 0 in the
            // first dimension so the [3] still counts.
            expect_sym(']');
            if(!token_next_is_operator("[")){
                break;
            }
            array_brackets_add(brackets, parser_bracket_of(0));
            continue;
        }
        parse_expressionable_root(history);
        expect_sym(']');

        // The size is a constant expression: `int a[N * 2]`, `char
        // s[sizeof(int) + 1]`. Variable-length arrays are not supported.
        struct node* exp_node = parser_fold_required(node_pop(), "An array size");
        make_bracket_node(exp_node);

        struct node* bracket_node = node_pop();
        array_brackets_add(brackets, bracket_node);
    }
    return brackets;
}

// ---- Brace initializers (C11 6.7.9) ----
//
// An initializer is read into leaves: (offset, scalar type, value), one
// per scalar it sets, following the designators and brace elision of
// 6.7.9p17-20. A global keeps the leaves for the backends to write as
// data (NODE_TYPE_INITIALIZER); a local becomes stores after the
// declaration (parser_lower_local_initializer).

static void parse_initializer_value(struct datatype* type, size_t offset, struct vector* leaves);

static bool parser_is_aggregate(struct datatype* type){
    return (type->flags & DATATYPE_FLAG_IS_ARRAY)
        || (datatype_is_struct_or_union(type) && !(type->flags & DATATYPE_FLAG_IS_POINTER));
}

// The first dimension of an array type; 0 when it is `[]`.
static size_t parser_array_length(struct datatype* type){
    struct vector* brackets = type->array.brackets ? type->array.brackets->n_brackets : 0;
    if(!brackets || !vector_count(brackets)){
        return 0;
    }
    struct node* first = *(struct node**)vector_at(brackets, 0);
    return first->bracket.inner->llnum;
}

// The element type of an array type: the same type with its first
// dimension gone.
static struct datatype parser_array_element_type(struct datatype* type){
    struct datatype element = *type;
    struct vector* brackets = type->array.brackets ? type->array.brackets->n_brackets : 0;
    int count = brackets ? vector_count(brackets) : 0;
    if(count <= 1){
        element.flags &= ~DATATYPE_FLAG_IS_ARRAY;
        element.array.brackets = 0;
        element.array.size     = 0;
        return element;
    }
    element.array.brackets = array_brackets_new();
    for(int i = 1; i < count; i++){
        array_brackets_add(element.array.brackets, *(struct node**)vector_at(brackets, i));
    }
    element.array.size = array_brackets_calculate_size(&element, element.array.brackets);
    return element;
}

// The members of a struct or union, `int a, b;` lists flattened.
static struct vector* parser_struct_members(struct datatype* type){
    struct vector* members = vector_create(sizeof(struct node*));
    struct node* su = type->struct_node;
    if(!su || !su->_struct.body_n){
        compiler_error(current_process, "Cannot initialize an incomplete struct or union\n");
    }
    struct vector* statements = su->_struct.body_n->body.statements;
    for(int i = 0; i < vector_count(statements); i++){
        struct node* statement = *(struct node**)vector_at(statements, i);
        if(statement->type == NODE_TYPE_VARIABLE){
            vector_push(members, &statement);
        } else if(statement->type == NODE_TYPE_VARIABLE_LIST){
            for(int j = 0; j < vector_count(statement->var_list.list); j++){
                vector_push(members, vector_at(statement->var_list.list, j));
            }
        }
    }
    return members;
}

// Element `index` of an aggregate: its type and offset from the start.
static void parser_aggregate_element(struct datatype* type, struct vector* members, size_t index,
                                     struct datatype* element_out, size_t* offset_out){
    if(type->flags & DATATYPE_FLAG_IS_ARRAY){
        *element_out = parser_array_element_type(type);
        *offset_out  = index * datatype_size(element_out);
        return;
    }
    struct node* member = *(struct node**)vector_at(members, index);
    *element_out = member->var.type;
    *offset_out  = member->var.aoffset;
}

static void parser_add_leaf(struct vector* leaves, size_t offset, struct datatype* type, struct node* value){
    struct initializer_leaf leaf = { .offset = offset, .type = *type, .value = value };
    vector_push(leaves, &leaf);
}

// `char s[N] = "abc"`: one leaf per byte, the NUL included when it
// fits. Returns the number of chars the string needs with its NUL.
static size_t parser_string_initializer(struct datatype* type, size_t offset, struct vector* leaves){
    struct token* token = token_next();
    struct buffer* joined = buffer_create();
    for(const char* c = token->sval; *c; c++){
        buffer_write(joined, *c);
    }
    while(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING){
        for(const char* c = token_next()->sval; *c; c++){
            buffer_write(joined, *c);
        }
    }
    buffer_write(joined, 0x00);
    const char* str = buffer_ptr(joined);
    size_t needed = strlen(str) + 1;
    size_t length = parser_array_length(type);
    size_t count  = length ? (needed < length ? needed : length) : needed;
    struct datatype element = parser_array_element_type(type);
    for(size_t i = 0; i < count; i++){
        struct constant c = { (unsigned char)str[i], datatype_for_int() };
        parser_add_leaf(leaves, offset + i, &element, constant_node(&c));
    }
    return needed;
}

static bool parser_is_char_array(struct datatype* type){
    return (type->flags & DATATYPE_FLAG_IS_ARRAY) && type->array.brackets
        && vector_count(type->array.brackets->n_brackets) <= 1
        && datatype_element_size(type) == DATA_SIZE_BYTE
        && !(type->flags & DATATYPE_FLAG_IS_POINTER);
}

static bool parser_next_is_designator(void){
    return token_next_is_operator("[") || token_next_is_operator(".");
}

// One designator, `[const]` or `.member`; returns the index it selects.
static size_t parse_designator(struct datatype* type, struct vector* members){
    if(token_next_is_operator("[")){
        if(!(type->flags & DATATYPE_FLAG_IS_ARRAY)){
            compiler_error(current_process, "An array designator [ ] used on a non-array\n");
        }
        expect_op("[");
        parse_expressionable_root(history_begin(0));
        struct node* index = parser_fold_required(node_pop(), "An array designator");
        expect_sym(']');
        size_t length = parser_array_length(type);
        if((long long)index->llnum < 0 || (length && index->llnum >= length)){
            compiler_error(current_process, "Array designator index %lld is outside the array\n", (long long)index->llnum);
        }
        return index->llnum;
    }
    expect_op(".");
    struct token* name = token_next();
    if(!members || !token_is_identifier(name)){
        compiler_error(current_process, "A member designator . used on a non-struct\n");
    }
    for(int i = 0; i < vector_count(members); i++){
        struct node* member = *(struct node**)vector_at(members, i);
        if(member->var.name && S_EQ(member->var.name, name->sval)){
            return i;
        }
    }
    compiler_error(current_process, "No member named %s to initialize\n", name->sval);
    return 0;
}

// The rest of a designation after its first designator, then `=` and
// the value: `.a.b = 1`, `[1].x = 2`.
static void parse_designation_rest(struct datatype* type, size_t offset, struct vector* leaves){
    if(parser_next_is_designator()){
        struct vector* members = (type->flags & DATATYPE_FLAG_IS_ARRAY) ? 0
            : datatype_is_struct_or_union(type) ? parser_struct_members(type) : 0;
        size_t index = parse_designator(type, members);
        struct datatype element;
        size_t element_offset;
        parser_aggregate_element(type, members, index, &element, &element_offset);
        parse_designation_rest(&element, offset + element_offset, leaves);
        return;
    }
    expect_op("=");
    parse_initializer_value(type, offset, leaves);
}

// The elements of an aggregate from the current list. With `braced`
// the list is this aggregate's own `{ }`; without, the aggregate takes
// what it needs from the enclosing list (brace elision) and stops at a
// designator, which belongs to the enclosing list. Returns how many
// array elements were set (the highest index + 1).
static size_t parse_initializer_elements(struct datatype* type, size_t offset, struct vector* leaves, bool braced){
    bool is_array = type->flags & DATATYPE_FLAG_IS_ARRAY;
    bool is_union = !is_array && type->type == DATA_TYPE_UNION;
    struct vector* members = is_array ? 0 : parser_struct_members(type);
    size_t count = is_array ? parser_array_length(type) : (size_t)vector_count(members);
    size_t index = 0;
    size_t highest = 0;
    while(!token_next_is_symbol('}')){
        bool designated = false;
        if(parser_next_is_designator()){
            if(!braced){
                break;
            }
            index = parse_designator(type, members);
            designated = true;
        }
        if(count && index >= count){
            if(!braced){
                break;
            }
            compiler_error(current_process, "Too many initializers for the object\n");
        }
        struct datatype element;
        size_t element_offset;
        parser_aggregate_element(type, members, index, &element, &element_offset);
        if(designated){
            parse_designation_rest(&element, offset + element_offset, leaves);
        } else {
            parse_initializer_value(&element, offset + element_offset, leaves);
        }
        index++;
        if(index > highest){
            highest = index;
        }
        // A union takes one member unless a designator names another.
        bool full = (count && index >= count) || (is_union && !braced);
        if(full && !braced){
            break;
        }
        if(!token_next_is_operator(",")){
            break;
        }
        if(!braced){
            // Leave the comma to the enclosing list when it ends there or
            // goes on with a designator.
            vector_save(current_process->token_vec);
            token_next();
            bool stop = token_next_is_symbol('}') || parser_next_is_designator();
            vector_restore(current_process->token_vec);
            if(stop){
                break;
            }
        }
        token_next();
    }
    return highest;
}

// The initializer of one object of `type` at `offset`.
static void parse_initializer_value(struct datatype* type, size_t offset, struct vector* leaves){
    bool next_is_string = token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING;
    if(token_next_is_symbol('{')){
        expect_sym('{');
        if(parser_is_char_array(type) && token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING){
            parser_string_initializer(type, offset, leaves);
        } else if(parser_is_aggregate(type)){
            parse_initializer_elements(type, offset, leaves, true);
        } else if(!token_next_is_symbol('}')){
            // `int x = { 5 };`
            parse_initializer_value(type, offset, leaves);
        }
        if(token_next_is_operator(",")){
            token_next();
        }
        expect_sym('}');
        return;
    }
    if(parser_is_char_array(type) && next_is_string){
        parser_string_initializer(type, offset, leaves);
        return;
    }
    if(type->flags & DATATYPE_FLAG_IS_ARRAY){
        parse_initializer_elements(type, offset, leaves, false);
        return;
    }
    // A struct initialized from a struct value, or brace elision into a
    // struct member: an expression of struct type is a value; anything
    // else fills the members.
    parse_expressionable_root(history_begin(HISTORY_FLAG_NO_COMMA_OPERATOR));
    struct node* value = node_pop();
    if(datatype_is_struct_or_union(type) && !(type->flags & DATATYPE_FLAG_IS_POINTER)){
        struct datatype value_type;
        if(!parser_expression_type(value, &value_type) || !datatype_is_struct_or_union(&value_type)
           || (value_type.flags & DATATYPE_FLAG_IS_POINTER)){
            compiler_error(current_process, "Expected { or a struct value to initialize a struct member\n");
        }
    }
    if(parser_is_bool_object(type)){
        value = parser_bool_value(value);
    }
    parser_add_leaf(leaves, offset, type, value);
}

// Reads the initializer after `=` for a variable of `type`. An unsized
// array gets its length here. Returns the leaves, in source order.
static struct vector* parse_initializer(struct datatype* type){
    struct vector* leaves = vector_create(sizeof(struct initializer_leaf));
    bool unsized = (type->flags & DATATYPE_FLAG_IS_ARRAY) && parser_array_length(type) == 0;
    size_t length = 0;
    bool next_is_string = token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING;
    if(parser_is_char_array(type) && next_is_string){
        length = parser_string_initializer(type, 0, leaves);
    } else if(token_next_is_symbol('{') && (type->flags & DATATYPE_FLAG_IS_ARRAY)){
        expect_sym('{');
        if(parser_is_char_array(type) && token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING){
            length = parser_string_initializer(type, 0, leaves);
            if(token_next_is_operator(",")){
                token_next();
            }
        } else {
            length = parse_initializer_elements(type, 0, leaves, true);
        }
        expect_sym('}');
    } else {
        parse_initializer_value(type, 0, leaves);
    }

    if(unsized){
        // The length becomes the first dimension.
        // `int m[][3]` carries a 0 first dimension (parse_array_brackets);
        // `int a[]` carries none.
        struct array_brackets* brackets = array_brackets_new();
        array_brackets_add(brackets, parser_bracket_of(length));
        struct vector* old = type->array.brackets ? type->array.brackets->n_brackets : 0;
        int old_count = old ? vector_count(old) : 0;
        for(int i = old_count ? 1 : 0; i < old_count; i++){
            array_brackets_add(brackets, *(struct node**)vector_at(old, i));
        }
        type->array.brackets = brackets;
        type->array.size     = array_brackets_calculate_size(type, brackets);
    }
    return leaves;
}

// Sorted by offset; a later leaf replaces an earlier one it overlaps
// (`{ [0] = 1, [0] = 2 }` keeps 2).
static struct vector* parser_sorted_leaves(struct vector* leaves){
    struct vector* sorted = vector_create(sizeof(struct initializer_leaf));
    int n = vector_count(leaves);
    for(int i = 0; i < n; i++){
        struct initializer_leaf* leaf = vector_at(leaves, i);
        size_t start = leaf->offset;
        size_t end   = start + datatype_size(&leaf->type);
        bool overwritten = false;
        for(int j = i + 1; j < n && !overwritten; j++){
            struct initializer_leaf* later = vector_at(leaves, j);
            size_t later_end = later->offset + datatype_size(&later->type);
            overwritten = later->offset < end && start < later_end;
        }
        if(overwritten){
            continue;
        }
        int at = vector_count(sorted);
        while(at > 0 && ((struct initializer_leaf*)vector_at(sorted, at - 1))->offset > start){
            at--;
        }
        vector_push_at(sorted, at, leaf);
    }
    return sorted;
}

// A global's leaves must be constants, string literals for pointers,
// or addresses (`&g`, an array or function name). Constants are folded
// and converted to the leaf's type.
static struct node* parser_global_initializer_node(struct token* name_token, struct vector* leaves){
    struct vector* sorted = parser_sorted_leaves(leaves);
    for(int i = 0; i < vector_count(sorted); i++){
        struct initializer_leaf* leaf = vector_at(sorted, i);
        struct node* value = leaf->value;
        bool is_address = (value->type == NODE_TYPE_UNARY && S_EQ(value->unary.op, "&")
                           && value->unary.operand->type == NODE_TYPE_IDENTIFIER)
                       || value->type == NODE_TYPE_IDENTIFIER
                       || value->type == NODE_TYPE_STRING;
        if(is_address && (leaf->type.flags & DATATYPE_FLAG_IS_POINTER)){
            continue;
        }
        struct constant c;
        if(datatype_is_struct_or_union(&leaf->type) && !(leaf->type.flags & DATATYPE_FLAG_IS_POINTER)){
            compiler_error(current_process, "The initializer of global %s must be a constant expression\n",
                name_token ? name_token->sval : "variable");
        }
        if(!constant_fold(current_process, value, &c)){
            compiler_error(current_process, "The initializer of global %s must be a constant expression\n",
                name_token ? name_token->sval : "variable");
        }
        c.value = constant_value_as(&c, &leaf->type);
        c.type  = leaf->type;
        leaf->value = constant_node(&c);
    }
    node_create(&(struct node){ .type = NODE_TYPE_INITIALIZER, .init.leaves = sorted });
    return node_pop();
}

// A pointer to `type`, as a cast target.
static struct datatype parser_pointer_to(struct datatype* type){
    struct datatype pointer = *type;
    pointer.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    pointer.array.brackets = 0;
    pointer.array.size     = 0;
    pointer.flags |= DATATYPE_FLAG_IS_POINTER;
    pointer.pointer_depth = type->pointer_depth + 1;
    return pointer;
}

// `*(T*)((char*)&name + offset)`, the object a leaf sets.
static struct node* parser_object_at(const char* name, size_t offset, struct datatype* type){
    struct datatype char_type = { .type = DATA_TYPE_CHAR, .size = DATA_SIZE_BYTE, .type_str = "char" };
    struct datatype char_pointer = parser_pointer_to(&char_type);
    node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = name });
    make_unary_node("&", node_pop());
    make_cast_node(&char_pointer, node_pop());
    struct node* address = node_pop();
    if(offset){
        struct constant c = { offset, datatype_for_int() };
        make_exp_node(address, constant_node(&c), "+");
        make_exp_parentheses_node(node_pop());
        address = node_pop();
    }
    struct datatype target = parser_pointer_to(type);
    make_cast_node(&target, address);
    make_exp_parentheses_node(node_pop());
    make_unary_node("*", node_pop());
    struct node* object = node_pop();
    object->unary.indirection.depth = 1;
    return object;
}

static void parser_pending_store(const char* name, size_t offset, struct datatype* type, struct node* value){
    make_exp_node(parser_object_at(name, offset, type), value, "=");
    struct node* store = node_pop();
    vector_push(parser_pending_statements, &store);
}

// A local's initializer: the bytes no leaf sets are zeroed (6.7.9p21),
// in the widest stores that fit, then each leaf is stored. The stores
// run right after the declaration.
static void parser_lower_local_initializer(const char* name, struct datatype* type, struct vector* leaves){
    struct vector* sorted = parser_sorted_leaves(leaves);
    size_t size = datatype_size(type);
    char* covered = calloc(size ? size : 1, 1);
    for(int i = 0; i < vector_count(sorted); i++){
        struct initializer_leaf* leaf = vector_at(sorted, i);
        size_t end = leaf->offset + datatype_size(&leaf->type);
        for(size_t b = leaf->offset; b < end && b < size; b++){
            covered[b] = 1;
        }
    }
    size_t widest = current_process->target->long_size;
    for(size_t b = 0; b < size;){
        if(covered[b]){
            b++;
            continue;
        }
        size_t chunk = widest;
        while(chunk > 1 && (b % chunk || b + chunk > size || memchr(covered + b, 1, chunk))){
            chunk /= 2;
        }
        struct datatype zero_type = chunk == DATA_SIZE_BYTE ? (struct datatype){ .type = DATA_TYPE_CHAR, .size = 1, .type_str = "char" }
                                  : chunk == DATA_SIZE_WORD ? (struct datatype){ .type = DATA_TYPE_SHORT, .size = 2, .type_str = "short" }
                                  : chunk == DATA_SIZE_DWORD ? (struct datatype){ .type = DATA_TYPE_INTEGER, .size = 4, .type_str = "int" }
                                  : (struct datatype){ .type = DATA_TYPE_LONG, .size = 8, .type_str = "long" };
        struct constant zero = { 0, datatype_for_int() };
        parser_pending_store(name, b, &zero_type, constant_node(&zero));
        b += chunk;
    }
    free(covered);
    for(int i = 0; i < vector_count(sorted); i++){
        struct initializer_leaf* leaf = vector_at(sorted, i);
        parser_pending_store(name, leaf->offset, &leaf->type, leaf->value);
    }
}

// `(T){ ... }` (C11 6.5.2.5): an unnamed object initialized like a
// variable. In a function it is a hidden local `__compound_N`, declared
// and initialized just before the statement that uses it; at file scope
// a hidden global. The expression is that object's name.
static void parse_compound_literal(struct datatype* dtype){
    static int count = 0;
    char* name = malloc(32);
    sprintf(name, "__compound_%i", count++);
    struct token* name_token = calloc(1, sizeof(struct token));
    name_token->type = TOKEN_TYPE_IDENTIFIER;
    name_token->sval = name;
    name_token->pos  = current_process->pos;

    struct datatype type = *dtype;
    struct vector* leaves = parse_initializer(&type);
    if(!parser_current_function || current_process->scope.current == current_process->scope.root){
        make_variable_node(&type, name_token, parser_global_initializer_node(name_token, leaves));
        struct node* var_node = node_pop();
        vector_push(current_process->node_tree_vec, &var_node);
    } else {
        int pending_before = vector_count(parser_pending_statements);
        make_variable_node_and_register(history_begin(0), &type, name_token, 0);
        struct node* var_node = node_pop();
        vector_push(parser_preceding_statements, &var_node);
        parser_lower_local_initializer(name, &type, leaves);
        for(int i = pending_before; i < vector_count(parser_pending_statements); i++){
            vector_push(parser_preceding_statements, vector_at(parser_pending_statements, i));
        }
        while(vector_count(parser_pending_statements) > pending_before){
            vector_pop(parser_pending_statements);
        }
    }
    node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = name });
}

// `static int n = 10;` inside a function keeps its value between calls
// (C11 6.2.4p3); it was an ordinary stack variable. It
// becomes a global named `__static_<function>_<name>_<N>`, initialized
// like one, and the local name stands for it in this scope. The
// declaration statement itself is blank.
static void parse_static_local(struct datatype* dtype, struct token* name_token, struct node* value_node){
    static int count = 0;
    const char* function = parser_current_function->func.name;
    char* name = malloc(strlen(function) + strlen(name_token->sval) + 32);
    sprintf(name, "__static_%s_%s_%i", function, name_token->sval, count++);
    if(value_node && value_node->type != NODE_TYPE_INITIALIZER){
        value_node = parser_global_initializer(dtype, name_token, value_node);
    }

    struct token* global_name = calloc(1, sizeof(struct token));
    *global_name = *name_token;
    global_name->sval = name;
    make_variable_node(dtype, global_name, value_node);
    struct node* var_node = node_pop();
    vector_push(current_process->node_tree_vec, &var_node);

    struct parser_enumerator e = { .name = name_token->sval, .depth = parser_scope_depth(), .rename = name };
    vector_push(parser_enumerators, &e);
    node_create(&(struct node){ .type = NODE_TYPE_BLANK });
}

// Variable declarator: `name` already consumed; optionally followed by
// `[N]...` (ch44) or `= expr`.
static void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history){
    struct node* value_node = 0;

    // ch44: array brackets, e.g. `int x[4][3];`
    if(token_next_is_operator("[")){
        struct array_brackets* brackets = parse_array_brackets(history);
        dtype->array.brackets = brackets;
        dtype->array.size     = array_brackets_calculate_size(dtype, brackets);
        dtype->flags         |= DATATYPE_FLAG_IS_ARRAY;
    }

    bool is_global = current_process->scope.current == current_process->scope.root;
    bool is_static_local = (dtype->flags & DATATYPE_FLAG_IS_STATIC) && name_token && parser_current_function
       && !is_global
       && !(history->flags & (HISTORY_FLAG_IS_UPWARD_STACK | HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION));
    struct vector* leaves = 0;
    if(token_next_is_operator("=")){
        token_next();
        bool string_for_array = parser_is_char_array(dtype)
            && token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING;
        if(token_next_is_symbol('{') || string_for_array){
            // A brace initializer, or `char s[] = "hi"`, which stored the
            // low byte of the string's address.
            leaves = parse_initializer(dtype);
            if(is_global || is_static_local){
                value_node = parser_global_initializer_node(name_token, leaves);
            }
        } else {
            parse_expressionable_root(history_down(history, history->flags | HISTORY_FLAG_NO_COMMA_OPERATOR));
            value_node = node_pop();
            if(parser_is_bool_object(dtype)){
                value_node = parser_bool_value(value_node);
            }
            if(is_global){
                value_node = parser_global_initializer(dtype, name_token, value_node);
            }
        }
    }

    if(is_static_local){
        parse_static_local(dtype, name_token, value_node);
        return;
    }

    // `struct T v = w;` copies the whole struct: it becomes `v = w;`
    // after the declaration. The initializer path stored one register's
    // worth of bytes.
    struct node* struct_value = 0;
    if(!is_global && value_node && name_token && datatype_is_struct_or_union(dtype)
       && !(dtype->flags & (DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_ARRAY))){
        struct_value = value_node;
        value_node   = 0;
    }

    make_variable_node_and_register(history, dtype, name_token, value_node);
    if(struct_value){
        node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = name_token->sval });
        make_exp_node(node_pop(), struct_value, "=");
        struct node* copy = node_pop();
        vector_push(parser_pending_statements, &copy);
    }
    if(leaves && !is_global){
        if(!name_token){
            compiler_error(current_process, "An initializer needs a variable name\n");
        }
        parser_lower_local_initializer(name_token->sval, dtype, leaves);
    }
}

// ch72: parse a function body. Adds INSIDE_FUNCTION_BODY to history so
// nested code (variable decls inside the function) gets stack offsets.
static void parse_function_body(struct history* history){
    parse_body(0, history_down(history, history->flags | HISTORY_FLAG_INSIDE_FUNCTION_BODY));
}

// ch73 forward decl - definition is below.
static struct vector* parse_function_arguments(struct history* history);

// Consume exactly `amount` "." operators (for `...` variadics).
static void token_read_dots(size_t amount){
    for(size_t i = 0; i < amount; i++){
        expect_op(".");
    }
}

// ch73: parse one parameter: a datatype + optional identifier name.
// Routes through parse_variable so the existing scope-push machinery
// kicks in (with HISTORY_FLAG_IS_UPWARD_STACK set by the caller).
static void parse_variable_full(struct history* history){
    struct datatype dtype;
    parse_datatype(&dtype);

    struct token* name_token = 0;
    if(!parse_function_pointer_declarator(&dtype, &name_token) && token_is_identifier(token_peek_next())){
        name_token = token_next();
    }
    parse_variable(&dtype, name_token, history);
}

// ch72: parse a function declaration / definition. The `(` is the next
// token. ch73 fills in argument parsing.
// Labels have function scope (C11 6.2.1p3): the asm label is
// `<function>_<label>`, so two functions may use the same name. It
// was `label_<name>`, and the parser never built a label at all.
static const char* parser_label_name(const char* label){
    const char* function = parser_current_function ? parser_current_function->func.name : "file";
    char* name = malloc(strlen(function) + strlen(label) + 2);
    sprintf(name, "%s_%s", function, label);
    return name;
}

static bool parser_name_in(struct vector* names, const char* name){
    for(int i = 0; i < vector_count(names); i++){
        if(S_EQ(*(const char**)vector_at(names, i), name)){
            return true;
        }
    }
    return false;
}

// Every goto in the function names a label defined in it.
static void parser_check_gotos(const char* function){
    for(int i = 0; i < vector_count(parser_function_gotos); i++){
        const char* name = *(const char**)vector_at(parser_function_gotos, i);
        if(!parser_name_in(parser_function_labels, name)){
            compiler_error(current_process, "goto %s: no such label in %s\n",
                name + strlen(function) + 1, function);
        }
    }
}

static void parse_function(struct datatype* ret_type, struct token* name_token, struct history* history){
    struct vector* arguments_vector = 0;
    parser_scope_new();
    // ch149: open the matching resolver-side scope.
    resolver_default_new_scope(current_process->resolver, 0);
    make_function_node(ret_type, name_token->sval, 0, 0);
    struct node* function_node = node_peek();
    parser_current_function = function_node;

    // For functions returning struct/union by value, the caller pushes
    // a hidden first arg (pointer to result), so the real args start
    // one slot further along. One target arg slot. Not
    // for a pointer to a struct, which comes back in a register.
    if(datatype_is_struct_or_union_non_pointer(ret_type)){
        function_node->func.args.stack_addition += current_process->target->arg_slot_size;
    }

    expect_op("(");
    arguments_vector = parse_function_arguments(history_begin(0));
    expect_sym(')');

    function_node->func.args.vector = arguments_vector;
    if(symresolver_get_symbol_for_native_function(current_process, name_token->sval)){
        function_node->func.flags |= FUNCTION_NODE_FLAG_IS_NATIVE;
    }

    if(token_next_is_symbol('{')){
        parser_function_labels = vector_create(sizeof(const char*));
        parser_function_gotos  = vector_create(sizeof(const char*));
        parse_function_body(history_begin(0));
        struct node* body_node = node_pop();
        function_node->func.body_n = body_node;
        parser_check_gotos(name_token->sval);
    } else {
        expect_sym(';');
    }

    parser_current_function = 0;
    // ch149: close the resolver-side scope first.
    resolver_default_finish_scope(current_process->resolver);
    parser_scope_finish();
}

// ch63: handle the symbol case at statement-start. Only `{` is
// supported for now - opens a brace body. Other symbols still error.
static void parse_symbol(void){
    if(token_next_is_symbol('{')){
        size_t variable_size = 0;
        struct history* history = history_begin(HISTORY_FLAG_IS_GLOBAL_SCOPE);
        parse_body(&variable_size, history);
        struct node* body_node = node_pop();
        node_push(body_node);
        return;
    }
    compiler_error(current_process, "Unexpected %s\n", parser_token_text(token_peek_next()));
}

// One statement: keyword-led (declarations / control-flow) or a bare
// expression-statement terminated by `;`.
// `__asm__ [volatile] ("text" [: ...]);`. Basic asm: the
// strings are one instruction text; clobbers after `:` are read and
// ignored (nothing lives in registers across a statement), operands
// are not supported.
static bool parser_next_is_asm(void){
    struct token* token = token_peek_next();
    return token_is_identifier(token) && (S_EQ(token->sval, "__asm__") || S_EQ(token->sval, "asm"));
}

static void parse_asm(void){
    token_next();
    while(token_is_keyword(token_peek_next(), "volatile")
          || (token_is_identifier(token_peek_next()) && S_EQ(token_peek_next()->sval, "__volatile__"))){
        token_next();
    }
    expect_op("(");
    struct buffer* text = buffer_create();
    while(token_peek_next()->type == TOKEN_TYPE_STRING){
        for(const char* c = token_next()->sval; *c; c++){
            buffer_write(text, *c);
        }
    }
    buffer_write(text, 0);
    while(!token_next_is_symbol(')')){
        struct token* token = token_next();
        if(token_is_operator(token, "(")){
            compiler_error(current_process, "asm operands are not supported, only clobbers\n");
        }
    }
    expect_sym(')');
    expect_sym(';');
    node_create(&(struct node){ .type = NODE_TYPE_ASM, .sval = buffer_ptr(text) });
}

static void parse_statement(struct history* history){
    if(parser_next_is_asm()){
        parse_asm();
        return;
    }
    if(!token_peek_next()){
        compiler_error(current_process, "Expected a statement or '}' but found the end of the file\n");
    }
    // `;` alone is an empty statement; a blank node keeps the
    // one-node-per-statement rule.
    if(token_is_symbol(token_peek_next(), ';')){
        expect_sym(';');
        node_create(&(struct node){ .type = NODE_TYPE_BLANK });
        return;
    }
    // `name:` is a label. It becomes a statement of its own; the
    // statement it labels follows as the next one.
    if(token_is_identifier(token_peek_next())){
        vector_save(current_process->token_vec);
        struct token* name = token_next();
        bool is_label = token_is_symbol(token_peek_next(), ':');
        if(!is_label){
            vector_restore(current_process->token_vec);
        } else {
            vector_save_purge(current_process->token_vec);
            expect_sym(':');
            const char* label = parser_label_name(name->sval);
            if(parser_function_labels){
                if(parser_name_in(parser_function_labels, label)){
                    compiler_error(current_process, "Label %s is defined twice\n", name->sval);
                }
                vector_push(parser_function_labels, &label);
            }
            node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = label });
            make_label_node(node_pop());
            return;
        }
    }
    // `{ ... }` is a compound statement with its own scope.
    if(token_is_symbol(token_peek_next(), '{')){
        size_t var_size = 0;
        parse_body(&var_size, history);
        return;
    }
    if(token_peek_next()->type == TOKEN_TYPE_KEYWORD){
        size_t count_before = node_count();
        struct token* keyword = token_peek_next();
        parse_keyword(history);
        if(node_count() <= count_before){
            compiler_error(current_process, "The keyword '%s' is not supported here\n", keyword->sval);
        }
        return;
    }

    parse_expressionable_root(history);
    if(token_peek_next()->type == TOKEN_TYPE_SYMBOL
       && !token_is_symbol(token_peek_next(), ';')){
        parse_symbol();
        return;
    }
    expect_sym(';');
}

static void parser_append_size_for_node(struct history* history, size_t* _variable_size, struct node* node);

static void parser_append_size_for_node_struct_union(struct history* history,
                                                    size_t* _variable_size,
                                                    struct node* node){
    *_variable_size += variable_size(node);
    if(node->var.type.flags & DATATYPE_FLAG_IS_POINTER){
        return;
    }
    struct node* body = variable_struct_or_union_body_node(node);
    if(body){
        struct node* largest = body->body.largest_var_node;
        if(largest){
            *_variable_size += align_value(*_variable_size, largest->var.type.size);
        }
    }
}

static void parser_append_size_for_variable_list(struct history* history,
                                                 size_t* variable_size,
                                                 struct vector* vec){
    vector_set_peek_pointer(vec, 0);
    struct node* node = vector_peek_ptr(vec);
    while(node){
        parser_append_size_for_node(history, variable_size, node);
        node = vector_peek_ptr(vec);
    }
}

static void parser_append_size_for_node(struct history* history, size_t* _variable_size, struct node* node){
    if(!node){
        return;
    }
    if(node->type == NODE_TYPE_VARIABLE){
        if(node_is_struct_or_union_variable(node)){
            parser_append_size_for_node_struct_union(history, _variable_size, node);
            return;
        }
        *_variable_size += variable_size(node);
    } else if(node->type == NODE_TYPE_VARIABLE_LIST){
        parser_append_size_for_variable_list(history, _variable_size, node->var_list.list);
    }
}

static void parser_struct_member_extent(struct node* member, size_t* end, size_t* align){
    size_t member_end = member->var.aoffset + variable_size(member);
    size_t member_align = datatype_alignment(&member->var.type);
    if(member_end > *end){
        *end = member_end;
    }
    if(member_align > *align){
        *align = member_align;
    }
}

// A struct ends after its last member, a union after its largest; both
// round up to their strictest member alignment (C11 6.7.2.1p15-17).
// The offsets come from parser_scope_offset_for_structure (union
// members are all at 0).
static void parser_finalize_struct_or_union_body(struct node* body_node, struct vector* body_vec,
                                                 struct node* largest_align_eligible_var_node){
    size_t end   = 0;
    size_t align = 1;
    vector_set_peek_pointer(body_vec, 0);
    struct node* statement = vector_peek_ptr(body_vec);
    while(statement){
        if(statement->type == NODE_TYPE_VARIABLE){
            parser_struct_member_extent(statement, &end, &align);
        } else if(statement->type == NODE_TYPE_VARIABLE_LIST){
            struct vector* list = statement->var_list.list;
            vector_set_peek_pointer(list, 0);
            struct node* member = vector_peek_ptr(list);
            while(member){
                parser_struct_member_extent(member, &end, &align);
                member = vector_peek_ptr(list);
            }
        }
        statement = vector_peek_ptr(body_vec);
    }

    int pad = compute_sum_padding(body_vec);
    body_node->body.largest_var_node = largest_align_eligible_var_node;
    body_node->body.padded           = (pad != 0);
    body_node->body.align            = align;
    body_node->body.size             = align_value((int)end, (int)align);
    body_node->body.statements       = body_vec;
}

// ch55: real body finalization. Unions get sized to their largest
// member; otherwise we sum padding, then realign to the largest
// align-eligible variable's natural size.
static void parser_finalize_body(struct history* history, struct node* body_node,
                                 struct vector* body_vec, size_t* _variable_size,
                                 struct node* largest_align_eligible_var_node,
                                 struct node* largest_possible_var_node){
    if(history->flags & (HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION)){
        parser_finalize_struct_or_union_body(body_node, body_vec, largest_align_eligible_var_node);
        *_variable_size = body_node->body.size;
        return;
    }
    int pad = compute_sum_padding(body_vec);
    *_variable_size += pad;

    if(largest_align_eligible_var_node){
        *_variable_size = align_value(*_variable_size,
                                      largest_align_eligible_var_node->var.type.size);
    }

    body_node->body.largest_var_node = largest_align_eligible_var_node;
    body_node->body.padded           = (pad != 0);
    body_node->body.size             = *_variable_size;
    body_node->body.statements       = body_vec;
}

static void parser_append_size_for_node(struct history* history, size_t* _variable_size, struct node* node);

// The hidden declarations of a statement's compound literals go right
// before it; their stack space counts like any local's.
static void parser_take_preceding_statements(struct vector* body_vec, struct history* history, size_t* variable_size){
    for(int i = 0; i < vector_count(parser_preceding_statements); i++){
        struct node* statement = *(struct node**)vector_at(parser_preceding_statements, i);
        vector_push(body_vec, &statement);
        if(variable_size && statement->type == NODE_TYPE_VARIABLE){
            parser_append_size_for_node(history, variable_size, statement);
        }
    }
    vector_clear(parser_preceding_statements);
}

// The statements a declaration left (parser_pending_statements) go
// right after it in the body.
static void parser_take_pending_statements(struct vector* body_vec){
    for(int i = 0; i < vector_count(parser_pending_statements); i++){
        vector_push(body_vec, vector_at(parser_pending_statements, i));
    }
    vector_clear(parser_pending_statements);
}

// ch49: single-statement body (no braces), e.g. `y = 30;` in
// `if (x) y = 30;`. Brace bodies come in ch54.
static void parse_body_single_statement(size_t* variable_size, struct vector* body_vec, struct history* history){
    make_body_node(0, 0, false, 0);
    struct node* body_node = node_pop();
    body_node->binded.owner = parser_current_body;
    parser_current_body     = body_node;

    parse_statement(history_down(history, history->flags));
    struct node* stmt_node = node_pop();
    parser_take_preceding_statements(body_vec, history, variable_size);
    vector_push(body_vec, &stmt_node);
    parser_take_pending_statements(body_vec);

    parser_append_size_for_node(history, variable_size, stmt_node);
    struct node* largest_var_node = (stmt_node->type == NODE_TYPE_VARIABLE) ? stmt_node : 0;
    parser_finalize_body(history, body_node, body_vec, variable_size,
                         largest_var_node, largest_var_node);
    parser_current_body = body_node->binded.owner;
    node_push(body_node);
}

// ch62: { stmt; stmt; ... } walk statements between braces, track
// largest var (for alignment), and finalize the body.
static void parse_body_multiple_statements(size_t* variable_size,
                                           struct vector* body_vec,
                                           struct history* history){
    make_body_node(0, 0, false, 0);
    struct node* body_node = node_pop();
    body_node->binded.owner = parser_current_body;
    parser_current_body     = body_node;

    struct node* stmt_node = 0;
    struct node* largest_possible_var_node       = 0;
    struct node* largest_align_eligible_var_node = 0;

    expect_sym('{');

    while(!token_next_is_symbol('}')){
        parse_statement(history_down(history, history->flags));
        stmt_node = node_pop();

        if(stmt_node->type == NODE_TYPE_VARIABLE){
            if(!largest_possible_var_node
               || largest_possible_var_node->var.type.size <= stmt_node->var.type.size){
                largest_possible_var_node = stmt_node;
            }
            if(variable_node_is_primitive(stmt_node)){
                if(!largest_align_eligible_var_node
                   || largest_align_eligible_var_node->var.type.size <= stmt_node->var.type.size){
                    largest_align_eligible_var_node = stmt_node;
                }
            }
        }

        parser_take_preceding_statements(body_vec, history, variable_size);
        vector_push(body_vec, &stmt_node);
        parser_append_size_for_node(history, variable_size, variable_node_or_list(stmt_node));
        parser_take_pending_statements(body_vec);
    }

    expect_sym('}');

    parser_finalize_body(history, body_node, body_vec, variable_size,
                         largest_align_eligible_var_node, largest_possible_var_node);
    parser_current_body = body_node->binded.owner;
    node_push(body_node);
}

// Body entry. Single-statement (ch49) or { ... } (ch62).
static void parse_body(size_t* variable_size, struct history* history){
    parser_scope_new();
    // ch149: open a matching resolver-side scope for the body.
    resolver_default_new_scope(current_process->resolver, 0);
    size_t tmp_size = 0;
    if(!variable_size){
        variable_size = &tmp_size;
    }
    struct vector* body_vec = vector_create(sizeof(struct node*));
    if(!token_next_is_symbol('{')){
        parse_body_single_statement(variable_size, body_vec, history);
        resolver_default_finish_scope(current_process->resolver);
        parser_scope_finish();
        return;
    }
    parse_body_multiple_statements(variable_size, body_vec, history);
    resolver_default_finish_scope(current_process->resolver);
    parser_scope_finish();

    // ch100: when this body sits inside a function, bubble its
    // accumulated variable size up to the function's stack_size.
    if(variable_size){
        if(history->flags & HISTORY_FLAG_INSIDE_FUNCTION_BODY){
            parser_current_function->func.stack_size += *variable_size;
        }
    }
}

// ch64: real struct body parser. Walks `{...}` via parse_body,
// builds a NODE_TYPE_STRUCT, attaches an optional name-variable
// (the `v` in `struct foo {...} v;`), and demands a trailing `;`.
static void parse_struct_no_new_scope(struct datatype* dtype, bool is_forward_declaration){
    struct node* body_node = 0;
    size_t body_variable_size = 0;

    if(!is_forward_declaration){
        parse_body(&body_variable_size, history_begin(HISTORY_FLAG_INSIDE_STRUCTURE));
        body_node = node_pop();
    }

    make_struct_node(dtype->type_str, body_node);
    struct node* struct_node = node_pop();
    if(body_node){
        dtype->size = body_node->body.size;
    }
    dtype->struct_node = struct_node;
    node_push(struct_node);
}

// Optional attached variable: `struct foo {...} v;`. It runs after the
// body's scope is closed, so `v` lands in the enclosing scope; it used
// to be registered inside the body's scope, which lost local ones.
// ch68: NULL-safe via token_is_identifier (handles EOF).
static void parse_struct_or_union_attached_variable(struct datatype* dtype){
    struct node* su_node = node_pop();
    if(token_is_identifier(token_peek_next())){
        struct token* var_name = token_next();
        su_node->flags |= NODE_FLAG_HAS_VARIABLE_COMBINED;
        // If the struct was anonymous, the variable name becomes the
        // type name too (so other declarations can refer to it).
        if(dtype->flags & DATATYPE_FLAG_STRUCT_UNION_NO_NAME){
            dtype->type_str     = var_name->sval;
            dtype->flags       &= ~DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
            su_node->_struct.name = var_name->sval;
        }
        // parse_variable reads `[N]` and `= ...` too: `union u {...} g =
        // { 1 };` and `struct s {...} a[2];`.
        parse_variable(dtype, var_name, history_begin(0));
        // _struct and _union share their layout.
        su_node->_struct.var = node_pop();
    }

    expect_sym(';');
    node_push(su_node);
}

static void parse_struct(struct datatype* dtype){
    bool is_forward_declaration = !token_is_symbol(token_peek_next(), '{');
    if(!is_forward_declaration){
        parser_scope_new();
        // ch149: keep the resolver scope in lockstep with the parser one.
        resolver_default_new_scope(current_process->resolver, 0);
    }
    parse_struct_no_new_scope(dtype, is_forward_declaration);
    if(!is_forward_declaration){
        resolver_default_finish_scope(current_process->resolver);
        parser_scope_finish();
    }
    parse_struct_or_union_attached_variable(dtype);
}

// ch99: parse_union_no_scope mirrors parse_struct_no_new_scope but
// runs the body with HISTORY_FLAG_INSIDE_UNION so the body layout
// code keeps the largest field's size instead of summing.
static void parse_union_no_scope(struct datatype* dtype, bool is_forward_declaration){
    struct node* body_node = 0;
    size_t body_variable_size = 0;
    if(!is_forward_declaration){
        parse_body(&body_variable_size, history_begin(HISTORY_FLAG_INSIDE_UNION));
        body_node = node_pop();
    }

    make_union_node(dtype->type_str, body_node);
    struct node* union_node = node_pop();
    if(body_node){
        dtype->size = body_node->body.size;
    }
    dtype->union_node = union_node;
    node_push(union_node);
}

static void parse_union(struct datatype* dtype){
    bool is_forward_declaration = !token_is_symbol(token_peek_next(), '{');
    if(!is_forward_declaration){
        parser_scope_new();
        // ch149: matching resolver-side scope.
        resolver_default_new_scope(current_process->resolver, 0);
    }
    parse_union_no_scope(dtype, is_forward_declaration);
    if(!is_forward_declaration){
        resolver_default_finish_scope(current_process->resolver);
        parser_scope_finish();
    }
    parse_struct_or_union_attached_variable(dtype);
}

// ch73: parse a function's argument list (everything between the
// already-consumed `(` and the upcoming `)`).
static struct vector* parse_function_arguments(struct history* history){
    parser_scope_new();
    struct vector* arguments_vec = vector_create(sizeof(struct node*));
    while(!token_next_is_symbol(')')){
        // `...` variadic - consume the three dots and stop.
        if(token_next_is_operator(".")){
            token_read_dots(3);
            parser_scope_finish();
            return arguments_vec;
        }

        parse_variable_full(history_down(history, history->flags | HISTORY_FLAG_IS_UPWARD_STACK));
        struct node* argument_node = node_pop();
        vector_push(arguments_vec, &argument_node);

        if(!token_next_is_operator(",")){
            break;
        }
        token_next();   // eat the comma
    }
    parser_scope_finish();
    return arguments_vec;
}

static void parse_struct_or_union(struct datatype* dtype){
    switch(dtype->type){
        case DATA_TYPE_STRUCT:
            parse_struct(dtype);
            break;
        case DATA_TYPE_UNION:
            // ch99: real union body now parses.
            parse_union(dtype);
            break;
        default:
            compiler_error(current_process,
                "COMPILER BUG: The provided datatype is not a structure or union\n");
    }
}

// ch33 entry. ch34+ does the variable / function / struct dispatch
// off the parsed datatype.
// ch98: `struct dog;` - parse the structure as a forward declaration.
// We delegate to parse_struct; the lack of a `{` body causes
// make_struct_node to set NODE_FLAG_IS_FORWARD_DECLARATION.
static void parse_forward_declaration(struct datatype* dtype){
    parse_struct(dtype);
}

static void parse_variable_function_or_struct_union(struct history* history){
    struct datatype dtype;
    parse_datatype(&dtype);

    // ch64: struct / union body, e.g. `struct abc { ... };`
    // parse_struct_or_union builds and pushes the struct node and
    // consumes the trailing `;`. We pop it, register the symbol, push
    // back so the caller's pop+push pair sees it.
    if(datatype_is_struct_or_union(&dtype) && token_next_is_symbol('{')){
        parse_struct_or_union(&dtype);

        struct node* su_node = node_pop();
        symresolver_build_for_node(current_process, su_node);
        if((su_node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED)
           && current_process->scope.current != current_process->scope.root){
            // A local `struct S {...} v;` is the declaration of v; the
            // type is already registered.
            node_push(su_node->_struct.var);
            return;
        }
        node_push(su_node);
        return;
    }

    // ch98: `struct dog;` - forward declaration. The fixup system from
    // ch96/97 lets later `struct dog* x;` references resolve once the
    // real body lands.
    if(token_next_is_symbol(';')){
        if(parser_last_datatype_was_enum){
            // `enum color { RED, GREEN };` declares only its constants;
            // a blank statement keeps the one-node-per-statement rule.
            expect_sym(';');
            node_create(&(struct node){ .type = NODE_TYPE_BLANK });
            return;
        }
        parse_forward_declaration(&dtype);
        return;
    }

    // ch41: swallow the decorative "int" in "long int" / "float int" /
    // "double int". The book keeps the real type as long/float/double
    // and silently drops the int.
    parser_ignore_int(&dtype);

    // ch42: `int abc;` - the next token must be the variable name, or
    // a function pointer declarator `(*name)(...)` holds it.
    struct token* name_token = 0;
    if(!parse_function_pointer_declarator(&dtype, &name_token)){
        name_token = token_next();
    }
    if(!name_token || name_token->type != TOKEN_TYPE_IDENTIFIER){
        compiler_error(current_process,
            "Expecting a valid name for the given variable declaration\n");
    }

    // ch72: if next is `(`, this is a function declaration.
    if(token_next_is_operator("(")){
        parse_function(&dtype, name_token, history);
        return;
    }

    // Each declarator gets its own copy of the type: parse_variable adds
    // `[N]` to the type it is given, so with one shared copy `int a[4],
    // b;` made b an array too. Peers start from the base type without
    // the first declarator's `*` and read their own.
    struct datatype base_dtype = dtype;
    base_dtype.flags &= ~DATATYPE_FLAG_IS_POINTER;
    base_dtype.pointer_depth = 0;
    struct datatype first_dtype = dtype;
    parse_variable(&first_dtype, name_token, history);

    // ch43: `int a, b, c;` - gather any comma-separated peers into a
    // NODE_TYPE_VARIABLE_LIST.
    if(token_is_operator(token_peek_next(), ",")){
        struct vector* var_list = vector_create(sizeof(struct node*));
        struct node* var_node   = node_pop();
        // A static local leaves a blank in its place (parse_static_local).
        if(var_node->type != NODE_TYPE_BLANK){
            vector_push(var_list, &var_node);
        }
        while(token_is_operator(token_peek_next(), ",")){
            token_next();
            struct datatype peer_dtype = base_dtype;
            int depth = parser_get_pointer_depth();
            if(depth > 0){
                peer_dtype.flags |= DATATYPE_FLAG_IS_POINTER;
                peer_dtype.pointer_depth = depth;
            }
            name_token = token_next();
            if(!name_token || name_token->type != TOKEN_TYPE_IDENTIFIER){
                compiler_error(current_process, "Expecting a variable name after ',' in the declaration\n");
            }
            parse_variable(&peer_dtype, name_token, history);
            var_node = node_pop();
            if(var_node->type != NODE_TYPE_BLANK){
                vector_push(var_list, &var_node);
            }
        }
        if(vector_count(var_list)){
            make_variable_list_node(var_list);
        } else {
            node_create(&(struct node){ .type = NODE_TYPE_BLANK });
        }
    }

    expect_sym(';');
}

static void parse_if_stmt(struct history* history);

// ch79: parse the body of a bare `else`. Returns a NODE_TYPE_STATEMENT_ELSE.
static struct node* parse_else(struct history* history){
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node* body_node = node_pop();
    make_else_node(body_node);
    return node_pop();
}

// ch79: after an `if` body has been parsed, optionally consume an
// `else` (and possibly an `else if` chain). Returns the next-link
// node or NULL.
static struct node* parse_else_or_else_if(struct history* history){
    if(!token_next_is_keyword("else")){
        return 0;
    }
    token_next();   // eat "else"

    if(token_next_is_keyword("if")){
        parse_if_stmt(history_down(history, 0));
        return node_pop();
    }
    return parse_else(history_down(history, 0));
}

// ch78/79: parse `if (cond) <body> [else if ... | else ...]`.
static void parse_if_stmt(struct history* history){
    expect_keyword("if");
    expect_op("(");
    parse_expressionable_root(history);
    expect_sym(')');

    struct node* cond_node = node_pop();
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node* body_node = node_pop();

    make_if_node(cond_node, body_node, parse_else_or_else_if(history));
}

// ch83: shared helper - consume `keyword (` expr `)` and leave expr on stack.
static void parse_keyword_parentheses_expression(const char* keyword){
    expect_keyword(keyword);
    expect_op("(");
    parse_expressionable_root(history_begin(0));
    expect_sym(')');
}

// ch83: `while (exp) body`.
static void parse_while(struct history* history){
    parse_keyword_parentheses_expression("while");
    struct node* exp_node = node_pop();
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node* body_node = node_pop();
    make_while_node(exp_node, body_node);
}

// ch84: `do body while (exp);`.
static void parse_do_while(struct history* history){
    expect_keyword("do");
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node* body_node = node_pop();
    parse_keyword_parentheses_expression("while");
    struct node* exp_node = node_pop();
    expect_sym(';');
    make_do_while_node(body_node, exp_node);
}

// ch85: `switch (exp) body`. ch89 wires the case-collection logic.
// ch94: cases are collected into the history's _switch.case_data and
// handed off to the switch node.
static void parse_switch(struct history* history){
    struct parser_history_switch sw = parser_new_switch_statement(history);
    parse_keyword_parentheses_expression("switch");
    struct node* exp_node = node_pop();
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node* body_node = node_pop();
    make_switch_node(exp_node, body_node, sw.case_data->cases, sw.case_data->has_default_case);
    parser_end_switch_statement(&sw);
}

// ch86: `break;` / `continue;`.
static void parse_break(struct history* history){
    (void)history;
    expect_keyword("break");
    expect_sym(';');
    make_break_node();
}

static void parse_continue(struct history* history){
    (void)history;
    expect_keyword("continue");
    expect_sym(';');
    make_continue_node();
}

// ch88: `goto label;`.
static void parse_goto(struct history* history){
    (void)history;
    expect_keyword("goto");
    struct token* label = token_next();
    if(!token_is_identifier(label)){
        compiler_error(current_process, "Expected a label name after goto\n");
    }
    expect_sym(';');
    const char* name = parser_label_name(label->sval);
    if(parser_function_gotos){
        vector_push(parser_function_gotos, &name);
    }
    node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = name });
    struct node* label_node = node_pop();
    make_goto_node(label_node);
}

// ch89: `case expr:` and `default:`.
// ch94: only integer-constant case expressions are supported; the
// resulting NODE_TYPE_STATEMENT_CASE is registered with the active
// switch so codegen can later index into it.
static void parse_case(struct history* history){
    expect_keyword("case");
    parse_expressionable_root(history);
    // `case -1:`, `case 'a' + 1:` and enumerators are constant
    // expressions; only a plain number was accepted before.
    struct node* exp_node = parser_fold_required(node_pop(), "A case label");
    expect_sym(':');
    make_case_node(exp_node);

    struct node* case_node = node_pop();
    parser_register_case(history, case_node);
    // book forgets to push the case node back, so the enclosing
    // body's `node_pop()` blows up on the first `case` it sees. Push
    // it back so the case appears as a body statement too.
    node_push(case_node);
}

// The switch must know it has a default label: without it the case
// jumps fell out of the switch for an unmatched value and the default
// body never ran.
static void parse_default(struct history* history){
    expect_keyword("default");
    expect_sym(':');
    if(!(history->flags & HISTORY_FLAG_IN_SWITCH_STATEMENT)){
        compiler_error(current_process, "'default' is only allowed inside a switch\n");
    }
    if(history->_switch.case_data->has_default_case){
        compiler_error(current_process, "This switch already has a default label\n");
    }
    history->_switch.case_data->has_default_case = true;
    make_default_node();
}

// ch82: a `for` part that ends with `;` (init / cond). Returns true if
// it actually parsed an expression; false for empty.
static bool parse_for_loop_part(struct history* history){
    if(token_next_is_symbol(';')){
        token_next();   // eat `;`
        return false;
    }
    parse_expressionable_root(history);
    expect_sym(';');
    return true;
}

// ch82: the `loop` part terminates with `)`. Returns true if non-empty.
static bool parse_for_loop_part_loop(struct history* history){
    if(token_next_is_symbol(')')){
        return false;
    }
    parse_expressionable_root(history);
    return true;
}

static void parse_for_stmt(struct history* history){
    struct node* init_node = 0;
    struct node* cond_node = 0;
    struct node* loop_node = 0;
    struct node* body_node = 0;

    expect_keyword("for");
    expect_op("(");
    // `for (int i = 0; ...)`: the declaration is scoped to the loop. It
    // gets a scope of its own around the loop and its bytes in the
    // function's frame; it was rejected before.
    struct token* first = token_peek_next();
    bool declares = first && first->type == TOKEN_TYPE_KEYWORD
        && (keyword_is_datatype(first->sval) || is_keyword_variable_modifier(first->sval));
    if(declares){
        parser_scope_new();
        resolver_default_new_scope(current_process->resolver, 0);
        parse_variable_function_or_struct_union(history_down(history, history->flags));
        init_node = node_pop();
        if(init_node->type != NODE_TYPE_VARIABLE && init_node->type != NODE_TYPE_VARIABLE_LIST){
            compiler_error(current_process, "A for loop can only declare variables\n");
        }
        size_t declaration_size = 0;
        parser_append_size_for_node(history, &declaration_size, init_node);
        parser_current_function->func.stack_size += declaration_size;
    } else if(parse_for_loop_part(history_down(history, history->flags))){
        init_node = node_pop();
    }
    // Each part parses on its own copy of the history. The expression
    // parser sets flags on the one it is given, and the body read the
    // loop part's EXPRESSION_FLAG_RIGHT_NODE as HISTORY_FLAG_INSIDE_UNION:
    // its locals were sized as a union and the frame left them out.
    if(parse_for_loop_part(history_down(history, history->flags)))      cond_node = node_pop();
    if(parse_for_loop_part_loop(history_down(history, history->flags))) loop_node = node_pop();
    expect_sym(')');

    size_t var_size = 0;
    parse_body(&var_size, history);
    body_node = node_pop();
    if(declares){
        resolver_default_finish_scope(current_process->resolver);
        parser_scope_finish();
    }
    make_for_node(init_node, cond_node, loop_node, body_node);
}

// ch81: `return [expr];`. Bare `return;` -> return-node with NULL exp.
static void parse_return(struct history* history){
    expect_keyword("return");
    if(token_next_is_symbol(';')){
        expect_sym(';');
        make_return_node(0);
        return;
    }
    parse_expressionable_root(history);
    struct node* exp_node = node_pop();
    // Returning from a _Bool function converts the value to _Bool.
    if(parser_current_function && parser_is_bool_object(&parser_current_function->func.rtype)){
        exp_node = parser_bool_value(exp_node);
    }
    make_return_node(exp_node);
    expect_sym(';');
}

static bool parser_expression_type(struct node* node, struct datatype* out);

// The type the resolver gives a name, member or subscript. A subscript
// that reaches an element keeps the array's flags in the resolver; for
// sizeof and the operators around it that is the element type.
static bool parser_resolved_type(struct node* node, struct datatype* out){
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    if(!resolver_result_ok(result) || !result->last_entity){
        return false;
    }
    struct resolver_entity* last = result->last_entity;
    *out = last->dtype;
    if(last->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET && (out->flags & DATATYPE_FLAG_IS_ARRAY)
       && (int)array_brackets_count(out) <= last->array.index + 1){
        out->flags &= ~DATATYPE_FLAG_IS_ARRAY;
    }
    if(last->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET && (last->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)){
        out->flags &= ~DATATYPE_FLAG_IS_ARRAY;
    }
    return true;
}

// The type of an expression the parser has just built, for sizeof. The
// operand of sizeof is not evaluated (C11 6.5.3.4), so only its type is
// needed, and the parser can work it out: variables are registered with
// the resolver as they are declared.
static bool parser_expression_type(struct node* node, struct datatype* out){
    switch(node->type){
        case NODE_TYPE_NUMBER:
            *out = datatype_for_number_node(node);
            return true;
        case NODE_TYPE_STRING: {
            // A string literal is a char array holding its NUL.
            struct datatype dtype = {0};
            dtype.type       = DATA_TYPE_CHAR;
            dtype.type_str   = "char";
            dtype.size       = DATA_SIZE_BYTE;
            dtype.flags      = DATATYPE_FLAG_IS_ARRAY;
            dtype.array.size = strlen(node->sval) + 1;
            *out = dtype;
            return true;
        }
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return parser_expression_type(node->parenthesis.exp, out);
        case NODE_TYPE_CAST:
            *out = node->cast.dtype;
            return true;
        case NODE_TYPE_IDENTIFIER:
            return parser_resolved_type(node, out);
        case NODE_TYPE_UNARY: {
            struct datatype operand;
            if(!parser_expression_type(node->unary.operand, &operand)){
                return false;
            }
            if(op_is_indirection(node->unary.op)){
                operand = datatype_decay(&operand);
                *out = *datatype_pointer_reduce(&operand, node->unary.indirection.depth);
            } else if(op_is_address(node->unary.op)){
                operand.flags &= ~DATATYPE_FLAG_IS_ARRAY;
                operand.flags |= DATATYPE_FLAG_IS_POINTER;
                operand.pointer_depth++;
                *out = operand;
            } else if(S_EQ(node->unary.op, "!")){
                *out = datatype_for_int();
            } else if(is_increment_operator(node->unary.op)){
                *out = operand;
            } else {
                *out = datatype_promote(&operand);
            }
            return true;
        }
        case NODE_TYPE_EXPRESSION: {
            const char* op = node->exp.op;
            if(is_access_operator(op) || S_EQ(op, "[]") || S_EQ(op, "()")){
                return parser_resolved_type(node, out);
            }
            if(is_logical_operator(op)){
                *out = datatype_for_int();
                return true;
            }
            if(is_node_assignment(node)){
                return parser_expression_type(node->exp.left, out);
            }
            if(S_EQ(op, ",")){
                return parser_expression_type(node->exp.right, out);
            }
            if(S_EQ(op, "?")){
                return parser_expression_type(node->exp.right->tenary.true_node, out);
            }
            struct datatype left;
            struct datatype right;
            if(!parser_expression_type(node->exp.left, &left) || !parser_expression_type(node->exp.right, &right)){
                return false;
            }
            left  = datatype_decay(&left);
            right = datatype_decay(&right);
            struct datatype_binary types;
            datatype_binary_types(&left, &right, op, &types);
            *out = types.result;
            return true;
        }
    }
    return false;
}

// ch232: `sizeof` as a NUMBER node holding the size, worked out at parse
// time; codegen sees a constant of type size_t. `sizeof(type)` was the
// only form; `sizeof(expr)` and `sizeof expr` failed.
static void parse_sizeof(struct history* history){
    (void)history;
    expect_keyword("sizeof");
    struct datatype dtype;
    struct token* after_paren = 0;
    if(token_next_is_operator("(")){
        expect_op("(");
        after_paren = token_peek_next();
    }
    if(after_paren && after_paren->type == TOKEN_TYPE_KEYWORD
       && (keyword_is_datatype(after_paren->sval) || is_keyword_variable_modifier(after_paren->sval))){
        parse_datatype(&dtype);
        expect_sym(')');
    } else {
        size_t count_before = node_count();
        if(after_paren){
            parse_expressionable_root(history_begin(0));
            expect_sym(')');
        } else {
            parse_expressionable(parser_unary_operand_history());
        }
        parser_expect_new_node(count_before, "an operand after sizeof");
        struct node* operand = node_pop();
        if(!parser_expression_type(operand, &dtype)){
            compiler_error(current_process, "Cannot work out the size of this sizeof operand\n");
        }
    }
    // sizeof yields a size_t, which is unsigned long on every target.
    node_create(&(struct node){ .type = NODE_TYPE_NUMBER, .llnum = datatype_size(&dtype),
        .num.flags = NUMBER_FLAG_UNSIGNED | NUMBER_FLAG_LONG });
}

static void parse_keyword(struct history* history){
    struct token* token = token_peek_next();
    if(S_EQ(token->sval, "sizeof")){
        parse_sizeof(history);
        return;
    }
    if(is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval)){
        parse_variable_function_or_struct_union(history);
        return;
    }
    if(S_EQ(token->sval, "return")){
        parse_return(history);
        return;
    }
    if(S_EQ(token->sval, "if")){
        parse_if_stmt(history);
        return;
    }
    if(S_EQ(token->sval, "for")){
        parse_for_stmt(history);
        return;
    }
    if(S_EQ(token->sval, "while")){
        parse_while(history);
        return;
    }
    if(S_EQ(token->sval, "do")){
        parse_do_while(history);
        return;
    }
    if(S_EQ(token->sval, "switch")){
        parse_switch(history);
        return;
    }
    if(S_EQ(token->sval, "break")){
        parse_break(history);
        return;
    }
    if(S_EQ(token->sval, "continue")){
        parse_continue(history);
        return;
    }
    if(S_EQ(token->sval, "goto")){
        parse_goto(history);
        return;
    }
    if(S_EQ(token->sval, "case")){
        parse_case(history);
        return;
    }
    if(S_EQ(token->sval, "default")){
        parse_default(history);
        return;
    }
}

static int parse_expressionable_single(struct history* history){
    struct token* token = token_peek_next();
    if(!token){
        return -1;
    }

    // This ORed NODE_FLAG_INSIDE_EXPRESSION (a node flag, 0b1) into the
    // history, where 0b1 is HISTORY_FLAG_INSIDE_UNION. The array size in
    // `int a[3]` then marked the declaration as a union member.
    int res = -1;
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
            parse_single_token_to_node();
            res = 0;
            break;

        case TOKEN_TYPE_IDENTIFIER:
            parse_identifier(history);
            res = 0;
            break;

        case TOKEN_TYPE_STRING:
            parse_single_token_to_node();
            res = 0;
            break;

        case TOKEN_TYPE_OPERATOR:
            // ch176: forward parse_exp's return so the caller knows
            // when the unary-operand check bailed.
            res = parse_exp(history);
            break;

        case TOKEN_TYPE_KEYWORD:
            parse_keyword(history);
            res = 0;
            break;
    }
    return res;
}

static void parse_expressionable(struct history* history){
    while(parse_expressionable_single(history) == 0){
    }
}

// Entry from parse_next for top-level keyword declarations. As of
// ch42 variables push a real node. ch47 added bare `struct foo {};`
// which pushes nothing, so guard the pop/push.
static void parse_keyword_for_global(void){
    // ch149: top-level parse runs under HISTORY_FLAG_IS_GLOBAL_SCOPE
    // and we register variable / function / struct / union names with
    // the symbol resolver so codegen can look them up.
    parse_keyword(history_begin(HISTORY_FLAG_IS_GLOBAL_SCOPE));
    if(!vector_empty(current_process->node_vec)){
        struct node* node = node_pop();
        switch(node->type){
            case NODE_TYPE_VARIABLE:
            case NODE_TYPE_FUNCTION:
            case NODE_TYPE_STRUCT:
            case NODE_TYPE_UNION:
                symresolver_build_for_node(current_process, node);
                break;
        }
        node_push(node);
    }
}

static int parse_next(void){
    struct token* token = token_peek_next();
    if(!token){
        return -1;
    }

    int res = 0;
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_STRING:
            parse_expressionable(history_begin(0));
            break;

        case TOKEN_TYPE_KEYWORD:
            parse_keyword_for_global();
            break;

        case TOKEN_TYPE_SYMBOL:
            parse_symbol();
            break;

        default:
            return -1;
    }
    return res;
}

int parse(struct compile_process* process){
    current_process   = process;
    parser_last_token = 0;
    parser_previous_token = 0;
    parser_saw_bool = false;
    parser_enumerators = vector_create(sizeof(struct parser_enumerator));
    parser_pending_statements = vector_create(sizeof(struct node*));
    parser_preceding_statements = vector_create(sizeof(struct node*));
    scope_create_root(process);
    // ch66: symresolver_initialize now happens in compile_process_create.
    node_set_vector(process->node_vec, process->node_tree_vec);
    // ch77: allocate the BLANK sentinel used by empty `()`.
    parser_blank_node = node_create(&(struct node){ .type = NODE_TYPE_BLANK });
    node_pop();  // BLANK is referenced by pointer, not via the stack.
    // ch97: spin up the parser-side fixup system.
    parser_fixup_sys = fixup_sys_new();

    struct node* node = 0;
    vector_set_peek_pointer(process->token_vec, 0);
    while(parse_next() == 0){
        // ch36: keyword parses don't push a node yet; only push to the
        // tree when there's actually something on the scratch stack.
        if(!vector_empty(process->node_vec)){
            node = node_peek();
            vector_push(process->node_tree_vec, &node);
        }
    }
    // parse_next stops at a token it has no rule for. Everything after
    // it used to be dropped without a word.
    if(token_peek_next()){
        compiler_error(process, "Unexpected %s at file scope\n", parser_token_text(token_peek_next()));
    }

    // ch97: every forward struct reference must resolve by end of parse.
    assert(fixups_resolve(parser_fixup_sys));
    // ch105: hand the scope back to codegen, which sets up its own root.
    scope_free_root(process);
    return PARSE_ALL_OK;
}
