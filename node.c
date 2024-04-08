#include <assert.h>
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
    struct node* last_node_root = vector_empty(node_vector) ? 0 : vector_back_ptr(node_vector_root);

    vector_pop(node_vector);

    if(last_node == last_node_root){
        vector_pop(node_vector_root);
    }
    return last_node;
}
