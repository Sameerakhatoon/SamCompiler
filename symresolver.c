#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

static void symresolver_push_symbol(struct compile_process* process, struct symbol* sym);
static void symresolver_build_for_variable_node(struct compile_process* process, struct node* node);
static void symresolver_build_for_function_node(struct compile_process* process, struct node* node);
static void symresolver_build_for_structure_node(struct compile_process* process, struct node* node);
static void symresolver_build_for_union_node(struct compile_process* process, struct node* node);

static void symresolver_push_symbol(struct compile_process* process, struct symbol* sym){
    vector_push(process->symbols.table, &sym);
}

void symresolver_initialize(struct compile_process* process){
    process->symbols.tables = vector_create(sizeof(struct vector*));
}

// Save the current symbol table onto the stack, swap in a fresh empty
// one. Used when entering a function body so locals shadow without
// permanently clobbering globals.
void symresolver_new_table(struct compile_process* process){
    vector_push(process->symbols.tables, &process->symbols.table);
    process->symbols.table = vector_create(sizeof(struct symbol*));
}

void symresolver_end_table(struct compile_process* process){
    struct vector* last_table = vector_back_ptr(process->symbols.tables);
    process->symbols.table    = last_table;
    vector_pop(process->symbols.tables);
}

struct symbol* symresolver_get_symbol(struct compile_process* process, const char* name){
    vector_set_peek_pointer(process->symbols.table, 0);
    struct symbol* symbol = vector_peek_ptr(process->symbols.table);
    while(symbol){
        if(S_EQ(symbol->name, name)){
            break;
        }
        symbol = vector_peek_ptr(process->symbols.table);
    }
    return symbol;
}

struct symbol* symresolver_get_symbol_for_native_function(struct compile_process* process, const char* name){
    struct symbol* sym = symresolver_get_symbol(process, name);
    if(!sym || sym->type != SYMBOL_TYPE_NATIVE_FUNCTION){
        return 0;
    }
    return sym;
}

struct symbol* symresolver_register_symbol(struct compile_process* process,
                                           const char* sym_name, int type, void* data){
    if(symresolver_get_symbol(process, sym_name)){
        return 0;
    }
    struct symbol* sym = calloc(1, sizeof(struct symbol));
    sym->name = sym_name;
    sym->type = type;
    sym->data = data;
    symresolver_push_symbol(process, sym);
    return sym;
}

struct node* symresolver_node(struct symbol* sym){
    if(sym->type != SYMBOL_TYPE_NODE){
        return 0;
    }
    return sym->data;
}

// Each kind of declaration is recognized but not yet handled; the
// later chapters fill these in.
static void symresolver_build_for_variable_node(struct compile_process* process, struct node* node){
    (void)node;
    compiler_error(process, "Variables not yet supported\n");
}

static void symresolver_build_for_function_node(struct compile_process* process, struct node* node){
    (void)node;
    compiler_error(process, "Functions are not yet supported\n");
}

static void symresolver_build_for_structure_node(struct compile_process* process, struct node* node){
    if(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION){
        // Forward declarations don't register; the real one will.
        return;
    }
    symresolver_register_symbol(process, node->_struct.name, SYMBOL_TYPE_NODE, node);
}

static void symresolver_build_for_union_node(struct compile_process* process, struct node* node){
    (void)node;
    compiler_error(process, "Unions are not yet supported\n");
}

void symresolver_build_for_node(struct compile_process* process, struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE:
            symresolver_build_for_variable_node(process, node);
            break;
        case NODE_TYPE_FUNCTION:
            symresolver_build_for_function_node(process, node);
            break;
        case NODE_TYPE_STRUCT:
            symresolver_build_for_structure_node(process, node);
            break;
        case NODE_TYPE_UNION:
            symresolver_build_for_union_node(process, node);
            break;
        // Other node types can't become symbols; silently skip.
    }
}
