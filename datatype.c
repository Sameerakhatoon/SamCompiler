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

// Size of one element. Pointers are always DWORD; everything else
// uses the recorded size.
size_t datatype_element_size(struct datatype* dtype){
    if(dtype->flags & DATATYPE_FLAG_IS_POINTER){
        return DATA_SIZE_DWORD;
    }
    return dtype->size;
}

// Size ignoring pointer-ness: arrays still use the array total.
size_t datatype_size_no_ptr(struct datatype* dtype){
    if(dtype->flags & DATATYPE_FLAG_IS_ARRAY){
        return dtype->array.size;
    }
    return dtype->size;
}

// Full size: pointers are DWORD, arrays use their total size, else
// the raw type size.
size_t datatype_size(struct datatype* dtype){
    if((dtype->flags & DATATYPE_FLAG_IS_POINTER) && dtype->pointer_depth > 0){
        return DATA_SIZE_DWORD;
    }
    if(dtype->flags & DATATYPE_FLAG_IS_ARRAY){
        return dtype->array.size;
    }
    return dtype->size;
}
