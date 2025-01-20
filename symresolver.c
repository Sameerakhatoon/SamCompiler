#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

static void symresolver_push_symbol(struct compile_process* process, struct symbol* sym);
static void symresolver_build_for_variable_node(struct compile_process* process, struct node* node);
static void symresolver_build_for_function_node(struct compile_process* process, struct node* node);
static void symresolver_build_for_structure_node(struct compile_process* process, struct node* node);
static void symresolver_build_for_union_node(struct compile_process* process, struct node* node);

// Lookups used to strcmp every symbol of the table in order, so a file
// with n functions took O(n^2) to compile. Every registered symbol is
// also chained into a hash keyed by name; an entry remembers the table
// (vector) it belongs to, because each function body gets its own.
#define SYMRESOLVER_HASH_SIZE 4096

// Struct and union tags are a name space of their own (C11 6.2.3): `struct
// fixup_config` and a function fixup_config() are two names. A tag's
// entry is marked `tag`; it is in the hash only, not in a table vector,
// so the name lookups below never see it.
struct symresolver_entry {
    struct vector*            table;
    struct symbol*            sym;
    bool                      tag;
    struct symresolver_entry* next;
};

static struct symresolver_entry* symresolver_hash[SYMRESOLVER_HASH_SIZE];

// FNV-1a. A NULL name (never found by S_EQ) lands in bucket 0.
static unsigned symresolver_hash_name(const char* name){
    unsigned hash = 2166136261u;
    if(!name){
        return 0;
    }
    for(const unsigned char* c = (const unsigned char*)name; *c; c++){
        hash = (hash ^ *c) * 16777619u;
    }
    return hash % SYMRESOLVER_HASH_SIZE;
}

static void symresolver_push_symbol(struct compile_process* process, struct symbol* sym){
    vector_push(process->symbols.table, &sym);
    // A table holds one symbol per name, so the chain order only
    // matters between tables.
    struct symresolver_entry* entry = calloc(1, sizeof(struct symresolver_entry));
    unsigned bucket = symresolver_hash_name(sym->name);
    entry->table = process->symbols.table;
    entry->sym   = sym;
    entry->next  = symresolver_hash[bucket];
    symresolver_hash[bucket] = entry;
}

// Free every symbol and the hash. Called once the compile is over.
void symresolver_free_all(void){
    for(int i = 0; i < SYMRESOLVER_HASH_SIZE; i++){
        struct symresolver_entry* entry = symresolver_hash[i];
        while(entry){
            struct symresolver_entry* next = entry->next;
            free(entry->sym);
            free(entry);
            entry = next;
        }
        symresolver_hash[i] = 0;
    }
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

static struct symbol* symresolver_find(struct vector* table, const char* name, bool tag){
    if(!name){
        return 0;
    }
    struct symresolver_entry* entry = symresolver_hash[symresolver_hash_name(name)];
    for(; entry; entry = entry->next){
        if(entry->table == table && entry->tag == tag && S_EQ(entry->sym->name, name)){
            return entry->sym;
        }
    }
    return 0;
}

struct symbol* symresolver_get_symbol(struct compile_process* process, const char* name){
    return symresolver_find(process->symbols.table, name, false);
}

// The struct or union with this tag in the current table.
struct symbol* symresolver_get_tag(struct compile_process* process, const char* name){
    return symresolver_find(process->symbols.table, name, true);
}

// The tag in the current table or, failing that, in the saved ones,
// newest first (see symresolver_get_symbol_any).
struct symbol* symresolver_get_tag_any(struct compile_process* process, const char* name){
    struct symbol* symbol = symresolver_get_tag(process, name);
    for(int i = vector_count(process->symbols.tables) - 1; !symbol && i >= 0; i--){
        symbol = symresolver_find(*(struct vector**)vector_at(process->symbols.tables, i), name, true);
    }
    return symbol;
}

// A struct or union node under its tag; NULL when the tag is taken.
struct symbol* symresolver_register_tag(struct compile_process* process, const char* tag, struct node* node){
    if(symresolver_get_tag(process, tag)){
        return 0;
    }
    struct symbol* sym = calloc(1, sizeof(struct symbol));
    sym->name = tag;
    sym->type = SYMBOL_TYPE_NODE;
    sym->data = node;
    struct symresolver_entry* entry = calloc(1, sizeof(struct symresolver_entry));
    unsigned bucket = symresolver_hash_name(tag);
    entry->table = process->symbols.table;
    entry->sym   = sym;
    entry->tag   = true;
    entry->next  = symresolver_hash[bucket];
    symresolver_hash[bucket] = entry;
    return sym;
}

// The symbol in the current table or, failing that, in the saved ones,
// newest first. The validator works in a table of its own and finds
// what the parser registered (structs, native functions) this way.
struct symbol* symresolver_get_symbol_any(struct compile_process* process, const char* name){
    struct symbol* symbol = symresolver_get_symbol(process, name);
    for(int i = vector_count(process->symbols.tables) - 1; !symbol && i >= 0; i--){
        struct vector* table = *(struct vector**)vector_at(process->symbols.tables, i);
        for(int j = 0; table && j < vector_count(table); j++){
            struct symbol* candidate = *(struct symbol**)vector_at(table, j);
            if(S_EQ(candidate->name, name)){
                symbol = candidate;
                break;
            }
        }
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
// ch149: register the variable / function by name so the resolver
// can find it. Replaces the placeholder errors.
static void symresolver_build_for_variable_node(struct compile_process* process, struct node* node){
    symresolver_register_symbol(process, node->var.name, SYMBOL_TYPE_NODE, node);
}

// A definition that follows its prototype replaces it as the symbol, so
// lookups see the function that has a body.
static void symresolver_build_for_function_node(struct compile_process* process, struct node* node){
    struct symbol* existing = symresolver_get_symbol(process, node->func.name);
    struct node* previous = existing ? symresolver_node(existing) : 0;
    if(previous && previous->type == NODE_TYPE_FUNCTION && !previous->func.body_n && node->func.body_n){
        existing->data = node;
        return;
    }
    symresolver_register_symbol(process, node->func.name, SYMBOL_TYPE_NODE, node);
}

static void symresolver_build_for_structure_node(struct compile_process* process, struct node* node){
    if(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION){
        // Forward declarations don't register; the real one will.
        return;
    }
    symresolver_register_tag(process, node->_struct.name, node);
}

// ch99: register the union in the symbol table the same way as
// structs. Forward declarations are skipped.
static void symresolver_build_for_union_node(struct compile_process* process, struct node* node){
    if(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION){
        return;
    }
    symresolver_register_tag(process, node->_union.name, node);
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
