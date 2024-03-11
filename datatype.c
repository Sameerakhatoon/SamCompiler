#include <string.h>
#include "compiler.h"

// Helpers that operate on datatypes from the parser side. ch34 starts
// it; later chapters keep piling on.

bool datatype_is_struct_or_union(struct datatype* dtype){
    return dtype->type == DATA_TYPE_STRUCT || dtype->type == DATA_TYPE_UNION;
}

bool datatype_is_struct_or_union_for_name(const char* name){
    return S_EQ(name, "union") || S_EQ(name, "struct");
}

// Size used when indexing into an array of this type. For
// `struct foo* p; p[0];` we want sizeof(struct foo), not sizeof(p).
size_t datatype_size_for_array_access(struct datatype* dtype){
    if(datatype_is_struct_or_union(dtype)
       && (dtype->flags & DATATYPE_FLAG_IS_POINTER)
       && dtype->pointer_depth == 1){
        return dtype->size;
    }
    return datatype_size(dtype);
}

// Size of one element. Pointers take the target pointer size;
// everything else uses the recorded size.
size_t datatype_element_size(struct datatype* dtype){
    if(dtype->flags & DATATYPE_FLAG_IS_POINTER){
        return target_current()->pointer_size;
    }
    return dtype->size;
}

// Alignment of a member of this type inside a struct or union. A
// scalar aligns to its size, capped at the pointer size, which gives the
// SysV rules on every target: on i386 a double or a 64-bit integer is
// 4-aligned in a struct. An array aligns like its element (`size` is
// the element size), a struct or union like its strictest member.
size_t datatype_alignment(struct datatype* dtype){
    size_t max = target_current()->pointer_size;
    if(dtype->flags & DATATYPE_FLAG_IS_POINTER){
        return max;
    }
    if(datatype_is_struct_or_union(dtype)){
        struct node* body = dtype->struct_node ? dtype->struct_node->_struct.body_n : 0;
        return body && body->body.align ? body->body.align : 1;
    }
    if(dtype->size == 0){
        return 1;
    }
    return dtype->size < max ? dtype->size : max;
}

// An int or long of `size` bytes, signed or not, as an operation or a
// constant has it. `is_long` keeps long ranked above int when the two
// are the same size (x86).
static struct datatype datatype_integer(size_t size, bool is_signed, bool is_long){
    struct datatype dtype = {0};
    dtype.type     = is_long ? DATA_TYPE_LONG : DATA_TYPE_INTEGER;
    dtype.type_str = is_long ? "long" : "int";
    dtype.size     = size;
    if(is_signed){
        dtype.flags |= DATATYPE_FLAG_IS_SIGNED;
    }
    return dtype;
}

static bool datatype_is_long(struct datatype* dtype){
    return dtype->type == DATA_TYPE_LONG || dtype->size > DATA_SIZE_DWORD;
}

// Integer conversion rank (C11 6.3.1.1): by size, and long above int.
static int datatype_rank(struct datatype* dtype){
    return (int)dtype->size * 2 + (datatype_is_long(dtype) ? 1 : 0);
}

// The type of an integer constant (C11 6.4.4.1p5): the first of these
// that holds the value. A decimal constant without u tries int then
// long; one written in hex, octal or binary may also be unsigned int or
// unsigned long; u limits the list to unsigned types, l starts it at
// long. long long is the target's long. A value no type holds becomes
// unsigned long, as gcc does with a warning.
struct datatype datatype_for_number_node(struct node* node){
    int flags = node->num.flags;
    unsigned long long value = node->llnum;
    size_t long_size = target_current()->long_size;
    bool any_sign = (flags & NUMBER_FLAG_NOT_DECIMAL) || (flags & NUMBER_FLAG_UNSIGNED);
    struct {
        size_t size;
        bool   is_signed;
        bool   is_long;
    } candidates[] = {
        { DATA_SIZE_DWORD, true,  false },
        { DATA_SIZE_DWORD, false, false },
        { long_size,       true,  true  },
        { long_size,       false, true  },
    };
    for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++){
        bool is_signed = candidates[i].is_signed;
        if((flags & NUMBER_FLAG_LONG) && !candidates[i].is_long) continue;
        if((flags & NUMBER_FLAG_UNSIGNED) && is_signed) continue;
        if(!is_signed && !any_sign) continue;
        int bits = candidates[i].size * 8 - (is_signed ? 1 : 0);
        if(bits >= 64 || value <= (~0ULL >> (64 - bits))){
            struct datatype dtype = datatype_integer(candidates[i].size, is_signed, candidates[i].is_long);
            dtype.flags |= DATATYPE_FLAG_IS_LITERAL;
            return dtype;
        }
    }
    struct datatype dtype = datatype_integer(long_size, false, true);
    dtype.flags |= DATATYPE_FLAG_IS_LITERAL;
    return dtype;
}

