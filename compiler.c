#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"

static struct compile_process* compile_include_path(const char* path, struct compile_process* parent_process);

// -D and -U, in command-line order, as preprocessor lines.
static struct vector* compiler_cli_macro_lines;

void compiler_add_cli_macro_line(const char* line){
    if(!compiler_cli_macro_lines){
        compiler_cli_macro_lines = vector_create(sizeof(const char*));
    }
    vector_push(compiler_cli_macro_lines, &line);
}

// The -D / -U lines are lexed from a memory FILE*. The string-buffer
// source of tokens_build_for_string() cannot be used: its push_char
// appends to the end of the buffer, so a character the lexer reads
// ahead and gives back (the second `(` of `((a)*(a))`) moves to the end.
static char compiler_memory_next_char(struct lex_process* lex_process){
    return getc(lex_process_private(lex_process));
}

static char compiler_memory_peek_char(struct lex_process* lex_process){
    FILE* file = lex_process_private(lex_process);
    char c = getc(file);
    ungetc(c, file);
    return c;
}

static void compiler_memory_push_char(struct lex_process* lex_process, char c){
    ungetc(c, lex_process_private(lex_process));
}

static struct lex_process_functions compiler_memory_lex_functions = {
    .next_char = compiler_memory_next_char,
    .peek_char = compiler_memory_peek_char,
    .push_char = compiler_memory_push_char,
};

// Put the -D / -U lines in front of the source's own tokens, so the
// preprocessor defines and undefines them, in order, before line 1
// (after the predefined macros, which -U can therefore remove).
static int compiler_prepend_cli_macros(struct compile_process* process){
    if(!compiler_cli_macro_lines || !vector_count(compiler_cli_macro_lines)){
        return 0;
    }
    struct buffer* text = buffer_create();
    vector_set_peek_pointer(compiler_cli_macro_lines, 0);
    const char* line = vector_peek_ptr(compiler_cli_macro_lines);
    while(line){
        buffer_printf(text, "%s\n", line);
        line = vector_peek_ptr(compiler_cli_macro_lines);
    }
    buffer_write(text, 0);
    const char* source_text = buffer_ptr(text);
    FILE* file = fmemopen((void*)source_text, strlen(source_text), "r");
    if(!file){
        return -1;
    }
    struct lex_process* lex_process = lex_process_create(process, &compiler_memory_lex_functions, file);
    if(!lex_process || lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK){
        fclose(file);
        return -1;
    }
    fclose(file);
    struct vector* tokens = lex_process_tokens(lex_process);
    struct vector* source = process->token_vec_original;
    for(int i = 0; i < vector_count(source); i++){
        vector_push(tokens, vector_at(source, i));
    }
    process->token_vec_original = tokens;
    return 0;
}

// -E: write the preprocessed tokens back out as C. A token starts a
// new line when it comes from a later line of the same file as the
// tokens before it; a macro's expansion carries the position of the
// #define, so it neither breaks the line nor moves the line count.
// Otherwise the lexer's whitespace flag decides the spaces. Comments
// are dropped as gcc -E does.
static void compiler_write_string_literal(FILE* out, const char* s){
    fputc('"', out);
    for(const unsigned char* c = (const unsigned char*)s; *c; c++){
        switch(*c){
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out);  break;
            case '\t': fputs("\\t", out);  break;
            case '\r': fputs("\\r", out);  break;
            default:
                if(*c < 32 || *c > 126){
                    fprintf(out, "\\%03o", *c);
                } else {
                    fputc(*c, out);
                }
        }
    }
    fputc('"', out);
}

