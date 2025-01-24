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
static void            parser_wide_string_object(struct vector* units, int unit_size, bool is_wchar);
static void            token_read_dots(size_t amount);
static void            parser_declare_vla(struct datatype* dtype, struct token* name_token, struct vector* dims, struct history* history);
static const char*     parser_vla_size_of(const char* name);
static struct parser_vla* parser_vla_find(const char* name);
static void            parser_vla_pointer_to_array(struct token* name_token, struct vector* dims, size_t element_size, bool is_parameter);
static bool            parser_vla_active(void);
static struct node*    parser_vla_lower(struct node* node);
static struct node*    parser_identifier(const char* name);
static bool            parse_function_pointer_declarator(struct datatype* dtype, struct token** name_out);
static struct array_brackets* parse_array_brackets(struct history* history);
static bool            parser_name_in(struct vector* names, const char* name);
static bool            parser_is_statement_keyword(const char* keyword);

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
// How many loops, and loops or switches, enclose the statement being
// parsed: `break` needs one of either, `continue` a loop. Outside them
// both reached codegen without a target and segfaulted.
static int                     parser_loop_depth;
static int                     parser_breakable_depth;
// Set by parse_function_arguments when the list ends in `...`.
static bool                    parser_arguments_variadic;
// the width of the bit-field member being declared (-1 for
// none) and whether it has no name; the names of every bit-field
// declared so far, which expressions are lowered for; and whether the
// expression being parsed is a whole statement, its value unused.
static int                     parser_bitfield_width = -1;
static bool                    parser_bitfield_unnamed;
static struct vector*          parser_bitfield_names;
static bool                    parser_expression_is_statement;
static const struct bitfield*  parser_initializer_bitfield;
struct parser_vla_block {
    // The hidden local holding the stack pointer before the block's
    // first variable-length array, or NULL while it has none.
    const char*    save;
    // the stack pointer before each of its variable-length
    // arrays (const char*, in order), for a goto that leaves the block
    // or jumps back over one; and a number naming the block.
    struct vector* saves;
    int            id;
};
struct parser_vla_jump {
    bool is_loop;
    // The number of blocks open when the loop or switch began.
    int  depth;
};
// the shape of a multi-dimensional variable-length array, or
// of a pointer to an array (`int (*p)[n]`, a parameter `int a[][n]`),
// all of which are a flat `T*` here. dims counts the subscripts down to
// an element; stride k is how many elements `[k]` steps over: a hidden
// local evaluated at the declaration, or a constant.
struct parser_vla_stride {
    const char*   name;
    unsigned long value;
};
struct parser_vla_info {
    int                       dims;
    struct parser_vla_stride* strides;
    size_t                    element_size;
};
struct parser_vla {
    const char* name;
    // The hidden local holding its size in bytes, for sizeof; NULL for a
    // pointer to an array and for a name that hides an outer one.
    const char* size;
    int         depth;
    // NULL for a one-dimensional array and for a hiding name.
    struct parser_vla_info* info;
};
// A parameter that is a pointer to an array (`int a[][n]`, `int
// (*a)[n]`): its strides are worked out when the body starts.
struct parser_vla_param {
    const char*     name;
    // The array's dimensions, the ones after the pointer.
    struct vector*  dims;
    size_t          element_size;
};
// An expression that stands for part of such an array: `a` (level 0),
// `a[i]` (level 1), `p + 1`. Subscripting it steps over stride[level].
struct parser_vla_view {
    struct node*            node;
    struct parser_vla_info* info;
    int                     level;
    // The view is offset elements (NULL for 0) into base, the name of
    // the array or pointer.
    const char*             base;
    struct node*            offset;
};
// For a goto out of a block with a variable-length array: where each
// label is (the blocks open there and how many arrays each had), and
// each goto, with a blank statement before it that becomes the restore.
struct parser_vla_place {
    int            id;
    int            count;
    struct vector* saves;
};
struct parser_vla_label {
    const char*    name;
    struct vector* blocks;
};
struct parser_vla_goto {
    const char*    name;
    struct vector* blocks;
    struct node*   restore;
};
// variable-length arrays. parse_array_brackets may leave a
// runtime size for parse_variable when parser_vla_allowed; the blocks
// being parsed (struct parser_vla_block), the loops and switches being
// parsed (struct parser_vla_jump), and the arrays in scope (struct
// parser_vla).
// parse_array_brackets leaves every dimension (struct node*,
// NULL for `[]`) in parser_vla_dims, as parser_vla_mode asks: for a
// local or a sizeof type when one is not constant, for a parameter also
// when there are two or more, for a pointer to an array always. A
// pointer to an array declarator leaves its dimensions in
// parser_vla_pointer_dims for parse_variable.
enum {
    PARSER_VLA_MODE_NONE,
    PARSER_VLA_MODE_LOCAL,
    PARSER_VLA_MODE_PARAMETER,
    PARSER_VLA_MODE_ALWAYS,
};
static int                     parser_vla_mode;
static struct vector*          parser_vla_blocks;
static struct vector*          parser_vla_jumps;
static struct vector*          parser_vlas;
static struct vector*          parser_vla_dims;
static size_t                  parser_vla_pointer_element;
static struct vector*          parser_vla_pointer_dims;
static struct vector*          parser_vla_params;
static struct vector*          parser_vla_views;
static struct vector*          parser_vla_done;
static struct vector*          parser_vla_labels;
static struct vector*          parser_vla_gotos;
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

// the thread-local variables declared so far, on i386 only
// (parser_thread_local_object).
struct parser_thread_local {
    const char*     name;
    struct datatype type;
};
static struct vector*              parser_thread_locals;
static void                        parser_note_thread_local(const char* name, struct datatype* dtype);
static struct parser_thread_local* parser_thread_local_find(const char* name);
static struct node*                parser_thread_local_object(struct parser_thread_local* tls);
static struct node*                parser_thread_local_address(struct node* node);

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
static void          parser_ignore_int(struct datatype* dtype);
static void          parse_variable_function_or_struct_union(struct history* history);
static bool          parser_is_int_valid_after_datatype(struct datatype* dtype);
static void          parser_ignore_int(struct datatype* dtype);
static void          parse_expressionable_root(struct history* history);
static void          make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node);
static void          make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node);
static void          parse_variable(struct datatype* dtype, struct token* name_token, struct history* history);
// The variable whose initializer is being read, and its type.
static const char*     parser_declaring_name;
static struct datatype parser_declaring_type;
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

