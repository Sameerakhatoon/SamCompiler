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
        if(lex_process->argument_string_buffer){
            buffer_write(lex_process->argument_string_buffer, c);
        }
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
        assert(lex_process->parentheses_buffer);
        tmp_token.between_brackets = buffer_ptr(lex_process->parentheses_buffer);
        if(lex_process->argument_string_buffer){
            tmp_token.between_arguments = buffer_ptr(lex_process->argument_string_buffer);
        }
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

static int lexer_digit_value(char c){
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 99;
}

// An integer constant (C11 6.4.4.1): decimal, octal (a leading 0),
// hexadecimal (0x / 0X) or binary (0b / 0B, a GNU extension), then a u
// and an l / ll suffix in either order. The whole spelling is read
// first, so `08`, `0x`, `0b12` and `10q` are errors instead of a number
// followed by stray tokens; before, `010` was ten, `0xFF` stopped at
// the F, and `5u` was 5 followed by an identifier `u`. The value is
// checked against 64 bits; datatype_for_number_node picks the type.
extern struct lex_process_functions lexer_string_buffer_functions;

// The character after the next one, read nowhere. A file is read
// through stdio, which can push one character back; in-memory text
// (tokens_build_for_string) is looked at directly.
static char lexer_peek_second(void){
    if(lex_process->function == &lexer_string_buffer_functions){
        struct buffer* buf = lex_process_private(lex_process);
        return buf->rindex + 1 < buf->len ? buf->data[buf->rindex + 1] : 0;
    }
    FILE* fp = lex_process->compiler->cfile.fp;
    int first = fgetc(fp);
    int second = fgetc(fp);
    if(second != EOF){
        ungetc(second, fp);
    }
    if(first != EOF){
        ungetc(first, fp);
    }
    return second == EOF ? 0 : (char)second;
}

static struct token* token_make_integer(const char* text);
static struct token* token_make_floating(const char* text);

// A preprocessing number (C11 6.4.8): digits, letters, `_` and `.`,
// and a sign right after an exponent letter (e or E, p or P in hex).
// `prefix` is a `.` already read (`.5`), or 0. The spelling is a
// floating constant when it has a `.` or an exponent; an integer
// otherwise.
static struct token* token_make_number_with_prefix(char prefix){
    struct buffer* buffer = buffer_create();
    if(prefix){
        buffer_write(buffer, prefix);
    }
    bool hex = false;
    bool floating = prefix == '.';
    for(;;){
        char c = peekc();
        int len = buffer->len;
        const char* sofar = buffer_ptr(buffer);
        char last = len ? sofar[len - 1] : 0;
        if(len == 1 && sofar[0] == '0' && (c == 'x' || c == 'X')){
            hex = true;
        }
        bool exponent_sign = (c == '+' || c == '-')
            && (hex ? (last == 'p' || last == 'P') : (last == 'e' || last == 'E'));
        if(!(isalnum(c) || c == '_' || c == '.' || exponent_sign)){
            break;
        }
        if(c == '.' || (hex ? (c == 'p' || c == 'P') : (c == 'e' || c == 'E'))){
            floating = true;
        }
        buffer_write(buffer, nextc());
    }
    buffer_write(buffer, 0x00);
    const char* text = buffer_ptr(buffer);
    return floating ? token_make_floating(text) : token_make_integer(text);
}

static struct token* token_make_number(void){
    return token_make_number_with_prefix(0);
}

// A floating constant (C11 6.4.4.2): decimal or hexadecimal (`0x1.8p3`),
// then f or F for float or nothing for double (l or L, long double, is
// rejected). The value is read by the C library, strtof for
// a float so it is rounded once, and kept as the bits of the double in
// llnum, which holds a float's value exactly too.
static struct token* token_make_floating(const char* text){
    size_t len = strlen(text);
    char* digits = malloc(len + 1);
    strcpy(digits, text);
    int flags = NUMBER_FLAG_FLOATING;
    int number_type = NUMBER_TYPE_DOUBLE;
    char last = len ? digits[len - 1] : 0;
    bool hex = digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X');
    if(last == 'f' || last == 'F'){
        flags |= NUMBER_FLAG_FLOAT;
        number_type = NUMBER_TYPE_FLOAT;
        digits[--len] = 0;
    } else if(last == 'l' || last == 'L'){
        compiler_error(lex_process->compiler, "%s is a long double, which is not supported\n", text);
    }
    if(hex && !strpbrk(digits, "pP")){
        compiler_error(lex_process->compiler, "The hexadecimal floating constant %s needs a p exponent\n", text);
    }
    char* end = 0;
    double value = 0;
    if(flags & NUMBER_FLAG_FLOAT){
        value = strtof(digits, &end);
    } else {
        value = strtod(digits, &end);
    }
    if(len == 0 || *end != 0 || end == digits){
        compiler_error(lex_process->compiler, "Invalid floating constant %s\n", text);
    }
    free(digits);
    unsigned long long bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return token_create(&(struct token){
        .type      = TOKEN_TYPE_NUMBER,
        .llnum     = bits,
        .num.type  = number_type,
        .num.flags = flags,
    });
}