static void compiler_emit_preprocessed(struct compile_process* process, FILE* out){
    struct token* previous = NULL;
    const char* file = NULL;
    int line = 0;
    for(int i = 0; i < vector_count(process->token_vec); i++){
        struct token* token = vector_at(process->token_vec, i);
        if(token->type == TOKEN_TYPE_NEWLINE || token->type == TOKEN_TYPE_COMMENT){
            continue;
        }
        const char* token_file = token->pos.filename ? token->pos.filename : "";
        bool same_file = file && S_EQ(file, token_file);
        if(previous){
            if(same_file && token->pos.line > line){
                fputc('\n', out);
            } else if(previous->whitespace || !same_file){
                fputc(' ', out);
            }
        }
        if(!same_file || token->pos.line > line){
            file = token_file;
            line = token->pos.line;
        }
        switch(token->type){
            case TOKEN_TYPE_IDENTIFIER:
            case TOKEN_TYPE_KEYWORD:
            case TOKEN_TYPE_OPERATOR:
                fputs(token->sval, out);
                break;
            case TOKEN_TYPE_SYMBOL:
                fputc(token->cval, out);
                break;
            case TOKEN_TYPE_NUMBER:
                fprintf(out, (token->num.flags & NUMBER_FLAG_NOT_DECIMAL) ? "0x%llx" : "%llu", token->llnum);
                if(token->num.flags & NUMBER_FLAG_UNSIGNED){
                    fputc('U', out);
                }
                if(token->num.flags & NUMBER_FLAG_LONG){
                    fputc('L', out);
                }
                break;
            case TOKEN_TYPE_STRING:
                compiler_write_string_literal(out, token->sval);
                break;
        }
        previous = token;
    }
    if(previous){
        fputc('\n', out);
    }
}

// The default set of input-source callbacks. They read characters from the
// compile_process input FILE*.
struct lex_process_functions compiler_lex_functions = {
    .next_char = compile_process_next_char,
    .peek_char = compile_process_peek_char,
    .push_char = compile_process_push_char,
};

static int      compiler_errors;
static int      compiler_warnings;
static jmp_buf* compiler_recovery;

void compiler_set_recovery(jmp_buf* point){
    compiler_recovery = point;
}

int compiler_error_count(void){
    return compiler_errors;
}

// "1 error generated.", "2 errors and 1 warning generated." Printed
// once, when the compile fails.
void compiler_finish_errors(void){
    if(!compiler_errors){
        return;
    }
    fprintf(stderr, "%i error%s", compiler_errors, compiler_errors == 1 ? "" : "s");
    if(compiler_warnings){
        fprintf(stderr, " and %i warning%s", compiler_warnings, compiler_warnings == 1 ? "" : "s");
    }
    fprintf(stderr, " generated.\n");
    exit(-1);
}

// One diagnostic line: the message (many end in "\n", which is dropped
// so the position stays on the same line), then where. Before, a
// validator error printed `line 0, col 0 in file (null)`.
static void compiler_report(const char* kind, struct pos* pos, const char* msg, va_list args){
    char text[1024];
    vsnprintf(text, sizeof(text), msg, args);
    size_t len = strlen(text);
    while(len && (text[len - 1] == '\n' || text[len - 1] == ' ')){
        text[--len] = 0;
    }
    const char* file = pos && pos->filename ? pos->filename : "<unknown>";
    fprintf(stderr, "%s%s on line %i, col %i in file %s\n", kind, text,
        pos ? pos->line : 0, pos ? pos->col : 0, file);
}

static void compiler_fail(void){
    compiler_errors++;
    if(compiler_recovery){
        longjmp(*compiler_recovery, 1);
    }
    compiler_finish_errors();
}

// ch255: error formatter for the validator. Same shape as
// compiler_error, but anchored at a parse-tree node's position so
// validation diagnostics point at the offending source line.
void compiler_node_error(struct node* node, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    compiler_report("", node ? &node->pos : 0, msg, args);
    va_end(args);
    compiler_fail();
}

// An error at `pos` (may be NULL), for code that has no process or node.
void compiler_error_at(struct pos* pos, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    compiler_report("", pos, msg, args);
    va_end(args);
    compiler_fail();
}

void compiler_error(struct compile_process* compiler, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    compiler_report("", &compiler->pos, msg, args);
    va_end(args);
    compiler_fail();
}

void compiler_warning(struct compile_process* compiler, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    compiler_report("warning: ", &compiler->pos, msg, args);
    va_end(args);
    compiler_warnings++;
}

void compiler_node_warning(struct node* node, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    compiler_report("warning: ", &node->pos, msg, args);
    va_end(args);
    compiler_warnings++;
}

