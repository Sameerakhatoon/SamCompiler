#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

static void node_set_vector_internal(struct vector* vec, struct vector* root_vec);

// Module-private: the two vectors the node helpers operate on. Set once
// per parse() pass via node_set_vector. Owned by compile_process.
static struct vector* node_vector      = 0;
static struct vector* node_vector_root = 0;

void node_set_vector(struct vector* vec, struct vector* root_vec){
    node_set_vector_internal(vec, root_vec);
}

static void node_set_vector_internal(struct vector* vec, struct vector* root_vec){
    node_vector      = vec;
    node_vector_root = root_vec;
}

void node_push(struct node* node){
    vector_push(node_vector, &node);
}

struct node* node_peek_or_null(void){
    return vector_back_ptr_or_null(node_vector);
}

struct node* node_peek(void){
    return *(struct node**)(vector_back(node_vector));
}

// Pop from the scratch vector. If the popped node also happens to sit
// at the top of node_vector_root (the AST-roots vector), pop it from
// there too - keeps the two vectors consistent when the parser walks
// back over a finished top-level node.
struct node* node_pop(void){
    struct node* last_node      = vector_back_ptr(node_vector);
    struct node* last_node_root = vector_empty(node_vector_root) ? 0 : vector_back_ptr_or_null(node_vector_root);

    vector_pop(node_vector);

    if(last_node == last_node_root){
        vector_pop(node_vector_root);
    }
    return last_node;
}

bool node_is_expressionable(struct node* node){
    return node
        && (node->type == NODE_TYPE_EXPRESSION
         || node->type == NODE_TYPE_EXPRESSION_PARENTHESES
         || node->type == NODE_TYPE_UNARY
         || node->type == NODE_TYPE_IDENTIFIER
         || node->type == NODE_TYPE_NUMBER
         || node->type == NODE_TYPE_STRING);
}

struct node* node_peek_expressionable_or_null(void){
    struct node* last_node = node_peek_or_null();
    return node_is_expressionable(last_node) ? last_node : 0;
}

void make_exp_node(struct node* left_node, struct node* right_node, const char* op){
    assert(left_node);
    assert(right_node);
    node_create(&(struct node){
        .type      = NODE_TYPE_EXPRESSION,
        .exp.left  = left_node,
        .exp.right = right_node,
        .exp.op    = op,
    });
}

void make_bracket_node(struct node* inner){
    node_create(&(struct node){
        .type          = NODE_TYPE_BRACKET,
        .bracket.inner = inner,
    });
}

// ch49: the currently-being-parsed body node. The parser writes to it
// while consuming statements, and binded.owner restores the parent on
// exit. Exported (not static) because parser.c walks it directly.
struct node* parser_current_body = 0;

void make_body_node(struct vector* body_vec, size_t size, bool padded, struct node* largest_var_node){
    node_create(&(struct node){
        .type                  = NODE_TYPE_BODY,
        .body.statements       = body_vec,
        .body.size             = size,
        .body.padded           = padded,
        .body.largest_var_node = largest_var_node,
    });
}

// Copy the caller's stack-allocated node onto the heap, push onto the
// scratch stack, and return the heap pointer. TODO: set binded.owner
// and binded.function when the parser starts threading the AST.
struct node* node_create(struct node* _node){
    struct node* node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    node_push(node);
    return node;
}
