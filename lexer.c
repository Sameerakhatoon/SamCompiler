#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"

// While the predicate exp() is true on the peeked char, append it to buffer
// and consume it. The buffer is the caller's; this macro is loop sugar.
#define LEX_GETC_IF(buffer, c, exp)      \
    for(c = peekc(); exp; c = peekc()){  \
        buffer_write(buffer, c);         \
        nextc();                         \
    }

static char           peekc(void);
static char           nextc(void);
static void           pushc(char c);
static struct pos     lex_file_position(void);
static struct token*  token_create(struct token* _token);
static struct token*  lexer_last_token(void);
static struct token*  handle_whitespace(void);
static const char*    read_number_str(void);
static unsigned long long read_number(void);
static struct token*  token_make_number_for_value(unsigned long long number);
static struct token*  token_make_number(void);
static struct token*  token_make_string(char start_delim, char end_delim);
static bool           op_treated_as_one(char op);
static bool           is_single_operator(char op);
static bool           op_valid(const char* op);
static void           read_op_flush_back_keep_first(struct buffer* buffer);
static const char*    read_op(void);
static void           lex_new_expression(void);
bool                  lex_is_in_expression(void);
static struct token*  token_make_operator_or_string(void);

struct token* read_next_token(void);

// Module-private state. The lex driver writes lex_process at the top of
// lex(), then every helper here reads it directly. tmp_token is the
// scratch struct token_create copies the caller's struct into; we hand
// out a pointer to it so the caller can immediately vector_push it
// (vector_push memcpy's, so no aliasing problem).
static struct lex_process* lex_process;
static struct token        tmp_token;

static char peekc(void){
    return lex_process->function->peek_char(lex_process);
}

static char nextc(void){
    char c = lex_process->function->next_char(lex_process);
    lex_process->pos.col += 1;
    if(c == '\n'){
        lex_process->pos.line += 1;
        lex_process->pos.col   = 1;
    }
    return c;
}

static void pushc(char c){
    lex_process->function->push_char(lex_process, c);
}

static struct pos lex_file_position(void){
    return lex_process->pos;
}

static struct token* token_create(struct token* _token){
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();
    return &tmp_token;
}

static struct token* lexer_last_token(void){
    return vector_back_or_null(lex_process->token_vec);
}

static struct token* handle_whitespace(void){
    struct token* last_token = lexer_last_token();
    if(last_token){
        last_token->whitespace = true;
    }

    nextc();
    return read_next_token();
}

static const char* read_number_str(void){
    struct buffer* buffer = buffer_create();
    char c;
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));
    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

static unsigned long long read_number(void){
    const char* s = read_number_str();
    return atoll(s);
}

static struct token* token_make_number_for_value(unsigned long long number){
    return token_create(&(struct token){ .type = TOKEN_TYPE_NUMBER, .llnum = number });
}

static struct token* token_make_number(void){
    return token_make_number_for_value(read_number());
}

// Reads a string literal delimited by start_delim/end_delim. Consumes the
// opening delimiter (asserted), then everything up to but not including
// the closing delimiter. ch9 just skips backslashes; real escape handling
// arrives later.
static struct token* token_make_string(char start_delim, char end_delim){
    struct buffer* buf = buffer_create();
    assert(nextc() == start_delim);

    char c;
    for(c = nextc(); c != end_delim && c != EOF; c = nextc()){
        if(c == '\\'){
            // TODO(ch-later): real escape-sequence handling.
            continue;
        }
        buffer_write(buf, c);
    }

    buffer_write(buf, 0x00);
    return token_create(&(struct token){ .type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buf) });
}

// "Treated as one" operators never combine with the next char into a
// two-char operator. e.g. '(' on its own is fine; ".." would mean
// something else (the `...` ellipsis is handled elsewhere).
static bool op_treated_as_one(char op){
    return op == '('
        || op == '['
        || op == ','
        || op == '.'
        || op == '*'
        || op == '?';
}

// Single-char operators that can also appear as the second char of a
// two-char op (so we know whether to greedily consume one more char).
static bool is_single_operator(char op){
    return op == '+' || op == '-' || op == '/' || op == '*'
        || op == '=' || op == '>' || op == '<' || op == '|'
        || op == '&' || op == '^' || op == '%' || op == '!'
        || op == '(' || op == '[' || op == ',' || op == '.'
        || op == '~' || op == '?';
}

