#include "compiler.h"

int compile_file(const char* filename, const char* out_filename, int flags){
    int res = COMPILER_FILE_COMPILED_OK;
    struct compile_process* process = compile_process_create(filename, out_filename, flags);
    if(!process){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // Perform lexical analysis.
    // Perform parsing.
    // Perform code generation.

out:
    return res;
}
