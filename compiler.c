#include <stdarg.h>
#include <stdlib.h>
#include "compiler.h"

static void compiler_emit_pos(struct compile_process* compiler);

// The default set of input-source callbacks. They read characters from the
// compile_process input FILE*.
struct lex_process_functions compiler_lex_functions = {
    .next_char = compile_process_next_char,
    .peek_char = compile_process_peek_char,
    .push_char = compile_process_push_char,
};

void compiler_error(struct compile_process* compiler, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    compiler_emit_pos(compiler);
    exit(-1);
}

void compiler_warning(struct compile_process* compiler, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    compiler_emit_pos(compiler);
}

static void compiler_emit_pos(struct compile_process* compiler){
    const char* file = compiler->pos.filename ? compiler->pos.filename : "<unknown>";
    fprintf(stderr, " on line %i, col %i in file %s\n",
        compiler->pos.line, compiler->pos.col, file);
}

int compile_file(const char* filename, const char* out_filename, int flags){
    int                     res         = COMPILER_FILE_COMPILED_OK;
    struct compile_process* process     = 0;
    struct lex_process*     lex_process = 0;

    process = compile_process_create(filename, out_filename, flags);
    if(!process){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // Perform lexical analysis.
    lex_process = lex_process_create(process, &compiler_lex_functions, 0);
    if(!lex_process){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    if(lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // Perform parsing.
    // Perform code generation.

out:
    return res;
}
