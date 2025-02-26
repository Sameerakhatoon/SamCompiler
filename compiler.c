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

// ch229: try to open + lex + preprocess `<include_dir>/<filename>`
// (or the bare filename if the joined path doesn't exist). Returns
// the resulting compile_process on success, NULL on any failure.
// Only lex + preprocess run; parse + codegen stay with the outer
// translation unit.
struct compile_process* compile_include_for_include_dir(const char* include_dir, const char* filename, struct compile_process* parent_process){
    char tmp_filename[512];
    sprintf(tmp_filename, "%s/%s", include_dir, filename);
    if(file_exists(tmp_filename)){
        filename = tmp_filename;
    }
    struct compile_process* process = compile_process_create(filename, NULL, parent_process->flags, parent_process);
    if(!process){
        return NULL;
    }

    struct lex_process* lex_process = lex_process_create(process, &compiler_lex_functions, NULL);
    if(!lex_process){
        return NULL;
    }

    if(lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK){
        return NULL;
    }

    process->token_vec_original = lex_process_tokens(lex_process);
    if(preprocessor_run(process) < 0){
        return NULL;
    }

    return process;
}

/**
 * @brief Includes a file to be compiled, returns a new compile process that
 *        represents the file to be compiled. Walks parent_process->include_dirs.
 *
 * Only lexical analysis + preprocessing run; parsing and codegen are
 * excluded since they happen on the parent.
 */
struct compile_process* compile_include(const char* filename, struct compile_process* parent_process){
    struct compile_process* new_process = NULL;
    const char* include_dir = compiler_include_dir_begin(parent_process);
    while(include_dir && !new_process){
        new_process = compile_include_for_include_dir(include_dir, filename, parent_process);
        include_dir = compiler_include_dir_next(parent_process);
    }

    return new_process;
}

int compile_file(const char* filename, const char* out_filename, int flags){
    int                     res         = COMPILER_FILE_COMPILED_OK;
    struct compile_process* process     = 0;
    struct lex_process*     lex_process = 0;

    process = compile_process_create(filename, out_filename, flags, NULL);
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

    // ch200: stash the raw lexer output as token_vec_original; the
    // preprocessor will fill token_vec by walking that.
    process->token_vec_original = lex_process_tokens(lex_process);
    if(preprocessor_run(process) != 0){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // Perform parsing.
    if(parse(process) != PARSE_ALL_OK){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // ch104: codegen stage. Right now just emits a placeholder line
    // through asm_push so we can wire the v-table without committing
    // to any real instruction selection yet.
    // ch243: validator stage. Currently scaffolded - returns ALL_OK -
    // but the failure mode is wired so later chapters can short-circuit
    // compilation on a check failure.
    if(validate(process) != VALIDATION_ALL_OK){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    if(codegen(process) != CODEGEN_ALL_OK){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // ch143: flush + close the asm output file so NASM can read it.
    if(process->ofile){
        fclose(process->ofile);
        process->ofile = 0;
    }

out:
    return res;
}
