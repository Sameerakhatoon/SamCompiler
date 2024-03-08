#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags){
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

    process->flags    = flags;
    process->cfile.fp = file;
    process->ofile    = out_file;
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
