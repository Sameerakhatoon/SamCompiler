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

// ch124: struct field-offset walk. Iterates the struct body's
// variable nodes (forward or backward), summing sizes and aligning to
// each new member, until it finds `var_name` (or runs out). Sets
// *var_node_out to whichever variable node it stopped on.
struct node* body_largest_variable_node(struct node* body_node){
    if(!body_node){
        return 0;
    }
    if(body_node->type != NODE_TYPE_BODY){
        return 0;
    }
    return body_node->body.largest_var_node;
}

struct node* variable_struct_or_union_largest_variable_node(struct node* var_node){
    return body_largest_variable_node(variable_struct_or_union_body_node(var_node));
}

int struct_offset(struct compile_process* compile_proc, const char* struct_name, const char* var_name, struct node** var_node_out, int last_pos, int flags){
    struct symbol* struct_sym = symresolver_get_symbol(compile_proc, struct_name);
    assert(struct_sym && struct_sym->type == SYMBOL_TYPE_NODE);
    struct node* node = struct_sym->data;
    assert(node_is_struct_or_union(node));

    struct vector* struct_vars_vec = node->_struct.body_n->body.statements;
    vector_set_peek_pointer(struct_vars_vec, 0);
    if(flags & STRUCT_ACCESS_BACKWARDS){
        vector_set_peek_pointer_end(struct_vars_vec);
        vector_set_flag(struct_vars_vec, VECTOR_FLAG_PEEK_DECREMENT);
    }

    struct node* var_node_cur  = variable_node(vector_peek_ptr(struct_vars_vec));
    struct node* var_node_last = 0;
    int position = last_pos;
    *var_node_out = 0;
    while(var_node_cur){
        *var_node_out = var_node_cur;
        if(var_node_last){
            position += variable_size(var_node_last);
            if(variable_node_is_primitive(var_node_cur)){
                position = align_value_treat_positive(position, var_node_cur->var.type.size);
            } else {
                position = align_value_treat_positive(position,
                    variable_struct_or_union_largest_variable_node(var_node_cur)->var.type.size);
            }
        }
        if(S_EQ(var_node_cur->var.name, var_name)){
            break;
        }
        var_node_last = var_node_cur;
        var_node_cur  = variable_node(vector_peek_ptr(struct_vars_vec));
    }
    vector_unset_flag(struct_vars_vec, VECTOR_FLAG_PEEK_DECREMENT);
    return position;
}

// ch127: access / array / parens operator + node predicates. Moved
// is_array_node here so all three families live together.
bool is_access_operator(const char* op){
    return S_EQ(op, "->") || S_EQ(op, ".");
}

bool is_access_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_access_operator(node->exp.op);
}

bool is_access_node_with_op(struct node* node, const char* op){
    return is_access_node(node) && S_EQ(node->exp.op, op);
}

bool is_array_operator(const char* op){
    return S_EQ(op, "[]");
}

bool is_array_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_array_operator(node->exp.op);
}

bool is_parentheses_operator(const char* op){
    return S_EQ(op, "()");
}

bool is_parentheses_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_parentheses_operator(node->exp.op);
}

// ch119: byte offset for the index-th access into dtype.
int array_offset(struct datatype* dtype, int index, int index_value){
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)
       || (index == (int)vector_count(dtype->array.brackets->n_brackets) - 1)){
        return index_value * datatype_element_size(dtype);
    }
    return array_multiplier(dtype, index, index_value) * datatype_element_size(dtype);
}
