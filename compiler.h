#ifndef SAMCOMPILER_H
#define SAMCOMPILER_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "helpers/arena.h"

// PATH_MAX comes from <limits.h> on POSIX systems; others may not
// define it.
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// String equality with NULL-safe operands. Used everywhere from the lexer
// onwards to compare keyword / operator spellings.
#define S_EQ(str, str2) \
    (str && str2 && (strcmp(str, str2) == 0))

// ch226: assertion-based bailout with an attached message.
#define FAIL_ERR(message) assert(0 == 1 && message)

typedef struct pos pos_t;
typedef struct token token_t;

// Source position attached to every token (and most AST nodes later).
struct pos {
    int         line;
    int         col;
    const char* filename;
};

// ch137: C stack alignment + rounding macro for function prologue.
#define C_STACK_ALIGNMENT 16
#define C_ALIGN(size) ((size) % C_STACK_ALIGNMENT) ? (size) + (C_STACK_ALIGNMENT - ((size) % C_STACK_ALIGNMENT)) : (size)

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

// Number subtype, recorded on TOKEN_TYPE_NUMBER tokens. Distinguishes
// `5837` (NORMAL) from `5837L` (LONG) from `1.5f` (FLOAT) etc.
enum {
    NUMBER_TYPE_NORMAL,
    NUMBER_TYPE_LONG,
    NUMBER_TYPE_FLOAT,
    NUMBER_TYPE_DOUBLE,
};

// What an integer constant's spelling says about its type (C11
// 6.4.4.1): the suffixes, and whether it was written in decimal. Kept
// on the token and the NODE_TYPE_NUMBER node; datatype_for_number_node
// turns them into the constant's type.
enum {
    NUMBER_FLAG_UNSIGNED    = 0b001,   // u or U
    NUMBER_FLAG_LONG        = 0b010,   // l, L, ll or LL
    NUMBER_FLAG_NOT_DECIMAL = 0b100,   // 0x, 0b or a leading 0
    // A folded constant expression: its type is exactly int, unsigned
    // int, long or unsigned long as UNSIGNED and LONG say, and llnum
    // holds the value in two's complement.
    NUMBER_FLAG_TYPED       = 0b1000,
    // A floating constant (`1.5`, `2e3`, `0x1p4`): llnum holds the bits
    // of its value as a double. FLOAT: the f suffix, the type is float.
    NUMBER_FLAG_FLOATING    = 0b10000,
    NUMBER_FLAG_FLOAT       = 0b100000,
};

enum {
    TOKEN_FLAG_IS_CUSTOM_OPERATOR = 0b00000001,
    // The string token of `#include <x.h>`, as opposed to "x.h".
    TOKEN_FLAG_ANGLED_INCLUDE     = 0b00000010,
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

    // Set on TOKEN_TYPE_NUMBER tokens only. Records L / f / d suffixes.
    struct token_number {
        int type;
        // NUMBER_FLAG_*.
        int flags;
    } num;

    // Set on a u"", U"" or L"" TOKEN_TYPE_STRING: the bytes of one code
    // unit (2 or 4) and the units, the terminating 0 not included.
    // unit_size is 0 for a plain or u8"" string, which keeps its bytes
    // in sval.
    struct token_wide {
        int           unit_size;
        unsigned int* units;
        int           count;
        // L"": wchar_t, as opposed to char16_t / char32_t.
        bool          is_wchar;
    } wide;

    // True if there is whitespace between this token and the next token.
    // i.e. for input "* a", whitespace is set on the operator token "*".
    bool whitespace;

    // For tokens inside parens, e.g. "(5+10+20)", the original substring.
    const char* between_brackets;

    // Anything between function call arguments. ABC(hello world)
    const char* between_arguments;
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
    struct buffer*                argument_string_buffer;
    struct lex_process_functions* function;

    // Opaque blob owned by the caller; the lexer never touches it.
    void* private;
};

// Result codes returned from compile_file.
enum {
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS,
};

// Symbol table entry. The resolver builds these out of declaration
// nodes (ch40+); native (built-in) functions get their own type.
enum {
    SYMBOL_TYPE_NODE,
    SYMBOL_TYPE_NATIVE_FUNCTION,
    SYMBOL_TYPE_UNKNOWN,
};

typedef struct symbol symbol_t;
struct symbol {
    const char* name;
    int         type;
    void*       data;
};

// Lexical scope: a stack of entity pointers + parent. The parser
// pushes variables, functions, struct members, etc. into the current
// scope; the resolver later walks back up via parent links.
typedef struct scope scope_t;
struct scope {
    int flags;
    // Vector of void* (pointers to whatever the entity is).
    struct vector* entities;
    // Total bytes this scope's entities occupy (aligned to 16).
    size_t size;
    // Parent in the scope chain; NULL for root.
    struct scope* parent;
};

struct compile_process_input_file {
    FILE*       fp;
    const char* abs_path;
};

struct code_generator;
struct resolver_process;
struct generator;
struct native_function;
struct node;
struct resolver_entity;
struct datatype;

struct generator_entity_address {
    bool        is_stack;
    long        offset;
    const char* address;
    const char* base_address;
};

#define GENERATOR_BEGIN_EXPRESSION(gen)
#define GENERATOR_END_EXPRESSION(gen) gen->end_exp(gen)

typedef void (*ASM_PUSH_PROTOTYPE)(const char* ins, ...);
typedef void (*NATIVE_FUNCTION_CALL)(struct generator* generator, struct native_function* func, struct vector* arguments);
typedef void (*GENERATOR_GENERATE_EXPRESSION)(struct generator* generator, struct node* node, int flags);
typedef void (*GENERATOR_ENTITY_ADDRESS)(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out);
typedef void (*GENERATOR_END_EXPRESSION)(struct generator* generator);
typedef void (*GENERATOR_FUNCTION_RETURN)(struct datatype* dtype, const char* fmt, ...);
// per-target halves of the stdarg natives.
//   va_start_store: the address of the last named parameter, of
//                   `last_size` bytes, is in the backend's address
//                   register; store the address after it (the first
//                   variadic argument) at list_address.
//   va_arg_fetch:   the address of the va_list is pushed; step the
//                   list past one argument of `size` bytes and push
//                   that argument's address through ret.
typedef void (*GENERATOR_VA_START_STORE)(struct generator* generator, const char* list_address, int last_size);
typedef void (*GENERATOR_VA_ARG_FETCH)(struct generator* generator, int size);

struct generator {
    ASM_PUSH_PROTOTYPE            asm_push;
    GENERATOR_GENERATE_EXPRESSION gen_exp;
    GENERATOR_END_EXPRESSION      end_exp;
    GENERATOR_ENTITY_ADDRESS      entity_address;
    GENERATOR_FUNCTION_RETURN     ret;
    GENERATOR_VA_START_STORE      va_start_store;
    GENERATOR_VA_ARG_FETCH        va_arg_fetch;

    struct compile_process* compiler;

    // Private data for the generator
    void* private;
};

struct native_function_callbacks {
    NATIVE_FUNCTION_CALL call;
};

struct native_function {
    const char*                       name;
    struct native_function_callbacks  callbacks;
};

struct symbol* native_create_function(struct compile_process* compiler, const char* name,
                                      struct native_function_callbacks* callbacks);
// the i386 halves of the variable-length array builtins
// (exp64_vla is the 64-bit one).
void codegen_x86_native_vla_save(struct generator* generator, struct native_function* func, struct vector* arguments);
void codegen_x86_native_vla_alloc(struct generator* generator, struct native_function* func, struct vector* arguments);
void codegen_x86_native_vla_restore(struct generator* generator, struct native_function* func, struct vector* arguments);
struct native_function* native_function_get(struct compile_process* compiler, const char* name);

// ch200: preprocessor scaffolding. Definitions, included files, the
// preprocessor itself - all live in their own structs so the
// preprocessor can shuttle state across nested includes.
struct preprocessor;
struct preprocessor_definition;
struct preprocessor_function_argument;
struct preprocessor_included_file;

typedef void (*PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION)(struct preprocessor* preprocessor, struct preprocessor_included_file* included_file);
// ch231: lookup table for static (built-in) includes such as
// stddef-internal.h / stdarg-internal.h.
PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION preprocessor_static_include_handler_for(const char* filename);

enum {
    PREPROCESSOR_DEFINITION_STANDARD,
    PREPROCESSOR_DEFINITION_MACRO_FUNCTION,
    PREPROCESSOR_DEFINITION_NATIVE_CALLBACK,
    PREPROCESSOR_DEFINITION_TYPEDEF,
};

struct preprocessor_function_argument {
    // Tokens for this argument; vector of struct token.
    struct vector* tokens;

    // The same tokens with their macros expanded (C11 6.10.3.1); NULL
    // until a macro call fills it. Operands of # and ## use `tokens`.
    struct vector* expanded;
};

struct preprocessor_function_arguments {
    // Vector of struct preprocessor_function_argument.
    struct vector* arguments;
};

typedef int (*PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE)(struct preprocessor_definition* definition, struct preprocessor_function_arguments* arguments);
typedef struct vector* (*PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE)(struct preprocessor_definition* definition, struct preprocessor_function_arguments* arguments);

struct preprocessor_definition {
    // Standard, macro function, native callback, or typedef.
    int type;

    // Name; e.g. `#define ABC` -> "ABC".
    const char* name;
    union {
        struct standard_preprocessor_definition {
            // Definition value tokens (can be multiple lines).
            // Vector of struct token.
            struct vector* value;

            // Function arguments in order, e.g. ABC(a, b, c).
            // Vector of const char*.
            struct vector* arguments;

            // Set when the list ends in `...`; the last argument is then
            // named __VA_ARGS__ and takes every argument left over.
            bool variadic;
        } standard;

        struct typedef_preprocessor_definition {
            struct vector* value;
        } _typedef;

        struct native_callback_preprocessor_definition {
            PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE evaluate;
            PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE    value;
        } native;
    };

    struct preprocessor* preprocessor;

    // Nonzero while this macro's replacement is being rescanned; its
    // name is then left alone (C11 6.10.3.4p2), so `#define A A` ends.
    int expanding;
};

struct preprocessor_included_file {
    char filename[PATH_MAX];
};

struct preprocessor {
    // Vector of struct preprocessor_definition*.
    struct vector* definitions;
    // Vector of struct preprocessor_node*.
    struct vector* exp_vector;

    struct expressionable* expressionable;

    struct compile_process* compiler;

    // Vector of struct preprocessor_included_file*.
    struct vector* includes;

    // Vector of const char*: absolute paths of files that said
    // `#pragma once`; a later #include of one of them adds nothing.
    struct vector* once_files;
};

struct preprocessor* preprocessor_create(struct compile_process* compiler);
int                  preprocessor_run(struct compile_process* compiler);

