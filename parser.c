#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

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
        // Upstream typo preserved: writes DATA_TYPE_DOUBLE into .size
        // instead of .type. Caught in g02 (next commit).
        datatype_out->size = DATA_TYPE_DOUBLE;
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
        case DATA_TYPE_EXPECT_UNION:
            compiler_error(current_process, "Structure and union types are currently unsupported\n");
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

// ch33 entry. ch34+ does the variable / function / struct dispatch
// off the parsed datatype.
static void parse_variable_function_or_struct_union(struct history* history){
    struct datatype dtype;
    parse_datatype(&dtype);
    // ch34+ will look at the next token to decide variable vs function
    // vs struct-body and build the appropriate node. For now the
    // datatype is parsed and discarded.
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
        default:
            return -1;
    }
    return res;
}

int parse(struct compile_process* process){
    current_process   = process;
    parser_last_token = 0;
    node_set_vector(process->node_vec, process->node_tree_vec);

    struct node* node = 0;
    vector_set_peek_pointer(process->token_vec, 0);
    while(parse_next() == 0){
        node = node_peek();
        vector_push(process->node_tree_vec, &node);
    }
    return PARSE_ALL_OK;
}
