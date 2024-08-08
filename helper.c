#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

// Misc parser-side helpers, ch52+.

size_t variable_size(struct node* var_node){
    assert(var_node->type == NODE_TYPE_VARIABLE);
    return datatype_size(&var_node->var.type);
}

// For a struct/union variable, follow the type back to its body node.
// Unions are still unimplemented; returns NULL for them for now.
struct node* variable_struct_or_union_body_node(struct node* node){
    if(!node_is_struct_or_union_variable(node)){
        return 0;
    }
    if(node->var.type.type == DATA_TYPE_STRUCT){
        return node->var.type.struct_node->_struct.body_n;
    }
    // TODO(later): union body.
    return 0;
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

// ch119: walk the array bracket vector starting at index+1 and
// multiply each declared dimension into index_value. Returns the
// element multiplier for index `index` into a (possibly multi-dim)
// array. Non-array datatype returns index_value unchanged.
int array_multiplier(struct datatype* dtype, int index, int index_value){
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)){
        return index_value;
    }
    vector_set_peek_pointer(dtype->array.brackets->n_brackets, index + 1);
    int size_sum = index_value;
    struct node* bracket = vector_peek_ptr(dtype->array.brackets->n_brackets);
    while(bracket){
        assert(bracket->bracket.inner->type == NODE_TYPE_NUMBER);
        int declared = bracket->bracket.inner->llnum;
        size_sum *= declared;
        bracket = vector_peek_ptr(dtype->array.brackets->n_brackets);
    }
    return size_sum;
}

// ch122: ch124 ships the real `struct_offset`; this stub is here so
// ch122's resolver linker symbol resolves. Returns 0. ch124 will
// replace the body.
int struct_offset(struct compile_process* compiler, const char* struct_name, const char* var_name, struct node** out_node_out, int last_pos, int flags){
    (void)compiler; (void)struct_name; (void)var_name; (void)out_node_out; (void)last_pos; (void)flags;
    return 0;
}

// ch119: byte offset for the index-th access into dtype.
int array_offset(struct datatype* dtype, int index, int index_value){
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)
       || (index == (int)vector_count(dtype->array.brackets->n_brackets) - 1)){
        return index_value * datatype_element_size(dtype);
    }
    return array_multiplier(dtype, index, index_value) * datatype_element_size(dtype);
}