// ch227: native preprocessor definitions (e.g. __LINE__) plug in via
// these helpers from preprocessor/native.c.
void                  preprocessor_create_definitions(struct preprocessor* preprocessor);
struct token*         preprocessor_previous_token(struct compile_process* compiler);
struct vector*        preprocessor_build_value_vector_for_integer(int value);
struct preprocessor_definition* preprocessor_definition_create(const char* name, struct vector* value_vec, struct vector* arguments, struct preprocessor* preprocessor);
struct preprocessor_definition* preprocessor_definition_create_native(const char* name,
    PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE evaluate,
    PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE    value,
    struct preprocessor* preprocessor);

// everything that differs between the machines we emit code
// for. One static instance per target lives in target.c; the
// compile_process points at the one it compiles for, and include
// processes inherit it from their parent.
struct target {
    const char* name;

    // Data model. int stays 4 bytes on every target.
    size_t pointer_size;
    size_t long_size;
    // Plain `char` is signed in the i386 and x86-64 psABIs and unsigned
    // in AAPCS64 and the RISC-V psABI.
    bool   plain_char_unsigned;

    // Bytes one push of the codegen expression stack takes.
    size_t stack_slot_size;
    // Bytes between the frame pointer and the first argument slot
    // (saved frame pointer + return address).
    size_t frame_header_size;
    // Bytes one argument occupies at the callee side.
    size_t arg_slot_size;
    size_t stack_align;
    // Arguments passed in registers. 0 means all on the stack.
    int    max_reg_args;

    // the code generator, see struct codegen_backend.
    const struct codegen_backend* backend;

    // printf formats run by the driver: asm_cmd takes (asm, obj),
    // link_cmd takes (obj, out). link_fallback_cmd is used when the
    // first word of link_cmd is not on the PATH.
    const char* asm_cmd;
    const char* link_cmd;
    const char* link_fallback_cmd;
    // Prefix for running a linked binary on the host, "" if native.
    const char* run_cmd;

    // macros the preprocessor predefines for this target,
    // ended by a { 0 } entry.
    const struct target_macro* macros;

    // a SamOS target. Headers come from the SamOS sysroot and
    // pc_includes, never /usr/include, and link_cmd takes (sysroot,
    // obj, out): crt0.o, libsamos.a and user.ld from <sysroot>/lib.
    bool samos;
};

// what a code generator provides. codegen() calls generate;
// rdefault.c formats stack addresses as <frame_register><offset>.
struct codegen_backend {
    const char* name;
    int (*generate)(struct compile_process* process);
    const char* frame_register;
    // Written before a global's name in the addresses rdefault.c
    // formats: "$" on i386, where NASM then reads a name such as `gs`
    // or `eax` as a symbol. 0 for none (exp64 backends mark their own).
    const char* symbol_prefix;
};

struct target_macro {
    const char* name;
    int         value;
};

const struct target* target_x86(void);
const struct target* target_for_name(const char* name);
const struct target* target_current(void);
void                 target_set_current(const struct target* target);
bool                 target_wchar_unsigned(const struct target* target);

struct compile_process {
    // Flags controlling how this file should be compiled.
    int flags;

    // the machine this file is compiled for.
    const struct target* target;

    struct pos                        pos;
    struct compile_process_input_file cfile;

    // Untampered token vector, contains definitions + source code tokens.
    // The preprocessor walks this vector and populates token_vec below.
    struct vector* token_vec_original;

    // Token vector from lexical analysis; populated after lex() runs.
    // The parser will consume it; for now it's just attached so the
    // pipeline owns its outputs.
    struct vector* token_vec;

    // Parser output:
    //   node_vec       - every node ever allocated (parser scratch).
    //   node_tree_vec  - only the top-level AST roots.
    struct vector* node_vec;
    struct vector* node_tree_vec;

    // Lexical-scope chain. ch38+ uses this to remember declared
    // entities (variables, functions, struct members) so later passes
    // can resolve names.
    struct {
        struct scope* root;
        struct scope* current;
    } scope;

    // Symbol tables (ch40+). `table` is the currently-active one;
    // `tables` keeps the stack of saved tables (struct vector*).
    struct {
        struct vector* table;
        struct vector* tables;
    } symbols;

    FILE* ofile;

    // ch108: codegen state - entry/exit label vectors.
    struct code_generator* generator;
    // ch137: resolver, owned by compile_process_create.
    struct resolver_process* resolver;

    // ch200: vector of const char* representing include directories.
    struct vector*         include_dirs;
    struct preprocessor*   preprocessor;

    // A21: the program names float or double or has a floating
    // constant. The x86 backend looks for floating point only then.
    bool                   uses_floating;
};

int                     compile_file(const char* filename, const char* out_filename, int flags);
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags, struct compile_process* parent_process);
void                    compile_process_close_files(struct compile_process* process);
void                    compiler_teardown(void);
// Stops the compile with an error when the recursion over a deeply
// nested program is about to exhaust the stack (E7).
void                    compiler_check_stack(struct compile_process* compiler);

// ch229: include-directory iteration + include compilation.
const char* compiler_include_dir_begin(struct compile_process* process);
const char* compiler_include_dir_next(struct compile_process* process);
void        compiler_setup_default_include_directories(struct vector* include_vec);
// directories the driver adds (-I, the samos sysroot). They come
// before the defaults; a samos target uses only these.
void        compiler_add_include_dir(const char* dir);
// The driver's pc_includes, found next to the compiler binary. When
// set, the defaults are this directory and /usr/include instead of
// ./pc_includes and ../pc_includes, which depended on the current
// directory.
void        compiler_set_own_include_dir(const char* dir);
// -D / -U from the driver, as "#define NAME VALUE" / "#undef NAME"
// lines run through the preprocessor ahead of each top-level source.
void        compiler_add_cli_macro_line(const char* line);
void        compiler_clear_cli_macro_lines(void);
struct compile_process* compile_include(const char* filename, struct compile_process* parent_process);
// `#include <x.h>` (angled) skips the including file's own directory;
// `#include "x.h"` searches it first.
struct compile_process* compile_include_search(const char* filename, struct compile_process* parent_process, bool quoted);

// ch229: helpers.
bool file_exists(const char* filename);
// Canonical absolute path of an existing file into out (PATH_MAX
// bytes); false when it cannot be resolved.
bool compiler_real_path(const char* path, char* out);

void compiler_error(struct compile_process* compiler, const char* msg, ...);
void compiler_warning(struct compile_process* compiler, const char* msg, ...);
void compiler_node_error(struct node* node, const char* msg, ...);
void compiler_node_warning(struct node* node, const char* msg, ...);
void compiler_error_at(struct pos* pos, const char* msg, ...);

// Error recovery. An error normally
// ends the compile; while a recovery point is set (the validator sets
// one per statement), compiler_error and compiler_node_error count the
// error and longjmp there instead, so the next statement is checked
// too. compiler_finish_errors prints the totals ("2 errors and 1
// warning generated.") and exits with -1 when there was an error.
#include <setjmp.h>
void compiler_set_recovery(jmp_buf* point);
int  compiler_error_count(void);
void compiler_finish_errors(void);

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

// ch20: lex a literal string instead of a FILE*. Returns a lex_process
// whose token_vec has already been populated, or NULL on failure.
// Lets the preprocessor (much later) re-lex macro expansions.
struct lex_process* tokens_build_for_string(struct compile_process* compiler, const char* str);

// ============================================================================
// Parser (ch24+)
// ============================================================================

enum {
    PARSE_ALL_OK,
    PARSE_GENERAL_ERROR,
};

// ch243: validator return codes. ALL_OK so compile_file's
// `validate(process) != VALIDATION_ALL_OK` short-circuit catches
// failures uniformly with parse / codegen.
enum {
    VALIDATION_ALL_OK,
    VALIDATION_GENERAL_ERROR,
};

enum {
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_EXPRESSION_PARENTHESES,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_STRING,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_VARIABLE_LIST,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY,
    NODE_TYPE_STATEMENT_RETURN,
    NODE_TYPE_STATEMENT_IF,
    NODE_TYPE_STATEMENT_ELSE,
    NODE_TYPE_STATEMENT_WHILE,
    NODE_TYPE_STATEMENT_DO_WHILE,
    NODE_TYPE_STATEMENT_FOR,
    NODE_TYPE_STATEMENT_BREAK,
    NODE_TYPE_STATEMENT_CONTINUE,
    NODE_TYPE_STATEMENT_SWITCH,
    NODE_TYPE_STATEMENT_CASE,
    NODE_TYPE_STATEMENT_DEFAULT,
    NODE_TYPE_STATEMENT_GOTO,

    NODE_TYPE_UNARY,
    NODE_TYPE_TENARY,
    NODE_TYPE_LABEL,
    NODE_TYPE_STRUCT,
    NODE_TYPE_UNION,
    NODE_TYPE_BRACKET,
    NODE_TYPE_CAST,
    NODE_TYPE_BLANK,
    // `__asm__ volatile("text");`, the text in sval.
    NODE_TYPE_ASM,
    // The brace initializer of a global: its leaves in node->init.
    NODE_TYPE_INITIALIZER,
};

enum {
    NODE_FLAG_INSIDE_EXPRESSION      = 0b00000001,
    NODE_FLAG_IS_FORWARD_DECLARATION = 0b00000010,
    NODE_FLAG_HAS_VARIABLE_COMBINED  = 0b00000100,
    // On a variable: the validator saw a name refer to it.
    NODE_FLAG_IS_REFERENCED          = 0b00001000,
    // A bit-field access the parser has lowered: it stands for the
    // whole unit the bit-field lives in.
    NODE_FLAG_BITFIELD_UNIT          = 0b00010000,
};

// Datatype shape is needed before struct node because var nodes embed
// it by value. Original tables/enums for DATATYPE_* / DATA_TYPE_* /
// DATA_TYPE_EXPECT_* stay below (they're independent of the layout).
struct node;

// Forward decl: array brackets info (defined further down so we don't
// need to reorder more of the file).
struct array_brackets;

struct datatype {
    int flags;
    int type;
    struct datatype* secondary;
    const char* type_str;
    size_t size;
    int    pointer_depth;
    union {
        struct node* struct_node;
        struct node* union_node;
    };

    // ch44: array brackets info, plus the total array size.
    struct array {
        struct array_brackets* brackets;
        size_t                 size;
    } array;

    // With DATATYPE_FLAG_IS_FUNCTION_POINTER: what the function returns.
    struct datatype* function_return;
    // With DATATYPE_FLAG_IS_FUNCTION_POINTER: the parameter types, or
    // NULL when the declarator names none (`int (*f)()`).
    struct function_prototype* function_params;
};

// The parameters of a function pointer's type: a vector of struct
// datatype, and whether `...` follows them.
struct function_prototype {
    struct vector* types;
    bool           variadic;
};

// ch44: array-bracket list. A vector of NODE_TYPE_BRACKET nodes, one
// per `[N]` in a declarator.
struct array_brackets {
    struct vector* n_brackets;
};

typedef struct node node_t;

// One scalar (or struct-valued) part of an initializer: the object
// at `offset` bytes into the variable, of `type`, gets `value`.
struct initializer_leaf {
    size_t          offset;
    struct datatype type;
    struct node*    value;
    // A bit-field: bit_width bits from bit_offset of the unit of `type`
    // at `offset`; 0 for any other object.
    int             bit_offset;
    int             bit_width;
};

