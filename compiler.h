#ifndef SAMCOMPILER_H
#define SAMCOMPILER_H

#include <stdio.h>
#include <stdbool.h>

typedef struct pos pos_t;
typedef struct token token_t;

// Source position attached to every token (and most AST nodes later).
struct pos {
    int         line;
    int         col;
    const char* filename;
};

enum {
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_OPERATOR,
    TOKEN_TYPE_SYMBOL,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_COMMENT,
    TOKEN_TYPE_NEWLINE,
};

struct token {
    int type;
    int flags;

    union {
        char               cval;
        const char*        sval;
        unsigned int       inum;
        unsigned long      lnum;
        unsigned long long llnum;
        void*              any;
    };

    // True if there is whitespace between this token and the next token.
    // i.e. for input "* a", whitespace is set on the operator token "*".
    bool whitespace;

    // For tokens inside parens, e.g. "(5+10+20)", the original substring.
    const char* between_brackets;
};

// Result codes returned from compile_file.
enum {
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS,
};

typedef struct compile_process compile_process_t;
typedef struct compile_process_input_file compile_process_input_file_t;

struct compile_process_input_file {
    FILE*       fp;
    const char* abs_path;
};

struct compile_process {
    // Flags controlling how this file should be compiled.
    int flags;

    struct compile_process_input_file cfile;

    FILE* ofile;
};

int                     compile_file(const char* filename, const char* out_filename, int flags);
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);

#endif
