#include <stdlib.h>
#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

struct array_brackets* array_brackets_new(void){
    struct array_brackets* brackets = calloc(1, sizeof(struct array_brackets));
    brackets->n_brackets = vector_create(sizeof(struct node*));
    return brackets;
}

void array_brackets_free(struct array_brackets* brackets){
    free(brackets);
}

void array_brackets_add(struct array_brackets* brackets, struct node* bracket_node){
    assert(bracket_node->type == NODE_TYPE_BRACKET);
    vector_push(brackets->n_brackets, &bracket_node);
}

struct vector* array_brackets_node_vector(struct array_brackets* brackets){
    return brackets->n_brackets;
}

// TODO(later chapters): real array sizing. For now `int x[4][3]`
// returns size 0; the parser still records the bracket nodes.
size_t array_brackets_calculate_size_from_index(struct datatype* dtype,
                                                struct array_brackets* brackets,
                                                int index){
    (void)dtype; (void)brackets; (void)index;
    return 0;
}

size_t array_brackets_calculate_size(struct datatype* dtype, struct array_brackets* brackets){
    return array_brackets_calculate_size_from_index(dtype, brackets, 0);
}

int array_total_indexes(struct datatype* dtype){
    assert(dtype->flags & DATATYPE_FLAG_IS_ARRAY);
    return vector_count(dtype->array.brackets->n_brackets);
}
