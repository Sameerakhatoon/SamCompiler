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
