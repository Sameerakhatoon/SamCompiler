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
    NODE_FLAG_INSIDE_EXPRESSION = 0b00000001,
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

        struct var {
            struct datatype type;
            // ch53: bytes of padding to insert before this variable
            // to satisfy alignment.
            int             padding;
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

        // ch49: NODE_TYPE_BODY - a sequence of statement nodes.
        struct body {
            struct vector* statements;
            size_t         size;
            bool           padded;
            struct node*   largest_var_node;
        } body;
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

// ch52: size helpers that look at variable nodes (not datatypes).
size_t variable_size(struct node* var_node);
size_t variable_size_for_list(struct node* var_list_node);

// ch53: alignment/padding helpers used by the body sizing logic.
int padding(int val, int to);
int align_value(int val, int to);
int align_value_treat_positive(int val, int to);
int compute_sum_padding(struct vector* vec);

// ch27: take a stack-allocated node, copy it to the heap, push onto the
// parser's node stack, return the new pointer.
struct node* node_create(struct node* _node);
// ch28: build a NODE_TYPE_EXPRESSION linking left + op + right.
void         make_exp_node(struct node* left_node, struct node* right_node, const char* op);
// ch44: build a NODE_TYPE_BRACKET wrapping a single inner expression.
void         make_bracket_node(struct node* inner);
// ch49: build a NODE_TYPE_BODY around a statement vector.
void         make_body_node(struct vector* body_vec, size_t size, bool padded, struct node* largest_var_node);

bool         node_is_expressionable(struct node* node);
struct node* node_peek_expressionable_or_null(void);

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
struct node*   symresolver_node(struct symbol* sym);
void           symresolver_build_for_node(struct compile_process* process, struct node* node);

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

#endif