// ch115: per-function stack frame model. Every push (local variable,
// saved register, saved BP, pushed value) is recorded as one of these
// so codegen / resolver can compute byte offsets from EBP and assert
// invariants like "the frame is empty at function exit".
struct stack_frame_data {
    struct datatype dtype;
};

struct stack_frame_element {
    int                     flags;
    int                     type;
    const char*             name;
    int                     offset_from_bp;
    struct stack_frame_data data;
};

// one expression-stack push, taken from the current target
// (4 bytes on x86).
#define STACK_PUSH_SIZE (target_current()->stack_slot_size)

enum {
    STACK_FRAME_ELEMENT_TYPE_LOCAL_VARIABLE,
    STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER,
    STACK_FRAME_ELEMENT_TYPE_SAVED_BP,
    STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
    STACK_FRAME_ELEMENT_TYPE_UNKNOWN,
};

enum {
    STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS = 0b00000001,
    STACK_FRAME_ELEMENT_FLAG_ELEMENT_NOT_FOUND = 0b00000010,
    STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL      = 0b00000100,
    STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE      = 0b00001000,
};

void                        stackframe_pop(struct node* func_node);
struct stack_frame_element* stackframe_back(struct node* func_node);
struct stack_frame_element* stackframe_back_expect(struct node* func_node, int expecting_type, const char* expecting_name);
void                        stackframe_pop_expecting(struct node* func_node, int expecting_type, const char* expecting_name);
void                        stackframe_peek_start(struct node* func_node);
struct stack_frame_element* stackframe_peek(struct node* func_node);
void                        stackframe_push(struct node* func_node, struct stack_frame_element* element);
void                        stackframe_sub(struct node* func_node, int type, const char* name, size_t amount);
void                        stackframe_add(struct node* func_node, int type, const char* name, size_t amount);
void                        stackframe_assert_empty(struct node* func_node);

struct node {
    int        type;
    int        flags;
    struct pos pos;

    // Tracks where in the AST this node sits: its owning body and the
    // function it belongs to, both set when the parser binds the node
    // into the tree. NULL while the node is being constructed.
    struct node_binded {
        struct node* owner;
        struct node* function;
    } binded;

    // Composite node payloads. NODE_TYPE_EXPRESSION fills `exp`,
    // NODE_TYPE_VARIABLE fills `var`, etc.
    union {
        struct exp {
            struct node* left;
            struct node* right;
            const char*  op;
        } exp;

        // ch77: NODE_TYPE_EXPRESSION_PARENTHESES wraps one inner exp.
        struct parenthesis {
            struct node* exp;
        } parenthesis;

        // NODE_TYPE_NUMBER: NUMBER_FLAG_* from the constant's spelling.
        // The value itself is in llnum.
        struct node_number {
            int flags;
            // A folded `sizeof expr`: the operand, kept so the validator
            // can check its names (and count them as used). Not generated.
            struct node* sizeof_operand;
        } num;

        struct var {
            struct datatype type;
            // ch53: bytes of padding to insert before this variable
            // to satisfy alignment.
            int             padding;
            // ch58: aligned offset of this variable in its scope.
            int             aoffset;
            const char*     name;
            struct node*    val;
            // A bit-field member: `width` bits from bit
            // `bit_offset` of the unit of the member's type at aoffset.
            // An unnamed one only takes room.
            struct bitfield {
                bool is_bitfield;
                bool unnamed;
                int  width;
                int  bit_offset;
            } bitfield;
        } var;

        struct varlist {
            // Vector of struct node* (the comma-separated peers).
            struct vector* list;
        } var_list;

        struct bracket {
            // `int x[50]` -> .inner is NODE_TYPE_NUMBER(50).
            struct node* inner;
        } bracket;

        // ch48: NODE_TYPE_STRUCT carries name + body + optional
        // attached variable (`struct foo { ... } v;`).
        struct _struct {
            const char*  name;
            struct node* body_n;
            struct node* var;
        } _struct;

        // ch99: NODE_TYPE_UNION - same shape as struct.
        struct _union {
            const char*  name;
            struct node* body_n;
            struct node* var;
        } _union;

        // ch49: NODE_TYPE_BODY - a sequence of statement nodes.
        struct body {
            struct vector* statements;
            size_t         size;
            bool           padded;
            struct node*   largest_var_node;
            // Struct and union bodies: the strictest member alignment.
            size_t         align;
        } body;

        // ch78+: control-flow statements share this payload.
        struct statement {
            // ch81: `return [expr];`. NULL exp means bare `return;`.
            struct return_stmt {
                struct node* exp;
            } return_stmt;
            struct if_stmt {
                struct node* cond_node;
                struct node* body_node;
                // `else if` chain / `else` body - NULL if absent.
                struct node* next;
            } if_stmt;
            // ch79: `else { ... }` body.
            struct else_stmt {
                struct node* body_node;
            } else_stmt;
            // ch82: `for (init; cond; loop) body`.
            struct for_stmt {
                struct node* init_node;
                struct node* cond_node;
                struct node* loop_node;
                struct node* body_node;
            } for_stmt;
            // ch83: `while (exp) body`.
            struct while_stmt {
                struct node* exp_node;
                struct node* body_node;
            } while_stmt;
            // ch84: `do { body } while (exp);` (same payload as while).
            struct do_while_stmt {
                struct node* exp_node;
                struct node* body_node;
            } do_while_stmt;
            // ch85+: switch / case / break / continue / goto / labels.
            struct switch_stmt {
                struct node*   exp;
                struct node*   body;
                // Vector of `struct case_or_default*`.
                struct vector* cases;
                bool           has_default_case;
            } switch_stmt;
            struct _case_stmt {
                struct node* exp;
            } _case;
            struct _goto_stmt {
                struct node* label;
            } _goto;
            struct _label {
                struct node* name;
            } label;
        } stmt;

        // ch90: NODE_TYPE_TENARY payload (`cond ? true : false`).
        struct node_tenary {
            struct node* true_node;
            struct node* false_node;
        } tenary;

        // ch93: NODE_TYPE_CAST payload `(T) operand`.
        struct cast {
            struct datatype dtype;
            struct node*    operand;
        } cast;

        // NODE_TYPE_INITIALIZER: vector of struct initializer_leaf,
        // sorted by offset, none overlapping.
        struct initializer {
            struct vector* leaves;
        } init;

        // ch130: NODE_TYPE_UNARY. For `**p` the op is "*" and the
        // indirection.depth captures the chain length.
        struct unary {
            const char*  op;
            struct node* operand;
            // UNARY_FLAG_*.
            int          flags;
            union {
                struct indirection {
                    int depth;
                } indirection;
            };
        } unary;

        // ch71: NODE_TYPE_FUNCTION payload.
        struct function {
            int             flags;
            struct datatype rtype;
            const char*     name;
            struct function_arguments {
                // Vector of struct node* (NODE_TYPE_VARIABLE).
                struct vector* vector;
                // Bytes to add to EBP to reach the first argument.
                size_t stack_addition;
            } args;
            // NULL for a prototype; otherwise the body.
            struct node* body_n;
            // ch115: per-function stack frame model. The vector holds
            // struct stack_frame_element entries (one per push).
            struct stack_frame {
                struct vector* elements;
            } frame;
            // Total bytes needed for locals.
            size_t stack_size;
        } func;
    };

    // Composite node payloads grow chapter by chapter.

    union {
        char               cval;
        const char*        sval;
        unsigned int       inum;
        unsigned long      lnum;
        unsigned long long llnum;
    };
};

int  parse(struct compile_process* process);
// The position of the token the parser read last.
struct pos parser_position(void);

// ch243: validator entry point. Scaffolded only; later chapters
// hang the actual checks off this.
int  validate(struct compile_process* process);

// ch26: node stack helpers. node_set_vector points them at the parser's
// scratch / root vectors; the rest of the parser uses node_push /
// node_peek* / node_pop to manipulate the work-stack.
void         node_set_vector(struct vector* vec, struct vector* root_vec);
void         node_push(struct node* node);
size_t       node_count(void);
struct node* node_peek_or_null(void);
struct node* node_peek(void);
struct node* node_pop(void);

bool token_is_keyword(struct token* token, const char* value);
bool token_is_identifier(struct token* token);
bool token_is_symbol(struct token* token, char c);
// Preserves the book's "seperator" spelling.
bool token_is_nl_or_comment_or_newline_seperator(struct token* token);
bool keyword_is_datatype(const char* str);
bool token_is_primitive_keyword(struct token* token);

// ch226: serialize a token vector into a string and re-lex it. Used
// by the preprocessor's ## concat to collapse adjacent tokens.
void tokens_join_buffer_write_token(struct buffer* fmt_buf, struct token* token);
struct vector* tokens_join_vector(struct compile_process* compiler, struct vector* token_vec);
struct lex_process* tokens_build_for_string(struct compile_process* compiler, const char* str);
bool token_is_operator(struct token* token, const char* val);
bool is_operator_token(struct token* token);
bool datatype_is_struct_or_union_for_name(const char* name);
void datatype_set_void(struct datatype* dtype);
bool datatype_is_void_no_ptr(struct datatype* dtype);
bool datatype_is_struct_or_union(struct datatype* dtype);

// ch51: size helpers used by codegen + resolver later.
size_t datatype_size_for_array_access(struct datatype* dtype);
size_t datatype_element_size(struct datatype* dtype);
struct datatype datatype_array_decay(struct datatype* dtype);
bool   datatype_is_whole_array(struct datatype* dtype);
size_t datatype_alignment(struct datatype* dtype);

// constant.c: integer constant expressions (C11 6.6).
struct constant {
    // The value as a 64-bit register holds it for `type`.
    unsigned long long value;
    struct datatype    type;
};
bool               constant_fold(struct compile_process* process, struct node* node, struct constant* out);
struct node*       constant_node(struct constant* c);
unsigned long long constant_value_as(struct constant* c, struct datatype* dtype);
// False for a floating constant (`1.5`, `2.0 * 3`).
bool               constant_is_integer(struct constant* c);
// Array-to-pointer decay: `int a[3]` used as a value has type `int*`.
struct datatype datatype_decay(struct datatype* dtype);
// C integer typing (C11 6.3.1, 6.4.4.1): the type of a constant, the
// promoted type of an operand, and how a binary operator runs.
struct datatype datatype_for_number_node(struct node* node);
long long       datatype_canonical_value(unsigned long long value, struct datatype* dtype);
// Floating point: float or double (not a pointer or array of them),
// the type of `size` bytes, and a number node's value as a double.
bool            datatype_is_floating(struct datatype* dtype);
struct datatype datatype_floating(size_t size);
double          number_node_double(struct node* node);
// The bits of `value` as the floating type of `size` bytes stores it.
unsigned long long floating_bits(double value, size_t size);
double          floating_value(unsigned long long bits, size_t size);
struct datatype datatype_promote(struct datatype* dtype);
struct datatype_binary {
    // The type the instruction works in: its width and signedness pick
    // the instruction (idiv or div, setl or setb, sar or shr).
    struct datatype operation;
    // The type of the value pushed afterwards (int for a comparison).
    struct datatype result;
    // Pointer + integer: multiply that operand by the element size.
    size_t          scale_left;
    size_t          scale_right;
    // Pointer - pointer: divide the byte difference by the element size.
    size_t          pointer_difference;
};
void datatype_binary_types(struct datatype* left, struct datatype* right, const char* op, struct datatype_binary* out);
size_t datatype_size_no_ptr(struct datatype* dtype);
size_t datatype_size(struct datatype* dtype);
bool   datatype_is_primitive(struct datatype* dtype);
bool   datatype_is_struct_or_union_non_pointer(struct datatype* dtype);
struct datatype datatype_for_numeric(void);
struct datatype datatype_for_int(void);
struct datatype datatype_for_string(void);
struct datatype* datatype_thats_a_pointer(struct datatype* d1, struct datatype* d2);
struct datatype* datatype_pointer_reduce(struct datatype* datatype, int by);
bool is_logical_operator(const char* op);
bool is_logical_node(struct node* node);

