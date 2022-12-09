#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags, struct compile_process* parent_process){
    struct compile_process* process = 0;
    FILE* file     = 0;
    FILE* out_file = 0;

    file = fopen(filename, "r");
    if(!file){
        goto out_err;
    }

    if(filename_out){
        out_file = fopen(filename_out, "w");
        if(!out_file){
            goto out_err;
        }
    }

    process = calloc(1, sizeof(struct compile_process));
    if(!process){
        goto out_err;
    }

    process->flags                = flags;
    process->cfile.fp             = file;
    process->cfile.abs_path       = filename;
    process->ofile                = out_file;
    process->generator            = codegenerator_new(process);
    // ch137: default resolver lives next to the codegen.
    process->resolver             = resolver_default_new_process(process);
    process->pos.line             = 1;
    process->pos.col              = 1;
    process->pos.filename         = filename;
    process->token_vec            = vector_create(sizeof(struct token));
    process->token_vec_original   = vector_create(sizeof(struct token));
    process->node_vec             = vector_create(sizeof(struct node*));
    process->node_tree_vec        = vector_create(sizeof(struct node*));

    // ch66: kick off the symbol resolver here so anyone creating a
    // compile_process gets a usable symbol table without parse() having
    // to do double duty.
    symresolver_initialize(process);
    symresolver_new_table(process);

    // ch200: nested compile_processes (e.g. for includes) inherit the
    // parent's preprocessor + include-dirs vector; top-level creates fresh.
    if(parent_process){
        process->preprocessor = parent_process->preprocessor;
        process->include_dirs = parent_process->include_dirs;
    }
    else{
        process->preprocessor = preprocessor_create(process);
        process->include_dirs = vector_create(sizeof(const char*));
        // setup default include dirs...
    }

    return process;

out_err:
    if(out_file){
        fclose(out_file);
    }
    if(file){
        fclose(file);
    }
    return 0;
}

char compile_process_next_char(struct lex_process* lex_process){
    struct compile_process* compiler = lex_process->compiler;
    compiler->pos.col += 1;
    char c = getc(compiler->cfile.fp);
    if(c == '\n'){
        compiler->pos.line += 1;
        compiler->pos.col   = 1;
    }
    return c;
}

char compile_process_peek_char(struct lex_process* lex_process){
    struct compile_process* compiler = lex_process->compiler;
    char c = getc(compiler->cfile.fp);
    ungetc(c, compiler->cfile.fp);
    return c;
}

void compile_process_push_char(struct lex_process* lex_process, char c){
    struct compile_process* compiler = lex_process->compiler;
    ungetc(c, compiler->cfile.fp);
}
