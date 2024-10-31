#include "compiler.h"
#include "helpers/vector.h"

// The value of a scalar global, `int g = 5;` or `char* s = "hi";`. A
// brace initializer, `char s[] = "hi"` and an address are leaves of a
// NODE_TYPE_INITIALIZER instead (parser_global_initializer_node), which
// codegen_emit_initializer writes.

// The bytes a number stores, as an integer: a floating constant (folded
// to the type of what it initializes) as the bits of a float or a
// double.
long long initializer_number_bits(struct node* value){
    if(value->num.flags & NUMBER_FLAG_FLOATING){
        size_t size = (value->num.flags & NUMBER_FLAG_FLOAT) ? DATA_SIZE_DWORD : DATA_SIZE_DDWORD;
        return (long long)floating_bits(number_node_double(value), size);
    }
    return (long long)value->llnum;
}

// The assembler operand for the value: a number, the label of a string
// (from the backend's register_string), or the name of a function or
// global (`&g` too).
void initializer_value_asm(struct node* value, const char* (*register_string)(const char* str), char* out, size_t len){
    switch(value->type){
        case NODE_TYPE_NUMBER:
            snprintf(out, len, "%lld", initializer_number_bits(value));
            return;
        case NODE_TYPE_STRING:
            snprintf(out, len, "%s", register_string(value->sval));
            return;
        case NODE_TYPE_IDENTIFIER:
            snprintf(out, len, "%s", value->sval);
            return;
        case NODE_TYPE_CAST:
            initializer_value_asm(value->cast.operand, register_string, out, len);
            return;
        case NODE_TYPE_UNARY:
            if(S_EQ(value->unary.op, "&") && value->unary.operand->type == NODE_TYPE_IDENTIFIER){
                snprintf(out, len, "%s", value->unary.operand->sval);
                return;
            }
            break;
    }
    compiler_node_error(value, "A global's initializer must be a constant, a string or an address\n");
}