// ch52: size helpers that look at variable nodes (not datatypes).
size_t       variable_size(struct node* var_node);
size_t       variable_size_for_list(struct node* var_list_node);
struct node* variable_node(struct node* node);
bool         variable_node_is_primitive(struct node* node);

// ch53: alignment/padding helpers used by the body sizing logic.
int padding(int val, int to);
int align_value(int val, int to);
int align_value_treat_positive(int val, int to);
int compute_sum_padding(struct vector* vec);

// ch27: take a stack-allocated node, copy it to the heap, push onto the
// parser's node stack, return the new pointer.
struct node* node_create(struct node* _node);

// ch65: symbol -> node accessors.
struct node* node_from_sym(struct symbol* sym);
struct node* node_from_symbol(struct compile_process* process, const char* name);
struct node* struct_node_for_name(struct compile_process* process, const char* name);
struct node* union_node_for_name(struct compile_process* process, const char* name);
void         make_union_node(const char* name, struct node* body_node);
bool         node_is_expression(struct node* node, const char* op);
bool         is_array_node(struct node* node);
bool         is_access_operator(const char* op);
bool         is_access_node(struct node* node);
bool         is_access_node_with_op(struct node* node, const char* op);
bool         is_array_operator(const char* op);
bool         is_parentheses_operator(const char* op);
bool         is_parentheses_node(struct node* node);
bool         is_argument_operator(const char* op);
bool         is_argument_node(struct node* node);
bool         node_valid(struct node* node);
void         datatype_decrement_pointer(struct datatype* dtype);
size_t       array_brackets_count(struct datatype* dtype);
bool         is_unary_operator(const char* op);
bool         op_is_indirection(const char* op);
bool         op_is_address(const char* op);
void         make_unary_node(const char* op, struct node* operand_node);
// ch176: parser-side unary-operand compatibility predicates.
bool         is_parentheses(const char* op);
bool         unary_operand_compatible(struct token* token);
bool         is_node_assignment(struct node* node);
// "++" or "--", prefix or postfix.
bool         is_increment_operator(const char* op);
// The arithmetic an assignment does: "-=" -> "-", "<<=" -> "<<",
// "++" -> "+", "--" -> "-". NULL for plain "=".
const char*  assignment_arithmetic_operator(const char* op);

enum {
    // `x++` / `x--`: the value is the operand before the step.
    UNARY_FLAG_IS_POSTFIX = 0b00000001,
};

// ch104: codegen entry point + status enum. Module 2/3 fills in the
// real instruction-emit work; ch104 just lands the skeleton.
enum {
    CODEGEN_ALL_OK,
    CODEGEN_GENERAL_ERROR,
};

// ch143: compile-process flags driving the post-compile pipeline
// (run NASM + link, or just emit the .o).
enum {
    COMPILE_PROCESS_EXECUTE_NASM     = 0b00000001,
    COMPILE_PROCESS_EXPORT_AS_OBJECT = 0b00000010,
    // -E: stop after the preprocessor and write the expanded tokens
    // as C text instead of asm.
    COMPILE_PROCESS_PREPROCESS_ONLY  = 0b00000100,
};

// ch108: entry / exit "label" book-keeping for break / continue.
struct codegen_entry_point {
    int id;
};

struct codegen_exit_point {
    int id;
};

// ch110: each unique string literal gets registered once and given a
// `str_<N>` label. label is char[] so we can sprintf into it directly.
struct string_table_element {
    const char* str;
    char        label[50];
};

// ch166: moved here from parser.c so codegen can iterate switch cases.
struct parsed_switch_case {
    // The case value, already a constant of the case expression's type.
    long long index;
};

// The label suffix for a case value: the value, with `m` for a minus
// sign so `case -1:` gives a valid label (`case_m1`).
const char* codegen_case_label(long long value);

// How a backend writes initialized data (codegen_emit_initializer).
struct initializer_emitter {
    void        (*value)(size_t size, long long value);
    void        (*symbol)(size_t size, const char* symbol);
    void        (*zero)(size_t count);
    const char* (*string_label)(const char* str);
};
// Writes the bytes of a global whose value is a NODE_TYPE_INITIALIZER:
// each leaf in turn, zeros between and after them.
void codegen_emit_initializer(struct node* var_node, const struct initializer_emitter* emitter);

struct code_generator {
    // ch164: nested switch-statement bookkeeping. `current` is the
    // innermost switch's data; outer switches stack into `swtiches`
    // (book typo preserved).
    struct generator_switch_stmt {
        struct generator_switch_stmt_entity {
            int id;
        } current;
        struct vector* swtiches;     // vector of generator_switch_stmt_entity
    } _switch;

    struct vector* string_table;     // vector of struct string_table_element*
    struct vector* entry_points;     // vector of struct codegen_entry_point*
    struct vector* exit_points;      // vector of struct codegen_exit_point*
    // ch142: response stack used by codegen to communicate result
    // info up through recursive expression emit.
    struct vector* responses;
    // ch187: extra `.data` lines emitted during codegen and flushed
    // right before `.rodata`. Used for things like per-call function
    // pointer slots.
    struct vector* custom_data_section;
};

// ch142: codegen response system. Each call may push a response onto
// the stack via codegen_response_expect; callees pull/acknowledge
// to communicate "I produced this entity" up the call chain.
enum {
    RESPONSE_FLAG_ACKNOWLEDGED      = 0b00000001,
    RESPONSE_FLAG_PUSHED_STRUCTURE  = 0b00000010,
    RESPONSE_FLAG_RESOLVED_ENTITY   = 0b00000100,
    RESPONSE_FLAG_UNARY_GET_ADDRESS = 0b00001000,
};

// ch142: composite flag masks used by the expression generator.
#define EXPRESSION_GEN_MATHABLE (         \
    EXPRESSION_IS_ADDITION |              \
    EXPRESSION_IS_SUBTRACTION |           \
    EXPRESSION_IS_MULTIPLICATION |        \
    EXPRESSION_IS_DIVISION |              \
    EXPRESSION_IS_MODULAS |               \
    EXPRESSION_IS_FUNCTION_CALL |         \
    EXPRESSION_INDIRECTION |              \
    EXPRESSION_GET_ADDRESS |              \
    EXPRESSION_IS_ABOVE |                 \
    EXPRESSION_IS_ABOVE_OR_EQUAL |        \
    EXPRESSION_IS_BELOW |                 \
    EXPRESSION_IS_BELOW_OR_EQUAL |        \
    EXPRESSION_IS_EQUAL |                 \
    EXPRESSION_IS_NOT_EQUAL |             \
    EXPRESSION_LOGICAL_AND |              \
    EXPRESSION_LOGICAL_OR |               \
    EXPRESSION_IN_LOGICAL_EXPRESSION |    \
    EXPRESSION_IS_BITSHIFT_LEFT |         \
    EXPRESSION_IS_BITSHIFT_RIGHT |        \
    EXPRESSION_IS_BITWISE_OR |            \
    EXPRESSION_IS_BITWISE_AND |           \
    EXPRESSION_IS_BITWISE_XOR)

// ch147: EXPRESSION_IN_LOGICAL_EXPRESSION is intentionally NOT in
// this mask - it must propagate down nested logical sub-expressions.
#define EXPRESSION_UNINHERITABLE_FLAGS (                                                 \
    EXPRESSION_FLAG_RIGHT_NODE | EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS |                 \
    EXPRESSION_IS_ADDITION | EXPRESSION_IS_MODULAS | EXPRESSION_IS_SUBTRACTION |         \
    EXPRESSION_IS_MULTIPLICATION | EXPRESSION_IS_DIVISION |                              \
    EXPRESSION_IS_ABOVE | EXPRESSION_IS_ABOVE_OR_EQUAL |                                 \
    EXPRESSION_IS_BELOW | EXPRESSION_IS_BELOW_OR_EQUAL | EXPRESSION_IS_EQUAL |           \
    EXPRESSION_IS_NOT_EQUAL | EXPRESSION_LOGICAL_AND |                                   \
    EXPRESSION_IS_BITSHIFT_LEFT | EXPRESSION_IS_BITSHIFT_RIGHT |                         \
    EXPRESSION_IS_BITWISE_OR | EXPRESSION_IS_BITWISE_AND | EXPRESSION_IS_BITWISE_XOR |   \
    EXPRESSION_IS_ASSIGNMENT | IS_ALONE_STATEMENT)

int                    codegen(struct compile_process* process);
// the backends.
int                    codegen_x86(struct compile_process* process);
int                    codegen_x86_64(struct compile_process* process);
int                    codegen_aarch64(struct compile_process* process);
int                    codegen_riscv64(struct compile_process* process);
struct code_generator* codegenerator_new(struct compile_process* process);

// exp64.c, the expression generator of the 64-bit backends.
// The backend supplies the machine operations below. R0 (register 0)
// is the value register, R1 (register 1) the second operand; every
// value is canonical in 64 bits (signed types sign extended, unsigned
// zero extended). push/pop use the expression stack and its ledger.
enum {
    EXP64_BRANCH_IF_ZERO,
    EXP64_BRANCH_IF_NOT_ZERO,
    EXP64_BRANCH_ALWAYS,
};

// B1: how one argument travels, from exp64_arg_layout, which the
// caller and the callee both use. A value is a run of 8-byte words: a
// scalar one, a struct its size rounded up to 8.
enum {
    EXP64_ARG_WORDS,    // the words themselves, in registers and/or on the stack
    EXP64_ARG_REF,      // one word: the address of a copy the caller makes
};

struct exp64_arg {
    int kind;
    int size;           // bytes of the value
    int words;          // 8-byte words of the value
    // A float or double in a floating-point argument register: reg is
    // that register (xmm, v or fa), reg_words 1.
    bool fp;
    int reg;            // first argument register, or -1
    int reg_words;      // words in registers from reg on
    int stack;          // outgoing stack word of the first word not in a register, or -1
    int copy;           // REF: byte offset of the copy in the outgoing area
    int slot;           // caller: expression-stack slot of word 0, counted from the top
};

