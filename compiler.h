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
    } num;

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

struct compile_process {
    // Flags controlling how this file should be compiled.
    int flags;

    struct pos                        pos;
    struct compile_process_input_file cfile;

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
};

enum {
    NODE_FLAG_INSIDE_EXPRESSION      = 0b00000001,
    NODE_FLAG_IS_FORWARD_DECLARATION = 0b00000010,
    NODE_FLAG_HAS_VARIABLE_COMBINED  = 0b00000100,
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
};

// ch44: array-bracket list. A vector of NODE_TYPE_BRACKET nodes, one
// per `[N]` in a declarator.
struct array_brackets {
    struct vector* n_brackets;
};

typedef struct node node_t;

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

#define STACK_PUSH_SIZE 4

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

        struct var {
            struct datatype type;
            // ch53: bytes of padding to insert before this variable
            // to satisfy alignment.
            int             padding;
            // ch58: aligned offset of this variable in its scope.
            int             aoffset;
            const char*     name;
            struct node*    val;
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

        // ch130: NODE_TYPE_UNARY. For `**p` the op is "*" and the
        // indirection.depth captures the chain length.
        struct unary {
            const char*  op;
            struct node* operand;
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

// ch26: node stack helpers. node_set_vector points them at the parser's
// scratch / root vectors; the rest of the parser uses node_push /
// node_peek* / node_pop to manipulate the work-stack.
void         node_set_vector(struct vector* vec, struct vector* root_vec);
void         node_push(struct node* node);
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
bool token_is_operator(struct token* token, const char* val);
bool datatype_is_struct_or_union_for_name(const char* name);
bool datatype_is_struct_or_union(struct datatype* dtype);

// ch51: size helpers used by codegen + resolver later.
size_t datatype_size_for_array_access(struct datatype* dtype);
size_t datatype_element_size(struct datatype* dtype);
size_t datatype_size_no_ptr(struct datatype* dtype);
size_t datatype_size(struct datatype* dtype);
bool   datatype_is_primitive(struct datatype* dtype);
bool   datatype_is_struct_or_union_non_pointer(struct datatype* dtype);
struct datatype datatype_for_numeric(void);
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
    int index;
};

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
struct code_generator* codegenerator_new(struct compile_process* process);

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
struct symbol* symresolver_get_symbol_for_native_function(struct compile_process* process, const char* name);
struct symbol* symresolver_register_symbol(struct compile_process* process, const char* sym_name, int type, void* data);

// ch74: returns the function's stack_addition (bytes between EBP and
// the first argument; typically 8 = saved EBP + return EIP).
size_t function_node_argument_stack_addition(struct node* node);
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
};

enum {
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_VARIABLE,
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_FUNCTION,
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_ARRAY_BRACKET,
};

struct resolver_default_entity_data {
    int  type;
    char address[60];        // "[ebp-4]" / "[var_name+4]"
    char base_address[60];   // "ebp" / "var_name"
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
        char address[60];        // "[ebp-4]" / "[name+4]"
        char base_address[60];   // "EBP"     / "global_variable_name"
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
        } func_call_data;

        struct resolver_entity_rule {
            struct resolver_entity_rule_left  { int flags; } left;
            struct resolver_entity_rule_right { int flags; } right;
        } rule;

        struct resolver_indirection {
            int depth;
        } indirection;
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
    FUNCTION_NODE_FLAG_IS_NATIVE = 0b00000001,
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
