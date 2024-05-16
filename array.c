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

// Bytes occupied by the array starting from `index`. Multiplies the
// element size by each remaining bracket dimension. e.g. for
// `int x[4][3]` with dtype->size==4, starting at index 0 returns
// 4 * 4 * 3 = 48.
size_t array_brackets_calculate_size_from_index(struct datatype* dtype,
                                                struct array_brackets* brackets,
                                                int index){
    struct vector* array_vec = array_brackets_node_vector(brackets);
    size_t size = dtype->size;
    if(index >= vector_count(array_vec)){
        // Past the last bracket; we're sizing a single element.
        return size;
    }
    vector_set_peek_pointer(array_vec, index);
    struct node* bn = vector_peek_ptr(array_vec);
    if(!bn){
        return 0;
    }
    while(bn){
        assert(bn->bracket.inner->type == NODE_TYPE_NUMBER);
        int number = bn->bracket.inner->llnum;
        size *= number;
        bn = vector_peek_ptr(array_vec);
    }
    return size;
}

size_t array_brackets_calculate_size(struct datatype* dtype, struct array_brackets* brackets){
    return array_brackets_calculate_size_from_index(dtype, brackets, 0);
}

int array_total_indexes(struct datatype* dtype){
    assert(dtype->flags & DATATYPE_FLAG_IS_ARRAY);
    return vector_count(dtype->array.brackets->n_brackets);
}
