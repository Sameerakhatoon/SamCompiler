#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

// Owned by node.c, threaded through statements so children know
// which body they belong to.
extern struct node* parser_current_body;

// ch55: history flags carried through nested parses.
enum {
    HISTORY_FLAG_INSIDE_UNION    = 0b00000001,
    HISTORY_FLAG_IS_UPWARD_STACK = 0b00000010,
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
    int flags;
};

static struct history* history_begin(int flags);
static struct history* history_down(struct history* history, int flags);

static struct compile_process* current_process;
static struct token*           parser_last_token;

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
static void          parse_identifier(struct history* history);
static bool          is_keyword_variable_modifier(const char* val);
static void          parse_datatype_modifiers(struct datatype* dtype);
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
static int           parse_expressionable_single(struct history* history);
static void          parse_expressionable(struct history* history);
static int           parse_next(void);

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

static void parser_ignore_nl_or_comment(struct token* token){
    while(token && token_is_nl_or_comment_or_newline_seperator(token)){
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token* token_next(void){
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    if(next_token){
        current_process->pos = next_token->pos;
    }
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token* token_peek_next(void){
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

static void parse_single_token_to_node(void){
    struct token* token = token_next();
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
            node_create(&(struct node){ .type = NODE_TYPE_NUMBER,     .llnum = token->llnum });
            break;
        case TOKEN_TYPE_IDENTIFIER:
            node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval  = token->sval  });
            break;
        case TOKEN_TYPE_STRING:
            node_create(&(struct node){ .type = NODE_TYPE_STRING,     .sval  = token->sval  });
            break;
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
// the right-hand operator?" Used to decide whether (a OPl b) OPr c
// should rewrite to a OPl (b OPr c). Right-associative ops never claim
// priority over themselves.
static bool parser_left_op_has_priority(const char* op_left, const char* op_right){
    struct expressionable_op_precedence_group* group_left  = 0;
    struct expressionable_op_precedence_group* group_right = 0;

    if(S_EQ(op_left, op_right)){
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

// Recursive reorder: walks the freshly-built expression subtree and
// flips ordering where the parent op deserves higher precedence.
static void parser_reorder_expression(struct node** node_out){
    struct node* node = *node_out;
    if(node->type != NODE_TYPE_EXPRESSION){
        return;
    }
    if(node->exp.left->type != NODE_TYPE_EXPRESSION
       && node->exp.right
       && node->exp.right->type != NODE_TYPE_EXPRESSION){
        return;
    }

    // The interesting case: left is a leaf, right is an expression.
    // e.g. `50 * (20 + 120)` parsed top-down. If our op has higher
    // precedence than the right's op, shift left.
    if(node->exp.left->type != NODE_TYPE_EXPRESSION
       && node->exp.right
       && node->exp.right->type == NODE_TYPE_EXPRESSION){
        const char* right_op = node->exp.right->exp.op;
        if(parser_left_op_has_priority(node->exp.op, right_op)){
            parser_node_shift_children_left(node);
            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }
}

// Binary expression: pop the left operand off the node stack, consume
// the operator, parse the right operand, then assemble a NODE_TYPE_EXPRESSION.
// ch29 will fold precedence-aware reordering into the trailing comment.
static void parse_exp_normal(struct history* history){
    struct token* op_token = token_peek_next();
    const char*   op       = op_token->sval;
    struct node*  node_left = node_peek_expressionable_or_null();
    if(!node_left){
        return;
    }

    token_next();          // consume operator

    node_pop();            // detach the left operand
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    parse_expressionable_for_op(history_down(history, history->flags), op);
    struct node* node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    make_exp_node(node_left, node_right, op);
    struct node* exp_node = node_pop();

    // ch30: reorder for operator precedence.
    parser_reorder_expression(&exp_node);

    node_push(exp_node);
}

static int parse_exp(struct history* history){
    parse_exp_normal(history);
    return 0;
}

// g01: assert compares a token's type with TOKEN_TYPE_IDENTIFIER,
// not NODE_TYPE_IDENTIFIER. The book ships this wrong; see
// docs/gotchas/G01-token-type-vs-node-type.md.
static void parse_identifier(struct history* history){
    assert(token_peek_next()->type == TOKEN_TYPE_IDENTIFIER);
    parse_single_token_to_node();
}

// Keywords that prefix or modify a datatype (signed/unsigned, storage
// class, const, etc.) but don't name a type themselves.
static bool is_keyword_variable_modifier(const char* val){
    return S_EQ(val, "unsigned") || S_EQ(val, "signed")
        || S_EQ(val, "static")   || S_EQ(val, "const")
        || S_EQ(val, "extern")   || S_EQ(val, "__ignore_typecheck__");
}

// Consume a run of modifier keywords, OR-ing matching DATATYPE_FLAG_*s
// into dtype. Stops at the first non-modifier keyword.
static void parse_datatype_modifiers(struct datatype* dtype){
    struct token* token = token_peek_next();
    while(token && token->type == TOKEN_TYPE_KEYWORD){
        if(!is_keyword_variable_modifier(token->sval)){
            break;
        }

        if(S_EQ(token->sval, "signed")){
            dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
        } else if(S_EQ(token->sval, "unsigned")){
            dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
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
}

static bool token_next_is_operator(const char* op){
    struct token* token = token_peek_next();
    return token_is_operator(token, op);
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
}

// Convenience: push an entity-bearing pointer into the current scope.
static void parser_scope_push(struct node* node, size_t size){
    scope_push(current_process, node, size);
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

// Consume the datatype keyword + (optional) secondary primitive (the
// "int" in "long int", say) and hand both back to the caller.
static void parser_get_datatype_tokens(struct token** datatype_token,
                                       struct token** datatype_secondary_token){
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

// "long int" -> primary=long (4 bytes), secondary=int (4 bytes). The
// total size is primary + secondary; the secondary datatype is held
// hanging off the primary's `.secondary` field.
static void parser_datatype_adjust_size_for_secondary(struct datatype* datatype,
                                                      struct token* datatype_secondary_token){
    if(!datatype_secondary_token){
        return;
    }
    struct datatype* secondary = calloc(1, sizeof(struct datatype));
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, 0, secondary);
    datatype->size      += secondary->size;
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
    } else if(S_EQ(datatype_token->sval, "short")){
        datatype_out->type = DATA_TYPE_SHORT;
        datatype_out->size = DATA_SIZE_WORD;
    } else if(S_EQ(datatype_token->sval, "int")){
        datatype_out->type = DATA_TYPE_INTEGER;
        datatype_out->size = DATA_SIZE_DWORD;
    } else if(S_EQ(datatype_token->sval, "long")){
        datatype_out->type = DATA_TYPE_LONG;
        datatype_out->size = DATA_SIZE_DWORD;
    } else if(S_EQ(datatype_token->sval, "float")){
        datatype_out->type = DATA_TYPE_FLOAT;
        datatype_out->size = DATA_SIZE_DWORD;
    } else if(S_EQ(datatype_token->sval, "double")){
        // g02: write the type to .type, not .size.
        datatype_out->type = DATA_TYPE_DOUBLE;
        datatype_out->size = DATA_SIZE_DWORD;
    } else {
        compiler_error(current_process, "BUG: Invalid primitive datatype\n");
    }

    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
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
            // ch47: real struct sizing comes later; for now stamp type
            // + zero size and let parse_struct fill in the rest.
            datatype_out->type = DATA_TYPE_STRUCT;
            datatype_out->size = 0;
            break;
        case DATA_TYPE_EXPECT_UNION:
            datatype_out->type = DATA_TYPE_UNION;
            datatype_out->size = 0;
            break;
        default:
            compiler_error(current_process, "BUG: Unsupported datatype expectation\n");
    }
}

static void parser_datatype_init(struct token* datatype_token,
                                 struct token* datatype_secondary_token,
                                 struct datatype* datatype_out,
                                 int pointer_depth, int expected_type){
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token,
                                       datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;

    // 64-bit `long long` not supported; warn and clamp to 32-bit.
    if(S_EQ(datatype_token->sval, "long")
       && datatype_secondary_token
       && S_EQ(datatype_secondary_token->sval, "long")){
        compiler_warning(current_process,
            "Our compiler does not support 64 bit longs, therefore your long long is defaulting to 32 bits\n");
        datatype_out->size = DATA_SIZE_DWORD;
    }
}

static void parse_datatype_type(struct datatype* dtype){
    struct token* datatype_token            = 0;
    struct token* datatype_secondary_token  = 0;
    parser_get_datatype_tokens(&datatype_token, &datatype_secondary_token);
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

    parser_datatype_init(datatype_token, datatype_secondary_token, dtype, pointer_depth, expected_type);
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token, dtype, pointer_depth, expected_type);
}

// modifier* type modifier*
static void parse_datatype(struct datatype* dtype){
    memset(dtype, 0, sizeof(struct datatype));
    dtype->flags |= DATATYPE_FLAG_IS_SIGNED;

    parse_datatype_modifiers(dtype);
    parse_datatype_type(dtype);
    parse_datatype_modifiers(dtype);
}

// "long int" / "float int" / "double int" are tolerated abbreviations
// where `int` is purely decorative. Anything else with a trailing
// `int` is a hard error.
static bool parser_is_int_valid_after_datatype(struct datatype* dtype){
    return dtype->type == DATA_TYPE_LONG
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
static void parse_expressionable_root(struct history* history){
    parse_expressionable(history);
    struct node* result_node = node_pop();
    node_push(result_node);
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
}

// ch58: compute the variable's stack offset based on the last
// declared entity in the current (non-global) scope. Local stacks
// grow downward (offset goes negative); upward-stack call frames
// reverse the sign.
static void parser_scope_offset_for_stack(struct node* node, struct history* history){
    struct parser_scope_entity* last_entity = parser_scope_last_entity_stop_global_scope();
    bool upward_stack = history->flags & HISTORY_FLAG_IS_UPWARD_STACK;
    int offset = -(int)variable_size(node);
    if(upward_stack){
        // TODO(post-ch58): real upward-stack offsets land later.
        compiler_error(current_process, "Upward-stack offsets are not yet implemented\n");
    }

    if(last_entity){
        offset += variable_node(last_entity->node)->var.aoffset;
        if(variable_node_is_primitive(node)){
            variable_node(node)->var.padding =
                padding(upward_stack ? offset : -offset, node->var.type.size);
        }
    }
    variable_node(node)->var.aoffset = offset;
}

static void parser_scope_offset(struct node* node, struct history* history){
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

    // Only assign a stack offset if we're inside some non-global
    // scope (a function body / struct body). Global vars get offset 0
    // and live in the data segment later.
    if(current_process->scope.current != current_process->scope.root){
        parser_scope_offset(var_node, history);
    }

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
static struct array_brackets* parse_array_brackets(struct history* history){
    struct array_brackets* brackets = array_brackets_new();
    while(token_next_is_operator("[")){
        expect_op("[");
        if(token_is_symbol(token_peek_next(), ']')){
            // `int x[];` - empty brackets, no size.
            expect_sym(']');
            break;
        }
        parse_expressionable_root(history);
        expect_sym(']');

        struct node* exp_node = node_pop();
        make_bracket_node(exp_node);

        struct node* bracket_node = node_pop();
        array_brackets_add(brackets, bracket_node);
    }
    return brackets;
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

    if(token_next_is_operator("=")){
        token_next();
        parse_expressionable_root(history);
        value_node = node_pop();
    }

    make_variable_node_and_register(history, dtype, name_token, value_node);
}

static void parse_symbol(void){
    compiler_error(current_process, "Symbols are not yet supported\n");
}

// One statement: keyword-led (declarations / control-flow) or a bare
// expression-statement terminated by `;`.
static void parse_statement(struct history* history){
    if(token_peek_next()->type == TOKEN_TYPE_KEYWORD){
        parse_keyword(history);
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

// ch55: real body finalization. Unions get sized to their largest
// member; otherwise we sum padding, then realign to the largest
// align-eligible variable's natural size.
static void parser_finalize_body(struct history* history, struct node* body_node,
                                 struct vector* body_vec, size_t* _variable_size,
                                 struct node* largest_align_eligible_var_node,
                                 struct node* largest_possible_var_node){
    if(history->flags & HISTORY_FLAG_INSIDE_UNION){
        if(largest_possible_var_node){
            *_variable_size = variable_size(largest_possible_var_node);
        }
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

// ch49: single-statement body (no braces), e.g. `y = 30;` in
// `if (x) y = 30;`. Brace bodies come in ch54.
static void parse_body_single_statement(size_t* variable_size, struct vector* body_vec, struct history* history){
    make_body_node(0, 0, false, 0);
    struct node* body_node = node_pop();
    body_node->binded.owner = parser_current_body;
    parser_current_body     = body_node;

    parse_statement(history_down(history, history->flags));
    struct node* stmt_node = node_pop();
    vector_push(body_vec, &stmt_node);

    parser_append_size_for_node(history, variable_size, stmt_node);
    struct node* largest_var_node = (stmt_node->type == NODE_TYPE_VARIABLE) ? stmt_node : 0;
    parser_finalize_body(history, body_node, body_vec, variable_size,
                         largest_var_node, largest_var_node);
    parser_current_body = body_node->binded.owner;
    node_push(body_node);
}

// Body entry. Brace-delimited bodies are stubbed - ch54 fills them in.
static void parse_body(size_t* variable_size, struct history* history){
    parser_scope_new();
    size_t tmp_size = 0;
    if(!variable_size){
        variable_size = &tmp_size;
    }
    struct vector* body_vec = vector_create(sizeof(struct node*));
    if(!token_next_is_symbol('{')){
        parse_body_single_statement(variable_size, body_vec, history);
        parser_scope_finish();
        return;
    }
    // TODO(ch54): walk a brace-delimited body.
    parser_scope_finish();
}

// ch47: minimally walk `{ ... }` so the parser doesn't get stuck on
// the brace. Real body parsing arrives in ch48+.
static void parse_struct_no_new_scope(struct datatype* dtype){
    (void)dtype;
    if(!token_is_symbol(token_peek_next(), '{')){
        return;
    }
    expect_sym('{');
    // Skip every token until the matching '}'. Nested braces aren't
    // expected in the ch47 input ("struct abc {};") but are tolerated
    // here so we don't pop into the global parse loop with garbage.
    int depth = 1;
    while(depth > 0){
        struct token* t = token_next();
        if(!t){
            compiler_error(current_process, "Unterminated struct body\n");
        }
        if(t->type == TOKEN_TYPE_SYMBOL && t->cval == '{') depth++;
        if(t->type == TOKEN_TYPE_SYMBOL && t->cval == '}') depth--;
    }
}

static void parse_struct(struct datatype* dtype){
    // `struct foo;` (no `{`) is a forward declaration - skip scope.
    bool is_forward_declaration = !token_is_symbol(token_peek_next(), '{');
    if(!is_forward_declaration){
        parser_scope_new();
    }
    parse_struct_no_new_scope(dtype);
    if(!is_forward_declaration){
        parser_scope_finish();
    }
}

static void parse_struct_or_union(struct datatype* dtype){
    switch(dtype->type){
        case DATA_TYPE_STRUCT:
            parse_struct(dtype);
            break;
        case DATA_TYPE_UNION:
            // ch48+ adds union parsing; nothing here for now.
            break;
        default:
            compiler_error(current_process,
                "COMPILER BUG: The provided datatype is not a structure or union\n");
    }
}

// ch33 entry. ch34+ does the variable / function / struct dispatch
// off the parsed datatype.
static void parse_variable_function_or_struct_union(struct history* history){
    struct datatype dtype;
    parse_datatype(&dtype);

    // ch47: struct / union body, e.g. `struct abc { ... };`
    if(datatype_is_struct_or_union(&dtype) && token_next_is_symbol('{')){
        parse_struct_or_union(&dtype);
        // Bare `struct abc { ... };` with no declarator after - swallow
        // the semicolon and bail; nothing to emit here yet (ch48+
        // builds the real struct node).
        if(token_next_is_symbol(';')){
            expect_sym(';');
            return;
        }
    }

    // ch41: swallow the decorative "int" in "long int" / "float int" /
    // "double int". The book keeps the real type as long/float/double
    // and silently drops the int.
    parser_ignore_int(&dtype);

    // ch42: `int abc;` - the next token must be the variable name.
    struct token* name_token = token_next();
    if(name_token->type != TOKEN_TYPE_IDENTIFIER){
        compiler_error(current_process,
            "Expecting a valid name for the given variable declaration\n");
    }

    // TODO(ch46+): if the next token is `(`, this is a function decl,
    // not a variable. ch42 only handles the variable path.

    parse_variable(&dtype, name_token, history);

    // ch43: `int a, b, c;` - gather any comma-separated peers into a
    // NODE_TYPE_VARIABLE_LIST.
    if(token_is_operator(token_peek_next(), ",")){
        struct vector* var_list = vector_create(sizeof(struct node*));
        struct node* var_node   = node_pop();
        vector_push(var_list, &var_node);
        while(token_is_operator(token_peek_next(), ",")){
            token_next();
            name_token = token_next();
            parse_variable(&dtype, name_token, history);
            var_node = node_pop();
            vector_push(var_list, &var_node);
        }
        make_variable_list_node(var_list);
    }

    expect_sym(';');
}

static void parse_keyword(struct history* history){
    struct token* token = token_peek_next();
    if(is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval)){
        parse_variable_function_or_struct_union(history);
        return;
    }
}

static int parse_expressionable_single(struct history* history){
    struct token* token = token_peek_next();
    if(!token){
        return -1;
    }

    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
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
            parse_exp(history);
            res = 0;
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
    parse_keyword(history_begin(0));
    if(!vector_empty(current_process->node_vec)){
        struct node* node = node_pop();
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

        default:
            return -1;
    }
    return res;
}

int parse(struct compile_process* process){
    current_process   = process;
    parser_last_token = 0;
    scope_create_root(process);
    node_set_vector(process->node_vec, process->node_tree_vec);

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
    return PARSE_ALL_OK;
}