// A call as exp64 hands it to the backend. The arguments' words are on
// the expression stack, each value's first word on top of its others,
// the first argument nearest the top; temporaries an argument left
// behind (the room of a struct a nested call returned) may sit between
// them, which `slot` accounts for.
struct exp64_call {
    const char*       symbol;           // the callee, or NULL: its address is under the arguments
    int               count;
    struct exp64_arg* args;
    int               slots;            // expression-stack slots above the callee's address (or the room)
    int               stack_words;      // outgoing stack words
    int               area;             // bytes of the outgoing area: the stack words, then the REF copies
    // simple: the slots, top down, are the arguments' words in order;
    // the first reg_words go to consecutive registers from first_reg
    // and the rest are the stack words, in order. Plain scalar calls.
    bool              simple;
    int               reg_words;
    int               first_reg;
    int               struct_size;      // room for a returned struct under everything (8-aligned), or 0
    bool              struct_in_regs;   // it comes back in the two return registers, not through memory
    int               fp_regs;          // floating-point argument registers used (al on x86_64)
    int               float_return;     // a float (4) or double (8) result: from xmm0, d0/s0 or fa0 to R0; else 0
};

struct exp64_ops {
    void (*push)(struct datatype* dtype);                       // push R0
    // Push the struct of `size` bytes at R0 as 8-byte words, the last
    // first, reading no byte past it.
    void (*push_struct)(int size);
    void (*pop)(int reg);                                       // pop into R0 or R1
    void (*mov_r1)(void);                                       // R1 = R0
    void (*swap)(void);                                         // R0 <-> R1
    // Jump to label unless R0 op R1 holds; with op NULL, if R0 is 0.
    void (*branch_false)(const char* op, bool is_signed, const char* label);
    void (*imm)(int reg, long long value);
    void (*frame_address)(int offset);                          // R0 = frame + offset
    void (*symbol_address)(const char* name);                   // R0 = &name
    void (*string_address)(const char* str);                    // R0 = the literal's address
    void (*add_imm)(long long value);                           // R0 += value
    void (*mul_imm)(int reg, long long value);                  // reg *= value
    void (*load)(int size, bool is_signed);                     // R0 = *R0
    void (*store)(int size);                                    // *R1 = R0
    void (*copy)(int size);                                     // size bytes from [R1] to [R0]
    void (*zero_frame)(int offset, int size);                   // zero a local
    void (*extend)(int reg, int size, bool is_signed);          // reg = canonical low `size` bytes
    void (*binop)(const char* op, int size, bool is_signed);    // R0 = R0 op R1 on size bytes
    void (*compare)(const char* op, bool is_signed);            // R0 = R0 op R1, 64 bits, 0 or 1
    void (*unop)(const char* op, int size);                     // - ~ !
    void (*branch)(int label, int when);
    void (*label)(int label);
    // Room for a struct a call returns, on the expression stack.
    void (*reserve)(int size);
    // Release the top `slots` slots of the expression stack.
    void (*drop)(int slots);
    // The call (struct exp64_call). With struct_size, the result goes
    // to the reserved room under everything, through the hidden
    // pointer or from the return registers, and R0 is its address
    // after the call.
    void (*call)(const struct exp64_call* call);
    void (*asm_text)(const char* text);                         // inline asm
    // Floating point. A float or double value is its bits in R0: all 64
    // of a double, the low 32 of a float (the bits above do not count).
    // size is 4 for float, 8 for double. Only fbinop and fcompare read
    // R1; the others keep it.
    void (*fbinop)(const char* op, int size);                   // R0 = R0 op R1: + - * /
    void (*fcompare)(const char* op, int size);                 // R0 = R0 op R1, 0 or 1 (false if unordered, but !=)
    // R0, a canonical integer, as a float of `size`; from_u64: it is
    // an unsigned 64-bit value, else it converts as a signed one.
    void (*int_to_float)(int size, bool from_u64);
    // R0, a float of `size`, truncated to a signed 64-bit integer, or
    // to an unsigned one with to_u64.
    void (*float_to_int)(int size, bool to_u64);
    void (*float_resize)(int from, int to);                     // float <-> double
    // R0 = the stack pointer, and the stack pointer = R0.
    void (*sp_get)(void);
    void (*sp_set)(void);
};

// The floating-point argument registers every 64-bit ABI here has:
// xmm0-7, v0-7, fa0-7.
#define EXP64_FP_ARG_REGISTERS 8

void            exp64_parameters(struct node* function, int register_home, int stack_home, int fp_home);
int             exp64_fp_parameter_registers(struct node* function);
struct datatype exp64_return_value(struct node* node, struct datatype* rtype);
void            exp64_test(struct node* node);
bool            exp64_returns_in_memory(struct datatype* dtype);

void            exp64_begin(const struct exp64_ops* ops, struct compile_process* process);
void            exp64_set_function(struct node* function);
bool            exp64_output_muted(void);
struct datatype exp64_generate(struct node* node);
struct datatype exp64_push(struct node* node);
struct datatype exp64_typeof(struct node* node);
void            exp64_initialize_local(struct node* var);
void            exp64_condition(struct node* node, const char* false_label);
struct node*    exp64_static_local(struct resolver_entity* entity, struct node* var);

// ch28: build a NODE_TYPE_EXPRESSION linking left + op + right.
void         make_exp_node(struct node* left_node, struct node* right_node, const char* op);
// ch77: wrap an inner expression in NODE_TYPE_EXPRESSION_PARENTHESES.
void         make_exp_parentheses_node(struct node* exp_node);
bool         node_is_expression_or_parentheses(struct node* node);
bool         node_is_value_type(struct node* node);
// ch44: build a NODE_TYPE_BRACKET wrapping a single inner expression.
void         make_bracket_node(struct node* inner);
// ch49: build a NODE_TYPE_BODY around a statement vector.
void         make_body_node(struct vector* body_vec, size_t size, bool padded, struct node* largest_var_node);
// ch64: build a NODE_TYPE_STRUCT with optional body (NULL = forward decl).
void         make_struct_node(const char* name, struct node* body_node);
// ch72: build a NODE_TYPE_FUNCTION (body_node NULL = prototype).
struct node* make_function_node(struct datatype* ret_type, const char* name, struct vector* arguments, struct node* body_node);
// ch78: build a NODE_TYPE_STATEMENT_IF.
void         make_if_node(struct node* cond_node, struct node* body_node, struct node* next_node);
// ch79: build a NODE_TYPE_STATEMENT_ELSE.
void         make_else_node(struct node* body_node);
// ch81: build a NODE_TYPE_STATEMENT_RETURN.
void         make_return_node(struct node* exp_node);
// ch82: build a NODE_TYPE_STATEMENT_FOR.
void         make_for_node(struct node* init_node, struct node* cond_node, struct node* loop_node, struct node* body_node);
// ch83/84: while + do-while.
void         make_while_node(struct node* exp_node, struct node* body_node);
void         make_do_while_node(struct node* body_node, struct node* exp_node);
// ch85+: switch / case / continue / break / goto / label.
void         make_switch_node(struct node* exp_node, struct node* body_node, struct vector* cases, bool has_default_case);
void         make_case_node(struct node* exp_node);
void         make_continue_node(void);
void         make_break_node(void);
void         make_goto_node(struct node* label_node);
void         make_label_node(struct node* name_node);
void         make_default_node(void);
// ch90: build a NODE_TYPE_TENARY.
void         make_tenary_node(struct node* true_node, struct node* false_node);
// ch93: build a NODE_TYPE_CAST.
void         make_cast_node(struct datatype* dtype, struct node* operand_node);

bool         node_is_expressionable(struct node* node);
struct node* node_peek_expressionable_or_null(void);
bool         node_is_struct_or_union_variable(struct node* node);
struct node* variable_struct_or_union_body_node(struct node* node);

// ch119: array index helpers used by the resolver to compute offsets
// for array-bracket entities. `array_multiplier` walks brackets after
// `index` to compute the chunk-size in elements; `array_offset`
// multiplies that by the element byte size.
int array_multiplier(struct datatype* dtype, int index, int index_value);
int array_offset(struct datatype* dtype, int index, int index_value);
// ch62: pass through for VARIABLE_LIST; else unwrap to underlying var.
struct node* variable_node_or_list(struct node* node);

// initializer.c: the assembler operand of a scalar global's value.
// Brace initializers are leaves (NODE_TYPE_INITIALIZER, written by
// codegen_emit_initializer).
void           initializer_value_asm(struct node* value, const char* (*register_string)(const char* str), char* out, size_t len);
long long      initializer_number_bits(struct node* value);

// ch44: array-bracket helpers.
struct array_brackets* array_brackets_new(void);
void                   array_brackets_free(struct array_brackets* brackets);
void                   array_brackets_add(struct array_brackets* brackets, struct node* bracket_node);
struct vector*         array_brackets_node_vector(struct array_brackets* brackets);
size_t                 array_brackets_calculate_size_from_index(struct datatype* dtype, struct array_brackets* brackets, int index);
size_t                 array_brackets_calculate_size(struct datatype* dtype, struct array_brackets* brackets);
int                    array_total_indexes(struct datatype* dtype);

// Scope chain (ch38+). The parser creates a root scope at the start
// of a parse, pushes/pops nested scopes as it enters/leaves
// functions and blocks, and pushes entity pointers as it sees
// declarations.
struct scope* scope_create_root(struct compile_process* process);
void          scope_free_root(struct compile_process* process);
struct scope* scope_new(struct compile_process* process, int flags);
void          scope_iteration_start(struct scope* scope);
void          scope_iteration_end(struct scope* scope);
void*         scope_iterate_back(struct scope* scope);
void*         scope_last_entity_at_scope(struct scope* scope);
void*         scope_last_entity_from_scope_stop_at(struct scope* scope, struct scope* stop_scope);
void*         scope_last_entity_stop_at(struct compile_process* process, struct scope* stop_scope);
void*         scope_last_entity(struct compile_process* process);
void          scope_push(struct compile_process* process, void* ptr, size_t elem_size);
void          scope_finish(struct compile_process* process);
struct scope* scope_current(struct compile_process* process);

// Symbol resolver (ch40+). The parser feeds declaration nodes in; the
// resolver indexes them by name.
void           symresolver_initialize(struct compile_process* process);
void           symresolver_new_table(struct compile_process* process);
void           symresolver_end_table(struct compile_process* process);
struct symbol* symresolver_get_symbol(struct compile_process* process, const char* name);
struct symbol* symresolver_get_symbol_any(struct compile_process* process, const char* name);
struct symbol* symresolver_register_symbol(struct compile_process* process, const char* sym_name, int type, void* data);
struct symbol* symresolver_get_symbol_for_native_function(struct compile_process* process, const char* name);
struct symbol* symresolver_register_symbol(struct compile_process* process, const char* sym_name, int type, void* data);
void           symresolver_free_all(void);

// ch74: returns the function's stack_addition (bytes between EBP and
// the first argument; typically 8 = saved EBP + return EIP).
size_t function_node_argument_stack_addition(struct node* node);
long   arithmetic(struct compile_process* compiler, long left_operand, long right_operand, const char* op, bool* success);
struct node*   symresolver_node(struct symbol* sym);
void           symresolver_build_for_node(struct compile_process* process, struct node* node);

