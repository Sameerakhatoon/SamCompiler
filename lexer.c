#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
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
static int            lexer_number_type(char c);
static struct token*  token_make_number_for_value(unsigned long long number);
static struct token*  token_make_number(void);
static struct token*  token_make_string(char start_delim, char end_delim);
static bool           op_treated_as_one(char op);
static bool           is_single_operator(char op);
static bool           op_valid(const char* op);
static void           read_op_flush_back_keep_first(struct buffer* buffer);
static const char*    read_op(void);
static void           lex_new_expression(void);
static void           lex_finish_expression(void);
bool                  lex_is_in_expression(void);
static struct token*  token_make_operator_or_string(void);
static struct token*  token_make_symbol(void);
static struct token*  token_make_identifier_or_keyword(void);
struct token*         read_special_token(void);
static bool           is_keyword(const char* str);
static struct token*  token_make_newline(void);
static char           assert_next_char(char c);
struct token*         token_make_one_line_comment(void);
struct token*         token_make_multiline_comment(void);
struct token*         handle_comment(void);
char                  lex_get_escaped_char(char c);
struct token*         token_make_quote(void);
void                  lexer_pop_token(void);
bool                  is_hex_char(char c);
const char*           read_hex_number_str(void);
struct token*         token_make_special_number_hexadecimal(void);
void                  lexer_validate_binary_string(const char* str);
struct token*         token_make_special_number_binary(void);
struct token*         token_make_special_number(void);

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
    // While we're inside (), record every consumed char so that the
    // tokens born inside can carry the original substring via
    // between_brackets. Useful later for diagnostics that want to
    // quote the raw expression.
    if(lex_is_in_expression()){
        buffer_write(lex_process->parentheses_buffer, c);
    }
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

// Used when we already know the next char must be a particular value
// (e.g. the opening delimiter we just peeked at).
static char assert_next_char(char c){
    char next_c = nextc();
    assert(c == next_c);
    return next_c;
}

static struct pos lex_file_position(void){
    return lex_process->pos;
}

static struct token* token_create(struct token* _token){
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();
    if(lex_is_in_expression()){
        tmp_token.between_brackets = buffer_ptr(lex_process->parentheses_buffer);
    }
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

// Peek for an L / f / d suffix. The caller decides whether to consume it.
static int lexer_number_type(char c){
    int res = NUMBER_TYPE_NORMAL;
    if(c == 'L'){
        res = NUMBER_TYPE_LONG;
    } else if(c == 'f'){
        res = NUMBER_TYPE_FLOAT;
    }
    return res;
}

static struct token* token_make_number_for_value(unsigned long long number){
    int number_type = lexer_number_type(peekc());
    if(number_type != NUMBER_TYPE_NORMAL){
        nextc();
    }
    return token_create(&(struct token){
        .type     = TOKEN_TYPE_NUMBER,
        .llnum    = number,
        .num.type = number_type,
    });
}

static struct token* token_make_number(void){
    return token_make_number_for_value(read_number());
}

// Reads a string literal delimited by start_delim/end_delim. Consumes the
// opening delimiter (asserted), then everything up to but not including
// the closing delimiter. ch9 stubs out escapes; ch182 wires the real
// handler.
static void lex_handle_escape_number(struct buffer* buf){
    long long number = read_number();
    if(number > 255){
        compiler_error(lex_process->compiler,
            "Characters must be 0-255 wide chars are not yet supported\n");
    }
    buffer_write(buf, (char)number);
}

static void lex_handle_escape(struct buffer* buf){
    char c = peekc();
    if(isdigit(c)){
        lex_handle_escape_number(buf);
        return;
    }
    char co = lex_get_escaped_char(c);
    buffer_write(buf, co);
    nextc();
}

static struct token* token_make_string(char start_delim, char end_delim){
    struct buffer* buf = buffer_create();
    assert(nextc() == start_delim);