// Whitelist of operator spellings we accept. The lexer's job is only to
// tokenize; precedence / arity live in the parser.
static bool op_valid(const char* op){
    return S_EQ(op, "+")  || S_EQ(op, "-")  || S_EQ(op, "*")  || S_EQ(op, "/")
        || S_EQ(op, "!")  || S_EQ(op, "^")
        || S_EQ(op, "+=") || S_EQ(op, "-=") || S_EQ(op, "*=") || S_EQ(op, "/=")
        || S_EQ(op, ">>") || S_EQ(op, "<<") || S_EQ(op, ">=") || S_EQ(op, "<=")
        || S_EQ(op, ">")  || S_EQ(op, "<")
        || S_EQ(op, "||") || S_EQ(op, "&&") || S_EQ(op, "|")  || S_EQ(op, "&")
        || S_EQ(op, "++") || S_EQ(op, "--") || S_EQ(op, "=")
        || S_EQ(op, "!=") || S_EQ(op, "==") || S_EQ(op, "->")
        || S_EQ(op, "(")  || S_EQ(op, "[")  || S_EQ(op, ",")  || S_EQ(op, ".")
        || S_EQ(op, "...")|| S_EQ(op, "~")  || S_EQ(op, "?")  || S_EQ(op, "%");
}

// We greedily read N chars; if the resulting operator isn't valid we have
// to push everything but the first char back onto the input stream so the
// next read_next_token() picks them up fresh.
static void read_op_flush_back_keep_first(struct buffer* buffer){
    const char* data = buffer_ptr(buffer);
    int         len  = buffer->len;
    for(int i = len - 1; i >= 1; i--){
        if(data[i] == 0x00){
            continue;
        }
        pushc(data[i]);
    }
}

static const char* read_op(void){
    bool   single_operator = true;
    char   op              = nextc();
    struct buffer* buffer  = buffer_create();
    buffer_write(buffer, op);

    // If the first char isn't a "treated-as-one" op, try to greedily eat
    // one more char to form a two-char op.
    if(!op_treated_as_one(op)){
        op = peekc();
        if(is_single_operator(op)){
            buffer_write(buffer, op);
            nextc();
            single_operator = false;
        }
    }

    buffer_write(buffer, 0x00);
    char* ptr = buffer_ptr(buffer);
    if(!single_operator){
        if(!op_valid(ptr)){
            // The greedy two-char form isn't a real op; back the second
            // char out and truncate to the first char.
            read_op_flush_back_keep_first(buffer);
            ptr[1] = 0x00;
        }
    } else if(!op_valid(ptr)){
        compiler_error(lex_process->compiler, "The operator %s is not valid\n", ptr);
    }

    return ptr;
}

// Bump the nested-expression counter (per '(' we see). The first '(' also
// allocates the parentheses_buffer that records the text inside the
// parens for later between_brackets attribution.
static void lex_new_expression(void){
    lex_process->current_expression_count++;
    if(lex_process->current_expression_count == 1){
        lex_process->parentheses_buffer = buffer_create();
    }
}

bool lex_is_in_expression(void){
    return lex_process->current_expression_count > 0;
}

// '<' is the wart: in `#include <stdio.h>` it opens a string literal, not
// a less-than operator. We disambiguate by peeking at the previous token.
static struct token* token_make_operator_or_string(void){
    char op = peekc();
    if(op == '<'){
        struct token* last_token = lexer_last_token();
        if(token_is_keyword(last_token, "include")){
            return token_make_string('<', '>');
        }
    }

    struct token* token = token_create(&(struct token){
        .type = TOKEN_TYPE_OPERATOR,
        .sval = read_op(),
    });
    if(op == '('){
        lex_new_expression();
    }
    return token;
}

struct token* read_next_token(void){
    struct token* token = 0;
    char c = peekc();
    switch(c){
        NUMERIC_CASE:
            token = token_make_number();
            break;

        OPERATOR_CASE_EXCLUDING_DIVISION:
            token = token_make_operator_or_string();
            break;

        case '"':
            token = token_make_string('"', '"');
            break;

        // Whitespace is meaningless to the lexer, except it flips the
        // previous token's whitespace flag for later parser disambiguation.
        case ' ':
        case '\t':
            token = handle_whitespace();
            break;

        case EOF:
            // End of input. token stays NULL; lex() will stop the loop.
            break;

        default:
            compiler_error(lex_process->compiler, "Unexpected token\n");
    }
    return token;
}

int lex(struct lex_process* process){
    process->current_expression_count = 0;
    process->parentheses_buffer       = 0;
    lex_process                       = process;
    process->pos.filename             = process->compiler->cfile.abs_path;

    struct token* token = read_next_token();
    while(token){
        vector_push(process->token_vec, token);
        token = read_next_token();
    }
    return LEXICAL_ANALYSIS_ALL_OK;
}