// ============================================================================
// Resolver (ch117+) - shared types
// ============================================================================

enum {
    RESOLVER_ENTITY_FLAG_IS_STACK                = 0b00000001,
    RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY = 0b00000010,
    RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY = 0b00000100,
    RESOLVER_ENTITY_FLAG_DO_INDIRECTION          = 0b00001000,
    RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET         = 0b00010000,
    RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY = 0b00100000,
    RESOLVER_ENTITY_FLAG_WAS_CASTED              = 0b01000000,
    RESOLVER_ENTITY_FLAG_USES_ARRAY_BRACKETS     = 0b10000000,
};

enum {
    RESOLVER_ENTITY_TYPE_VARIABLE,
    RESOLVER_ENTITY_TYPE_FUNCTION,
    RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION,
    RESOLVER_ENTITY_TYPE_STRUCTURE,
    RESOLVER_ENTITY_TYPE_FUNCTION_CALL,
    RESOLVER_ENTITY_TYPE_ARRAY_BRACKET,
    RESOLVER_ENTITY_TYPE_RULE,
    RESOLVER_ENTITY_TYPE_GENERAL,
    RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS,
    RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION,
    RESOLVER_ENTITY_TYPE_UNSUPPORTED,
    RESOLVER_ENTITY_TYPE_CAST,
};

enum {
    RESOLVER_SCOPE_FLAG_IS_STACK = 0b00000001,
};

struct resolver_result;
struct resolver_process;
struct resolver_scope;
struct resolver_entity;

typedef void*                  (*RESOLVER_NEW_ARRAY_BRACKET_ENTITY)(struct resolver_result* result, struct node* array_entity_node);
typedef void                   (*RESOLVER_DELETE_SCOPE)(struct resolver_scope* scope);
typedef void                   (*RESOLVER_DELETE_ENTITY)(struct resolver_entity* entity);
typedef struct resolver_entity*(*RESOLVER_MERGE_ENTITIES)(struct resolver_process* process, struct resolver_result* result, struct resolver_entity* left_entity, struct resolver_entity* right_entity);
typedef void*                  (*RESOLVER_MAKE_PRIVATE)(struct resolver_entity* entity, struct node* node, int offset, struct resolver_scope* scope);
typedef void                   (*RESOLVER_SET_RESULT_BASE)(struct resolver_result* result, struct resolver_entity* base_entity);

struct resolver_callbacks {
    RESOLVER_NEW_ARRAY_BRACKET_ENTITY new_array_entity;
    RESOLVER_DELETE_SCOPE             delete_scope;
    RESOLVER_DELETE_ENTITY            delete_entity;
    RESOLVER_MERGE_ENTITIES           merge_entities;
    RESOLVER_MAKE_PRIVATE             make_private;
    RESOLVER_SET_RESULT_BASE          set_result_base;
};

struct resolver_process {
    struct resolver_scopes {
        struct resolver_scope* root;
        struct resolver_scope* current;
    } scope;

    // ch118: renamed from `process` to `compiler` so it doesn't shadow
    // the resolver-process-local in helpers.
    struct compile_process*   compiler;
    struct resolver_callbacks callbacks;
};

// ch118: resolver C API declarations.
bool                     resolver_result_failed(struct resolver_result* result);
bool                     resolver_result_ok(struct resolver_result* result);
bool                     resolver_result_finished(struct resolver_result* result);
struct resolver_entity*  resolver_result_entity_root(struct resolver_result* result);
struct resolver_entity*  resolver_result_entity_next(struct resolver_entity* entity);
bool                     resolver_entity_is_pointer_array_element(struct resolver_entity* entity);
bool                     resolver_entity_is_struct_element(struct resolver_entity* entity);
struct resolver_entity*  resolver_entity_clone(struct resolver_entity* entity);
struct resolver_entity*  resolver_result_entity(struct resolver_result* result);
struct resolver_result*  resolver_new_result(struct resolver_process* process);
void                     resolver_result_free(struct resolver_result* result);
struct resolver_scope*   resolver_process_scope_current(struct resolver_process* process);
void                     resolver_runtime_needed(struct resolver_result* result, struct resolver_entity* last_entity);
void                     resolver_result_entity_push(struct resolver_result* result, struct resolver_entity* entity);
struct resolver_entity*  resolver_result_peek(struct resolver_result* result);
struct resolver_entity*  resolver_result_peek_ignore_rule_entity(struct resolver_result* result);
struct resolver_entity*  resolver_result_pop(struct resolver_result* result);
struct vector*           resolver_array_data_vec(struct resolver_result* result);
struct compile_process*  resolver_compiler(struct resolver_process* process);
struct resolver_scope*   resolver_scope_current(struct resolver_process* process);
struct resolver_scope*   resolver_scope_root(struct resolver_process* process);
struct resolver_scope*   resolver_new_scope_create(void);
struct resolver_scope*   resolver_new_scope(struct resolver_process* resolver, void* private, int flags);
void                     resolver_finish_scope(struct resolver_process* resolver);
void                     resolver_free_lookup(void);
struct resolver_process* resolver_new_process(struct compile_process* compiler, struct resolver_callbacks* callbacks);
struct resolver_entity*  resolver_create_new_entity(struct resolver_result* result, int type, void* private);
struct resolver_entity*  resolver_create_new_entity_for_unsupported_node(struct resolver_result* result, struct node* node);
struct resolver_entity*  resolver_create_new_entity_for_array_bracket(struct resolver_result* result, struct resolver_process* process, struct node* node, struct node* array_index_node, int index, struct datatype* dtype, void* private, struct resolver_scope* scope);
struct resolver_entity*  resolver_create_new_entity_for_merged_array_bracket(struct resolver_result* result, struct resolver_process* process, struct node* node, struct node* array_index_node, int index, struct datatype* dtype, void* private, struct resolver_scope* scope);
struct resolver_entity*  resolver_create_new_unknown_entity(struct resolver_process* process, struct resolver_result* result, struct datatype* dtype, struct node* node, struct resolver_scope* scope, int offset);
struct resolver_entity*  resolver_create_new_unary_indirection_entity(struct resolver_process* process, struct resolver_result* result, struct node* node, int indirection_depth);
struct resolver_entity*  resolver_create_new_unary_get_address_entity(struct resolver_process* process, struct resolver_result* result, struct datatype* dtype, struct node* node, struct resolver_scope* scope, int offset);
struct resolver_entity*  resolver_create_new_cast_entity(struct resolver_process* process, struct resolver_scope* scope, struct datatype* cast_dtype);
struct resolver_entity*  resolver_create_new_entity_for_var_node_custom_scope(struct resolver_process* process, struct node* var_node, void* private, struct resolver_scope* scope, int offset);
struct resolver_entity*  resolver_create_new_entity_for_var_node(struct resolver_process* process, struct node* var_node, void* private, int offset);
struct resolver_entity*  resolver_new_entity_for_var_node_no_push(struct resolver_process* process, struct node* var_node, void* private, int offset, struct resolver_scope* scope);
struct resolver_entity*  resolver_new_entity_for_var_node(struct resolver_process* process, struct node* var_node, void* private, int offset);

// resolver_new_entity_for_rule moved below struct resolver_entity
// because the nested rule struct is only complete after the
// enclosing definition is parsed.
struct resolver_entity*  resolver_make_entity(struct resolver_process* process, struct resolver_result* result, struct datatype* custom_dtype, struct node* node, struct resolver_entity* guided_entity, struct resolver_scope* scope);
struct resolver_entity*  resolver_create_new_entity_for_function_call(struct resolver_result* result, struct resolver_process* process, struct resolver_entity* left_operand_entity, void* private);
struct resolver_entity*  resolver_register_function(struct resolver_process* process, struct node* func_node, void* private);

// ch136: default resolver implementation public surface.
struct resolver_default_entity_data* resolver_default_entity_private(struct resolver_entity* entity);
struct resolver_default_scope_data*  resolver_default_scope_private(struct resolver_scope* scope);
char*                                resolver_default_stack_asm_address(int stack_offset, char* out);
void                                 resolver_default_global_asm_address(const char* name, int offset, char* address_out);
void*                                resolver_default_make_private(struct resolver_entity* entity, struct node* node, int offset, struct resolver_scope* scope);
void                                 resolver_default_set_result_base(struct resolver_result* result, struct resolver_entity* base_entity);
struct resolver_default_entity_data* resolver_default_new_entity_data_for_var_node(struct node* var_node, int offset, int flags);
struct resolver_default_entity_data* resolver_default_new_entity_data_for_array_bracket(struct node* breacket_node);
struct resolver_default_entity_data* resolver_default_new_entity_data_for_function(struct node* func_node, int flags);
struct resolver_entity*              resolver_default_new_scope_entity(struct resolver_process* resolver, struct node* var_node, int offset, int flags);
struct resolver_entity*              resolver_default_register_function(struct resolver_process* resolver, struct node* func_node, int flags);
void                                 resolver_default_new_scope(struct resolver_process* resolver, int flags);
void                                 resolver_default_finish_scope(struct resolver_process* resolver);
void*                                resolver_default_new_array_entity(struct resolver_result* result, struct node* array_entity_node);
void                                 resolver_default_delete_entity(struct resolver_entity* entity);
void                                 resolver_default_delete_scope(struct resolver_scope* scope);
struct resolver_entity*              resolver_default_merge_entities(struct resolver_process* process, struct resolver_result* result, struct resolver_entity* left_entity, struct resolver_entity* right_entity);
struct resolver_process*             resolver_default_new_process(struct compile_process* compiler);
struct resolver_entity*  resolver_get_entity_in_scope_with_entity_type(struct resolver_result* result, struct resolver_process* resolver, struct resolver_scope* scope, const char* entity_name, int entity_type);

// ch124: struct_offset + helpers (declared earlier so ch122 compiled).
int                      struct_offset(struct compile_process* compiler, const char* struct_name, const char* var_name, struct node** out_node_out, int last_pos, int flags);
struct node*             body_largest_variable_node(struct node* body_node);
struct node*             variable_struct_or_union_largest_variable_node(struct node* var_node);
bool                     node_is_struct_or_union(struct node* node);
// The member called `name` among a struct body's statements, or NULL.
struct node*             struct_member_named(struct vector* statements, const char* name);

// ch124: struct_offset flags.
enum {
    STRUCT_ACCESS_BACKWARDS       = 0b00000001,
    STRUCT_STOP_AT_POINTER_ACCESS = 0b00000010,
};