    char c;
    for(c = nextc(); c != end_delim && c != EOF; c = nextc()){
        if(c == '\\'){
            // ch182: real escape-sequence handling.
            lex_handle_escape(buf);
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

static void lex_finish_expression(void){
    lex_process->current_expression_count--;
    if(lex_process->current_expression_count < 0){
        compiler_error(lex_process->compiler, "You closed an expression that you never opened\n");
    }
}

bool lex_is_in_expression(void){
    return lex_process->current_expression_count > 0;
}

// Subset of is_keyword: the spellings that introduce a datatype. The
// parser uses this when deciding "is this `int` or `struct` the start
// of a variable / function / struct declaration?"
bool keyword_is_datatype(const char* str){
    return S_EQ(str, "void")  || S_EQ(str, "char")
        || S_EQ(str, "int")   || S_EQ(str, "short")
        || S_EQ(str, "float") || S_EQ(str, "double")
        || S_EQ(str, "long")  || S_EQ(str, "struct")
        || S_EQ(str, "union");
}

// Reserved-word table. Anything spelled identically becomes a
// TOKEN_TYPE_KEYWORD instead of a TOKEN_TYPE_IDENTIFIER. "include" is
// in here so the operator-or-string disambiguation in ch10 can detect
// `#include <stdio.h>`.
static bool is_keyword(const char* str){
    return S_EQ(str, "unsigned") || S_EQ(str, "signed")
        || S_EQ(str, "char")     || S_EQ(str, "short")
        || S_EQ(str, "int")      || S_EQ(str, "long")
        || S_EQ(str, "float")    || S_EQ(str, "double")
        || S_EQ(str, "void")     || S_EQ(str, "struct")
        || S_EQ(str, "union")    || S_EQ(str, "static")
        || S_EQ(str, "__ignore_typecheck")
        || S_EQ(str, "return")   || S_EQ(str, "include")
        || S_EQ(str, "sizeof")   || S_EQ(str, "if")
        || S_EQ(str, "else")     || S_EQ(str, "while")
        || S_EQ(str, "for")      || S_EQ(str, "do")
        || S_EQ(str, "break")    || S_EQ(str, "continue")
        || S_EQ(str, "switch")   || S_EQ(str, "case")
        || S_EQ(str, "default")  || S_EQ(str, "goto")
        || S_EQ(str, "typedef")  || S_EQ(str, "const")
        || S_EQ(str, "extern")   || S_EQ(str, "restrict");
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

// "//" line comments: eat to newline-or-EOF and emit one COMMENT token.
struct token* token_make_one_line_comment(void){
    struct buffer* buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);
    return token_create(&(struct token){
        .type = TOKEN_TYPE_COMMENT,
        .sval = buffer_ptr(buffer),
    });
}

// "/* ... */" multiline comments. Loop: accumulate until '*' or EOF; on
// '*', consume it and peek for '/'. Unterminated comment is a fatal
// compiler_error.
struct token* token_make_multiline_comment(void){
    struct buffer* buffer = buffer_create();
    char c = 0;
    while(1){
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);
        if(c == EOF){
            compiler_error(lex_process->compiler, "You did not close this multiline comment\n");
        } else if(c == '*'){
            nextc();
            if(peekc() == '/'){
                nextc();
                break;
            }
        }
    }
    return token_create(&(struct token){
        .type = TOKEN_TYPE_COMMENT,
        .sval = buffer_ptr(buffer),
    });
}

// '/' could be the start of "//", "/*", or just the division operator.
// We peek twice: if it's a comment start, route accordingly; otherwise
// push '/' back so token_make_operator_or_string handles it.
struct token* handle_comment(void){
    char c = peekc();
    if(c == '/'){
        nextc();
        if(peekc() == '/'){
            nextc();
            return token_make_one_line_comment();
        } else if(peekc() == '*'){
            nextc();
            return token_make_multiline_comment();
        }

        pushc('/');
        return token_make_operator_or_string();
    }
    return 0;
}

static struct token* token_make_symbol(void){
    char c = nextc();
    if(c == ')'){
        lex_finish_expression();
    }
    return token_create(&(struct token){ .type = TOKEN_TYPE_SYMBOL, .cval = c });
}

// Eats [A-Za-z_][A-Za-z0-9_]* and returns it as an identifier. ch13 will
// later promote it to a keyword if the spelling matches a reserved word.
static struct token* token_make_identifier_or_keyword(void){
    struct buffer* buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c,
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_');

    buffer_write(buffer, 0x00);

    if(is_keyword(buffer_ptr(buffer))){
        return token_create(&(struct token){
            .type = TOKEN_TYPE_KEYWORD,
            .sval = buffer_ptr(buffer),
        });
    }

    return token_create(&(struct token){
        .type = TOKEN_TYPE_IDENTIFIER,
        .sval = buffer_ptr(buffer),
    });
}

// Catch-all for input the main dispatch switch doesn't recognize. Right
// now only "looks like the start of an identifier" qualifies.
struct token* read_special_token(void){
    char c = peekc();
    if(isalpha(c) || c == '_'){
        return token_make_identifier_or_keyword();
    }
    return 0;
}

// '\n' becomes its own token. The preprocessor will care about line
// boundaries (e.g. terminating a #define); the parser usually skips
// over them.
static struct token* token_make_newline(void){
    nextc();
    return token_create(&(struct token){ .type = TOKEN_TYPE_NEWLINE });
}

// Map the char that follows a '\' in a quote/string into the byte it
// represents. Only the common escapes - extra ones come later.
char lex_get_escaped_char(char c){
    char co = 0;
    switch(c){
        case 'n':   co = '\n'; break;
        case '\\':  co = '\\'; break;
        case 't':   co = '\t'; break;
        case '\'':  co = '\''; break;
    }
    return co;
}

// Drop the last token from the vector. Used when read_next_token has
// already emitted something (e.g. NUMBER 0) and we now realize we want
// to consume the *next* char (e.g. 'x') and replace the pair with a
// hex literal.
void lexer_pop_token(void){
    vector_pop(lex_process->token_vec);
}

bool is_hex_char(char c){
    c = tolower(c);
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

const char* read_hex_number_str(void){
    struct buffer* buffer = buffer_create();
    char c;
    LEX_GETC_IF(buffer, c, is_hex_char(c));
    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

// `0x` is just a hex prefix. By the time we get here, '0' has already
// been lexed as NUMBER(0); we throw it away, eat the 'x', and parse the
// real hex digits.
struct token* token_make_special_number_hexadecimal(void){
    nextc();   // consume the 'x'
    const char* number_str = read_hex_number_str();
    unsigned long number   = strtol(number_str, 0, 16);
    return token_make_number_for_value(number);
}

// Reject anything in the post-`0b` digit run that isn't '0' or '1'.
void lexer_validate_binary_string(const char* str){
    size_t len = strlen(str);
    for(size_t i = 0; i < len; i++){
        if(str[i] != '0' && str[i] != '1'){
            compiler_error(lex_process->compiler, "This is not a valid binary number\n");
        }
    }
}

// `0b1010`. After popping the leading NUMBER(0), eat 'b', then reuse
// read_number_str (which only accepts 0-9, so it will gather both
// valid and invalid digits, which we then validate explicitly).
struct token* token_make_special_number_binary(void){
    nextc();   // consume the 'b'
    const char* number_str = read_number_str();
    lexer_validate_binary_string(number_str);
    unsigned long number = strtol(number_str, 0, 2);
    return token_make_number_for_value(number);
}

// Called from the 'x' / 'b' cases of the dispatch switch. We can only
// take this path if the *previous* token was the literal NUMBER(0).
// If it wasn't, fall through to identifier handling so a bare "x"
// or "b" in source code still works.
struct token* token_make_special_number(void){
    struct token* token      = 0;
    struct token* last_token = lexer_last_token();
    if(!last_token || !(last_token->type == TOKEN_TYPE_NUMBER && last_token->llnum == 0)){
        return token_make_identifier_or_keyword();
    }

    lexer_pop_token();

    char c = peekc();
    if(c == 'x'){
        token = token_make_special_number_hexadecimal();
    } else if(c == 'b'){
        token = token_make_special_number_binary();
    }

    return token;
}

// Char literal: '<one byte>' or '\<escape>'. The value lives in cval but
// the token type is TOKEN_TYPE_NUMBER (a quoted char is the same as its
// numeric value in C, and the rest of the pipeline already understands
// numbers).
struct token* token_make_quote(void){
    assert_next_char('\'');
    char c = nextc();
    if(c == '\\'){
        c = nextc();
        c = lex_get_escaped_char(c);
    }
    if(nextc() != '\''){
        compiler_error(lex_process->compiler, "You opened a quote ' but did not close it with a ' character");
    }
    return token_create(&(struct token){ .type = TOKEN_TYPE_NUMBER, .cval = c });
}

struct token* read_next_token(void){
    struct token* token = 0;
    char c = peekc();

    // Try comment handling first; it consumes '/' if it's the start of
    // "//" or "/*", otherwise it pushes '/' back and falls through.
    token = handle_comment();
    if(token){
        return token;
    }

    switch(c){
        NUMERIC_CASE:
            token = token_make_number();
            break;

        OPERATOR_CASE_EXCLUDING_DIVISION:
            token = token_make_operator_or_string();
            break;

        SYMBOL_CASE:
            token = token_make_symbol();
            break;

        case 'b':
        case 'x':
            token = token_make_special_number();
            break;

        case '"':
            token = token_make_string('"', '"');
            break;

        case '\'':
            token = token_make_quote();
            break;

        // Whitespace is meaningless to the lexer, except it flips the
        // previous token's whitespace flag for later parser disambiguation.
        case ' ':
        case '\t':
            token = handle_whitespace();
            break;

        case '\n':
            token = token_make_newline();
            break;

        case EOF:
            // End of input. token stays NULL; lex() will stop the loop.
            break;

        default:
            token = read_special_token();
            if(!token){
                compiler_error(lex_process->compiler, "Unexpected token\n");
            }
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

// ============================================================================
// String-backed lexer source. The default lex_process v-table reads from a
// FILE*; this set reads from a `struct buffer*` stored in lex_process->private.
// Used by tokens_build_for_string() so callers can re-lex preprocessor
// expansions and similar in-memory text without round-tripping to disk.
// ============================================================================

char lexer_string_buffer_next_char(struct lex_process* process){
    struct buffer* buf = lex_process_private(process);
    return buffer_read(buf);
}

char lexer_string_buffer_peek_char(struct lex_process* process){
    struct buffer* buf = lex_process_private(process);
    return buffer_peek(buf);
}

void lexer_string_buffer_push_char(struct lex_process* process, char c){
    struct buffer* buf = lex_process_private(process);
    buffer_write(buf, c);
}

struct lex_process_functions lexer_string_buffer_functions = {
    .next_char = lexer_string_buffer_next_char,
    .peek_char = lexer_string_buffer_peek_char,
    .push_char = lexer_string_buffer_push_char,
};

struct lex_process* tokens_build_for_string(struct compile_process* compiler, const char* str){
    struct buffer* buffer = buffer_create();
    buffer_printf(buffer, "%s", str);
    struct lex_process* lp = lex_process_create(compiler, &lexer_string_buffer_functions, buffer);
    if(!lp){
        return 0;
    }
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){
        return 0;
    }
    return lp;
}