// ch229: try to open + lex + preprocess `<include_dir>/<filename>`
// (or the bare filename if the joined path doesn't exist). Returns
// the resulting compile_process on success, NULL on any failure.
// Only lex + preprocess run; parse + codegen stay with the outer
// translation unit.
struct compile_process* compile_include_for_include_dir(const char* include_dir, const char* filename, struct compile_process* parent_process){
    char tmp_filename[PATH_MAX];
    int written = snprintf(tmp_filename, sizeof(tmp_filename), "%s/%s", include_dir, filename);
    if(written < 0 || (size_t)written >= sizeof(tmp_filename)){
        compiler_error(parent_process, "Include path %s/%s is longer than %i characters", include_dir, filename, PATH_MAX - 1);
    }
    if(file_exists(tmp_filename)){
        // The process keeps the name for its error positions.
        filename = strdup(tmp_filename);
    }
    return compile_include_path(filename, parent_process);
}

// Lex and preprocess the include file at `path`.
static struct compile_process* compile_include_path(const char* path, struct compile_process* parent_process){
    struct compile_process* process = compile_process_create(path, NULL, parent_process->flags, parent_process);
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
    return compile_include_search(filename, parent_process, true);
}

// Join `dir` and `filename` into `out`; true when that file exists.
static bool compile_include_join(char* out, size_t out_size, const char* dir, const char* filename, struct compile_process* parent_process){
    int written = snprintf(out, out_size, "%s/%s", dir, filename);
    if(written < 0 || (size_t)written >= out_size){
        compiler_error(parent_process, "Include path %s/%s is longer than %i characters", dir, filename, (int)out_size - 1);
    }
    return file_exists(out);
}

// The search order of C11 6.10.2 as gcc does it: a quoted include
// looks in the including file's own directory first (an earlier version did
// this for every include, as the lexer did not yet tell "x.h" from <x.h>),
// then both kinds walk the include directories: -I, then pc_includes
// next to the compiler binary, then /usr/include. The current
// directory is not searched, so the result no longer depends on where
// the compiler runs.
struct compile_process* compile_include_search(const char* filename, struct compile_process* parent_process, bool quoted){
    char path[PATH_MAX];
    if(filename[0] == '/'){
        return file_exists(filename) ? compile_include_path(filename, parent_process) : NULL;
    }
    if(quoted){
        // Standard input has no directory: use the current one, as gcc does.
        char own_dir[PATH_MAX];
        snprintf(own_dir, sizeof(own_dir), "%s", parent_process->cfile.abs_path);
        char* slash = strrchr(own_dir, '/');
        if(slash){
            *slash = 0;
        } else {
            strcpy(own_dir, ".");
        }
        if(compile_include_join(path, sizeof(path), own_dir, filename, parent_process)){
            return compile_include_path(strdup(path), parent_process);
        }
    }
    const char* include_dir = compiler_include_dir_begin(parent_process);
    while(include_dir){
        if(compile_include_join(path, sizeof(path), include_dir, filename, parent_process)){
            return compile_include_path(strdup(path), parent_process);
        }
        include_dir = compiler_include_dir_next(parent_process);
    }
    return NULL;
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
    if(compiler_prepend_cli_macros(process) != 0){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }
    if(preprocessor_run(process) != 0){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    if(flags & COMPILE_PROCESS_PREPROCESS_ONLY){
        compiler_emit_preprocessed(process, process->ofile ? process->ofile : stdout);
        if(process->ofile){
            fclose(process->ofile);
            process->ofile = 0;
        }
        goto out;
    }

    // Perform parsing.
    if(parse(process) != PARSE_ALL_OK){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }
    // C11 6.9p1: a translation unit has at least one declaration. An
    // empty file compiled silently to an empty object.
    if(!vector_count(process->node_tree_vec)){
        compiler_warning(process, "%s declares nothing (ISO C requires at least one declaration)", filename);
    }

    // ch243: validator stage, on the finished AST. Most checks report
    // through compiler_error and exit; a non-OK result stops here too.
    if(validate(process) != VALIDATION_ALL_OK){
        res = COMPILER_FAILED_WITH_ERRORS;
        goto out;
    }

    // ch104: codegen stage. codegen.c hands the AST to the target's
    // backend, which writes the asm to the output file.
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