// ch137/138: codegen history flags. IS_ALONE_STATEMENT shipped in
// ch137; ch138 grows the enum to cover expression-context flags.
enum {
    EXPRESSION_FLAG_RIGHT_NODE                = 0b0000000000000001,
    EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS     = 0b0000000000000010,
    EXPRESSION_IN_FUNCTION_CALL_LEFT_OPERAND  = 0b0000000000000100,
    EXPRESSION_IS_ADDITION                    = 0b0000000000001000,
    EXPRESSION_IS_SUBTRACTION                 = 0b0000000000010000,
    EXPRESSION_IS_MULTIPLICATION              = 0b0000000000100000,
    EXPRESSION_IS_DIVISION                    = 0b0000000001000000,
    EXPRESSION_IS_FUNCTION_CALL               = 0b0000000010000000,
    EXPRESSION_INDIRECTION                    = 0b0000000100000000,
    EXPRESSION_GET_ADDRESS                    = 0b0000001000000000,
    EXPRESSION_IS_ABOVE                       = 0b0000010000000000,
    EXPRESSION_IS_ABOVE_OR_EQUAL              = 0b0000100000000000,
    EXPRESSION_IS_BELOW                       = 0b0001000000000000,
    EXPRESSION_IS_BELOW_OR_EQUAL              = 0b0010000000000000,
    EXPRESSION_IS_EQUAL                       = 0b0100000000000000,
    EXPRESSION_IS_NOT_EQUAL                   = 0b1000000000000000,
    EXPRESSION_LOGICAL_AND                    = 0b10000000000000000,
    EXPRESSION_LOGICAL_OR                     = 0b100000000000000000,
    EXPRESSION_IN_LOGICAL_EXPRESSION          = 0b1000000000000000000,
    EXPRESSION_IS_BITSHIFT_LEFT               = 0b10000000000000000000,
    EXPRESSION_IS_BITSHIFT_RIGHT              = 0b100000000000000000000,
    EXPRESSION_IS_BITWISE_OR                  = 0b1000000000000000000000,
    EXPRESSION_IS_BITWISE_AND                 = 0b10000000000000000000000,
    EXPRESSION_IS_BITWISE_XOR                 = 0b100000000000000000000000,
    EXPRESSION_IS_NOT_ROOT_NODE               = 0b1000000000000000000000000,
    EXPRESSION_IS_ASSIGNMENT                  = 0b10000000000000000000000000,
    IS_ALONE_STATEMENT                        = 0b100000000000000000000000000,
    EXPRESSION_IS_UNARY                       = 0b1000000000000000000000000000,
    IS_STATEMENT_RETURN                       = 0b10000000000000000000000000000,
    IS_RIGHT_OPERAND_OF_ASSIGNMENT            = 0b100000000000000000000000000000,
    IS_LEFT_OPERAND_OF_ASSIGNMENT             = 0b1000000000000000000000000000000,
    EXPRESSION_IS_MODULAS                     = 0b10000000000000000000000000000000,
};

bool          function_node_is_prototype(struct node* node);
bool          function_defined_in_tree(struct compile_process* process, const char* name);
bool          variable_defined_in_tree(struct compile_process* process, const char* name);
void          node_defined_forget(void);
bool          symbol_defined_in_tree(struct compile_process* process, const char* name);
size_t        function_node_stack_size(struct node* node);
struct vector* function_node_argument_vec(struct node* node);

struct resolver_array_data {
    // Vector of struct resolver_entity*.
    struct vector* array_entities;
};

// ch136: default resolver impl - private data shapes + flags.
enum {
    RESOLVER_DEFAULT_ENTITY_TYPE_STACK,
    RESOLVER_DEFAULT_ENTITY_TYPE_SYMBOL,
};

enum {
    RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK = 0b00000001,
    // B1: a struct parameter passed by reference: the slot holds its
    // address (exp64_parameters).
    RESOLVER_DEFAULT_ENTITY_FLAG_IS_REFERENCE   = 0b00000010,
};

enum {
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_VARIABLE,
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_FUNCTION,
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_ARRAY_BRACKET,
};

// The lexer rejects longer identifiers, so a name plus "+offset" always
// fits the resolver's address buffers below. They were 60 bytes and a
// 60-character name overflowed them.
#define IDENTIFIER_MAX_LENGTH 255
#define RESOLVER_ADDRESS_MAX  (IDENTIFIER_MAX_LENGTH + 64)

struct resolver_default_entity_data {
    int  type;
    char address[RESOLVER_ADDRESS_MAX];        // "[ebp-4]" / "[var_name+4]"
    char base_address[RESOLVER_ADDRESS_MAX];   // "ebp" / "var_name"
    int  offset;
    int  flags;
};

struct resolver_default_scope_data {
    int flags;
};

enum {
    RESOLVER_RESULT_FLAG_FAILED                              = 0b00000001,
    RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH       = 0b00000010,
    RESOLVER_RESULT_FLAG_PROCESSING_ARRAY_ENTITIES           = 0b00000100,
    RESOLVER_RESULT_FLAG_HAS_POINTER_ARRAY_ACCESS            = 0b00001000,
    RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX            = 0b00010000,
    RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE             = 0b00100000,
    RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE = 0b01000000,
    RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS                    = 0b10000000,
};

struct resolver_result {
    struct resolver_entity*    first_entity_const;
    struct resolver_entity*    identifier;
    struct resolver_entity*    last_struct_union_entity;
    struct resolver_array_data array_data;
    struct resolver_entity*    entity;
    struct resolver_entity*    last_entity;
    int                        flags;
    size_t                     count;

    struct resolver_result_base {
        char address[RESOLVER_ADDRESS_MAX];        // "[ebp-4]" / "[name+4]"
        char base_address[RESOLVER_ADDRESS_MAX];   // "EBP"     / "global_variable_name"
        int  offset;
    } base;
};

struct resolver_scope {
    int                    flags;
    struct vector*         entities;
    struct resolver_scope* next;
    struct resolver_scope* prev;
    void*                  private;
};

struct resolver_entity {
    int          type;
    int          flags;
    const char*  name;
    int          offset;
    struct node* node;

    union {
        struct resolver_entity_var_data {
            struct datatype dtype;
            struct resolver_array_runtime_ {
                struct datatype dtype;
                struct node*    index_node;
                int             multiplier;
            } array_runtime;
        } var_data;

        struct resolver_array {
            // ch119 dropped `multiplier` here; array_offset
            // recomputes it from the bracket vector instead.
            struct datatype dtype;
            struct node*    array_index_node;
            int             index;
        } array;

        struct resolver_entity_function_call_data {
            struct vector* arguments;   // vector of struct node*
            size_t         stack_size;
            // The callee is a function pointer object (a variable, array
            // element or member): the access chain leaves its address,
            // and the call loads the pointer stored there.
            bool           through_pointer;
            // The callee is the pointer another call returned
            // (`pick(2)(4, 5)`): it is already the value.
            bool           callee_is_value;
        } func_call_data;

        struct resolver_entity_rule {
            struct resolver_entity_rule_left  { int flags; } left;
            struct resolver_entity_rule_right { int flags; } right;
        } rule;

        struct resolver_indirection {
            int depth;
        } indirection;

        // ch237: NATIVE_FUNCTION entities carry the symbol they resolved
        // from so codegen can chase back to the registered callbacks.
        struct resolver_native_function {
            struct symbol* symbol;
        } native_func;
    };

    struct entity_last_resolve {
        struct node* referencing_node;
    } last_resolve;

    struct datatype          dtype;
    struct resolver_scope*   scope;
    struct resolver_result*  result;
    // ch132: renamed from `process` so it doesn't shadow the local
    // resolver-process parameter in helpers.
    struct resolver_process* resolver;
    void*                    private;
    struct resolver_entity*  next;
    struct resolver_entity*  prev;
};

// ch122: declared here so struct resolver_entity_rule is fully visible.
void resolver_new_entity_for_rule(struct resolver_process* process, struct resolver_result* result, struct resolver_entity_rule* rule);

// ch125: lookup helpers (entity_type = -1 means any).
struct resolver_entity* resolver_get_entity_for_type(struct resolver_result* result, struct resolver_process* resolver, const char* entity_name, int entity_type);
struct resolver_entity* resolver_get_entity(struct resolver_result* result, struct resolver_process* resolver, const char* entity_name);
struct resolver_entity* resolver_get_entity_in_scope(struct resolver_result* result, struct resolver_process* resolver, struct resolver_scope* scope, const char* entity_name);
struct resolver_entity* resolver_get_variable(struct resolver_result* result, struct resolver_process* resolver, const char* var_name);
struct resolver_entity* resolver_get_variable_from_local_scope(struct resolver_process* resolver, const char* var_name);
struct resolver_entity* resolver_get_function_in_scope(struct resolver_result* result, struct resolver_process* resolver, const char* func_name, struct resolver_scope* scope);
struct resolver_entity* resolver_get_function(struct resolver_result* result, struct resolver_process* resolver, const char* func_name);

