#include <stdarg.h>
#include <stdio.h>
#include "compiler.h"

// ch104: skeleton code generator. asm_push writes a formatted line to
// stdout and also to compile_process->ofile if one is open. Module 2
// builds out real instruction selection on top of this.

static struct compile_process* current_process = 0;

static void asm_push_args(const char* ins, va_list args);
static void asm_push(const char* ins, ...);

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

int codegen(struct compile_process* process){
    current_process = process;
    asm_push("jmp %s", "label_name");
    return CODEGEN_ALL_OK;
}