// The position of the token read last; node_create stamps it on nodes.
struct pos parser_position(void){
    struct pos none = {0};
    return current_process ? current_process->pos : none;
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

// The code units of the string literal `first` and of the literals
// right after it, which make one string (C11 5.1.1.2 translation phase
// 6), the terminating 0 included. *unit_size is 1 for plain strings,
// else the unit of the wide literal among them; a plain literal next
// to a wide one is widened (6.4.5p5).
static struct vector* parser_string_units(struct token* first, int* unit_size, bool* is_wchar){
    struct vector* tokens = vector_create(sizeof(struct token*));
    vector_push(tokens, &first);
    while(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING){
        struct token* next = token_next();
        vector_push(tokens, &next);
    }
    *unit_size = DATA_SIZE_BYTE;
    *is_wchar = false;
    for(int i = 0; i < vector_count(tokens); i++){
        struct token* t = *(struct token**)vector_at(tokens, i);
        if(!t->wide.unit_size){
            continue;
        }
        if(*unit_size != DATA_SIZE_BYTE && (*unit_size != t->wide.unit_size || *is_wchar != t->wide.is_wchar)){
            compiler_error(current_process, "Adjacent wide string literals of different kinds\n");
        }
        *unit_size = t->wide.unit_size;
        *is_wchar = t->wide.is_wchar;
    }
    struct vector* units = vector_create(sizeof(unsigned int));
    for(int i = 0; i < vector_count(tokens); i++){
        struct token* t = *(struct token**)vector_at(tokens, i);
        if(t->wide.unit_size){
            for(int u = 0; u < t->wide.count; u++){
                vector_push(units, &t->wide.units[u]);
            }
            continue;
        }
        for(const char* c = t->sval; *c; c++){
            unsigned int unit = (unsigned char)*c;
            vector_push(units, &unit);
        }
    }
    unsigned int zero = 0;
    vector_push(units, &zero);
    return units;
}

static void parse_single_token_to_node(void){
    struct token* token = token_next();
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
            if(token->num.flags & NUMBER_FLAG_FLOATING){
                current_process->uses_floating = true;
            }
            if(token->num.flags & NUMBER_FLAG_LONG_DOUBLE){
                current_process->uses_long_double = true;
            }
            node_create(&(struct node){ .type = NODE_TYPE_NUMBER,     .llnum = token->llnum, .num.flags = token->num.flags,
                                        .num.text = token->num.text });
            break;
        case TOKEN_TYPE_IDENTIFIER: {
            bool member = parser_previous_token
                && (token_is_operator(parser_previous_token, ".") || token_is_operator(parser_previous_token, "->"));
            struct parser_enumerator* e = member ? 0 : parser_enumerator_find(token->sval);
            struct parser_thread_local* tls = member ? 0 : parser_thread_local_find(e && e->rename ? e->rename : token->sval);
            if(tls){
                node_push(parser_thread_local_object(tls));
                break;
            }
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
            int unit_size;
            bool is_wchar;
            struct vector* units = parser_string_units(token, &unit_size, &is_wchar);
            if(unit_size > DATA_SIZE_BYTE){
                parser_wide_string_object(units, unit_size, is_wchar);
                break;
            }
            struct buffer* joined = buffer_create();
            for(int i = 0; i < vector_count(units) - 1; i++){
                buffer_write(joined, (char)*(unsigned int*)vector_at(units, i));
            }
            buffer_write(joined, 0x00);
            node_create(&(struct node){ .type = NODE_TYPE_STRING,     .sval  = buffer_ptr(joined)  });
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
    // The reorder asks about the same few operators over and over (twice
    // per rotation); remember the last answer instead of scanning the
    // table with strcmp each time.
    static char cached_op[8];
    static int  cached_index = -1;
    *group_out = 0;
    if(op && cached_op[0] && strcmp(cached_op, op) == 0){
        if(cached_index >= 0){
            *group_out = &op_precedence[cached_index];
        }
        return cached_index;
    }
    cached_op[0] = 0;
    if(op && strlen(op) < sizeof(cached_op)){
        strcpy(cached_op, op);
    }
    cached_index = -1;
    for(int i = 0; i < TOTAL_OPERATOR_GROUPS; i++){
        for(int b = 0; op_precedence[i].operators[b]; b++){
            const char* _op = op_precedence[i].operators[b];
            if(S_EQ(op, _op)){
                *group_out   = &op_precedence[i];
                cached_index = i;
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
    if(!group_left || !group_right){
        compiler_error(current_process, "'%s' cannot be used between two operands\n", group_left ? op_right : op_left);
    }
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

    struct node* right              = node->exp.right;
    const char*  right_op           = right->exp.op;
    struct node* new_exp_left_node  = node->exp.left;
    struct node* new_exp_right_node = right->exp.left;
    struct node* new_right_operand  = right->exp.right;

    // The old right node leaves the tree here, so it becomes the new
    // left node, rebuilt as make_exp_node would build a fresh one. A
    // new node per shift made a chain of n same-precedence terms
    // allocate O(n^2) nodes: 4,000 terms took 1.7 GB.
    *right = (struct node){
        .type      = NODE_TYPE_EXPRESSION,
        .exp.left  = new_exp_left_node,
        .exp.right = new_exp_right_node,
        .exp.op    = node->exp.op,
    };
    right->binded.owner    = parser_current_body;
    right->binded.function = parser_current_function;
    struct node* new_left_operand = right;

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

// ch101 + ch175: extra reordering. ch101 was too aggressive
// (`||` between the array and assignment tests reordered any
// `a[i] op b` regardless of op). ch175 tightens to:
//   - LHS array AND RHS assignment, or
//   - LHS `()` or `[]` AND RHS `,`.
static bool parser_needs_extra_reorder(struct node* node){
    return (is_array_node(node->exp.left) && is_node_assignment(node->exp.right))
        || ((node_is_expression(node->exp.left, "()")
             || node_is_expression(node->exp.left, "[]"))
            && node_is_expression(node->exp.right, ","));
}

// A right child the reorder leaves alone wherever it sits: reordering
// it changes nothing (mirrors parser_reorder_expression's tests) and it
// can never satisfy the extra rule from the right (not an assignment,
// not a comma).
static bool parser_is_quiet_right(struct node* node){
    if(!node){
        return false;
    }
    if(node->type != NODE_TYPE_EXPRESSION){
        return true;
    }
    if(!node->exp.right || is_node_assignment(node) || node_is_expression(node, ",")){
        return false;
    }
    bool complete_left = parser_is_complete_operand(node->exp.left);
    if(complete_left && node->exp.right->type != NODE_TYPE_EXPRESSION){
        return true;
    }
    if(complete_left && parser_left_op_has_priority(node->exp.op, node->exp.right->exp.op)){
        return false;
    }
    return !parser_needs_extra_reorder(node);
}

// A run: root, root->exp.left, ... down to tail, every one an expression
// whose operator is in one precedence group (not &&, || or ,, which
// never shift over themselves) and whose right child is quiet. For a
// left operand with priority over that group, the recursive reorder
// does nothing on a run but shift, level after level.
static struct {
    struct node* root;
    struct node* tail;
} parser_run;

static bool parser_is_run_op(const char* op){
    return op && !S_EQ(op, "&&") && !S_EQ(op, "||") && !S_EQ(op, ",");
}

static bool parser_same_group(const char* op_a, const char* op_b){
    struct expressionable_op_precedence_group* group = 0;
    int a = parser_get_precedence_for_operator(op_a, &group);
    int b = parser_get_precedence_for_operator(op_b, &group);
    return a >= 0 && a == b;
}

static bool parser_is_run_node(struct node* node, const char* root_op){
    return node && node->type == NODE_TYPE_EXPRESSION
        && parser_is_run_op(node->exp.op)
        && parser_same_group(node->exp.op, root_op)
        && parser_is_quiet_right(node->exp.right);
}

// Record the run that starts at exp_node, the operand the enclosing
// level of the right-recursive parse is about to reorder. Extending a
// run kept from the previous level costs O(1) per level for a chain.
static void parser_run_note(struct node* exp_node){
    if(parser_run.root != exp_node){
        parser_run.root = parser_run.tail = 0;
        if(!parser_is_run_node(exp_node, exp_node->exp.op)){
            return;
        }
        parser_run.root = parser_run.tail = exp_node;
    }
    const char* op = exp_node->exp.op;
    while(parser_is_run_node(parser_run.tail->exp.left, op)){
        parser_run.tail = parser_run.tail->exp.left;
    }
}

// `a + b + c + ...` is parsed right to left, so each new left operand
// travelled down the whole left spine of the already ordered right side,
// one shift and two recursive calls per level: O(n^2) for n terms.
//
// Shifting node (t1 op1 S0) down a run S0 .. S(k-1) ends with node
// holding S0's operator and operands, every Sj (j >= 1) in its place
// and (t1 op1 X) at the bottom, X being S(k-1)'s old left operand. So
// when the right operand is the recorded run, node takes S0's contents
// and the S0 node becomes the bottom, in O(1). Below the run (or with
// no record) the remaining levels that qualify are shifted in a loop.
// Returns the node where the shifting stopped, which the caller hands
// back to the full reorder, or NULL when not even one level qualifies.
// Same tree as the recursion built.
static struct node* parser_shift_run_left(struct node* node){
    struct node* cur = node;
    bool shifted = false;
    struct node* first = node->exp.right;
    if(first && first == parser_run.root && parser_run.tail != first
       && parser_is_run_node(first, first->exp.op)
       && parser_is_run_node(parser_run.tail, first->exp.op)
       && parser_left_op_has_priority(node->exp.op, first->exp.op)){
        struct node* tail = parser_run.tail;
        struct node* t1   = node->exp.left;
        const char*  op1  = node->exp.op;
        struct node* x    = tail->exp.left;
        node->exp.left  = first->exp.left;
        node->exp.right = first->exp.right;
        node->exp.op    = first->exp.op;
        *first = (struct node){
            .type      = NODE_TYPE_EXPRESSION,
            .exp.left  = t1,
            .exp.right = x,
            .exp.op    = op1,
        };
        first->binded.owner    = parser_current_body;
        first->binded.function = parser_current_function;
        tail->exp.left = first;
        // node now heads the same run down to tail.
        parser_run.root = node;
        cur     = first;
        shifted = true;
    }
    while(cur->exp.right
          && cur->exp.right->type == NODE_TYPE_EXPRESSION
          && parser_is_quiet_right(cur->exp.right->exp.right)
          && parser_left_op_has_priority(cur->exp.op, cur->exp.right->exp.op)){
        parser_node_shift_children_left(cur);
        cur = cur->exp.left;
        shifted = true;
    }
    return shifted ? cur : NULL;
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
            struct node* bottom = parser_shift_run_left(node);
            if(bottom){
                parser_reorder_expression(&bottom);
            } else {
                parser_node_shift_children_left(node);
                parser_reorder_expression(&node->exp.left);
                parser_reorder_expression(&node->exp.right);
            }
        }
    }

    if(parser_needs_extra_reorder(node)){
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
    // `&x` of an i386 thread-local x is the pointer its object
    // is read through; the i386 generator loaded the value for `&*`.
    struct node* address = S_EQ(op, "&") ? parser_thread_local_address(operand) : 0;
    if(address){
        node_push(address);
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
    // `v.t.flags++`: the inner level came back lifted, (t.flags)++, so
    // this one is v . (t.flags); reorder it into (v.t).flags. It was
    // left as it was, and the validator found no member name.
    parser_reorder_expression(&exp_node);
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
    // A prefix-only operator after an operand: `n !x`, `a ~b`.
    if(S_EQ(op, "!") || S_EQ(op, "~")){
        compiler_error(current_process, "Expected an operator or ';' but found '%s'\n", op);
    }

    token_next();          // consume operator

    // Only the run noted by the right operand's own parse below may be
    // used by this level's reorder.
    parser_run.root = parser_run.tail = 0;

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

    struct node* result = parser_hoist_postfix(exp_node);
    if(result == exp_node){
        parser_run_note(exp_node);
    } else {
        parser_run.root = parser_run.tail = 0;
    }
    node_push(result);
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
    } else if(!left_node){
        // `()` is a call's empty argument list, never a value: `*()`
        // reached the x86 backend and failed a stack frame assert.
        compiler_error(current_process, "Expected an expression but found ')'\n");
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
    if(parser_vla_pointer_dims){
        compiler_error(current_process, "A cast to a pointer to an array is not supported\n");
    }
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
    // `sizeof[1]`: a subscript with nothing before it reached the
    // resolver with no entity to index and failed an assert.
    if(!left_node || ((history->flags & EXPRESSION_IS_UNARY) && node_count() <= history->unary_operand_base)){
        compiler_error(current_process, "Expected an expression before '['\n");
    }
    node_pop();
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
    // `struct p {, int x; }`: a comma with nothing before it popped an
    // empty node stack.
    if(!node_count()){
        compiler_error(current_process, "Expected an expression before ','\n");
    }
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
        || S_EQ(val, "restrict") || S_EQ(val, "_Atomic")
        || S_EQ(val, "_Thread_local") || S_EQ(val, "__thread");
}

// `_Atomic (`: the atomic type specifier, `_Atomic(int*)`, rather than
// the qualifier.
static bool parser_atomic_specifier_next(void){
    if(!token_is_keyword(token_peek_next(), "_Atomic")){
        return false;
    }
    vector_save(current_process->token_vec);
    token_next();
    bool paren = token_is_operator(token_peek_next(), "(");
    vector_restore(current_process->token_vec);
    return paren;
}

// Consume a run of modifier keywords, OR-ing matching DATATYPE_FLAG_*s
// into dtype. Stops at the first non-modifier keyword. Returns whether
// `signed` or `unsigned` was among them.
static bool parse_datatype_modifiers(struct datatype* dtype){
    bool sign_given = false;
    struct token* token = token_peek_next();
    while(token && token->type == TOKEN_TYPE_KEYWORD){
        if(!is_keyword_variable_modifier(token->sval) || parser_atomic_specifier_next()){
            break;
        }

        if(S_EQ(token->sval, "_Atomic")){
            // the qualifier makes the object at the pointer
            // depth read so far atomic: `_Atomic int *p` is a pointer to
            // an atomic int, `int * _Atomic p` an atomic pointer.
            dtype->atomic_level = dtype->pointer_depth + 1;
        } else if(S_EQ(token->sval, "_Thread_local") || S_EQ(token->sval, "__thread")){
            dtype->flags |= DATATYPE_FLAG_IS_THREAD_LOCAL;
        } else if(S_EQ(token->sval, "signed")){
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
    while(vector_count(parser_vlas)
          && ((struct parser_vla*)vector_back(parser_vlas))->depth > parser_scope_depth()){
        vector_pop(parser_vlas);
    }
}

// Push a parser_scope_entity (carries node + flags + offset) into
// the current scope. ch61 retypes this to take the entity, not the
// raw node.
static void parser_scope_push(struct parser_scope_entity* entity, size_t size){
    scope_push(current_process, entity, size);
}

// Consume the next token and assert it's the given symbol character.
// Fatal compiler_error if not.
static void expect_sym(char c){
    struct token* next_token = token_next();
    if(!next_token || next_token->type != TOKEN_TYPE_SYMBOL || next_token->cval != c){
        compiler_error(current_process, "Expecting symbol %c but found %s\n", c, parser_token_text(next_token));
    }
}

static void expect_op(const char* op){
    struct token* next_token = token_next();
    if(!next_token || next_token->type != TOKEN_TYPE_OPERATOR || !S_EQ(next_token->sval, op)){
        compiler_error(current_process, "Expecting the operator %s but found %s\n", op, parser_token_text(next_token));
    }
}

static void expect_keyword(const char* keyword){
    struct token* next_token = token_next();
    if(!next_token || next_token->type != TOKEN_TYPE_KEYWORD || !S_EQ(next_token->sval, keyword)){
        compiler_error(current_process, "Expecting the keyword %s but found %s\n", keyword, parser_token_text(next_token));
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
    memcpy(sval, tmp_name, sizeof tmp_name);
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

// The `*`s of a declarator with the qualifiers after each: `* const *
// volatile`. Bit i of *consts is set when the i-th `*` is const
//; volatile and restrict change nothing (see
// is_keyword_variable_modifier). Before, a const after a `*` set the
// declaration's single const flag, and a `*` after it was a syntax
// error (`char* const* argv`).
static int parser_get_pointer_levels(int* consts){
    int depth = 0;
    *consts = 0;
    while(token_next_is_operator("*")){
        token_next();
        depth++;
        while(token_next_is_keyword("const") || token_next_is_keyword("volatile")
              || token_next_is_keyword("restrict")){
            if(token_next_is_keyword("const") && depth <= 30){
                *consts |= 1 << (depth - 1);
            }
            token_next();
        }
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
    // `long double` (or `double long`) is a type of its own,
    // 12 bytes on i386 and 16 on the 64-bit targets.
    if(datatype_secondary_token
       && ((S_EQ(datatype_token->sval, "long") && S_EQ(datatype_secondary_token->sval, "double"))
           || (S_EQ(datatype_token->sval, "double") && S_EQ(datatype_secondary_token->sval, "long")))){
        struct datatype ld = datatype_long_double();
        datatype_out->type     = ld.type;
        datatype_out->size     = ld.size;
        datatype_out->type_str = ld.type_str;
        current_process->uses_floating = true;
        current_process->uses_long_double = true;
        return;
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
        current_process->uses_floating = true;
    } else if(S_EQ(datatype_token->sval, "double")){
        // g02: write the type to .type, not .size. A double is 8 bytes
        // on every target (it was 4).
        datatype_out->type = DATA_TYPE_DOUBLE;
        datatype_out->size = DATA_SIZE_DDWORD;
        current_process->uses_floating = true;
    } else {
        compiler_error(current_process, "BUG: Invalid primitive datatype\n");
    }

    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}

// Look up a previously-defined struct by name; returns body size or 0.
static size_t size_of_struct(const char* struct_name){
    struct symbol* sym = symresolver_get_tag(current_process, struct_name);
    if(!sym){
        return 0;
    }
    struct node* n = sym->data;
    if(sym->type != SYMBOL_TYPE_NODE || n->type != NODE_TYPE_STRUCT){
        // `struct num` naming a union (or another symbol) is an error, not
        // an assert (found by tests/322-malformed-fuzz.sh).
        compiler_error(current_process, "%s is not a struct\n", struct_name);
    }
    return n->_struct.body_n->body.size;
}

// ch99: union version of size_of_struct.
static size_t size_of_union(const char* union_name){
    struct symbol* sym = symresolver_get_tag(current_process, union_name);
    if(!sym){
        return 0;
    }
    struct node* n = sym->data;
    if(sym->type != SYMBOL_TYPE_NODE || n->type != NODE_TYPE_UNION){
        // `union num` naming a struct (or another symbol) is an error, not
        // an assert (found by tests/322-malformed-fuzz.sh).
        compiler_error(current_process, "%s is not a union\n", union_name);
    }
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
// Where the parameters of a function declared to return a function
// pointer start (`int (*f(int))(int)`), for parse_function, or -1.
static int parser_function_declarator_params = -1;
// Where parse_function goes on after reading those parameters, or -1.
static int parser_function_resume = -1;

// A parenthesized parameter list, read and not kept.
static void parser_skip_parameters(void){
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
}

// The parameter list of a function pointer's type, `(int, char* s,
// ...)`: the types, a parameter's array adjusted to a pointer
// (C11 6.7.6.3p7). `(void)` has none; `()` gives NULL, parameters not
// specified.
static struct function_prototype* parser_parameter_types(void){
    expect_op("(");
    if(token_next_is_symbol(')')){
        expect_sym(')');
        return 0;
    }
    struct function_prototype* prototype = calloc(1, sizeof(struct function_prototype));
    prototype->types = vector_create(sizeof(struct datatype));
    while(!token_next_is_symbol(')')){
        if(token_next_is_operator(".")){
            token_read_dots(3);
            prototype->variadic = true;
            break;
        }
        struct datatype type;
        parse_datatype(&type);
        parser_ignore_int(&type);
        struct token* name = 0;
        if(!parse_function_pointer_declarator(&type, &name) && token_is_identifier(token_peek_next())){
            token_next();
        }
        // A pointer to an array passes as the pointer it is.
        parser_vla_pointer_dims = 0;
        if(token_next_is_operator("[") || (type.flags & DATATYPE_FLAG_IS_ARRAY)){
            if(token_next_is_operator("[")){
                parse_array_brackets(history_begin(0));
            }
            // An array typedef's brackets (parse_datatype) go too.
            type.flags &= ~DATATYPE_FLAG_IS_ARRAY;
            type.array.brackets = 0;
            type.array.size     = 0;
            type.flags |= DATATYPE_FLAG_IS_POINTER;
            type.pointer_depth++;
        }
        bool is_void = type.type == DATA_TYPE_VOID && !(type.flags & DATATYPE_FLAG_IS_POINTER);
        if(!is_void){
            vector_push(prototype->types, &type);
        }
        if(!token_next_is_operator(",")){
            break;
        }
        token_next();
    }
    expect_sym(')');
    return prototype;
}

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
    int consts = 0;
    int depth = parser_get_pointer_levels(&consts);
    struct token* name = 0;
    if(token_is_identifier(token_peek_next())){
        name = token_next();
    }
    struct array_brackets* brackets = 0;
    if(token_next_is_operator("[")){
        brackets = parse_array_brackets(history_begin(0));
    }
    // `int (*f(int x))(int)`: f is a function, with the parameters
    // right after its name, returning the function pointer. They are
    // skipped here and parsed by parse_function.
    if(name && token_next_is_operator("(")){
        parser_function_declarator_params = current_process->token_vec->pindex;
        parser_skip_parameters();
    }
    expect_sym(')');
    // `T (*p)[n]...`, a pointer to an array: a flat `T*`; the
    // dimensions go to parse_variable.
    if(token_next_is_operator("[")){
        if(depth != 1 || brackets){
            compiler_error(current_process, "Only a pointer to an array, `T (*p)[n]`, is supported\n");
        }
        parser_vla_pointer_element = datatype_size(dtype);
        parser_vla_mode = PARSER_VLA_MODE_ALWAYS;
        parse_array_brackets(history_begin(0));
        parser_vla_mode = PARSER_VLA_MODE_NONE;
        parser_vla_pointer_dims = parser_vla_dims;
        parser_vla_dims = 0;
        dtype->flags |= DATATYPE_FLAG_IS_POINTER;
        dtype->pointer_depth++;
        datatype_set_pointer_const(dtype, consts & 1);
        *name_out = name;
        return true;
    }
    // A call through the pointer converts its arguments to these
    // types and checks their number.
    struct function_prototype* prototype = parser_parameter_types();

    struct datatype* function_return = calloc(1, sizeof(struct datatype));
    *function_return = *dtype;
    dtype->flags |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_FUNCTION_POINTER;
    dtype->pointer_depth   = function_return->pointer_depth + depth;
    if(dtype->pointer_depth <= 30){
        dtype->pointer_const = function_return->pointer_const | (consts << function_return->pointer_depth);
    }
    dtype->function_return = function_return;
    dtype->function_params = prototype;
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
            if(!constant_fold(current_process, node_pop(), &value) || !constant_is_integer(&value)){
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

// Set when the type came from `_Atomic(type-name)`, whose own signedness
// (plain char included) is settled already.
static bool parser_type_sign_settled;

// `_Atomic(type-name)`, the atomic type specifier: the type
// in the parentheses, atomic at its own pointer depth, then the
// declarator's stars. A storage class given before it is kept.
static void parse_atomic_type_specifier(struct datatype* dtype){
    expect_keyword("_Atomic");
    expect_op("(");
    struct datatype inner;
    parse_datatype(&inner);
    parser_ignore_int(&inner);
    expect_sym(')');
    int given = dtype->flags & ~DATATYPE_FLAG_IS_SIGNED;
    *dtype = inner;
    dtype->flags |= given;
    dtype->atomic_level = inner.pointer_depth + 1;
    int depth = parser_get_pointer_depth();
    if(depth > 0){
        dtype->flags |= DATATYPE_FLAG_IS_POINTER;
        dtype->pointer_depth += depth;
    }
    parser_type_sign_settled = true;
}
// Set while parse_sizeof reads its type name: the brackets after it
// are left for parse_sizeof, which knows a VLA type.
static bool parser_sizeof_type_brackets;

// Whether parse_datatype_type read `signed` or `unsigned` after the type
// keyword.
static bool parser_type_sign_given;

static void parse_datatype_type(struct datatype* dtype){
    struct token* datatype_token            = 0;
    if(parser_atomic_specifier_next()){
        parser_last_datatype_was_enum = false;
        parse_atomic_type_specifier(dtype);
        return;
    }
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
        if(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_IDENTIFIER){
            datatype_token = token_next();
        } else {
            datatype_token = parser_build_random_type_name();
            dtype->flags |= DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
        }
    }

    // Qualifiers between the type and its `*`s (`int const* p`,
    // `char unsigned`) belong to the base type.
    parser_type_sign_given = parse_datatype_modifiers(dtype);

    // `int**`, `int* const*` etc.
    int pointer_const = 0;
    int pointer_depth = parser_get_pointer_levels(&pointer_const);

    // Keep the spelling so downstream can render it.
    dtype->type_str      = datatype_token->sval;
    dtype->pointer_depth = pointer_depth;
    dtype->pointer_const = pointer_const;
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
    parser_type_sign_settled = false;
    parse_datatype_type(dtype);
    parser_sign_given = false;
    sign_given |= parser_type_sign_settled;
    parser_type_sign_settled = false;
    sign_given |= parser_type_sign_given;
    sign_given |= parse_datatype_modifiers(dtype);
    // Plain char is unsigned on aarch64 and riscv64; it was
    // always signed, so `char c = 200` read back as -56 there.
    if(dtype->type == DATA_TYPE_CHAR && !sign_given && current_process->target->plain_char_unsigned){
        dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
    }
    if(dtype->flags & DATATYPE_FLAG_IS_BOOL){
        dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
    }
    // Brackets right after the type are part of it: an array typedef
    // (`typedef long buf[8];` makes `buf b;` read `long [8] b`, see
    // preprocessor_handle_typedef_token) or the abstract declarator of
    // `sizeof(int[3])` and `(int[]){ 1, 2 }`. A declarator's own
    // brackets go in front of these (parser_join_brackets). sizeof reads
    // its own (parse_sizeof): `sizeof(int[n][m])` may be a VLA type.
    if(token_next_is_operator("[") && !parser_sizeof_type_brackets){
        struct array_brackets* brackets = parse_array_brackets(history_begin(0));
        dtype->array.brackets = brackets;
        dtype->array.size     = array_brackets_calculate_size(dtype, brackets);
        dtype->flags         |= DATATYPE_FLAG_IS_ARRAY;
        if(token_next_is_operator("*")){
            compiler_error(current_process, "A pointer to an array type is not supported\n");
        }
    }
}

static struct node* parser_bracket_of(size_t n);

// The declarator's own brackets, then those its type already has from an
// array typedef: `buf b[2]` with `typedef long buf[8]` is long[2][8].
// `buf b[]` keeps a 0 first dimension for the initializer to fill.
static struct array_brackets* parser_join_brackets(struct array_brackets* own, struct datatype* dtype){
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY) || !dtype->array.brackets){
        return own;
    }
    struct array_brackets* joined = array_brackets_new();
    if(!vector_count(own->n_brackets)){
        array_brackets_add(joined, parser_bracket_of(0));
    }
    struct vector* lists[2] = { own->n_brackets, dtype->array.brackets->n_brackets };
    for(int l = 0; l < 2; l++){
        for(int i = 0; i < vector_count(lists[l]); i++){
            array_brackets_add(joined, *(struct node**)vector_at(lists[l], i));
        }
    }
    return joined;
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

// The places of a call's arguments, the comma operators taken apart.
static void parser_argument_slots(struct node** slot, struct vector* out){
    struct node* node = *slot;
    if(!node || node->type == NODE_TYPE_BLANK){
        return;
    }
    if(node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, ",")){
        parser_argument_slots(&node->exp.left, out);
        parser_argument_slots(&node->exp.right, out);
        return;
    }
    vector_push(out, &slot);
}

// An argument for a _Bool parameter of a function called by name is 0
// or 1 too: `f(flags & 0x800)` passed the low byte on x86, where the
// call does not convert its arguments.
static void parser_convert_bool_arguments(struct node* call){
    if(call->exp.left->type != NODE_TYPE_IDENTIFIER){
        return;
    }
    struct node* function = 0;
    struct vector* tree = current_process->node_tree_vec;
    for(int i = vector_count(tree) - 1; i >= 0 && !function; i--){
        struct node* candidate = *(struct node**)vector_at(tree, i);
        if(candidate->type == NODE_TYPE_FUNCTION && S_EQ(candidate->func.name, call->exp.left->sval)){
            function = candidate;
        }
    }
    if(!function && parser_current_function && S_EQ(parser_current_function->func.name, call->exp.left->sval)){
        function = parser_current_function;
    }
    struct vector* params = function ? function->func.args.vector : 0;
    if(!params){
        return;
    }
    struct node** right = &call->exp.right;
    if(*right && (*right)->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        right = &(*right)->parenthesis.exp;
    }
    struct vector* slots = vector_create(sizeof(struct node**));
    parser_argument_slots(right, slots);
    for(int i = 0; i < vector_count(slots) && i < vector_count(params); i++){
        struct node** slot = *(struct node***)vector_at(slots, i);
        struct node* param = *(struct node**)vector_at(params, i);
        if(param->type == NODE_TYPE_VARIABLE && parser_is_bool_object(&param->var.type) && !parser_is_bool_value(*slot)){
            *slot = parser_bool_value(*slot);
        }
    }
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
    if(S_EQ(node->exp.op, "()")){
        parser_convert_bool_arguments(node);
        return;
    }
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

// ---------------------------------------------------------------
// bit-field access. A bit-field member is a member of its
// declared type at the offset of the unit that holds it, so the
// backends read and write that whole unit; the finished expression is
// rewritten here so that a read takes the field's bits out of the unit
// and a store puts them into it. The rewritten member access is marked
// NODE_FLAG_BITFIELD_UNIT and stands for the unit from then on.
// ---------------------------------------------------------------

static struct datatype parser_bitfield_int_type(int size, bool is_signed){
    struct datatype t = {0};
    t.type     = size == 1 ? DATA_TYPE_CHAR : size == 2 ? DATA_TYPE_SHORT : size == 4 ? DATA_TYPE_INTEGER : DATA_TYPE_LONG;
    t.type_str = size == 1 ? "char" : size == 2 ? "short" : size == 4 ? "int" : "long";
    t.size     = size;
    t.flags    = is_signed ? DATATYPE_FLAG_IS_SIGNED : 0;
    return t;
}

static struct node* parser_bitfield_parens(struct node* node){
    make_exp_parentheses_node(node);
    return node_pop();
}

static struct node* parser_bitfield_binary(struct node* left, struct node* right, const char* op){
    make_exp_node(left, right, op);
    return parser_bitfield_parens(node_pop());
}

static struct node* parser_bitfield_cast(struct datatype* type, struct node* operand){
    make_cast_node(type, operand);
    return parser_bitfield_parens(node_pop());
}

static struct node* parser_bitfield_number(unsigned long long value, struct datatype* type){
    struct constant c = { value, *type };
    return constant_node(&c);
}

// The type of a call of a function defined or declared above
// (`make(1).bits`), which the resolver does not know while parsing.
static bool parser_call_type(struct node* node, struct datatype* out){
    while(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    if(node->type != NODE_TYPE_EXPRESSION || !S_EQ(node->exp.op, "()") || node->exp.left->type != NODE_TYPE_IDENTIFIER){
        return false;
    }
    struct vector* tree = current_process->node_tree_vec;
    for(int i = vector_count(tree) - 1; i >= 0; i--){
        struct node* function = *(struct node**)vector_at(tree, i);
        if(function->type == NODE_TYPE_FUNCTION && S_EQ(function->func.name, node->exp.left->sval)){
            *out = function->func.rtype;
            return true;
        }
    }
    return false;
}

// The bit-field member `node` accesses, or NULL when it is no bit-field
// access. Only a member named like some bit-field is looked at.
static struct node* parser_bitfield_member(struct node* node){
    if(node->type != NODE_TYPE_EXPRESSION || !is_access_node(node) || (node->flags & NODE_FLAG_BITFIELD_UNIT)
       || node->exp.right->type != NODE_TYPE_IDENTIFIER || !parser_name_in(parser_bitfield_names, node->exp.right->sval)){
        return 0;
    }
    struct datatype type;
    if(!parser_call_type(node->exp.left, &type) && !parser_expression_type(node->exp.left, &type)){
        compiler_error(current_process, "Cannot work out the struct of the bit-field %s\n", node->exp.right->sval);
    }
    struct node* su = type.struct_node;
    if(!datatype_is_struct_or_union(&type) || !su || !su->_struct.body_n){
        return 0;
    }
    struct node* member = struct_member_named(su->_struct.body_n->body.statements, node->exp.right->sval);
    return member && member->var.bitfield.is_bitfield ? member : 0;
}

// The unit the bit-field `member` lives in, through the access `node`.
static struct node* parser_bitfield_unit(struct node* access){
    struct node* unit = calloc(1, sizeof(struct node));
    *unit = *access;
    unit->flags |= NODE_FLAG_BITFIELD_UNIT;
    return unit;
}

// The value of the field `member` in the unit value `unit`: shifted up
// to the top of the unit, then down, arithmetically for a signed field.
// A field narrower than int is an int (C11 6.3.1.1p2).
static struct node* parser_bitfield_extract(struct node* unit, struct node* member){
    struct datatype* type = &member->var.type;
    int size  = (int)datatype_size(type);
    int width = member->var.bitfield.width;
    bool is_signed = (type->flags & DATATYPE_FLAG_IS_SIGNED) && !(type->flags & DATATYPE_FLAG_IS_BOOL);
    struct datatype utype = parser_bitfield_int_type(size, false);
    struct datatype stype = parser_bitfield_int_type(size, true);
    struct datatype itype = datatype_for_int();
    int up   = size * 8 - member->var.bitfield.bit_offset - width;
    int down = size * 8 - width;
    struct node* value = parser_bitfield_cast(&utype, unit);
    if(up){
        value = parser_bitfield_cast(&utype, parser_bitfield_binary(value, parser_bitfield_number(up, &itype), "<<"));
    }
    if(is_signed){
        value = parser_bitfield_cast(&stype, value);
    }
    if(down){
        value = parser_bitfield_binary(value, parser_bitfield_number(down, &itype), ">>");
    }
    if(width < 32 && size <= DATA_SIZE_DWORD){
        return parser_bitfield_cast(&itype, value);
    }
    return parser_bitfield_cast(type, value);
}

// `unit = ((value << offset) & mask) | (unit & ~mask)`, in the unit's
// unsigned type. `read` and `write` are two copies of the unit's access.
// The value comes first, so the unit is read after anything the value
// stores (another field of the same unit, `s.y = (s.x = 1) + 1`).
static struct node* parser_bitfield_store(struct node* read, struct node* write, struct node* member, struct node* value){
    int size  = (int)datatype_size(&member->var.type);
    int width = member->var.bitfield.width;
    int shift = member->var.bitfield.bit_offset;
    struct datatype utype = parser_bitfield_int_type(size, false);
    struct datatype itype = datatype_for_int();
    struct datatype mtype = size > DATA_SIZE_DWORD ? utype : parser_bitfield_int_type(DATA_SIZE_DWORD, false);
    unsigned long long ones = width >= 64 ? ~0ULL : ((1ULL << width) - 1);
    unsigned long long mask = ones << shift;
    unsigned long long all  = size >= 8 ? ~0ULL : ((1ULL << (size * 8)) - 1);
    struct node* kept = parser_bitfield_binary(parser_bitfield_cast(&utype, read),
        parser_bitfield_number(all & ~mask, &mtype), "&");
    struct node* bits = parser_bitfield_cast(&utype, value);
    if(shift){
        bits = parser_bitfield_binary(bits, parser_bitfield_number(shift, &itype), "<<");
    }
    bits = parser_bitfield_binary(bits, parser_bitfield_number(mask, &mtype), "&");
    struct node* unit_value = parser_bitfield_cast(&utype, parser_bitfield_binary(bits, kept, "|"));
    make_exp_node(write, unit_value, "=");
    return node_pop();
}

static struct node* parser_lower_bitfields(struct node* node, bool value_used);

// `access op= value`, `++access`, `access--`: a store of the new value,
// and the expression's value when it is used.
static struct node* parser_bitfield_assignment(struct node* access, struct node* member, const char* op,
                                               struct node* value, bool postfix, bool value_used){
    struct node* new_value = value;
    if(!S_EQ(op, "=")){
        char* arith = strdup(op);
        arith[strlen(arith) - 1] = '\0';
        struct node* old = parser_bitfield_extract(parser_bitfield_unit(access), member);
        new_value = parser_bitfield_binary(old, value, arith);
    }
    struct node* store = parser_bitfield_store(parser_bitfield_unit(access), parser_bitfield_unit(access), member, new_value);
    if(!value_used){
        return store;
    }
    // The store's value is the new unit: the field's bits of it.
    struct node* result = parser_bitfield_extract(parser_bitfield_parens(store), member);
    if(postfix){
        // The old value, the new one stepped back in the field's width.
        struct datatype itype = datatype_for_int();
        bool up = S_EQ(op, "+=");
        struct node* back = parser_bitfield_binary(result, parser_bitfield_number(1, &itype), up ? "-" : "+");
        struct node* wrapped = calloc(1, sizeof(struct node));
        *wrapped = *member;
        wrapped->var.bitfield.bit_offset = 0;
        wrapped->var.type = parser_bitfield_int_type(DATA_SIZE_DWORD, (member->var.type.flags & DATATYPE_FLAG_IS_SIGNED) != 0);
        if(datatype_size(&member->var.type) > DATA_SIZE_DWORD){
            wrapped->var.type = member->var.type;
        }
        result = parser_bitfield_extract(back, wrapped);
    }
    return result;
}

static struct node* parser_lower_bitfields(struct node* node, bool value_used){
    if(!node){
        return node;
    }
    switch(node->type){
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            node->parenthesis.exp = parser_lower_bitfields(node->parenthesis.exp, value_used);
            return node;
        case NODE_TYPE_CAST:
            node->cast.operand = parser_lower_bitfields(node->cast.operand, true);
            return node;
        case NODE_TYPE_TENARY:
            node->tenary.true_node  = parser_lower_bitfields(node->tenary.true_node, value_used);
            node->tenary.false_node = parser_lower_bitfields(node->tenary.false_node, value_used);
            return node;
        case NODE_TYPE_BRACKET:
            node->bracket.inner = parser_lower_bitfields(node->bracket.inner, true);
            return node;
        case NODE_TYPE_UNARY: {
            struct node* member = parser_bitfield_member(node->unary.operand);
            if(member && is_increment_operator(node->unary.op)){
                struct datatype itype = datatype_for_int();
                return parser_bitfield_assignment(node->unary.operand, member, S_EQ(node->unary.op, "++") ? "+=" : "-=",
                    parser_bitfield_number(1, &itype), node->unary.flags & UNARY_FLAG_IS_POSTFIX, value_used);
            }
            if(member && op_is_address(node->unary.op)){
                compiler_error(current_process, "Cannot take the address of the bit-field %s\n", member->var.name);
            }
            node->unary.operand = parser_lower_bitfields(node->unary.operand, true);
            return node;
        }
        case NODE_TYPE_EXPRESSION:
            break;
        default:
            return node;
    }
    if(is_node_assignment(node)){
        struct node* member = parser_bitfield_member(node->exp.left);
        if(member){
            struct node* value = parser_lower_bitfields(node->exp.right, true);
            return parser_bitfield_assignment(node->exp.left, member, node->exp.op, value, false, value_used);
        }
    }
    struct node* member = parser_bitfield_member(node);
    if(member){
        return parser_bitfield_extract(parser_bitfield_unit(node), member);
    }
    node->exp.left = parser_lower_bitfields(node->exp.left, S_EQ(node->exp.op, ",") ? false : true);
    if(!is_access_node(node)){
        node->exp.right = parser_lower_bitfields(node->exp.right, S_EQ(node->exp.op, ",") ? value_used : true);
    }
    return node;
}

static void parse_expressionable_root(struct history* history){
    // Only the outermost expression of a statement has its value unused.
    bool is_statement = parser_expression_is_statement;
    parser_expression_is_statement = false;
    size_t count_before = node_count();
    parse_expressionable(history);
    parser_expect_new_node(count_before, "an expression");
    struct node* result_node = node_pop();
    // Only a program that declares a _Bool can assign to one.
    if(parser_saw_bool){
        parser_convert_bool_assignments(result_node);
    }
    if(vector_count(parser_bitfield_names)){
        result_node = parser_lower_bitfields(result_node, !is_statement);
    }
    // subscripts of multi-dimensional variable-length arrays.
    if(parser_vla_active()){
        result_node = parser_vla_lower(result_node);
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
            // by whole slots. On x86 every argument is pushed as at
            // least 4 bytes too; stepping by the type size put the
            // parameter after a char or short one at the wrong
            // address (`f(char a, char b)` read b from a+1).
            offset = align_value(offset, current_process->target->arg_slot_size);
        }
    }

    if(last_entity){
        offset += variable_node(last_entity->node)->var.aoffset;
        if(variable_node_is_primitive(node)){
            // A21: a double parameter is 4-aligned on the i386 stack,
            // like every argument slot (cdecl).
            size_t align = node->var.type.size;
            if(upward_stack && datatype_is_floating(&node->var.type) && align > current_process->target->arg_slot_size){
                align = current_process->target->arg_slot_size;
            }
            if(datatype_is_long_double(&node->var.type)){
                // 16 on the 64-bit targets, 4 on i386.
                align = datatype_alignment(&node->var.type);
            }
            // an atomic pointer at its own size: the exclusive
            // loads of aarch64 and riscv64 fault on a misaligned one. A
            // plain pointer local keeps the pointee's alignment, as the
            // locked golden output has it.
            if(datatype_is_atomic(&node->var.type) && (node->var.type.flags & DATATYPE_FLAG_IS_POINTER)){
                align = datatype_size(&node->var.type);
            }
            variable_node(node)->var.padding =
                padding(upward_stack ? offset : -offset, align);
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
// Where a member ends, in bits: a bit-field at its last bit.
static int parser_member_end_bits(struct node* member){
    if(member->var.bitfield.is_bitfield){
        return member->var.aoffset * 8 + member->var.bitfield.bit_offset + member->var.bitfield.width;
    }
    return (member->var.aoffset + (int)variable_size(member)) * 8;
}

static void parser_scope_offset_for_structure(struct node* node, struct history* history){
    (void)history;
    int end_bits = 0;
    // the previous member of this struct only. scope_last_entity
    // also searches the enclosing scopes, so the first member came
    // after the last global (`int g; struct s { long a; };` gave the
    // struct 4 bytes of padding plus alignment in its size).
    struct parser_scope_entity* last_entity = vector_back_ptr_or_null(current_process->scope.current->entities);
    if(last_entity){
        end_bits = parser_member_end_bits(variable_node(last_entity->node));
    }
    if(node->var.bitfield.is_bitfield){
        // The psABIs' rule: the bit-field goes right after the
        // previous member when it fits in the unit of its own type that
        // holds that bit, else at the start of the next unit; a width
        // of 0 only moves to the next unit. aoffset is the unit's
        // offset, and the unit may hold earlier members too.
        int unit_bits  = (int)datatype_size(&node->var.type) * 8;
        int align_bits = (int)datatype_alignment(&node->var.type) * 8;
        int unit = end_bits / align_bits * align_bits;
        int start = end_bits;
        if(node->var.bitfield.width == 0 ? end_bits != unit : end_bits + node->var.bitfield.width > unit + unit_bits){
            unit  = align_value(end_bits, align_bits);
            start = unit;
        }
        node->var.padding = 0;
        node->var.aoffset = unit / 8;
        node->var.bitfield.bit_offset = start - unit;
        return;
    }
    int offset = (end_bits + 7) / 8;
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
    if(parser_bitfield_width >= 0){
        // The width parse_variable read; the layout needs it below.
        var_node->var.bitfield.is_bitfield = true;
        var_node->var.bitfield.width       = parser_bitfield_width;
        var_node->var.bitfield.unnamed     = parser_bitfield_unnamed;
        if(!parser_bitfield_unnamed){
            vector_push(parser_bitfield_names, &var_node->var.name);
        }
        parser_bitfield_width   = -1;
        parser_bitfield_unnamed = false;
    }

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
    if(!constant_fold(current_process, node, &c) || !constant_is_integer(&c)){
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
    if(datatype_is_long_double(dtype)){
        // written as its bytes, from parser_global_initializer_node.
        struct vector* leaves = vector_create(sizeof(struct initializer_leaf));
        parser_add_leaf(leaves, 0, dtype, value_node);
        return parser_global_initializer_node(name_token, leaves);
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
    // For this declarator only, not one inside its size expression.
    int mode = parser_vla_mode;
    parser_vla_mode = PARSER_VLA_MODE_NONE;
    struct vector* dims = vector_create(sizeof(struct node*));
    bool runtime = false;
    while(token_next_is_operator("[")){
        expect_op("[");
        struct node* size = 0;
        if(token_is_symbol(token_peek_next(), ']')){
            expect_sym(']');
        } else {
            parse_expressionable_root(history);
            expect_sym(']');
            size = node_pop();
            struct constant value;
            if(!constant_fold(current_process, size, &value)){
                runtime = true;
            }
        }
        vector_push(dims, &size);
    }

    // A local or a parameter may have runtime
    // sizes; a parameter with two or more dimensions is a pointer to an
    // array, made like one with runtime sizes.
    int count = vector_count(dims);
    if((mode == PARSER_VLA_MODE_LOCAL && runtime)
       || (mode == PARSER_VLA_MODE_PARAMETER && (runtime || count >= 2))
       || mode == PARSER_VLA_MODE_ALWAYS){
        parser_vla_dims = dims;
        return brackets;
    }
    for(int i = 0; i < count; i++){
        struct node* size = *(struct node**)vector_at(dims, i);
        if(!size){
            // `int x[];` - empty brackets, no size. An initializer gives
            // the size (parse_variable); `int m[][3]` keeps a 0 in the
            // first dimension so the [3] still counts.
            if(i == count - 1){
                break;
            }
            array_brackets_add(brackets, parser_bracket_of(0));
            continue;
        }
        // The size is a constant expression: `int a[N * 2]`, `char
        // s[sizeof(int) + 1]`.
        struct node* exp_node = parser_fold_required(size, "An array size");
        make_bracket_node(exp_node);
        array_brackets_add(brackets, node_pop());
    }
    vector_free(dims);
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
    // An unnamed bit-field is not a member an initializer sets.
    for(int i = 0; i < vector_count(statements); i++){
        struct node* statement = *(struct node**)vector_at(statements, i);
        if(statement->type == NODE_TYPE_VARIABLE){
            if(!statement->var.bitfield.unnamed){
                vector_push(members, &statement);
            }
        } else if(statement->type == NODE_TYPE_VARIABLE_LIST){
            for(int j = 0; j < vector_count(statement->var_list.list); j++){
                struct node* member = *(struct node**)vector_at(statement->var_list.list, j);
                if(!member->var.bitfield.unnamed){
                    vector_push(members, &member);
                }
            }
        }
    }
    return members;
}

// Element `index` of an aggregate: its type and offset from the start.
static void parser_aggregate_element(struct datatype* type, struct vector* members, size_t index,
                                     struct datatype* element_out, size_t* offset_out){
    parser_initializer_bitfield = 0;
    if(type->flags & DATATYPE_FLAG_IS_ARRAY){
        *element_out = parser_array_element_type(type);
        *offset_out  = index * datatype_size(element_out);
        return;
    }
    struct node* member = *(struct node**)vector_at(members, index);
    *element_out = member->var.type;
    *offset_out  = member->var.aoffset;
    // The value parse_initializer_value reads next sets only the
    // member's bits of the unit at that offset.
    if(member->var.bitfield.is_bitfield){
        parser_initializer_bitfield = &member->var.bitfield;
    }
}

static void parser_add_leaf(struct vector* leaves, size_t offset, struct datatype* type, struct node* value){
    struct initializer_leaf leaf = { .offset = offset, .type = *type, .value = value };
    vector_push(leaves, &leaf);
}

// `char s[N] = "abc"`, `wchar_t w[] = L"abc"`: one leaf per code unit,
// the terminating 0 included when it fits. Returns the number of units
// the string needs with its 0.
static size_t parser_string_initializer(struct datatype* type, size_t offset, struct vector* leaves){
    int unit_size;
    bool is_wchar;
    struct vector* units = parser_string_units(token_next(), &unit_size, &is_wchar);
    size_t needed = vector_count(units);
    if(unit_size == DATA_SIZE_BYTE){
        // A plain string ends at its first NUL, as everywhere else.
        needed = 0;
        while(*(unsigned int*)vector_at(units, needed)){
            needed++;
        }
        needed++;
    }
    size_t length = parser_array_length(type);
    size_t count  = length ? (needed < length ? needed : length) : needed;
    struct datatype element = parser_array_element_type(type);
    for(size_t i = 0; i < count; i++){
        struct constant c = { *(unsigned int*)vector_at(units, i), datatype_for_int() };
        parser_add_leaf(leaves, offset + i * unit_size, &element, constant_node(&c));
    }
    return needed;
}

// A string literal initializes a one-dimensional array of its code
// units (C11 6.7.9p14-15): "" a char array, L"" and U"" an array of
// 4-byte integers (wchar_t, char32_t), u"" one of 2-byte ones.
static bool parser_is_string_array(struct datatype* type){
    struct token* next = token_peek_next();
    if(!next || next->type != TOKEN_TYPE_STRING){
        return false;
    }
    // The first wide literal of the run decides the unit.
    int unit = DATA_SIZE_BYTE;
    vector_save(current_process->token_vec);
    while(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING){
        struct token* t = token_next();
        if(t->wide.unit_size){
            unit = t->wide.unit_size;
            break;
        }
    }
    vector_restore(current_process->token_vec);
    return (type->flags & DATATYPE_FLAG_IS_ARRAY) && type->array.brackets
        && vector_count(type->array.brackets->n_brackets) <= 1
        && datatype_element_size(type) == (size_t)unit
        && !(type->flags & DATATYPE_FLAG_IS_POINTER)
        && !datatype_is_struct_or_union(type);
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
    int designator_start = current_process->token_vec->pindex;
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
    // `.llnum = 1` naming a member of an anonymous struct or union
    // member: select that member and read `.llnum` again as the next
    // designator, inside it (parse_designation_rest).
    for(int i = 0; i < vector_count(members); i++){
        struct node* member = *(struct node**)vector_at(members, i);
        struct node* body = (member->flags & NODE_FLAG_ANONYMOUS_MEMBER) ? variable_struct_or_union_body_node(member) : 0;
        if(body && struct_member_named(body->body.statements, name->sval)){
            vector_set_peek_pointer(current_process->token_vec, designator_start);
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
    const struct bitfield* bitfield = parser_initializer_bitfield;
    parser_initializer_bitfield = 0;
    if(token_next_is_symbol('{')){
        expect_sym('{');
        if(parser_is_string_array(type)){
            parser_string_initializer(type, offset, leaves);
        } else if(parser_is_aggregate(type)){
            parse_initializer_elements(type, offset, leaves, true);
        } else if(!token_next_is_symbol('}')){
            // `int x = { 5 };`
            parser_initializer_bitfield = bitfield;
            parse_initializer_value(type, offset, leaves);
        }
        if(token_next_is_operator(",")){
            token_next();
        }
        expect_sym('}');
        return;
    }
    if(parser_is_string_array(type)){
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
    int value_start = current_process->token_vec->pindex;
    parse_expressionable_root(history_begin(HISTORY_FLAG_NO_COMMA_OPERATOR));
    struct node* value = node_pop();
    if(datatype_is_struct_or_union(type) && !(type->flags & DATATYPE_FLAG_IS_POINTER)){
        struct datatype value_type;
        if(!parser_expression_type(value, &value_type) || !datatype_is_struct_or_union(&value_type)
           || (value_type.flags & DATATYPE_FLAG_IS_POINTER)){
            // Not a struct value: the members take it, as in `struct {
            // struct p a; int b; } x = { 0 };` (C11 6.7.9p20). That was
            // "Expected { or a struct value". Read it again for them.
            vector_set_peek_pointer(current_process->token_vec, value_start);
            parse_initializer_elements(type, offset, leaves, false);
            return;
        }
    }
    if(parser_is_bool_object(type)){
        value = parser_bool_value(value);
    }
    parser_add_leaf(leaves, offset, type, value);
    if(bitfield){
        struct initializer_leaf* leaf = vector_back(leaves);
        leaf->bit_offset = bitfield->bit_offset;
        leaf->bit_width  = bitfield->width;
    }
}

// Reads the initializer after `=` for a variable of `type`. An unsized
// array gets its length here. Returns the leaves, in source order.
static struct vector* parse_initializer(struct datatype* type){
    struct vector* leaves = vector_create(sizeof(struct initializer_leaf));
    bool unsized = (type->flags & DATATYPE_FLAG_IS_ARRAY) && parser_array_length(type) == 0;
    size_t length = 0;
    if(parser_is_string_array(type)){
        length = parser_string_initializer(type, 0, leaves);
    } else if(token_next_is_symbol('{') && (type->flags & DATATYPE_FLAG_IS_ARRAY)){
        expect_sym('{');
        if(parser_is_string_array(type)){
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
// The bits a leaf sets: a bit-field only its own.
static void parser_leaf_bits(struct initializer_leaf* leaf, size_t* start, size_t* end){
    *start = leaf->offset * 8 + leaf->bit_offset;
    *end   = leaf->bit_width ? *start + leaf->bit_width : (leaf->offset + datatype_size(&leaf->type)) * 8;
}

static struct vector* parser_sorted_leaves(struct vector* leaves){
    struct vector* sorted = vector_create(sizeof(struct initializer_leaf));
    int n = vector_count(leaves);
    for(int i = 0; i < n; i++){
        struct initializer_leaf* leaf = vector_at(leaves, i);
        size_t start, end;
        parser_leaf_bits(leaf, &start, &end);
        bool overwritten = false;
        for(int j = i + 1; j < n && !overwritten; j++){
            struct initializer_leaf* later = vector_at(leaves, j);
            size_t later_start, later_end;
            parser_leaf_bits(later, &later_start, &later_end);
            overwritten = later_start < end && start < later_end;
        }
        if(overwritten){
            continue;
        }
        int at = vector_count(sorted);
        while(at > 0 && ((struct initializer_leaf*)vector_at(sorted, at - 1))->offset > leaf->offset){
            at--;
        }
        vector_push_at(sorted, at, leaf);
    }
    return sorted;
}

// The folded bit-field leaves of a global as the bytes they set
//: fields share bytes, and a field's unit may hold earlier
// members, so neither can be written as a value of the unit's type.
// The bytes go in offset order among the other leaves.
static struct vector* parser_global_bitfield_bytes(struct vector* sorted){
    struct vector* bytes = vector_create(sizeof(struct initializer_leaf));
    struct vector* values = vector_create(sizeof(unsigned int));
    for(int i = 0; i < vector_count(sorted); i++){
        struct initializer_leaf* leaf = vector_at(sorted, i);
        if(!leaf->bit_width){
            continue;
        }
        unsigned long long value = leaf->value->llnum;
        for(int bit = 0; bit < leaf->bit_width; bit++){
            if(!((value >> bit) & 1)){
                continue;
            }
            size_t position = leaf->offset * 8 + leaf->bit_offset + bit;
            int found = -1;
            for(int b = 0; b < vector_count(bytes); b++){
                if(((struct initializer_leaf*)vector_at(bytes, b))->offset == position / 8){
                    found = b;
                }
            }
            if(found < 0){
                struct initializer_leaf byte = { .offset = position / 8,
                    .type = (struct datatype){ .type = DATA_TYPE_CHAR, .size = DATA_SIZE_BYTE, .type_str = "char" } };
                unsigned int zero = 0;
                vector_push(bytes, &byte);
                vector_push(values, &zero);
                found = vector_count(bytes) - 1;
            }
            *(unsigned int*)vector_at(values, found) |= 1u << (position % 8);
        }
    }
    struct vector* merged = vector_create(sizeof(struct initializer_leaf));
    for(int i = 0; i < vector_count(sorted); i++){
        struct initializer_leaf* leaf = vector_at(sorted, i);
        if(!leaf->bit_width){
            vector_push(merged, leaf);
        }
    }
    struct datatype char_type = { .type = DATA_TYPE_CHAR, .size = DATA_SIZE_BYTE, .type_str = "char" };
    for(int b = 0; b < vector_count(bytes); b++){
        struct initializer_leaf* byte = vector_at(bytes, b);
        struct constant c = { *(unsigned int*)vector_at(values, b), char_type };
        byte->value = constant_node(&c);
        int at = vector_count(merged);
        while(at > 0 && ((struct initializer_leaf*)vector_at(merged, at - 1))->offset > byte->offset){
            at--;
        }
        vector_push_at(merged, at, byte);
    }
    return merged;
}

// A global's leaves must be constants, string literals for pointers,
// or addresses (`&g`, an array or function name). Constants are folded
// and converted to the leaf's type.
// a long double leaf of a global becomes the words of its
// bytes in the target's format (long_double_fold), 8 bytes each on the
// 64-bit targets and 4 on i386, which every backend writes as numbers.
static struct vector* parser_long_double_leaves(struct token* name_token, struct vector* sorted){
    struct vector* out = vector_create(sizeof(struct initializer_leaf));
    for(int i = 0; i < vector_count(sorted); i++){
        struct initializer_leaf* leaf = vector_at(sorted, i);
        if(!datatype_is_long_double(&leaf->type)){
            vector_push(out, leaf);
            continue;
        }
        unsigned char bytes[16];
        if(!long_double_fold(current_process, leaf->value, bytes)){
            compiler_error(current_process, "The initializer of global %s must be a constant expression\n",
                name_token ? name_token->sval : "variable");
        }
        size_t word = current_process->target->pointer_size;
        for(size_t at = 0; at < leaf->type.size; at += word){
            unsigned long long v = 0;
            for(size_t b = word; b > 0; b--){
                v = (v << 8) | bytes[at + b - 1];
            }
            struct constant c = { v, datatype_for_number_node(&(struct node){ .num.flags = NUMBER_FLAG_TYPED | NUMBER_FLAG_UNSIGNED | NUMBER_FLAG_LONG }) };
            struct initializer_leaf part = { .offset = leaf->offset + at, .type = c.type, .value = constant_node(&c) };
            vector_push(out, &part);
        }
    }
    return out;
}

static struct node* parser_global_initializer_node(struct token* name_token, struct vector* leaves){
    struct vector* sorted = parser_long_double_leaves(name_token, parser_sorted_leaves(leaves));
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
    sorted = parser_global_bitfield_bytes(sorted);
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
    // An initializer may set a const object: `const int a[2] = {1, 2};`
    // was rejected as an assignment to one.
    target.flags &= ~DATATYPE_FLAG_IS_CONST;
    target.pointer_const = 0;
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
        // A bit-field's bytes are zeroed first, then its bits stored.
        for(size_t b = leaf->offset; b < end && b < size && !leaf->bit_width; b++){
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
        if(leaf->bit_width){
            // only the field's bits of its unit.
            struct node member = { .type = NODE_TYPE_VARIABLE };
            member.var.type = leaf->type;
            member.var.bitfield = (struct bitfield){ .is_bitfield = true, .width = leaf->bit_width, .bit_offset = leaf->bit_offset };
            struct node* store = parser_bitfield_store(parser_object_at(name, leaf->offset, &leaf->type),
                parser_object_at(name, leaf->offset, &leaf->type), &member, leaf->value);
            vector_push(parser_pending_statements, &store);
            continue;
        }
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

// ---------------------------------------------------------------
// variable-length arrays. `T a[n];` in a function becomes a
// `T* a` pointing at n * sizeof(T) bytes taken off the stack by
// __builtin_vla_alloc, and sizeof(a) the hidden local those bytes were
// counted into. The block's first one saves the stack pointer
// (__builtin_vla_save) and the block gives it back at its end
// (__builtin_vla_restore), so a loop does not grow the stack; break
// and continue give back the blocks they leave.
// ---------------------------------------------------------------

static struct token* parser_hidden_name(const char* format, int number){
    char* name = malloc(48);
    sprintf(name, format, number);
    struct token* token = calloc(1, sizeof(struct token));
    token->type = TOKEN_TYPE_IDENTIFIER;
    token->sval = name;
    token->pos  = current_process->pos;
    return token;
}

static struct node* parser_identifier(const char* name){
    node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = name });
    return node_pop();
}

// `name(argument)`, or `name()` without one.
static struct node* parser_builtin_call(const char* name, struct node* argument){
    make_exp_parentheses_node(argument ? argument : parser_blank_node);
    struct node* arguments = node_pop();
    make_exp_node(parser_identifier(name), arguments, "()");
    return node_pop();
}

// on i386 a thread-local variable has no address the
// resolver chains can write as an operand (NASM has no local-exec
// relocation, and the initial-exec offset is only known at run time),
// so every use of its name becomes `(*(T*)__builtin_tls_address("name"))`
// (an array: `((E*)__builtin_tls_address("name"))`), the shape va_arg
// already has. codegen_x86_native_tls_address gives this thread's
// address of the variable the name stands for where it is used.
static void parser_note_thread_local(const char* name, struct datatype* dtype){
    if(!(dtype->flags & DATATYPE_FLAG_IS_THREAD_LOCAL) || !S_EQ(current_process->target->backend->name, "x86")){
        return;
    }
    if(!native_function_get(current_process, "__builtin_tls_address")){
        native_create_function(current_process, "__builtin_tls_address",
            &(struct native_function_callbacks){ .call = codegen_x86_native_tls_address });
    }
    struct parser_thread_local tls = { .name = name, .type = *dtype };
    vector_push(parser_thread_locals, &tls);
}

static struct parser_thread_local* parser_thread_local_find(const char* name){
    for(int i = vector_count(parser_thread_locals) - 1; i >= 0; i--){
        struct parser_thread_local* tls = vector_at(parser_thread_locals, i);
        if(S_EQ(tls->name, name)){
            return tls;
        }
    }
    return 0;
}

static struct node* parser_thread_local_object(struct parser_thread_local* tls){
    node_create(&(struct node){ .type = NODE_TYPE_STRING, .sval = tls->name });
    struct node* call = parser_builtin_call("__builtin_tls_address", node_pop());
    bool array = tls->type.flags & DATATYPE_FLAG_IS_ARRAY;
    struct datatype target = array ? datatype_array_decay(&tls->type) : parser_pointer_to(&tls->type);
    target.flags &= ~(DATATYPE_FLAG_IS_THREAD_LOCAL | DATATYPE_FLAG_IS_STATIC | DATATYPE_FLAG_IS_EXTERN);
    make_cast_node(&target, call);
    make_exp_parentheses_node(node_pop());
    if(array){
        return node_pop();
    }
    make_unary_node("*", node_pop());
    struct node* object = node_pop();
    object->unary.indirection.depth = 1;
    make_exp_parentheses_node(object);
    return node_pop();
}

// The pointer inside a parser_thread_local_object, or NULL when `node`
// is not one.
static struct node* parser_thread_local_address(struct node* node){
    if(node->type != NODE_TYPE_EXPRESSION_PARENTHESES){
        return 0;
    }
    struct node* object = node->parenthesis.exp;
    if(object->type != NODE_TYPE_UNARY || !S_EQ(object->unary.op, "*")
       || object->unary.operand->type != NODE_TYPE_EXPRESSION_PARENTHESES){
        return 0;
    }
    struct node* cast = object->unary.operand->parenthesis.exp;
    if(cast->type != NODE_TYPE_CAST || cast->cast.operand->type != NODE_TYPE_EXPRESSION
       || !S_EQ(cast->cast.operand->exp.op, "()") || cast->cast.operand->exp.left->type != NODE_TYPE_IDENTIFIER
       || !S_EQ(cast->cast.operand->exp.left->sval, "__builtin_tls_address")){
        return 0;
    }
    return object->unary.operand;
}

static void parser_vla_natives(void){
    static const char* names[] = { "__builtin_vla_save", "__builtin_vla_alloc", "__builtin_vla_restore" };
    static NATIVE_FUNCTION_CALL calls[] = { codegen_x86_native_vla_save, codegen_x86_native_vla_alloc, codegen_x86_native_vla_restore };
    for(int i = 0; i < 3; i++){
        if(!native_function_get(current_process, names[i])){
            native_create_function(current_process, names[i], &(struct native_function_callbacks){ .call = calls[i] });
        }
    }
}

// A hidden local of `type`, declared before the current statement.
static void parser_hidden_local(struct token* name, struct datatype* type){
    make_variable_node_and_register(history_begin(0), type, name, 0);
    struct node* var_node = node_pop();
    vector_push(parser_preceding_statements, &var_node);
}

// Numbers the hidden locals of every array.
static int parser_vla_number;

// unsigned long, the type of sizes and strides.
static struct datatype parser_vla_size_type(void){
    struct datatype size_type = datatype_for_int();
    size_type.type = DATA_TYPE_LONG;
    size_type.type_str = "long";
    size_type.size = current_process->target->long_size;
    size_type.flags = 0;
    return size_type;
}

static struct node* parser_vla_number_node(unsigned long value){
    struct constant c = { value, parser_vla_size_type() };
    return constant_node(&c);
}

// `(unsigned long)(expr)`
static struct node* parser_vla_as_size(struct node* expr){
    struct datatype size_type = parser_vla_size_type();
    make_exp_parentheses_node(expr);
    make_cast_node(&size_type, node_pop());
    return node_pop();
}

static struct node* parser_vla_binary(struct node* left, struct node* right, const char* op){
    make_exp_node(left, right, op);
    return node_pop();
}

static struct node* parser_vla_parens(struct node* node){
    make_exp_parentheses_node(node);
    return node_pop();
}

// `name = value;` after the declaration.
static void parser_vla_assign_later(const char* name, struct node* value){
    struct node* store = parser_vla_binary(parser_identifier(name), value, "=");
    vector_push(parser_pending_statements, &store);
}

static struct node* parser_vla_stride_node(struct parser_vla_stride* stride){
    return stride->name ? parser_identifier(stride->name) : parser_vla_number_node(stride->value);
}

// The strides of an array whose dimensions are `dims` (the first may be
// NULL: a pointer to an array, a parameter): stride k is the product of
// the dimensions after k, a constant while they all are, else a hidden
// local set right after the declaration, so each size is evaluated once
// (from the last dimension to the first).
static struct parser_vla_info* parser_vla_info_new(struct vector* dims, size_t element_size, int number){
    struct parser_vla_info* info = calloc(1, sizeof(struct parser_vla_info));
    info->dims = vector_count(dims);
    info->element_size = element_size;
    info->strides = calloc(info->dims, sizeof(struct parser_vla_stride));
    struct datatype size_type = parser_vla_size_type();
    unsigned long product = 1;
    const char* product_name = 0;
    for(int k = info->dims - 2; k >= 0; k--){
        struct node* dim = *(struct node**)vector_at(dims, k + 1);
        if(!dim){
            compiler_error(current_process, "Only the first dimension of an array may be left out\n");
        }
        struct constant value;
        if(!product_name && constant_fold(current_process, dim, &value)){
            if(value.value <= 0){
                compiler_error(current_process, "An array size must be greater than 0\n");
            }
            product *= (unsigned long)value.value;
            info->strides[k].value = product;
            continue;
        }
        char format[48];
        snprintf(format, sizeof(format), "__vla_stride_%%i_%i", k);
        struct token* name = parser_hidden_name(format, number);
        parser_hidden_local(name, &size_type);
        struct node* previous = product_name ? parser_identifier(product_name) : parser_vla_number_node(product);
        parser_vla_assign_later(name->sval, parser_vla_binary(previous, parser_vla_as_size(dim), "*"));
        info->strides[k].name = name->sval;
        product_name = name->sval;
    }
    // The last subscript steps over single elements.
    info->strides[info->dims - 1].value = 1;
    return info;
}

// `T a[d0][d1]...;` in a block, some size not
// constant: `T* a` pointing at d0 * d1 * ... * sizeof(T) bytes taken off
// the stack. A multi-dimensional one keeps its strides (parser_vla_info)
// for the subscripts. Each array saves the stack pointer before it
// (__vla_save_N), for a goto that jumps back over it; the block gives
// its first one back at its end, and break and continue too.
static void parser_declare_vla(struct datatype* dtype, struct token* name_token, struct vector* dims, struct history* history){
    int number = parser_vla_number++;
    if(token_next_is_operator("=")){
        compiler_error(current_process, "The variable-length array %s cannot be initialized\n", name_token->sval);
    }
    if(!vector_count(parser_vla_blocks)){
        compiler_error(current_process, "A variable-length array must be in a block\n");
    }
    struct node* first = *(struct node**)vector_at(dims, 0);
    if(!first){
        compiler_error(current_process, "The variable-length array %s needs a size\n", name_token->sval);
    }
    parser_vla_natives();
    struct datatype element = *dtype;
    struct datatype size_type = parser_vla_size_type();
    struct parser_vla_info* info = 0;
    if(vector_count(dims) > 1){
        info = parser_vla_info_new(dims, datatype_size(&element), number);
    }

    // __vla_size_N = (unsigned long)(d0) * stride0 * sizeof(T);
    struct token* size_name = parser_hidden_name("__vla_size_%i", number);
    parser_hidden_local(size_name, &size_type);
    struct node* bytes = parser_vla_as_size(first);
    if(info){
        bytes = parser_vla_binary(bytes, parser_vla_stride_node(&info->strides[0]), "*");
    }
    parser_vla_assign_later(size_name->sval,
        parser_vla_binary(bytes, parser_vla_number_node(datatype_size(&element)), "*"));

    // __vla_save_N = __builtin_vla_save();
    struct parser_vla_block* block = vector_back(parser_vla_blocks);
    struct token* save_name = parser_hidden_name("__vla_save_%i", number);
    parser_hidden_local(save_name, &size_type);
    if(!block->save){
        block->save = save_name->sval;
    }
    if(!block->saves){
        block->saves = vector_create(sizeof(const char*));
    }
    vector_push(block->saves, &save_name->sval);
    parser_vla_assign_later(save_name->sval, parser_builtin_call("__builtin_vla_save", 0));

    // T* a; a = __builtin_vla_alloc(__vla_size_N);
    struct datatype pointer = parser_pointer_to(&element);
    make_variable_node_and_register(history, &pointer, name_token, 0);
    parser_vla_assign_later(name_token->sval,
        parser_builtin_call("__builtin_vla_alloc", parser_identifier(size_name->sval)));

    struct parser_vla vla = { .name = name_token->sval, .size = size_name->sval, .depth = parser_scope_depth(), .info = info };
    vector_push(parser_vlas, &vla);
}

// `name` is a pointer to an array with dimensions `dims`
// (`T (*p)[n][m]`, or the parameter `T p[][n][m]`), already declared a
// flat `T*`. A local's strides are set right after its declaration, a
// parameter's when the function body starts.
static void parser_vla_pointer_to_array(struct token* name_token, struct vector* dims, size_t element_size, bool is_parameter){
    struct vector* all = vector_create(sizeof(struct node*));
    struct node* none = 0;
    vector_push(all, &none);
    for(int i = 0; i < vector_count(dims); i++){
        vector_push(all, vector_at(dims, i));
    }
    if(is_parameter){
        struct parser_vla_param param = { .name = name_token->sval, .dims = all, .element_size = element_size };
        vector_push(parser_vla_params, &param);
        return;
    }
    struct parser_vla vla = { .name = name_token->sval, .depth = parser_scope_depth(),
        .info = parser_vla_info_new(all, element_size, parser_vla_number++) };
    vector_push(parser_vlas, &vla);
}

// The parameters that are pointers to arrays, at the start of the
// function body: their strides.
static void parser_vla_begin_parameters(void){
    for(int i = 0; i < vector_count(parser_vla_params); i++){
        struct parser_vla_param* param = vector_at(parser_vla_params, i);
        struct parser_vla vla = { .name = param->name, .depth = parser_scope_depth(),
            .info = parser_vla_info_new(param->dims, param->element_size, parser_vla_number++) };
        vector_push(parser_vlas, &vla);
    }
    vector_clear(parser_vla_params);
}

// The array (or pointer to an array, or a name hiding one) `name`
// stands for here, or NULL.
static struct parser_vla* parser_vla_find(const char* name){
    for(int i = vector_count(parser_vlas) - 1; i >= 0; i--){
        struct parser_vla* vla = vector_at(parser_vlas, i);
        if(S_EQ(vla->name, name)){
            return vla;
        }
    }
    return 0;
}

// The hidden local holding sizeof of the variable-length array `name`
// in scope, or NULL.
static const char* parser_vla_size_of(const char* name){
    struct parser_vla* vla = parser_vla_find(name);
    return vla ? vla->size : 0;
}

// ---- subscripts of a multi-dimensional array ----
//
// After an expression is parsed, parser_vla_lower rewrites what uses a
// multi-dimensional array or a pointer to an array (a "view": `a`,
// `a[i]`, `p + 1`, at the level of subscripts taken so far):
//
//     a[i]       (a + ((i) * stride0))        a view at level 1
//     a[i][j]    a[(i) * stride0 + (j)]       the element
//     *a         (a)                          a view at level 1
//     p + k      p + (k) * stride0            (also -, += and -=)
//     p++        p += stride0
//     p - q      (p - q) / stride0
//
// A view keeps the array's name and its offset in elements, so an
// element is always a subscript of the flat `T*` variable itself, the
// form all four backends handle (x86's access chains included); the
// rest are plain pointer expressions. Rewritten nodes are remembered
// (parser_vla_done) so an expression lowered again (an inner one is
// lowered when its own parse ends, then within the outer one) is not
// scaled twice.

static bool parser_vla_active(void){
    for(int i = 0; i < vector_count(parser_vlas); i++){
        struct parser_vla* vla = vector_at(parser_vlas, i);
        if(vla->size || vla->info){
            return true;
        }
    }
    return false;
}

static bool parser_vla_is_done(struct node* node){
    for(int i = vector_count(parser_vla_done) - 1; i >= 0; i--){
        if(*(struct node**)vector_at(parser_vla_done, i) == node){
            return true;
        }
    }
    return false;
}

static struct node* parser_vla_mark_done(struct node* node){
    vector_push(parser_vla_done, &node);
    return node;
}

static struct node* parser_vla_view_new(struct node* node, struct parser_vla_info* info, int level,
                                        const char* base, struct node* offset){
    struct parser_vla_view view = { node, info, level, base, offset };
    vector_push(parser_vla_views, &view);
    return node;
}

static bool parser_vla_view_of(struct node* node, struct parser_vla_view* out){
    while(node){
        for(int i = vector_count(parser_vla_views) - 1; i >= 0; i--){
            struct parser_vla_view* view = vector_at(parser_vla_views, i);
            if(view->node == node){
                *out = *view;
                return true;
            }
        }
        if(node->type != NODE_TYPE_EXPRESSION_PARENTHESES){
            break;
        }
        node = node->parenthesis.exp;
    }
    if(node && node->type == NODE_TYPE_IDENTIFIER){
        struct parser_vla* vla = parser_vla_find(node->sval);
        if(vla && vla->info){
            *out = (struct parser_vla_view){ node, vla->info, 0, node->sval, 0 };
            return true;
        }
    }
    return false;
}

// A view whose arithmetic steps over more than one element.
static bool parser_vla_view_scales(struct parser_vla_view* view){
    return view->level + 1 < view->info->dims;
}

// `(value) * stride[level]`
static struct node* parser_vla_scaled(struct node* value, struct parser_vla_info* info, int level){
    struct parser_vla_stride* stride = &info->strides[level];
    if(!stride->name && stride->value == 1){
        return parser_vla_parens(value);
    }
    return parser_vla_binary(parser_vla_parens(value), parser_vla_stride_node(stride), "*");
}

// offset op more, either of which may be NULL (0).
static struct node* parser_vla_offset(struct node* offset, struct node* more, const char* op){
    if(!more){
        return offset;
    }
    if(!offset){
        return S_EQ(op, "+") ? more : parser_vla_binary(parser_vla_number_node(0), more, op);
    }
    return parser_vla_binary(offset, more, op);
}

// The view at `level` that is `offset` elements into `base`: as a value
// the pointer `(base + offset)`; at the last level the element
// `base[offset]`, a subscript of the pointer variable itself, the form
// every backend handles best.
static struct node* parser_vla_view_at(struct parser_vla_info* info, int level, const char* base, struct node* offset){
    if(level < info->dims){
        struct node* value = offset
            ? parser_vla_mark_done(parser_vla_binary(parser_identifier(base), parser_vla_parens(offset), "+"))
            : parser_identifier(base);
        return parser_vla_view_new(parser_vla_parens(value), info, level, base, offset);
    }
    make_bracket_node(offset ? offset : parser_vla_number_node(0));
    struct node* element = parser_vla_mark_done(parser_vla_binary(parser_identifier(base), node_pop(), "[]"));
    return element;
}

// view[index]
static struct node* parser_vla_subscript(struct parser_vla_view* view, struct node* index){
    struct node* offset = parser_vla_offset(view->offset, parser_vla_scaled(index, view->info, view->level), "+");
    return parser_vla_view_at(view->info, view->level + 1, view->base, offset);
}

static struct node* parser_vla_lower(struct node* node);

static struct node* parser_vla_lower_unary(struct node* node){
    node->unary.operand = parser_vla_lower(node->unary.operand);
    struct parser_vla_view view;
    if(op_is_indirection(node->unary.op) && parser_vla_view_of(node->unary.operand, &view)){
        int depth = node->unary.indirection.depth > 0 ? node->unary.indirection.depth : 1;
        int level = view.level + depth;
        if(level > view.info->dims){
            return node;
        }
        return parser_vla_view_at(view.info, level, view.base, view.offset);
    }
    if(op_is_address(node->unary.op)){
        struct node* operand = node->unary.operand;
        while(operand->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            operand = operand->parenthesis.exp;
        }
        // `&a` of a variable-length array is its address, the pointer
        // a holds, not where that pointer is kept.
        if(operand->type == NODE_TYPE_IDENTIFIER){
            struct parser_vla* vla = parser_vla_find(operand->sval);
            if(vla && vla->size){
                return operand;
            }
        }
        // `&a[i]` points at the row a[i], which is where it starts.
        if(parser_vla_view_of(node->unary.operand, &view) && view.level > 0){
            return parser_vla_view_at(view.info, view.level - 1, view.base, view.offset);
        }
        return node;
    }
    if(is_increment_operator(node->unary.op) && parser_vla_view_of(node->unary.operand, &view)
       && parser_vla_view_scales(&view)){
        const char* op = S_EQ(node->unary.op, "++") ? "+=" : "-=";
        struct node* step = parser_vla_mark_done(
            parser_vla_binary(node->unary.operand, parser_vla_stride_node(&view.info->strides[view.level]), op));
        if(!(node->unary.flags & UNARY_FLAG_IS_POSTFIX)){
            return step;
        }
        // The old value: the new one stepped back.
        return parser_vla_parens(parser_vla_mark_done(parser_vla_binary(parser_vla_parens(step),
            parser_vla_stride_node(&view.info->strides[view.level]), S_EQ(op, "+=") ? "-" : "+")));
    }
    return node;
}

static struct node* parser_vla_lower_expression(struct node* node){
    const char* op = node->exp.op;
    node->exp.left = parser_vla_lower(node->exp.left);
    if(is_access_node(node)){
        return node;
    }
    node->exp.right = parser_vla_lower(node->exp.right);
    if(parser_vla_is_done(node)){
        return node;
    }
    struct parser_vla_view view;
    if(is_array_node(node) && node->exp.right && node->exp.right->type == NODE_TYPE_BRACKET
       && parser_vla_view_of(node->exp.left, &view)){
        return parser_vla_subscript(&view, node->exp.right->bracket.inner);
    }
    struct parser_vla_view left, right;
    bool left_view  = parser_vla_view_of(node->exp.left, &left) && parser_vla_view_scales(&left);
    bool right_view = parser_vla_view_of(node->exp.right, &right) && parser_vla_view_scales(&right);
    if(S_EQ(op, "-") && left_view && right_view && left.level == right.level){
        // Rows between two pointers into the same array.
        parser_vla_mark_done(node);
        struct parser_vla_stride* stride = &left.info->strides[left.level];
        return parser_vla_parens(parser_vla_binary(parser_vla_parens(node), parser_vla_stride_node(stride), "/"));
    }
    if((S_EQ(op, "+") || S_EQ(op, "-") || S_EQ(op, "+=") || S_EQ(op, "-=")) && left_view && !right_view){
        struct node* scaled = parser_vla_scaled(node->exp.right, left.info, left.level);
        node->exp.right = scaled;
        parser_vla_mark_done(node);
        if(S_EQ(op, "+=") || S_EQ(op, "-=")){
            return node;
        }
        return parser_vla_view_new(node, left.info, left.level, left.base, parser_vla_offset(left.offset, scaled, op));
    }
    if(S_EQ(op, "+") && right_view && !left_view){
        struct node* scaled = parser_vla_scaled(node->exp.left, right.info, right.level);
        node->exp.left = scaled;
        parser_vla_mark_done(node);
        return parser_vla_view_new(node, right.info, right.level, right.base, parser_vla_offset(right.offset, scaled, "+"));
    }
    return node;
}

static struct node* parser_vla_lower(struct node* node){
    if(!node){
        return node;
    }
    switch(node->type){
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            node->parenthesis.exp = parser_vla_lower(node->parenthesis.exp);
            return node;
        case NODE_TYPE_CAST:
            node->cast.operand = parser_vla_lower(node->cast.operand);
            return node;
        case NODE_TYPE_TENARY:
            node->tenary.true_node  = parser_vla_lower(node->tenary.true_node);
            node->tenary.false_node = parser_vla_lower(node->tenary.false_node);
            return node;
        case NODE_TYPE_BRACKET:
            node->bracket.inner = parser_vla_lower(node->bracket.inner);
            return node;
        case NODE_TYPE_UNARY:
            return parser_vla_lower_unary(node);
        case NODE_TYPE_EXPRESSION:
            return parser_vla_lower_expression(node);
    }
    return node;
}

// sizeof of a view: the bytes of one row at its level.
static struct node* parser_vla_view_size(struct parser_vla_view* view){
    struct parser_vla_stride* stride = &view->info->strides[view->level - 1];
    return parser_vla_binary(parser_vla_stride_node(stride), parser_vla_number_node(view->info->element_size), "*");
}

// ---- a goto out of a block with a variable-length array ----

static int parser_vla_block_ids;

// Where the parse is: the open blocks and how many arrays each has.
static struct vector* parser_vla_place_here(void){
    struct vector* places = vector_create(sizeof(struct parser_vla_place));
    for(int i = 0; i < vector_count(parser_vla_blocks); i++){
        struct parser_vla_block* block = vector_at(parser_vla_blocks, i);
        struct parser_vla_place place = { block->id, block->saves ? vector_count(block->saves) : 0, block->saves };
        vector_push(places, &place);
    }
    return places;
}

static void parser_vla_note_label(const char* name){
    struct parser_vla_label label = { name, parser_vla_place_here() };
    vector_push(parser_vla_labels, &label);
}

// A goto: a blank statement before it, which becomes the restore once
// the label is known, when some open block has an array.
static void parser_vla_note_goto(const char* name){
    struct vector* places = parser_vla_place_here();
    bool any = false;
    for(int i = 0; i < vector_count(places); i++){
        any |= ((struct parser_vla_place*)vector_at(places, i))->count > 0;
    }
    if(!any){
        vector_free(places);
        return;
    }
    node_create(&(struct node){ .type = NODE_TYPE_BLANK });
    struct node* restore = node_pop();
    vector_push(parser_preceding_statements, &restore);
    struct parser_vla_goto jump = { name, places, restore };
    vector_push(parser_vla_gotos, &jump);
}

// The save to give the stack back to for a goto from `from` to `to`:
// in a block both are in, the first array declared after the label
// (a jump back over it); else the first array of the outermost block
// the goto leaves. NULL when it frees nothing.
static const char* parser_vla_goto_save(struct vector* from, struct vector* to){
    int i = 0;
    for(; i < vector_count(from) && i < vector_count(to); i++){
        struct parser_vla_place* f = vector_at(from, i);
        struct parser_vla_place* t = vector_at(to, i);
        if(f->id != t->id){
            break;
        }
        if(f->count > t->count){
            return *(const char**)vector_at(f->saves, t->count);
        }
    }
    for(; i < vector_count(from); i++){
        struct parser_vla_place* f = vector_at(from, i);
        if(f->count > 0){
            return *(const char**)vector_at(f->saves, 0);
        }
    }
    return 0;
}

// At the end of a function body, every goto's labels are known.
static void parser_vla_finish_gotos(void){
    for(int i = 0; i < vector_count(parser_vla_gotos); i++){
        struct parser_vla_goto* jump = vector_at(parser_vla_gotos, i);
        for(int l = 0; l < vector_count(parser_vla_labels); l++){
            struct parser_vla_label* label = vector_at(parser_vla_labels, l);
            if(!S_EQ(label->name, jump->name)){
                continue;
            }
            const char* save = parser_vla_goto_save(jump->blocks, label->blocks);
            if(save){
                struct node* call = parser_builtin_call("__builtin_vla_restore", parser_identifier(save));
                struct node* blank = jump->restore;
                call->binded = blank->binded;
                *blank = *call;
            }
        }
    }
    vector_clear(parser_vla_gotos);
    vector_clear(parser_vla_labels);
    vector_clear(parser_vla_views);
    vector_clear(parser_vla_done);
}

// Before a break or continue: give back the stack of the blocks it
// leaves, from the outermost one inside the loop (or switch) that has a
// variable-length array.
static void parser_vla_leave(bool is_continue){
    for(int i = vector_count(parser_vla_jumps) - 1; i >= 0; i--){
        struct parser_vla_jump* jump = vector_at(parser_vla_jumps, i);
        if(is_continue && !jump->is_loop){
            continue;
        }
        for(int b = jump->depth; b < vector_count(parser_vla_blocks); b++){
            struct parser_vla_block* block = vector_at(parser_vla_blocks, b);
            if(block->save){
                struct node* restore = parser_builtin_call("__builtin_vla_restore", parser_identifier(block->save));
                vector_push(parser_preceding_statements, &restore);
                return;
            }
        }
        return;
    }
}

static void parser_vla_jump_begin(bool is_loop){
    struct parser_vla_jump jump = { .is_loop = is_loop, .depth = vector_count(parser_vla_blocks) };
    vector_push(parser_vla_jumps, &jump);
}

static void parser_vla_jump_end(void){
    vector_pop(parser_vla_jumps);
}

// A wide string literal (L"", u"", U"") is an array of static storage
// duration (C11 6.4.5p6): a hidden global `__wide_string_N` of its code
// units, the 0 included, which the expression names, as a compound
// literal does. Its element type: wchar_t is int (unsigned int on
// aarch64), char16_t unsigned short, char32_t unsigned int.
static void parser_wide_string_object(struct vector* units, int unit_size, bool is_wchar){
    static int count = 0;
    char* name = malloc(32);
    sprintf(name, "__wide_string_%i", count++);
    struct token* name_token = calloc(1, sizeof(struct token));
    name_token->type = TOKEN_TYPE_IDENTIFIER;
    name_token->sval = name;
    name_token->pos  = current_process->pos;

    struct datatype element = {0};
    if(unit_size == DATA_SIZE_WORD){
        element.type     = DATA_TYPE_SHORT;
        element.type_str = "short";
    } else {
        element.type     = DATA_TYPE_INTEGER;
        element.type_str = "int";
        if(is_wchar && !target_wchar_unsigned(current_process->target)){
            element.flags = DATATYPE_FLAG_IS_SIGNED;
        }
    }
    element.size = unit_size;
    struct datatype type = element;
    // Internal linkage, as a string literal has: every file numbers its
    // own `__wide_string_N` from 0, and exported ones clashed at link
    // time ("multiple definition of `__wide_string_0'").
    type.flags |= DATATYPE_FLAG_IS_ARRAY | DATATYPE_FLAG_IS_STATIC;
    type.array.brackets = array_brackets_new();
    array_brackets_add(type.array.brackets, parser_bracket_of(vector_count(units)));
    type.array.size = array_brackets_calculate_size(&type, type.array.brackets);

    struct vector* leaves = vector_create(sizeof(struct initializer_leaf));
    for(int i = 0; i < vector_count(units); i++){
        struct constant c = { *(unsigned int*)vector_at(units, i), datatype_for_int() };
        parser_add_leaf(leaves, i * unit_size, &element, constant_node(&c));
    }
    make_variable_node(&type, name_token, parser_global_initializer_node(name_token, leaves));
    struct node* var_node = node_pop();
    vector_push(current_process->node_tree_vec, &var_node);
    // sizeof(L"ab") and the type of the expression come from the
    // resolver, as for a static local.
    resolver_default_new_scope_entity(current_process->resolver, var_node, 0, 0);
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
    // The resolver gives sizeof its operand's type (`sizeof(text)`,
    // `sizeof(arg.argument)`); register the renamed global here, not as
    // a stack variable.
    resolver_default_new_scope_entity(current_process->resolver, var_node, 0, 0);

    struct parser_enumerator e = { .name = name_token->sval, .depth = parser_scope_depth(), .rename = name };
    vector_push(parser_enumerators, &e);
    parser_note_thread_local(name, dtype);
    node_create(&(struct node){ .type = NODE_TYPE_BLANK });
}

// `extern int g;` inside a function names the file-scope g, usually
// another object's (C11 6.2.2p4); it was a stack variable with no
// value. Like a static local it becomes a global node, here
// an extern one under its own name, and the declaration is blank.
static void parse_extern_local(struct datatype* dtype, struct token* name_token){
    make_variable_node(dtype, name_token, 0);
    struct node* var_node = node_pop();
    vector_push(current_process->node_tree_vec, &var_node);
    resolver_default_new_scope_entity(current_process->resolver, var_node, 0, 0);

    struct parser_enumerator e = { .name = name_token->sval, .depth = parser_scope_depth(), .rename = name_token->sval };
    vector_push(parser_enumerators, &e);
    node_create(&(struct node){ .type = NODE_TYPE_BLANK });
}

// Variable declarator: `name` already consumed; optionally followed by
// `[N]...` (ch44) or `= expr`.
static void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history){
    struct node* value_node = 0;

    bool plain_object = name_token
        && !(dtype->flags & (DATATYPE_FLAG_IS_STATIC | DATATYPE_FLAG_IS_EXTERN))
        && !(history->flags & (HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION));
    bool is_parameter = parser_current_function && (history->flags & HISTORY_FLAG_IS_UPWARD_STACK);
    bool is_block_local = parser_current_function && !is_parameter
        && current_process->scope.current != current_process->scope.root;
    // A local that hides a variable-length array of an outer block.
    if(is_block_local && name_token && parser_vla_find(name_token->sval)){
        struct parser_vla hide = { .name = name_token->sval, .depth = parser_scope_depth() };
        vector_push(parser_vlas, &hide);
    }

    // `T (*p)[n]...`, a pointer to an array, is a flat `T*`
    // whose subscripts step over the array's rows.
    if(parser_vla_pointer_dims && !name_token && is_parameter){
        // An unnamed parameter of a prototype: only its type matters.
        parser_vla_pointer_dims = 0;
    }
    if(parser_vla_pointer_dims){
        struct vector* dims = parser_vla_pointer_dims;
        parser_vla_pointer_dims = 0;
        if(!plain_object || !(is_parameter || is_block_local)){
            compiler_error(current_process, "A pointer to an array is only supported as a local or a parameter\n");
        }
        if(token_next_is_operator("[")){
            compiler_error(current_process, "An array of pointers to arrays is not supported\n");
        }
        parser_vla_pointer_to_array(name_token, dims, parser_vla_pointer_element, is_parameter);
    }

    // ch44: array brackets, e.g. `int x[4][3];`
    if(token_next_is_operator("[")){
        if(plain_object && is_block_local){
            parser_vla_mode = PARSER_VLA_MODE_LOCAL;
        } else if(plain_object && is_parameter){
            parser_vla_mode = PARSER_VLA_MODE_PARAMETER;
        }
        struct array_brackets* brackets = parse_array_brackets(history);
        parser_vla_mode = PARSER_VLA_MODE_NONE;
        if(parser_vla_dims){
            struct vector* dims = parser_vla_dims;
            parser_vla_dims = 0;
            if(!is_parameter){
                parser_declare_vla(dtype, name_token, dims, history);
                return;
            }
            // A parameter `T a[n][m]` is `T (*a)[m]` (C11 6.7.6.3p7).
            struct vector* inner = vector_create(sizeof(struct node*));
            for(int i = 1; i < vector_count(dims); i++){
                vector_push(inner, vector_at(dims, i));
            }
            size_t element_size = datatype_size(dtype);
            *dtype = parser_pointer_to(dtype);
            if(vector_count(inner)){
                parser_vla_pointer_to_array(name_token, inner, element_size, true);
            }
        } else {
            brackets = parser_join_brackets(brackets, dtype);
            dtype->array.brackets = brackets;
            dtype->array.size     = array_brackets_calculate_size(dtype, brackets);
            dtype->flags         |= DATATYPE_FLAG_IS_ARRAY;
        }
    }

    // A parameter declared as an array of T is a pointer to T (C11
    // 6.7.6.3p7): `long b[4]`, `long b[]` and an array typedef's `buf b`.
    // The parameter was laid out and read as the whole array before. A
    // pointer to an array (`int m[][3]`) has no type here, so a
    // multi-dimensional parameter keeps its array type.
    if((history->flags & HISTORY_FLAG_IS_UPWARD_STACK) && (dtype->flags & DATATYPE_FLAG_IS_ARRAY)
       && (!dtype->array.brackets || vector_count(dtype->array.brackets->n_brackets) <= 1)){
        dtype->flags        &= ~DATATYPE_FLAG_IS_ARRAY;
        dtype->array.brackets = 0;
        dtype->array.size     = 0;
        dtype->flags         |= DATATYPE_FLAG_IS_POINTER;
        dtype->pointer_depth++;
    }

    // a bit-field member, `int x : 3;` or `unsigned : 0;`.
    if((history->flags & (HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION)) && token_next_is_symbol(':')){
        token_next();
        parse_expressionable_root(history_begin(HISTORY_FLAG_NO_COMMA_OPERATOR));
        struct node* width = parser_fold_required(node_pop(), "A bit-field width");
        long long bits = (long long)width->llnum;
        if((dtype->flags & (DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_ARRAY)) || datatype_is_struct_or_union(dtype)
           || dtype->type == DATA_TYPE_VOID){
            compiler_error(current_process, "A bit-field must have an integer type\n");
        }
        if(bits < 0 || bits > (long long)datatype_size(dtype) * 8){
            compiler_error(current_process, "The bit-field width %lld does not fit its type\n", bits);
        }
        if(bits == 0 && !parser_bitfield_unnamed){
            compiler_error(current_process, "A bit-field of width 0 must have no name\n");
        }
        parser_bitfield_width = (int)bits;
    }

    bool is_global = current_process->scope.current == current_process->scope.root;
    bool is_static_local = (dtype->flags & DATATYPE_FLAG_IS_STATIC) && name_token && parser_current_function
       && !is_global
       && !(history->flags & (HISTORY_FLAG_IS_UPWARD_STACK | HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION));
    bool is_extern_local = (dtype->flags & DATATYPE_FLAG_IS_EXTERN) && name_token && parser_current_function
       && !is_global
       && !(history->flags & (HISTORY_FLAG_IS_UPWARD_STACK | HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION));
    // one object per thread is a global, a static local or an
    // extern; at block scope _Thread_local needs static or extern (C11
    // 6.7.1p3), and a parameter or a member cannot have it.
    if((dtype->flags & DATATYPE_FLAG_IS_THREAD_LOCAL)
       && ((history->flags & (HISTORY_FLAG_IS_UPWARD_STACK | HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION))
           || (!is_global && !is_static_local && !is_extern_local))){
        compiler_error(current_process, "_Thread_local %s must be a global, static or extern variable\n",
            name_token ? name_token->sval : "");
    }
    struct vector* leaves = 0;
    const char*     outer_declaring_name = parser_declaring_name;
    struct datatype outer_declaring_type = parser_declaring_type;
    if(token_next_is_operator("=")){
        token_next();
        // The name is in scope from the end of its declarator (C11
        // 6.2.1p7): `struct s* p = calloc(1, sizeof(*p));`.
        parser_declaring_name = name_token ? name_token->sval : 0;
        parser_declaring_type = *dtype;
        bool string_for_array = parser_is_string_array(dtype);
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
    parser_declaring_name = outer_declaring_name;
    parser_declaring_type = outer_declaring_type;

    if(is_static_local){
        parse_static_local(dtype, name_token, value_node);
        return;
    }
    if(is_extern_local){
        if(value_node){
            compiler_error(current_process, "The block scope extern %s cannot have an initializer\n", name_token->sval);
        }
        parse_extern_local(dtype, name_token);
        parser_note_thread_local(name_token->sval, dtype);
        return;
    }
    if(is_global && name_token){
        parser_note_thread_local(name_token->sval, dtype);
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
// Set when the last list parse_function_arguments read ended in `...`.
static bool parser_arguments_variadic;

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

// After a function's `(`: an identifier list (`f(a, b)`), the
// parameters of an old-style definition. A type name here is a keyword
// (typedefs are substituted), so an identifier followed by `,` or `)`
// is a parameter name.
static bool parser_next_is_identifier_list(void){
    if(!token_is_identifier(token_peek_next())){
        return false;
    }
    vector_save(current_process->token_vec);
    token_next();
    bool list = token_next_is_operator(",") || token_next_is_symbol(')');
    vector_restore(current_process->token_vec);
    return list;
}

// An old-style (K&R) definition, C11 6.9.1p6: `int f(a, b) char* b;
// { ... }`. The names, then the declarations before the body give
// their types, in any order; a name with no declaration is an int, and
// an array parameter is a pointer. The parameters are made in the
// order of the names, as parse_function_arguments makes them, and
// called the same way: the callers' arguments are converted to their
// types.
static struct vector* parse_identifier_list_parameters(struct history* history){
    struct vector* names = vector_create(sizeof(struct token*));
    while(true){
        struct token* name = token_next();
        if(!token_is_identifier(name)){
            compiler_error(current_process, "Expected a parameter name but found %s\n", parser_token_text(name));
        }
        vector_push(names, &name);
        if(!token_next_is_operator(",")){
            break;
        }
        token_next();
    }
    expect_sym(')');

    int count = vector_count(names);
    struct datatype* types = calloc(count, sizeof(struct datatype));
    bool* declared = calloc(count, sizeof(bool));
    while(!token_next_is_symbol('{') && !token_next_is_symbol(';')){
        struct datatype base;
        parse_datatype(&base);
        parser_ignore_int(&base);
        bool first = true;
        while(true){
            struct datatype type = base;
            if(!first){
                type.flags &= ~DATATYPE_FLAG_IS_POINTER;
                type.pointer_depth = parser_get_pointer_levels(&type.pointer_const);
                if(type.pointer_depth > 0){
                    type.flags |= DATATYPE_FLAG_IS_POINTER;
                }
            }
            first = false;
            struct token* name = 0;
            if(!parse_function_pointer_declarator(&type, &name)){
                name = token_next();
            }
            if(parser_vla_pointer_dims){
                compiler_error(current_process, "A pointer to an array is not supported in an old-style parameter declaration\n");
            }
            if(!token_is_identifier(name)){
                compiler_error(current_process, "Expected a parameter name in the declarations but found %s\n", parser_token_text(name));
            }
            if(token_next_is_operator("[")){
                parse_array_brackets(history);
                type.flags |= DATATYPE_FLAG_IS_POINTER;
                type.pointer_depth++;
            }
            int index = -1;
            for(int i = 0; i < count; i++){
                if(S_EQ((*(struct token**)vector_at(names, i))->sval, name->sval)){
                    index = i;
                }
            }
            if(index < 0){
                compiler_error(current_process, "%s is declared but is not a parameter\n", name->sval);
            }
            if(declared[index]){
                compiler_error(current_process, "The parameter %s is declared twice\n", name->sval);
            }
            types[index] = type;
            declared[index] = true;
            if(!token_next_is_operator(",")){
                break;
            }
            token_next();
        }
        expect_sym(';');
    }

    parser_scope_new();
    struct vector* arguments_vec = vector_create(sizeof(struct node*));
    for(int i = 0; i < count; i++){
        struct datatype type = declared[i] ? types[i] : datatype_for_int();
        parse_variable(&type, *(struct token**)vector_at(names, i),
            history_down(history, history->flags | HISTORY_FLAG_IS_UPWARD_STACK));
        struct node* argument_node = node_pop();
        vector_push(arguments_vec, &argument_node);
    }
    parser_scope_finish();
    return arguments_vec;
}

static void parse_function(struct datatype* ret_type, struct token* name_token, struct history* history){
    struct vector* arguments_vector = 0;
    // A function inside a function body, defined (a GNU extension) or
    // only declared (C allows a block-scope prototype), became a body
    // statement that codegen segfaulted on.
    if(parser_current_function){
        compiler_error(current_process, "Function %s is declared inside function %s; "
            "declare it at file scope (block-scope functions are not supported)\n",
            name_token->sval, parser_current_function->func.name);
    }
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
    vector_clear(parser_vla_params);
    if(parser_next_is_identifier_list()){
        arguments_vector = parse_identifier_list_parameters(history_begin(0));
    } else {
        arguments_vector = parse_function_arguments(history_begin(0));
        expect_sym(')');
    }
    if(parser_function_resume >= 0){
        // After the declarator the parameters were taken from.
        vector_set_peek_pointer(current_process->token_vec, parser_function_resume);
        parser_function_resume = -1;
    }

    function_node->func.args.vector = arguments_vector;
    if(parser_arguments_variadic){
        function_node->func.flags |= FUNCTION_NODE_FLAG_IS_VARIADIC;
    }
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
        parser_vla_finish_gotos();
    } else {
        expect_sym(';');
    }
    vector_clear(parser_vla_params);

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
    while(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_STRING){
        for(const char* c = token_next()->sval; *c; c++){
            buffer_write(text, *c);
        }
    }
    buffer_write(text, 0);
    while(!token_next_is_symbol(')')){
        struct token* token = token_next();
        if(!token){
            compiler_error(current_process, "Expected ')' to end the asm statement but found the end of the file\n");
        }
        if(token_is_operator(token, "(")){
            compiler_error(current_process, "asm operands are not supported, only clobbers\n");
        }
    }
    expect_sym(')');
    expect_sym(';');
    node_create(&(struct node){ .type = NODE_TYPE_ASM, .sval = buffer_ptr(text) });
}

static void parse_statement(struct history* history){
    compiler_check_stack(current_process);
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
            if(parser_current_function){
                parser_vla_note_label(label);
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

    parser_expression_is_statement = true;
    parse_expressionable_root(history);
    if(token_peek_next() && token_peek_next()->type == TOKEN_TYPE_SYMBOL
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
        if(node->var.type.type == DATA_TYPE_LONG_DOUBLE && !(node->var.type.flags & DATATYPE_FLAG_IS_POINTER)){
            // the up to 12 bytes that align a long double to
            // 16 count too, or the last ones of a frame overran it.
            *_variable_size += node->var.padding;
        }
    } else if(node->type == NODE_TYPE_VARIABLE_LIST){
        parser_append_size_for_variable_list(history, _variable_size, node->var_list.list);
    }
}

static void parser_struct_member_extent(struct node* member, size_t* end, size_t* align){
    size_t member_end = member->var.aoffset + variable_size(member);
    size_t member_align = datatype_alignment(&member->var.type);
    if(member->var.bitfield.is_bitfield){
        // A bit-field ends at the byte of its last bit; an unnamed one
        // does not align the struct (the psABIs).
        member_end = (parser_member_end_bits(member) + 7) / 8;
        if(member->var.bitfield.unnamed){
            member_align = 1;
        }
    }
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
    struct parser_vla_block vla_block = { .id = ++parser_vla_block_ids };
    vector_push(parser_vla_blocks, &vla_block);
    // a function's parameters that are pointers to arrays get
    // their strides first.
    if(vector_count(parser_vla_params) && vector_count(parser_vla_blocks) == 1 && parser_current_function){
        parser_vla_begin_parameters();
        parser_take_preceding_statements(body_vec, history, variable_size);
        parser_take_pending_statements(body_vec);
    }

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

    // the block gives back what its variable-length arrays
    // took off the stack.
    struct parser_vla_block* block = vector_back(parser_vla_blocks);
    if(block->save){
        struct node* restore = parser_builtin_call("__builtin_vla_restore", parser_identifier(block->save));
        vector_push(body_vec, &restore);
    }
    vector_pop(parser_vla_blocks);

    expect_sym('}');

    parser_finalize_body(history, body_node, body_vec, variable_size,
                         largest_align_eligible_var_node, largest_possible_var_node);
    parser_current_body = body_node->binded.owner;
    node_push(body_node);
}

static void parse_body(size_t* variable_size, struct history* history);

// The body of a loop (is_loop) or a switch: `break` may leave it, and
// `continue` too when it is a loop.
static void parse_loop_body(size_t* variable_size, struct history* history, bool is_loop){
    parser_breakable_depth++;
    parser_loop_depth += is_loop;
    parser_vla_jump_begin(is_loop);
    parse_body(variable_size, history);
    parser_vla_jump_end();
    parser_loop_depth -= is_loop;
    parser_breakable_depth--;
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
static void parse_struct_or_union_attached_variable(struct datatype* dtype, struct history* history){
    struct node* su_node = node_pop();
    if(token_is_identifier(token_peek_next())){
        struct token* var_name = token_next();
        su_node->flags |= NODE_FLAG_HAS_VARIABLE_COMBINED;
        // An untagged struct keeps its hidden tag. It used to take the
        // variable's name as its tag, so the member `struct { ... } scope;`
        // of compile_process replaced `struct scope` (C has no such
        // rule: the type has no name).
        if(dtype->flags & DATATYPE_FLAG_STRUCT_UNION_NO_NAME){
            dtype->flags       &= ~DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
        }
        // parse_variable reads `[N]` and `= ...` too: `union u {...} g =
        // { 1 };` and `struct s {...} a[2];`. The declaring context's
        // history decides its layout: `struct { int x; } data;` inside
        // a struct is a member, not a stack variable.
        parse_variable(dtype, var_name, history);
        // _struct and _union share their layout.
        su_node->_struct.var = node_pop();
    }

    expect_sym(';');
    node_push(su_node);
}

static void parse_struct(struct datatype* dtype, struct history* history){
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
    parse_struct_or_union_attached_variable(dtype, history);
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

static void parse_union(struct datatype* dtype, struct history* history){
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
    parse_struct_or_union_attached_variable(dtype, history);
}

// ch73: parse a function's argument list (everything between the
// already-consumed `(` and the upcoming `)`).
static struct vector* parse_function_arguments(struct history* history){
    parser_scope_new();
    struct vector* arguments_vec = vector_create(sizeof(struct node*));
    parser_arguments_variadic = false;
    while(!token_next_is_symbol(')')){
        // `...` variadic - consume the three dots and stop.
        if(token_next_is_operator(".")){
            token_read_dots(3);
            parser_arguments_variadic = true;
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
    // A parameter's own list (a function pointer) may have set it.
    parser_arguments_variadic = false;
    parser_scope_finish();
    return arguments_vec;
}

static void parse_struct_or_union(struct datatype* dtype, struct history* history){
    switch(dtype->type){
        case DATA_TYPE_STRUCT:
            parse_struct(dtype, history);
            break;
        case DATA_TYPE_UNION:
            // ch99: real union body now parses.
            parse_union(dtype, history);
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
    parse_struct(dtype, history_begin(0));
}

// A file-scope object declared twice without `extern`, `static bool x;`
// then `static bool x;` again (tentative definitions, C11 6.9.2): one
// object. Each declaration was a definition, a label twice in the asm.
// The one with the initializer, else the first, stays; the other
// becomes a blank statement (the resolver keeps both, by the same name).
// The file-scope definitions so far by name, so 20,000 globals do not
// make 200 million comparisons (tests/352). parse() clears it.
struct parser_global_definition {
    struct compile_process*          process;
    struct node*                     node;
    struct parser_global_definition* next;
};
static struct parser_global_definition* parser_global_definitions[4096];

static void parser_merge_tentative_definition(void){
    struct node* var = node_peek_or_null();
    if(!var || var->type != NODE_TYPE_VARIABLE || current_process->scope.current != current_process->scope.root
       || (var->var.type.flags & DATATYPE_FLAG_IS_EXTERN)){
        return;
    }
    struct parser_global_definition** definitions = parser_global_definitions;
    unsigned hash = 2166136261u;
    for(const unsigned char* c = (const unsigned char*)var->var.name; c && *c; c++){
        hash = (hash ^ *c) * 16777619u;
    }
    hash %= 4096;
    struct parser_global_definition* previous = definitions[hash];
    while(previous && (previous->process != current_process || !S_EQ(previous->node->var.name, var->var.name))){
        previous = previous->next;
    }
    if(!previous){
        struct parser_global_definition* entry = calloc(1, sizeof(struct parser_global_definition));
        entry->process = current_process;
        entry->node    = var;
        entry->next    = definitions[hash];
        definitions[hash] = entry;
        return;
    }
    if(var->var.val && previous->node->var.val){
        compiler_error(current_process, "%s is defined twice\n", var->var.name);
    }
    if(!var->var.val){
        node_pop();
        node_create(&(struct node){ .type = NODE_TYPE_BLANK });
        return;
    }
    // This one has the initializer: the earlier one gives way.
    struct vector* tree = current_process->node_tree_vec;
    for(int i = vector_count(tree) - 1; i >= 0; i--){
        struct node** slot = vector_at(tree, i);
        if(*slot == previous->node){
            node_create(&(struct node){ .type = NODE_TYPE_BLANK });
            *slot = node_pop();
            break;
        }
    }
    previous->node = var;
}

static void parse_variable_function_or_struct_union(struct history* history){
    struct datatype dtype;
    parse_datatype(&dtype);

    // ch64: struct / union body, e.g. `struct abc { ... };`
    // parse_struct_or_union builds and pushes the struct node and
    // consumes the trailing `;`. We pop it, register the symbol, push
    // back so the caller's pop+push pair sees it.
    if(datatype_is_struct_or_union(&dtype) && token_next_is_symbol('{')){
        parse_struct_or_union(&dtype, history);

        struct node* su_node = node_pop();
        symresolver_build_for_node(current_process, su_node);
        if((su_node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED)
           && current_process->scope.current != current_process->scope.root){
            // A local `struct S {...} v;` is the declaration of v; the
            // type is already registered.
            node_push(su_node->_struct.var);
            return;
        }
        if((dtype.flags & DATATYPE_FLAG_STRUCT_UNION_NO_NAME)
           && (history->flags & (HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION))){
            // `union { int a; long b; };` inside a struct or union is an
            // anonymous member (C11 6.7.2.1p13): laid out like a named
            // one under a hidden name, and its members are found through
            // it (struct_member_named).
            static int anonymous_count = 0;
            char* name = malloc(32);
            sprintf(name, "__anonymous_member_%i", anonymous_count++);
            struct token* name_token = calloc(1, sizeof(struct token));
            name_token->type = TOKEN_TYPE_IDENTIFIER;
            name_token->sval = name;
            name_token->pos  = current_process->pos;
            make_variable_node_and_register(history, &dtype, name_token, 0);
            node_peek()->flags |= NODE_FLAG_ANONYMOUS_MEMBER;
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
    parser_function_declarator_params = -1;
    if((history->flags & (HISTORY_FLAG_INSIDE_STRUCTURE | HISTORY_FLAG_INSIDE_UNION)) && token_next_is_symbol(':')){
        // An unnamed bit-field (`int : 3;`) takes room under a hidden
        // name nothing can refer to.
        static int unnamed_count = 0;
        char* name = malloc(32);
        sprintf(name, "__unnamed_bitfield_%i", unnamed_count++);
        name_token = calloc(1, sizeof(struct token));
        name_token->type = TOKEN_TYPE_IDENTIFIER;
        name_token->sval = name;
        name_token->pos  = current_process->pos;
        parser_bitfield_unnamed = true;
    } else if(!parse_function_pointer_declarator(&dtype, &name_token)){
        name_token = token_next();
    }
    if(!name_token || name_token->type != TOKEN_TYPE_IDENTIFIER){
        compiler_error(current_process,
            "Expecting a valid name for the given variable declaration\n");
    }

    // A function returning a function pointer: its parameters were
    // inside the declarator.
    if(parser_function_declarator_params >= 0){
        parser_function_resume = current_process->token_vec->pindex;
        vector_set_peek_pointer(current_process->token_vec, parser_function_declarator_params);
        parser_function_declarator_params = -1;
        parse_function(&dtype, name_token, history);
        return;
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
    base_dtype.pointer_const = 0;
    struct datatype first_dtype = dtype;
    parse_variable(&first_dtype, name_token, history);
    parser_merge_tentative_definition();

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
            int depth = parser_get_pointer_levels(&peer_dtype.pointer_const);
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
        parse_if_stmt(history_down(history, history->flags));
        return node_pop();
    }
    return parse_else(history_down(history, history->flags));
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
    parse_loop_body(&var_size, history, true);
    struct node* body_node = node_pop();
    make_while_node(exp_node, body_node);
}

// ch84: `do body while (exp);`.
static void parse_do_while(struct history* history){
    expect_keyword("do");
    size_t var_size = 0;
    parse_loop_body(&var_size, history, true);
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
    parse_loop_body(&var_size, history, false);
    struct node* body_node = node_pop();
    make_switch_node(exp_node, body_node, sw.case_data->cases, sw.case_data->has_default_case);
    parser_end_switch_statement(&sw);
}

// ch86: `break;` / `continue;`.
static void parse_break(struct history* history){
    (void)history;
    expect_keyword("break");
    if(!parser_breakable_depth){
        compiler_error(current_process, "'break' is only valid inside a loop or switch\n");
    }
    expect_sym(';');
    parser_vla_leave(false);
    make_break_node();
}

static void parse_continue(struct history* history){
    (void)history;
    expect_keyword("continue");
    if(!parser_loop_depth){
        compiler_error(current_process, "'continue' is only valid inside a loop\n");
    }
    expect_sym(';');
    parser_vla_leave(true);
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
    // a goto out of a block with a variable-length array (or
    // back over one) gives the stack back.
    if(parser_current_function){
        parser_vla_note_goto(name);
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
    parse_loop_body(&var_size, history, true);
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
    // The variable whose initializer this is: not registered yet, but
    // its name is in scope (parse_variable).
    if(node->type == NODE_TYPE_IDENTIFIER && parser_declaring_name && S_EQ(node->sval, parser_declaring_name)){
        *out = parser_declaring_type;
        return true;
    }
    // The name an access chain starts from must exist: the resolver
    // follows `zz[0]` into a NULL entity and segfaulted.
    struct node* root = node;
    while(root && (root->type == NODE_TYPE_EXPRESSION || root->type == NODE_TYPE_EXPRESSION_PARENTHESES)){
        root = root->type == NODE_TYPE_EXPRESSION ? root->exp.left : root->parenthesis.exp;
    }
    if(root && root->type == NODE_TYPE_IDENTIFIER
       && !resolver_get_variable(0, current_process->resolver, root->sval)
       && !symresolver_get_symbol(current_process, root->sval)){
        compiler_error(current_process, "'%s' is not declared\n", root->sval);
    }
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
// A call in the member and subscript chain `node` starts with.
static bool parser_has_call(struct node* node){
    while(node){
        if(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            node = node->parenthesis.exp;
        } else if(node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, "()")){
            return true;
        } else if(node->type == NODE_TYPE_EXPRESSION && (is_access_operator(node->exp.op) || S_EQ(node->exp.op, "[]"))){
            node = node->exp.left;
        } else {
            return false;
        }
    }
    return false;
}

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
            // A call of a function by name: its return type. The resolver
            // does not know the functions while the parser runs.
            if(S_EQ(op, "()") && parser_call_type(node, out)){
                return true;
            }
            // A member of a call's result, `node_peek()->flags`: the
            // resolver cannot follow the call, so the member is looked up
            // in the type parser_call_type gives.
            if(is_access_operator(op) && node->exp.right->type == NODE_TYPE_IDENTIFIER && parser_has_call(node->exp.left)){
                struct datatype base;
                if(!parser_expression_type(node->exp.left, &base)){
                    return false;
                }
                if(S_EQ(op, "->")){
                    base = *datatype_pointer_reduce(&base, 1);
                }
                struct node* su = datatype_is_struct_or_union(&base) ? base.struct_node : 0;
                struct node* member = su && su->_struct.body_n
                    ? struct_member_named(su->_struct.body_n->body.statements, node->exp.right->sval) : 0;
                if(!member){
                    return false;
                }
                *out = member->var.type;
                return true;
            }
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
    struct node* operand = 0;
    if(token_next_is_operator("(")){
        expect_op("(");
        after_paren = token_peek_next();
    }
    if(after_paren && after_paren->type == TOKEN_TYPE_KEYWORD
       && (keyword_is_datatype(after_paren->sval) || is_keyword_variable_modifier(after_paren->sval))){
        parser_sizeof_type_brackets = true;
        parse_datatype(&dtype);
        parser_sizeof_type_brackets = false;
        // `sizeof(int[4])`, and in a function `sizeof(int[n][m])`
        //, worked out when it runs.
        if(token_next_is_operator("[")){
            parser_vla_mode = parser_current_function ? PARSER_VLA_MODE_LOCAL : PARSER_VLA_MODE_NONE;
            struct array_brackets* brackets = parse_array_brackets(history_begin(0));
            parser_vla_mode = PARSER_VLA_MODE_NONE;
            if(parser_vla_dims){
                struct vector* dims = parser_vla_dims;
                parser_vla_dims = 0;
                struct node* bytes = parser_vla_number_node(datatype_size(&dtype));
                for(int i = 0; i < vector_count(dims); i++){
                    struct node* dim = *(struct node**)vector_at(dims, i);
                    if(!dim){
                        compiler_error(current_process, "sizeof of an array type needs its size\n");
                    }
                    bytes = parser_vla_binary(bytes, parser_vla_as_size(dim), "*");
                }
                expect_sym(')');
                node_push(parser_vla_parens(bytes));
                return;
            }
            dtype.array.brackets = brackets;
            dtype.array.size     = array_brackets_calculate_size(&dtype, brackets);
            dtype.flags         |= DATATYPE_FLAG_IS_ARRAY;
        }
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
        operand = node_pop();
        struct node* inner = operand;
        while(inner->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            inner = inner->parenthesis.exp;
        }
        // The size of a variable-length array is known when it runs.
        const char* vla_size = inner->type == NODE_TYPE_IDENTIFIER ? parser_vla_size_of(inner->sval) : 0;
        if(vla_size){
            node_push(parser_identifier(vla_size));
            return;
        }
        // So is a row of one, `sizeof a[i]`, `sizeof *p`.
        if(parser_vla_active()){
            operand = parser_vla_lower(operand);
            struct parser_vla_view view;
            if(parser_vla_view_of(operand, &view) && view.level > 0){
                node_push(parser_vla_parens(parser_vla_view_size(&view)));
                return;
            }
        }
        if(!parser_expression_type(operand, &dtype)){
            compiler_error(current_process, "Cannot work out the size of this sizeof operand\n");
        }
    }
    // sizeof yields a size_t, which is unsigned long on every target.
    node_create(&(struct node){ .type = NODE_TYPE_NUMBER, .llnum = datatype_size(&dtype),
        .num.flags = NUMBER_FLAG_UNSIGNED | NUMBER_FLAG_LONG, .num.sizeof_operand = operand });
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
            // `return 50 else + 1`: a statement keyword inside an
            // expression built nothing and consumed nothing, and this
            // loop never ended.
            if(parser_is_statement_keyword(token->sval)){
                compiler_error(current_process, "Expected an expression but found '%s'\n", token->sval);
            }
            parse_keyword(history);
            if(token_peek_next() == token){
                compiler_error(current_process, "The keyword '%s' is not supported here\n", token->sval);
            }
            res = 0;
            break;
    }
    return res;
}

// Keywords that begin a statement, never an expression or a declaration.
static bool parser_is_statement_keyword(const char* keyword){
    return S_EQ(keyword, "return") || S_EQ(keyword, "if")       || S_EQ(keyword, "else")
        || S_EQ(keyword, "while")  || S_EQ(keyword, "for")      || S_EQ(keyword, "do")
        || S_EQ(keyword, "break")  || S_EQ(keyword, "continue") || S_EQ(keyword, "switch")
        || S_EQ(keyword, "case")   || S_EQ(keyword, "default")  || S_EQ(keyword, "goto");
}

static bool parser_is_operand_token(struct token* token){
    return token && (token->type == TOKEN_TYPE_IDENTIFIER || token->type == TOKEN_TYPE_NUMBER
        || token->type == TOKEN_TYPE_STRING);
}

static void parse_expressionable(struct history* history){
    compiler_check_stack(current_process);
    while(parse_expressionable_single(history) == 0){
        // Two operands in a row (`foo x;` with foo an unknown type name,
        // `a b`) are not an expression. Both were pushed, the statement
        // took one and the other was left on the node stack, where it
        // replaced the function being parsed: main vanished (A13).
        struct token* next = token_peek_next();
        if(parser_is_operand_token(parser_last_token) && parser_is_operand_token(next)){
            char last[64];
            snprintf(last, sizeof(last), "%s", parser_token_text(parser_last_token));
            compiler_error(current_process, "Expected an operator or ';' after %s but found %s%s\n",
                last, parser_token_text(next),
                parser_last_token->type == TOKEN_TYPE_IDENTIFIER && next->type == TOKEN_TYPE_IDENTIFIER
                    ? " (is the first an unknown type name?)" : "");
        }
    }
}

// Entry from parse_next for top-level keyword declarations. As of
// ch42 variables push a real node. ch47 added bare `struct foo {};`
// which pushes nothing, so guard the pop/push.
static void parse_keyword_for_global(void){
    // Only declarations live at file scope. `return` there reached
    // parse_for_stmt with no function and segfaulted; `else` was never
    // consumed and the parser looped forever.
    struct token* keyword = token_peek_next();
    if(parser_is_statement_keyword(keyword->sval) || S_EQ(keyword->sval, "sizeof")){
        compiler_error(current_process, "Expected a declaration at file scope but found '%s'\n", keyword->sval);
    }
    // ch149: top-level parse runs under HISTORY_FLAG_IS_GLOBAL_SCOPE
    // and we register variable / function / struct / union names with
    // the symbol resolver so codegen can look them up.
    parse_keyword(history_begin(HISTORY_FLAG_IS_GLOBAL_SCOPE));
    if(token_peek_next() == keyword){
        compiler_error(current_process, "The keyword '%s' is not supported here\n", keyword->sval);
    }
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
    memset(parser_global_definitions, 0, sizeof(parser_global_definitions));
    parser_bitfield_width = -1;
    parser_bitfield_names = vector_create(sizeof(const char*));
    parser_vla_blocks = vector_create(sizeof(struct parser_vla_block));
    parser_vla_jumps  = vector_create(sizeof(struct parser_vla_jump));
    parser_vlas       = vector_create(sizeof(struct parser_vla));
    parser_vla_mode         = PARSER_VLA_MODE_NONE;
    parser_vla_dims         = 0;
    parser_vla_pointer_dims = 0;
    parser_vla_params = vector_create(sizeof(struct parser_vla_param));
    parser_vla_views  = vector_create(sizeof(struct parser_vla_view));
    parser_vla_done   = vector_create(sizeof(struct node*));
    parser_vla_labels = vector_create(sizeof(struct parser_vla_label));
    parser_vla_gotos  = vector_create(sizeof(struct parser_vla_goto));
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
    // gcc's __atomic and __sync builtins need no declaration.
    atomic_builtins_register(process);
    // gcc's memory order constants, __ATOMIC_RELAXED (0) to
    // __ATOMIC_SEQ_CST (5), for the builtins and <stdatomic.h>: file
    // scope enumerators, so the preprocessor's own definitions (which
    // its chapter tests count) stay as they were.
    static const char* orders[] = { "__ATOMIC_RELAXED", "__ATOMIC_CONSUME", "__ATOMIC_ACQUIRE",
                                    "__ATOMIC_RELEASE", "__ATOMIC_ACQ_REL", "__ATOMIC_SEQ_CST", 0 };
    for(int i = 0; orders[i]; i++){
        struct constant order = { i, datatype_for_int() };
        parser_enumerator_push(orders[i], &order, false);
    }
    parser_thread_locals = vector_create(sizeof(struct parser_thread_local));

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
