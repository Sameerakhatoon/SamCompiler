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

// Note: the book's assert here compares a TOKEN_TYPE_* with
// NODE_TYPE_IDENTIFIER, which is wrong type-wise. Shipped verbatim
// per the "follow the book" rule; the fix lands in g01.
static void parse_identifier(struct history* history){
    assert(token_peek_next()->type == NODE_TYPE_IDENTIFIER);
    parse_single_token_to_node();
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
