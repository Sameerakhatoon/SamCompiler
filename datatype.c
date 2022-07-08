#include "compiler.h"

// Helpers that operate on datatypes from the parser side. ch34 starts
// it; later chapters keep piling on.

bool datatype_is_struct_or_union(struct datatype* dtype){
    return dtype->type == DATA_TYPE_STRUCT || dtype->type == DATA_TYPE_UNION;
}

bool datatype_is_struct_or_union_for_name(const char* name){
    return S_EQ(name, "union") || S_EQ(name, "struct");
}
