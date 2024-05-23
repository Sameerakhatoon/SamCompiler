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
