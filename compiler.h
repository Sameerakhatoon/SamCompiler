#ifndef SAMCOMPILER_H
#define SAMCOMPILER_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// String equality with NULL-safe operands. Used everywhere from the lexer
// onwards to compare keyword / operator spellings.
#define S_EQ(str, str2) \
    (str && str2 && (strcmp(str, str2) == 0))

typedef struct pos pos_t;
typedef struct token token_t;

// Source position attached to every token (and most AST nodes later).
struct pos {
    int         line;
    int         col;
    const char* filename;
};

// Compact switch-case helper for "is this an ASCII digit". Used by the
// lexer's read_next_token dispatch.
#define NUMERIC_CASE \
    case '0':        \
    case '1':        \
    case '2':        \
    case '3':        \
    case '4':        \
    case '5':        \
    case '6':        \
    case '7':        \
    case '8':        \
    case '9'

// Every single-char that can start an operator, except '/'. Division is
// handled separately so the same path can also strip C comments later on.
#define OPERATOR_CASE_EXCLUDING_DIVISION \
    case '+':                            \
    case '-':                            \
    case '*':                            \
    case '>':                            \
    case '<':                            \
    case '^':                            \
    case '%':                            \
    case '!':                            \
    case '=':                            \
    case '~':                            \
    case '|':                            \
    case '&':                            \
    case '(':                            \
    case '[':                            \
    case ',':                            \
    case '.':                            \
    case '?'

// Symbol chars: never combine, always one token of TOKEN_TYPE_SYMBOL.
// Closing parens land here (not in OPERATOR_CASE) so we can decrement
// the expression counter when we see ')'.
#define SYMBOL_CASE \
    case '{':       \
    case '}':       \
    case ':':       \
    case ';':       \
    case '#':       \
    case '\\':      \
    case ')':       \
    case ']'

enum {
    LEXICAL_ANALYSIS_ALL_OK,
    LEXICAL_ANALYSIS_INPUT_ERROR,
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
    int        type;
    int        flags;
    struct pos pos;

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

// Forward decls for the lexer types so the function-pointer typedefs below
// can name them; the full definitions follow.
typedef struct lex_process            lex_process_t;
typedef struct lex_process_functions  lex_process_functions_t;
typedef struct compile_process        compile_process_t;
typedef struct compile_process_input_file compile_process_input_file_t;

// A lex_process talks to its input source through this trio of callbacks.
// Today the source is always a FILE*, but later (preprocessor) we'll plug
// in a string-backed source by swapping out these functions.
typedef char (*lex_process_next_char_fn)(struct lex_process* process);
typedef char (*lex_process_peek_char_fn)(struct lex_process* process);
typedef void (*lex_process_push_char_fn)(struct lex_process* process, char c);

struct lex_process_functions {
    lex_process_next_char_fn next_char;
    lex_process_peek_char_fn peek_char;
    lex_process_push_char_fn push_char;
};

struct lex_process {
    struct pos              pos;
    struct vector*          token_vec;
    struct compile_process* compiler;

    // Depth of nested parens we are currently inside. Each '(' bumps it,
    // each ')' drops it. Lets us capture the text between brackets for
    // e.g. "((50))".
    int                           current_expression_count;
    struct buffer*                parentheses_buffer;
    struct lex_process_functions* function;

    // Opaque blob owned by the caller; the lexer never touches it.
    void* private;
};

// Result codes returned from compile_file.
enum {
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS,
};

struct compile_process_input_file {
    FILE*       fp;
    const char* abs_path;
};

struct compile_process {
    // Flags controlling how this file should be compiled.
    int flags;

    struct pos                        pos;
    struct compile_process_input_file cfile;

    FILE* ofile;
};

int                     compile_file(const char* filename, const char* out_filename, int flags);
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);

void compiler_error(struct compile_process* compiler, const char* msg, ...);
void compiler_warning(struct compile_process* compiler, const char* msg, ...);

// FILE*-backed adapters that plug into lex_process_functions.
char compile_process_next_char(struct lex_process* lex_process);
char compile_process_peek_char(struct lex_process* lex_process);
void compile_process_push_char(struct lex_process* lex_process, char c);

struct lex_process* lex_process_create(struct compile_process* compiler,
                                       struct lex_process_functions* functions,
                                       void* private);
void                lex_process_free(struct lex_process* process);
void*               lex_process_private(struct lex_process* process);
struct vector*      lex_process_tokens(struct lex_process* process);
int                 lex(struct lex_process* process);

bool token_is_keyword(struct token* token, const char* value);

#endif
