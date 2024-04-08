#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

// target-independent codegen entry. Each target's backend lives
// in its own codegen_<target>.c; codegen() hands the translation unit
// to the one the compile_process targets.

int codegen(struct compile_process* process){
    return process->target->backend->generate(process);
}

// Generator state shared by every backend: string table, entry/exit
// label stacks, response stack, switch bookkeeping and the extra
// .data lines. Moved here from the x86 backend unchanged.
struct code_generator* codegenerator_new(struct compile_process* process){
    (void)process;
    struct code_generator* gen = calloc(1, sizeof(struct code_generator));
    // ch110: string table lives alongside entry/exit stacks.
    gen->string_table = vector_create(sizeof(struct string_table_element*));
    gen->entry_points = vector_create(sizeof(struct codegen_entry_point*));
    gen->exit_points  = vector_create(sizeof(struct codegen_exit_point*));
    // ch142: codegen response stack.
    gen->responses    = vector_create(sizeof(struct response*));
    // ch164: nested-switch bookkeeping vector.
    gen->_switch.swtiches = vector_create(sizeof(struct generator_switch_stmt_entity));
    // ch187: custom .data add-ons (per-call function pointer slots).
    gen->custom_data_section = vector_create(sizeof(const char*));
    return gen;
}

void codegen_emit_initializer(struct node* var_node, const struct initializer_emitter* emitter){
    struct vector* leaves = var_node->var.val->init.leaves;
    size_t size = variable_size(var_node);
    size_t at = 0;
    for(int i = 0; i < vector_count(leaves); i++){
        struct initializer_leaf* leaf = vector_at(leaves, i);
        if(leaf->offset > at){
            emitter->zero(leaf->offset - at);
        }
        size_t leaf_size = datatype_size(&leaf->type);
        struct node* value = leaf->value;
        switch(value->type){
            case NODE_TYPE_NUMBER:
                emitter->value(leaf_size, (long long)value->llnum);
                break;
            case NODE_TYPE_STRING:
                emitter->symbol(leaf_size, emitter->string_label(value->sval));
                break;
            case NODE_TYPE_UNARY:
                emitter->symbol(leaf_size, value->unary.operand->sval);
                break;
            case NODE_TYPE_IDENTIFIER:
                emitter->symbol(leaf_size, value->sval);
                break;
            default:
                compiler_node_error(var_node, "BUG: unexpected initializer value");
        }
        at = leaf->offset + leaf_size;
    }
    if(at < size){
        emitter->zero(size - at);
    }
}

// Non-negative values keep the spelling every backend used, so existing
// output is unchanged. The buffers rotate so one asm_push can use two.
const char* codegen_case_label(long long value){
    static char buffers[4][32];
    static int next = 0;
    char* buffer = buffers[next];
    next = (next + 1) % 4;
    if(value < 0){
        snprintf(buffer, sizeof(buffers[0]), "m%llu", 0ULL - (unsigned long long)value);
    } else {
        snprintf(buffer, sizeof(buffers[0]), "%lld", value);
    }
    return buffer;
}
