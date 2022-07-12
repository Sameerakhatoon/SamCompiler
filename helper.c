#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

// Misc parser-side helpers, ch52+.

size_t variable_size(struct node* var_node){
    assert(var_node->type == NODE_TYPE_VARIABLE);
    return datatype_size(&var_node->var.type);
}

// Sum the sizes of every variable in a NODE_TYPE_VARIABLE_LIST.
size_t variable_size_for_list(struct node* var_list_node){
    assert(var_list_node->type == NODE_TYPE_VARIABLE_LIST);
    size_t size = 0;
    vector_set_peek_pointer(var_list_node->var_list.list, 0);
    struct node* v = vector_peek_ptr(var_list_node->var_list.list);
    while(v){
        size += variable_size(v);
        v = vector_peek_ptr(var_list_node->var_list.list);
    }
    return size;
}

// Bytes of padding needed so that `val` becomes a multiple of `to`.
int padding(int val, int to){
    if(to <= 0){
        return 0;
    }
    if((val % to) == 0){
        return 0;
    }
    return to - (val % to) % to;
}

// Round `val` up to the nearest multiple of `to`.
int align_value(int val, int to){
    if(val % to){
        val += padding(val, to);
    }
    return val;
}

// Same, but negative inputs align toward negative-infinity.
int align_value_treat_positive(int val, int to){
    assert(to >= 0);
    if(val < 0){
        to = -to;
    }
    return align_value(val, to);
}

// Sum every variable node's padding field across a body's statements.
int compute_sum_padding(struct vector* vec){
    int total = 0;
    vector_set_peek_pointer(vec, 0);
    struct node* cur = vector_peek_ptr(vec);
    while(cur){
        if(cur->type == NODE_TYPE_VARIABLE){
            total += cur->var.padding;
        }
        cur = vector_peek_ptr(vec);
    }
    return total;
}