// ch126: follow / walk helpers + public entry.
struct resolver_entity*  resolver_follow_for_name(struct resolver_process* resolver, const char* name, struct resolver_result* result);
struct resolver_entity*  resolver_follow_identifier(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_variable(struct resolver_process* resolver, struct node* var_node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_struct_exp(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_exp(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_array(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct datatype*         resolver_get_datatype(struct resolver_process* resolver, struct node* node);
void                     resolver_build_function_call_arguments(struct resolver_process* resolver, struct node* argument_node, struct resolver_entity* root_func_call_entity, size_t* total_size_out);
struct resolver_entity*  resolver_follow_function_call(struct resolver_process* resolver, struct resolver_result* result, struct node* node);
struct resolver_entity*  resolver_follow_parentheses(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
void                     resolver_array_bracket_set_flags(struct resolver_entity* bracket_entity, struct datatype* dtype, struct node* bracket_node, int index);
struct resolver_entity*  resolver_follow_array_bracket(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_exp_parenthesis(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_unsupported_unary_node(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_unsupported_node(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_cast(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_indirection(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_unary_address(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_unary(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
struct resolver_entity*  resolver_follow_part_return_entity(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
void                     resolver_follow_part(struct resolver_process* resolver, struct node* node, struct resolver_result* result);
void                     resolver_execute_rules(struct resolver_process* resolver, struct resolver_result* result);
void                     resolver_merge_compile_times(struct resolver_process* resolver, struct resolver_result* result);
void                     resolver_finalize_result(struct resolver_process* resolver, struct resolver_result* result);
struct resolver_result*  resolver_follow(struct resolver_process* resolver, struct node* node);

// ============================================================================
// Datatypes (ch33+)
// ============================================================================

enum {
    DATATYPE_FLAG_IS_SIGNED              = 0b00000001,
    DATATYPE_FLAG_IS_STATIC              = 0b00000010,
    DATATYPE_FLAG_IS_CONST               = 0b00000100,
    DATATYPE_FLAG_IS_POINTER             = 0b00001000,
    DATATYPE_FLAG_IS_ARRAY               = 0b00010000,
    DATATYPE_FLAG_IS_EXTERN              = 0b00100000,
    DATATYPE_FLAG_IS_RESTRICT            = 0b01000000,
    DATATYPE_FLAG_IGNORE_TYPE_CHECKING   = 0b10000000,
    DATATYPE_FLAG_IS_SECONDARY           = 0b100000000,
    DATATYPE_FLAG_STRUCT_UNION_NO_NAME   = 0b1000000000,
    DATATYPE_FLAG_IS_LITERAL             = 0b10000000000,
    // a pointer to a function, `int (*f)(int)`.
    DATATYPE_FLAG_IS_FUNCTION_POINTER    = 0b100000000000,
    // _Bool: an unsigned char that only ever holds 0 or 1.
    DATATYPE_FLAG_IS_BOOL                = 0b1000000000000,
};

enum {
    DATA_TYPE_VOID,
    DATA_TYPE_CHAR,
    DATA_TYPE_SHORT,
    DATA_TYPE_INTEGER,
    DATA_TYPE_LONG,
    DATA_TYPE_FLOAT,
    DATA_TYPE_DOUBLE,
    DATA_TYPE_STRUCT,
    DATA_TYPE_UNION,
    DATA_TYPE_UNKNOWN,
};

// struct datatype is forward-defined earlier (so var nodes can embed
// it). Fields documented there.

enum {
    DATA_TYPE_EXPECT_PRIMITIVE,
    DATA_TYPE_EXPECT_UNION,
    DATA_TYPE_EXPECT_STRUCT,
};

// Convenience sizes for primitive types. Used by
// parser_datatype_init_type_and_size_for_primitive in ch35.
enum {
    DATA_SIZE_ZERO   = 0,
    DATA_SIZE_BYTE   = 1,
    DATA_SIZE_WORD   = 2,
    DATA_SIZE_DWORD  = 4,
    DATA_SIZE_DDWORD = 8,
};

// ch71: flags on NODE_TYPE_FUNCTION (.func.flags).
enum {
    FUNCTION_NODE_FLAG_IS_NATIVE   = 0b00000001,
    // The parameter list ends in `...`: a call may pass more arguments.
    FUNCTION_NODE_FLAG_IS_VARIADIC = 0b00000010,
};

// Operator precedence table - definitions moved out of expressionable.c
// in ch30 so the parser can extern the table and look operators up.
#define TOTAL_OPERATOR_GROUPS  14
#define MAX_OPERATORS_IN_GROUP 12

enum {
    ASSOCIATIVITY_LEFT_TO_RIGHT,
    ASSOCIATIVITY_RIGHT_TO_LEFT,
};

struct expressionable_op_precedence_group {
    char* operators[MAX_OPERATORS_IN_GROUP];
    int   associtivity;
};

// ch191: generic expressionable machinery - the preprocessor will
// reuse this so its #if / #elif arithmetic can lean on the parser's
// operator precedence + reorder logic without duplicating it.
enum {
    EXPRESSIONABLE_GENERIC_TYPE_NUMBER,
    EXPRESSIONABLE_GENERIC_TYPE_IDENTIFIER,
    EXPRESSIONABLE_GENERIC_TYPE_UNARY,
    EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES,
    EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION,
    EXPRESSIONABLE_GENERIC_TYPE_NON_GENERIC,
};

enum {
    EXPRESSIONABLE_IS_SINGLE,
    EXPRESSIONABLE_IS_PARENTHESES,
};

struct expressionable;

typedef void*       (*EXPRESSIONABLE_HANDLE_NUMBER)(struct expressionable* e);
typedef void*       (*EXPRESSIONABLE_HANDLE_IDENTIFIER)(struct expressionable* e);
typedef void        (*EXPRESSIONABLE_MAKE_EXPRESSION_NODE)(struct expressionable* e, void* left_node_ptr, void* right_node_ptr, const char* op);
typedef void        (*EXPRESSIONABLE_MAKE_TENARY_NODE)(struct expressionable* e, void* true_result_node, void* false_result_node);
typedef void        (*EXPRESSIONABLE_MAKE_PARENTHESES_NODE)(struct expressionable* e, void* node_ptr);
typedef void        (*EXPRESSIONABLE_MAKE_UNARY_NODE)(struct expressionable* e, const char* op, void* right_operand_node_ptr);
typedef void        (*EXPRESSIONABLE_MAKE_UNARY_INDIRECTION_NODE)(struct expressionable* e, int ptr_depth, void* right_operand_node_ptr);
typedef int         (*EXPRESSIONABLE_GET_NODE_TYPE)(struct expressionable* e, void* node);
typedef void*       (*EXPRESSIONABLE_GET_LEFT_NODE)(struct expressionable* e, void* target_node);
typedef void*       (*EXPRESSIONABLE_GET_RIGHT_NODE)(struct expressionable* e, void* target_node);
typedef const char* (*EXPRESSIONABLE_GET_NODE_OPERATOR)(struct expressionable* e, void* target_node);
typedef void**      (*EXPRESSIONABLE_GET_NODE_ADDRESS)(struct expressionable* e, void* target_node);
typedef void        (*EXPRESSIONABLE_SET_EXPRESSION_NODE)(struct expressionable* e, void* node, void* left_node, void* right_node, const char* op);
typedef bool        (*EXPRESSIONABLE_SHOULD_JOIN_NODES)(struct expressionable* e, void* previous_node, void* node);
typedef void*       (*EXPRESSIONABLE_JOIN_NODES)(struct expressionable* e, void* previous_node, void* node);
typedef bool        (*EXPRESSIONABLE_EXPECTING_ADDITIONAL_NODE)(struct expressionable* e, void* node);
typedef bool        (*EXPRESSIONABLE_IS_CUSTOM_OPERATOR)(struct expressionable* e, struct token* token);

struct expressionable_config {
    struct expressionable_callbacks {
        EXPRESSIONABLE_HANDLE_NUMBER                handle_number_callback;
        EXPRESSIONABLE_HANDLE_IDENTIFIER            handle_identifier_callback;
        EXPRESSIONABLE_MAKE_EXPRESSION_NODE         make_expression_node;
        EXPRESSIONABLE_MAKE_PARENTHESES_NODE        make_parentheses_node;
        EXPRESSIONABLE_MAKE_UNARY_NODE              make_unary_node;
        EXPRESSIONABLE_MAKE_UNARY_INDIRECTION_NODE  make_unary_indirection_node;
        EXPRESSIONABLE_MAKE_TENARY_NODE             make_tenary_node;
        EXPRESSIONABLE_GET_NODE_TYPE                get_node_type;
        EXPRESSIONABLE_GET_LEFT_NODE                get_left_node;
        EXPRESSIONABLE_GET_RIGHT_NODE               get_right_node;
        EXPRESSIONABLE_GET_NODE_OPERATOR            get_node_operator;
        EXPRESSIONABLE_GET_NODE_ADDRESS             get_left_node_address;
        EXPRESSIONABLE_GET_NODE_ADDRESS             get_right_node_address;
        EXPRESSIONABLE_SET_EXPRESSION_NODE          set_exp_node;
        EXPRESSIONABLE_SHOULD_JOIN_NODES            should_join_nodes;
        EXPRESSIONABLE_JOIN_NODES                   join_nodes;
        EXPRESSIONABLE_EXPECTING_ADDITIONAL_NODE    expecting_additional_node;
        EXPRESSIONABLE_IS_CUSTOM_OPERATOR           is_custom_operator;
    } callbacks;
};

enum {
    EXPRESSIONABLE_FLAG_IS_PREPROCESSOR_EXPRESSION = 0b00000001,
};

struct expressionable {
    int                         flags;
    struct expressionable_config config;
    struct vector*              token_vec;
    struct vector*              node_vec_out;
};

// ch197: full expressionable API surface, declared in one block so
// the preprocessor (and any future client) can pull it in via
// compiler.h instead of hand-declaring each function.
struct expressionable_callbacks* expressionable_callbacks(struct expressionable* expressionable);
void* expressionable_node_pop(struct expressionable* expressionable);
struct token* expressionable_token_next(struct expressionable* expressionable);

void expressionable_node_push(struct expressionable* expressionable, void* node_ptr);
void* expressionable_node_peek_or_null(struct expressionable* expressionable);
void expressionable_ignore_nl(struct expressionable* expressionable, struct token* next_token);
struct token* expressionable_peek_next(struct expressionable* expressionable);
bool expressionable_token_next_is_operator(struct expressionable* expressionable, const char* op);
void expressionable_init(struct expressionable* expressionable, struct vector* token_vector, struct vector* node_vector, struct expressionable_config* config, int flags);
struct expressionable* expressionable_create(struct expressionable_config* config, struct vector* token_vector, struct vector* node_vector, int flags);
int expressionable_parse_number(struct expressionable* expressionable);
int expressionable_parse_identifier(struct expressionable* expressionable);

bool expressionable_parser_left_op_has_priority(const char* op_left, const char* op_right);
void expressionable_parser_node_shift_children_left(struct expressionable* expressionable, void* node);

void expressionable_parser_reorder_expression(struct expressionable* expressionable, void** node_out);

bool expressionable_generic_type_is_value_expressionable(int type);
void expressionable_expect_op(struct expressionable* expressionable, const char* op);

void expressionable_expect_sym(struct expressionable* expressionable, char c);
void expressionable_deal_with_additional_expression(struct expressionable* expressionable);
void expressionable_parse_parentheses(struct expressionable* expressionable);
int expressionable_get_pointer_depth(struct expressionable* expressionable);
void expressionable_parse_for_indirection_unary(struct expressionable* expressionable);

void expressionable_parse_for_normal_unary(struct expressionable* expressionable);
void expressionable_parse_unary(struct expressionable* expressionable);
void expressionable_parse_for_operator(struct expressionable* expressionable);
void expressionable_parse_tenary(struct expressionable* expressionable);
int expressionable_parse_exp(struct expressionable* expressionable, struct token* token);
int expressionable_parse_token(struct expressionable* expressionable, struct token* token, int flags);
int expressionable_parse_single_with_flags(struct expressionable* expressionable, int flags);
int expressionable_parse_single(struct expressionable* expressionable);
void expressionable_parse(struct expressionable* expressionable);

// ch96: deferred-resolution "fixup" system. Lets the parser register a
// piece of work it cannot finish right now (forward decl references,
// etc.) and try again later via the fix callback. end runs when the
// fixup is freed so the implementor can release private state.
struct fixup;
typedef bool (*FIXUP_FIX)(struct fixup* fixup);
typedef void (*FIXUP_END)(struct fixup* fixup);

struct fixup_config {
    FIXUP_FIX fix;
    FIXUP_END end;
    void*     private;
};

struct fixup_system {
    struct vector* fixups;            // vector of struct fixup*
};

enum {
    FIXUP_FLAG_RESOLVED = 0b00000001,
};

struct fixup {
    int                  flags;
    struct fixup_system* system;
    struct fixup_config  config;
};

struct fixup_system* fixup_sys_new(void);
struct fixup_config* fixup_config(struct fixup* fixup);
void                 fixup_free(struct fixup* fixup);
void                 fixup_start_iteration(struct fixup_system* system);
struct fixup*        fixup_next(struct fixup_system* system);
void                 fixup_sys_fixups_free(struct fixup_system* system);
void                 fixup_sys_free(struct fixup_system* system);
int                  fixup_sys_unresolved_fixups_count(struct fixup_system* system);
struct fixup*        fixup_register(struct fixup_system* system, struct fixup_config* config);
bool                 fixup_resolve(struct fixup* fixup);
void*                fixup_private(struct fixup* fixup);
bool                 fixups_resolve(struct fixup_system* system);

#endif