// A constant as a 64-bit register holds it: a 4-byte signed value sign
// extended, a 4-byte unsigned value zero extended, a long as it is.
long long datatype_canonical_value(unsigned long long value, struct datatype* dtype){
    if(datatype_element_size(dtype) <= DATA_SIZE_DWORD){
        if(dtype->flags & DATATYPE_FLAG_IS_SIGNED){
            return (long long)(int)(unsigned int)value;
        }
        return (long long)(unsigned int)value;
    }
    return (long long)value;
}

// Integer promotion (C11 6.3.1.1p2): a char or short operand becomes
// int, since int holds all their values.
struct datatype datatype_promote(struct datatype* dtype){
    if((dtype->flags & DATATYPE_FLAG_IS_POINTER) || datatype_is_struct_or_union(dtype)){
        return *dtype;
    }
    if(datatype_element_size(dtype) < DATA_SIZE_DWORD){
        return datatype_integer(DATA_SIZE_DWORD, true, false);
    }
    return datatype_integer(datatype_element_size(dtype), dtype->flags & DATATYPE_FLAG_IS_SIGNED, datatype_is_long(dtype));
}

// The usual arithmetic conversions (C11 6.3.1.8p1) of two integer
// operands: the same signedness keeps the higher rank; otherwise the
// unsigned type wins unless the signed type is wider and so holds all
// its values; if not, the unsigned version of the signed type.
static struct datatype datatype_common(struct datatype* left, struct datatype* right){
    struct datatype a = datatype_promote(left);
    struct datatype b = datatype_promote(right);
    bool a_signed = a.flags & DATATYPE_FLAG_IS_SIGNED;
    bool b_signed = b.flags & DATATYPE_FLAG_IS_SIGNED;
    if(a_signed == b_signed){
        return datatype_rank(&a) >= datatype_rank(&b) ? a : b;
    }
    struct datatype* u = a_signed ? &b : &a;
    struct datatype* s = a_signed ? &a : &b;
    if(datatype_rank(u) >= datatype_rank(s)){
        return *u;
    }
    if(s->size > u->size){
        return *s;
    }
    return datatype_integer(s->size, false, datatype_is_long(s));
}

static bool datatype_op_is_comparison(const char* op){
    return S_EQ(op, "==") || S_EQ(op, "!=") || S_EQ(op, "<")
        || S_EQ(op, ">")  || S_EQ(op, "<=") || S_EQ(op, ">=");
}

// Bytes a pointer step moves: the size of what it points at.
static size_t datatype_pointer_step(struct datatype* pointer){
    return datatype_size(datatype_pointer_reduce(pointer, 1));
}