static struct token* token_make_integer(const char* text){
    int base = 10;
    int flags = 0;
    const char* digits = text;
    if(text[0] == '0' && (text[1] == 'x' || text[1] == 'X')){
        base = 16;
        digits = text + 2;
    } else if(text[0] == '0' && (text[1] == 'b' || text[1] == 'B')){
        base = 2;
        digits = text + 2;
    } else if(text[0] == '0' && isdigit(text[1])){
        base = 8;
        digits = text + 1;
    }
    if(base != 10){
        flags |= NUMBER_FLAG_NOT_DECIMAL;
    }

    unsigned long long value = 0;
    const char* p = digits;
    while(*p && lexer_digit_value(*p) < base){
        unsigned long long digit = lexer_digit_value(*p);
        if(value > (~0ULL - digit) / base){
            compiler_error(lex_process->compiler, "The integer constant %s does not fit in 64 bits\n", text);
        }
        value = value * base + digit;
        p++;
    }
    if(isdigit(*p) || (base == 16 && isxdigit(*p))){
        compiler_error(lex_process->compiler, "Invalid digit '%c' in the integer constant %s\n", *p, text);
    }
    if(p == digits){
        compiler_error(lex_process->compiler, "The integer constant %s has no digits\n", text);
    }

    int number_type = NUMBER_TYPE_NORMAL;
    const char* suffix = p;
    {
        bool seen_u = false;
        bool seen_l = false;
        while(*p){
            if((*p == 'u' || *p == 'U') && !seen_u){
                seen_u = true;
                p++;
            } else if((*p == 'l' || *p == 'L') && !seen_l){
                seen_l = true;
                // ll / LL, but not lL
                p += (p[1] == p[0]) ? 2 : 1;
            } else {
                compiler_error(lex_process->compiler, "Invalid suffix \"%s\" on the integer constant %s\n", suffix, text);
            }
        }
        if(seen_u){
            flags |= NUMBER_FLAG_UNSIGNED;
        }
        if(seen_l){
            flags |= NUMBER_FLAG_LONG;
            number_type = NUMBER_TYPE_LONG;
        }
    }

    return token_create(&(struct token){
        .type      = TOKEN_TYPE_NUMBER,
        .llnum     = value,
        .num.type  = number_type,
        .num.flags = flags,
    });
}

// Reads a string literal delimited by start_delim/end_delim. Consumes the
// opening delimiter (asserted), then everything up to but not including
// the closing delimiter. ch9 stubs out escapes; ch182 wires the real
// handler.
// One escape sequence, the backslash already read (C11 6.4.4.4): the
// simple escapes, one to three octal digits, or \x and hex digits. Only
// \n \\ \t \' were known before; anything else became 0 (so "a\"b"
// ended at the a), and \101 was read as decimal.
static char lex_read_escape(void){
    char c = nextc();
    switch(c){
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case 'a':  return '\a';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'v':  return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"':  return '"';
        case '?':  return '?';
    }
    if(c >= '0' && c <= '7'){
        int value = c - '0';
        for(int i = 0; i < 2 && peekc() >= '0' && peekc() <= '7'; i++){
            value = value * 8 + (nextc() - '0');
        }
        if(value > 255){
            compiler_error(lex_process->compiler, "The octal escape \\%o does not fit in a char\n", value);
        }
        return (char)value;
    }
    if(c == 'x'){
        int value = 0;
        int digits = 0;
        while(isxdigit(peekc())){
            value = value * 16 + lexer_digit_value(nextc());
            digits++;
            if(value > 255){
                compiler_error(lex_process->compiler, "The hex escape does not fit in a char\n");
            }
        }
        if(digits == 0){
            compiler_error(lex_process->compiler, "\\x needs at least one hex digit\n");
        }
        return (char)value;
    }
    compiler_error(lex_process->compiler, "Unknown escape sequence \\%c\n", c);
    return 0;
}

static struct token* token_make_string(char start_delim, char end_delim){
    struct buffer* buf = buffer_create();
    assert(nextc() == start_delim);

