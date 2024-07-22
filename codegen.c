#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// ch104: skeleton. ch105: iterate the AST and emit the three usual
// asm sections (.data, .text, .rodata) - the per-node emit hooks are
// still placeholders; the bodies fill in across the rest of Module 2.

static struct compile_process* current_process = 0;

static void          codegen_new_scope(int flags);
static void          codegen_finish_scope(void);
static struct node*  codegen_node_next(void);
static void          asm_push_args(const char* ins, va_list args);
static void          asm_push(const char* ins, ...);
static const char*   asm_keyword_for_size(size_t size, char* tmp_buf);
static void          codegen_generate_global_variable_for_primitive(struct node* node);
static void          codegen_generate_global_variable(struct node* node);
static void          codegen_generate_data_section_part(struct node* node);
static void          codegen_generate_data_section(void);
static void          codegen_generate_root_node(struct node* node);
static void          codegen_generate_root(void);
static void          codegen_write_strings(void);
static void          codegen_generate_rod(void);

// ch105: placeholders. The resolver (Module 3) is what these will
// delegate to once we have one; for now we just keep the call sites
// in place.
static void codegen_new_scope(int flags){
    (void)flags;
}

static void codegen_finish_scope(void){
}

static struct node* codegen_node_next(void){
    return vector_peek_ptr(current_process->node_tree_vec);
}

static void asm_push_args(const char* ins, va_list args){
    va_list args2;
    va_copy(args2, args);
    vfprintf(stdout, ins, args);
    fprintf(stdout, "\n");
    if(current_process->ofile){
        vfprintf(current_process->ofile, ins, args2);
        fprintf(current_process->ofile, "\n");
    }
    va_end(args2);
}

static void asm_push(const char* ins, ...){
    va_list args;
    va_start(args, ins);
    asm_push_args(ins, args);
    va_end(args);
}

// ch106: map a primitive's byte size to its NASM "db / dw / dd / dq"
// keyword. For non-primitive sizes we fall back to "times N db".
// tmp_buf is supplied by the caller so we don't return a static.
static const char* asm_keyword_for_size(size_t size, char* tmp_buf){
    const char* keyword = 0;
    switch(size){
        case DATA_SIZE_BYTE:   keyword = "db"; break;
        case DATA_SIZE_WORD:   keyword = "dw"; break;
        case DATA_SIZE_DWORD:  keyword = "dd"; break;
        case DATA_SIZE_DDWORD: keyword = "dq"; break;
        default:
            sprintf(tmp_buf, "times %lu db ", (unsigned long)size);
            return tmp_buf;
    }
    strcpy(tmp_buf, keyword);
    return tmp_buf;
}

static void codegen_generate_global_variable_for_primitive(struct node* node){
    char tmp_buf[256];
    if(node->var.val){
        // ch106: placeholder for emitting the initializer. Numeric
        // and string literal handling land in ch111 / ch112.
    }
    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_global_variable(struct node* node){
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    switch(node->var.type.type){
        case DATA_TYPE_VOID:
        case DATA_TYPE_CHAR:
        case DATA_TYPE_SHORT:
        case DATA_TYPE_INTEGER:
        case DATA_TYPE_LONG:
            codegen_generate_global_variable_for_primitive(node);
            break;
        case DATA_TYPE_DOUBLE:
        case DATA_TYPE_FLOAT:
            compiler_error(current_process, "Doubles and floats are not supported in our subset of C\n");
            break;
    }
}

static void codegen_generate_data_section_part(struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE:
            codegen_generate_global_variable(node);
            break;
        default:
            break;
    }
}

static void codegen_generate_data_section(void){
    asm_push("section .data");
    struct node* node = codegen_node_next();
    while(node){
        codegen_generate_data_section_part(node);
        node = codegen_node_next();
    }
}

static void codegen_generate_root_node(struct node* node){
    (void)node;
    // Per-node function emit lands in later chapters.
}

static void codegen_generate_root(void){
    asm_push("section .text");
    struct node* node = 0;
    while((node = codegen_node_next()) != 0){
        codegen_generate_root_node(node);
    }
}

static void codegen_write_strings(void){
    // String-table emit lands in ch110+.
}

static void codegen_generate_rod(void){
    asm_push("section .rodata");
    codegen_write_strings();
}

int codegen(struct compile_process* process){
    current_process = process;
    scope_create_root(process);

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_new_scope(0);
    codegen_generate_data_section();

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();
    codegen_finish_scope();

    codegen_generate_rod();
    return CODEGEN_ALL_OK;
}