// How `left op right` runs, for every binary operator a backend turns
// into one instruction. Before this, each backend took the signedness
// from the right operand alone and the width from the wider operand,
// and scaled the non-pointer side for every operator, so `-17 / 5`
// divided unsigned, `u > 1` compared signed, and `q > p` compared q
// with 4 * p.
void datatype_binary_types(struct datatype* left, struct datatype* right, const char* op, struct datatype_binary* out){
    memset(out, 0, sizeof(*out));
    out->scale_left  = 1;
    out->scale_right = 1;
    out->pointer_difference = 1;
    size_t pointer_size = target_current()->pointer_size;
    bool left_pointer  = left->flags & DATATYPE_FLAG_IS_POINTER;
    bool right_pointer = right->flags & DATATYPE_FLAG_IS_POINTER;

    if(datatype_op_is_comparison(op)){
        // Pointers compare as unsigned addresses, unscaled.
        if(left_pointer || right_pointer){
            out->operation = datatype_integer(pointer_size, false, pointer_size > DATA_SIZE_DWORD);
        } else {
            out->operation = datatype_common(left, right);
        }
        out->result = datatype_integer(DATA_SIZE_DWORD, true, false);
        return;
    }

    if((S_EQ(op, "+") || S_EQ(op, "-")) && (left_pointer || right_pointer)){
        out->operation = datatype_integer(pointer_size, false, pointer_size > DATA_SIZE_DWORD);
        if(left_pointer && right_pointer){
            // p - q counts elements, as a ptrdiff_t (long).
            out->pointer_difference = datatype_pointer_step(left);
            out->operation.flags |= DATATYPE_FLAG_IS_SIGNED;
            out->result = datatype_integer(target_current()->long_size, true, true);
            return;
        }
        if(left_pointer){
            out->scale_right = datatype_pointer_step(left);
            out->result = *left;
        } else {
            out->scale_left = datatype_pointer_step(right);
            out->result = *right;
        }
        return;
    }

    if(S_EQ(op, "<<") || S_EQ(op, ">>")){
        // A shift has the promoted type of its left operand.
        out->operation = datatype_promote(left);
        out->result    = out->operation;
        return;
    }

    out->operation = datatype_common(left, right);
    out->result    = out->operation;
}

// An array used as a value is the address of its first element, so its
// type becomes a pointer to the element type: `int a[3]` -> `int*`,
// `char* s[2]` -> `char**`. For `int m[2][3]` the result is
// `int*`; the row size is not kept, so `m + 1` is not supported.
struct datatype datatype_decay(struct datatype* dtype){
    struct datatype decayed = *dtype;
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)){
        return decayed;
    }
    decayed.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    decayed.flags |= DATATYPE_FLAG_IS_POINTER;
    decayed.pointer_depth++;
    return decayed;
}

// Size ignoring pointer-ness: arrays still use the array total.
size_t datatype_size_no_ptr(struct datatype* dtype){
    if(dtype->flags & DATATYPE_FLAG_IS_ARRAY){
        return dtype->array.size;
    }
    return dtype->size;
}

// Full size: arrays use their total size, pointers take the target
// pointer size, else the raw type size. The array test comes first:
// `int* v[2]` is an array of two pointers, and sizing it as one pointer
// overlapped v[1] with the next local.
size_t datatype_size(struct datatype* dtype){
    if(dtype->flags & DATATYPE_FLAG_IS_ARRAY){
        return dtype->array.size;
    }
    if((dtype->flags & DATATYPE_FLAG_IS_POINTER) && dtype->pointer_depth > 0){
        return target_current()->pointer_size;
    }
    return dtype->size;
}

bool datatype_is_primitive(struct datatype* dtype){
    return !datatype_is_struct_or_union(dtype);
}

// ch135: struct / union value (not a pointer to one).
bool datatype_is_struct_or_union_non_pointer(struct datatype* dtype){
    return dtype->type != DATA_TYPE_UNKNOWN
        && !datatype_is_primitive(dtype)
        && !(dtype->flags & DATATYPE_FLAG_IS_POINTER);
}

// ch237: set a datatype struct to void. Native functions use this as a
// default return type since we will never look at it from the caller.
// clear the whole struct first. The natives pass uninitialised
// stack datatypes here, and codegen used to read their leftover
// flags.
void datatype_set_void(struct datatype* dtype){
    memset(dtype, 0, sizeof(struct datatype));
    dtype->type     = DATA_TYPE_VOID;
    dtype->type_str = "void";
    dtype->size     = 0;
}

// ch257: helper for the validator's return-statement check. True
// when the dtype is `void` with no pointer indirection (so callers
// returning a value into it are a compile error).
bool datatype_is_void_no_ptr(struct datatype* dtype){
    return S_EQ(dtype->type_str, "void") && !(dtype->flags & DATATYPE_FLAG_IS_POINTER);
}