    char c;
    for(c = nextc(); c != end_delim; c = nextc()){
        if(c == EOF || c == '\n'){
            compiler_error(lex_process->compiler, "Missing the closing %c of a string\n", end_delim);
        }
        if(c == '\\' && end_delim == '"'){
            // ch182: real escape-sequence handling.
            buffer_write(buf, lex_read_escape());
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
// '*' used to be listed too, which lexed `*=` as `*` `=`. A pair that
// is not an operator (`**`, `*(`) is split again by read_op.
static bool op_treated_as_one(char op){
    return op == '('
        || op == '['
        || op == ','
        || op == '.'
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
        || S_EQ(op, "%=") || S_EQ(op, "&=") || S_EQ(op, "|=") || S_EQ(op, "^=")
        || S_EQ(op, "<<=")|| S_EQ(op, ">>=")
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
    // one more char to form a two-char op. `*=` too, and the
    // three-char `<<=` and `>>=`.
    char first = op;
    if(!op_treated_as_one(op) || (op == '*' && peekc() == '=')){
        op = peekc();
        if(is_single_operator(op)){
            buffer_write(buffer, op);
            nextc();
            single_operator = false;
            if((first == '<' || first == '>') && op == first && peekc() == '='){
                buffer_write(buffer, nextc());
            }
        }
    }

    // `<<=` and `>>=` are the only three-character operators besides
    // `...` (which the parser reads as three `.` tokens).
    if(!single_operator && buffer->len == 2 && peekc() == '='){
        const char* two = buffer_ptr(buffer);
        if((two[0] == '<' && two[1] == '<') || (two[0] == '>' && two[1] == '>')){
            buffer_write(buffer, nextc());
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

    // ch220 macro-strings part 2: if the previous token was an
    // identifier or a comma, we're entering a function-call arg
    // list. Track the literal substring per argument in this
    // separate buffer so #x stringification can quote it later.
    struct token* last_token = lexer_last_token();
    if(last_token && (last_token->type == TOKEN_TYPE_IDENTIFIER || token_is_operator(last_token, ","))){
        lex_process->argument_string_buffer = buffer_create();
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
        || S_EQ(str, "union") || S_EQ(str, "enum")
        || S_EQ(str, "_Bool");
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
        || S_EQ(str, "enum")     || S_EQ(str, "_Bool")
        || S_EQ(str, "__ignore_typecheck")
        || S_EQ(str, "return")   || S_EQ(str, "include")
        || S_EQ(str, "sizeof")   || S_EQ(str, "if")
        || S_EQ(str, "else")     || S_EQ(str, "while")
        || S_EQ(str, "for")      || S_EQ(str, "do")
        || S_EQ(str, "break")    || S_EQ(str, "continue")
        || S_EQ(str, "switch")   || S_EQ(str, "case")
        || S_EQ(str, "default")  || S_EQ(str, "goto")
        || S_EQ(str, "typedef")  || S_EQ(str, "const")
        || S_EQ(str, "extern")   || S_EQ(str, "restrict")
        || S_EQ(str, "volatile") || S_EQ(str, "register")
        || S_EQ(str, "inline")   || S_EQ(str, "auto");
}

// '<' is the wart: in `#include <stdio.h>` it opens a string literal, not
// a less-than operator. We disambiguate by peeking at the previous token.
static struct token* token_make_operator_or_string(void){
    char op = peekc();
    if(op == '.' && isdigit(lexer_peek_second())){
        // `.5` is a floating constant, not `.` and 5.
        nextc();
        return token_make_number_with_prefix('.');
    }
    if(op == '<'){
        struct token* last_token = lexer_last_token();
        if(token_is_keyword(last_token, "include")){
            struct token* token = token_make_string('<', '>');
            token->flags |= TOKEN_FLAG_ANGLED_INCLUDE;
            return token;
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
    // ch220 macro-strings part 2: peek first so lex_finish_expression
    // fires before the token is built. That ensures the closing `)`
    // for a macro call's args isn't itself recorded into the
    // argument_string_buffer.
    char c = peekc();
    if(c == ')'){
        lex_finish_expression();
    }
    nextc();
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

    size_t len = strlen(buffer_ptr(buffer));
    if(len > IDENTIFIER_MAX_LENGTH){
        compiler_error(lex_process->compiler, "Identifier %.32s... is %zu characters long, the limit is %i",
            buffer_ptr(buffer), len, IDENTIFIER_MAX_LENGTH);
    }

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
        // the rest of C's simple escapes (the shell prints
        // `\"a b\"` and `\f`); these gave 0, which cut a string short.
        // '\0' in a char literal still comes out 0 below.
        case '"':   co = '"';  break;
        case 'r':   co = '\r'; break;
        case 'f':   co = '\f'; break;
        case 'a':   co = '\a'; break;
        case 'b':   co = '\b'; break;
        case 'v':   co = '\v'; break;
        case '?':   co = '?';  break;
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
        c = lex_read_escape();
    }
    if(nextc() != '\''){
        compiler_error(lex_process->compiler, "You opened a quote ' but did not close it with a ' character");
    }
    // A char constant is an int with the value the char has as a plain
    // char: '\xff' is -1 where char is signed and 255 where it is not
    // (aarch64, riscv64). The byte alone in cval read as 255 everywhere.
    long long value = target_current()->plain_char_unsigned ? (long long)(unsigned char)c : (long long)(signed char)c;
    return token_create(&(struct token){ .type = TOKEN_TYPE_NUMBER, .llnum = value });
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
        // A CRLF file: the \r before each \n is whitespace. It was an
        // unexpected character before.
        case '\r':
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
    process->argument_string_buffer   = 0;
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

// ch226: alternate lex source - read from a string buffer instead of
// the compile_process's FILE*. Used by tokens_join_vector to re-lex
// a freshly concatenated token stream.
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
    struct lex_process* lex_process = lex_process_create(compiler, &lexer_string_buffer_functions, buffer);
    if(!lex_process){
        return NULL;
    }
    if(lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK){
        return NULL;
    }
    return lex_process;
}
