#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include <assert.h>

enum
{
    PREPROCESSOR_FLAG_EVALUATE_NODE = 0b00000001
};

enum
{
    PREPROCESSOR_NUMBER_NODE,
    PREPROCESSOR_IDENTIFIER_NODE,
    PREPROCESSOR_KEYWORD_NODE,
    PREPROCESSOR_UNARY_NODE,
    PREPROCESSOR_EXPRESSION_NODE,
    PREPROCESSOR_PARENTHESES_NODE,
    PREPROCESSOR_JOINED_NODE,
    PREPROCESSOR_TENARY_NODE
};

struct preprocessor_node
{
    int type;
    struct preprocessor_const_val
    {
        union
        {
            char cval;
            unsigned int inum;
            long lnum;
            long long llnum;
            unsigned long ulnum;
            unsigned long long ullnum;
        };

    } const_val;

    union
    {
        struct preprocessor_exp_node
        {
            struct preprocessor_node *left;
            struct preprocessor_node *right;
            const char *op;
        } exp;

        struct preprocessor_unary_node
        {
            struct preprocessor_node *operand_node;
            const char *op;
            struct preprocessor_unary_indirection
            {
                int depth;
            } indirection;
        } unary_node;

        struct preprocessor_parenthesis
        {
            struct preprocessor_node *exp;
        } parenthesis;

        struct preprocessor_joined_node
        {
            struct preprocessor_node *left;
            struct preprocessor_node *right;
        } joined;

        struct preprocessor_tenary_node
        {
            struct preprocessor_node *true_node;
            struct preprocessor_node *false_node;
        } tenary;
    };

    const char *sval;
};

void preprocessor_handle_token(struct compile_process *compiler, struct token *token);
int preprocessor_parse_evaluate(struct compile_process *compiler, struct vector *token_vec);
int preprocessor_evaluate(struct compile_process *compiler, struct preprocessor_node *root_node);
struct preprocessor_definition *preprocessor_get_definition(struct preprocessor *preprocessor, const char *name);
struct preprocessor_definition *preprocessor_get_definition_other_kind(struct preprocessor *preprocessor, struct preprocessor_definition *definition);

int preprocessor_evaluate_unary(struct compile_process *compiler, struct preprocessor_node *node)
{
    int res = 0;
    const char *op = node->unary_node.op;
    struct preprocessor_node *right_operand = node->unary_node.operand_node;
    if (S_EQ(op, "!"))
    {
        res = !preprocessor_evaluate(compiler, right_operand);
    }
    else if (S_EQ(op, "~"))
    {
        res = ~preprocessor_evaluate(compiler, right_operand);
    }
    else if (S_EQ(op, "-"))
    {
        res = -preprocessor_evaluate(compiler, right_operand);
    }
    else
    {
        compiler_error(compiler, "The operator is not supported");
    }
    return res;
}

int preprocessor_evaluate_parentheses(struct compile_process *compiler, struct preprocessor_node *node)
{
    return preprocessor_evaluate(compiler, node->parenthesis.exp);
}

const char *preprocessor_pull_string_from(struct preprocessor_node *root_node)
{
    const char *result = NULL;
    switch (root_node->type)
    {
        case PREPROCESSOR_PARENTHESES_NODE:
            result = preprocessor_pull_string_from(root_node->parenthesis.exp);
            break;

        case PREPROCESSOR_KEYWORD_NODE:
        case PREPROCESSOR_IDENTIFIER_NODE:
            result = root_node->sval;
            break;

        case PREPROCESSOR_EXPRESSION_NODE:
            result = preprocessor_pull_string_from(root_node->exp.left);
            break;
    }

    return result;
}

const char *preprocessor_pull_defined_value(struct compile_process *compiler, struct preprocessor_node *joined_node)
{
    const char *val = preprocessor_pull_string_from(joined_node->joined.right);
    if (!val)
    {
        compiler_error(compiler, "Expecting an identifier node for defined keyword right operand");
    }

    return val;
}

int preprocessor_evaluate_joined_node_defined(struct compile_process *compiler, struct preprocessor_node *node)
{
    const char *right_val = preprocessor_pull_defined_value(compiler, node);
    return preprocessor_get_definition(compiler->preprocessor, right_val) != NULL;
}

int preprocessor_evaluate_joined_node(struct compile_process *compiler, struct preprocessor_node *node)
{
    if (node->joined.left->type != PREPROCESSOR_KEYWORD_NODE)
    {
        return 0;
    }

    int res = 0;
    if (S_EQ(node->joined.left->sval, "defined"))
    {
        res = preprocessor_evaluate_joined_node_defined(compiler, node);
    }

    return res;
}

int preprocessor_evaluate(struct compile_process *compiler, struct preprocessor_node *root_node);
int preprocessor_handle_identifier_for_token_vector(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec, struct token *token);
struct vector *preprocessor_definition_value(struct preprocessor_definition *definition);
void preprocessor_token_vec_push_src_resolve_definition(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec, struct token *token);
void preprocessor_token_vec_push_src_resolve_definitions(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec);
struct preprocessor_definition *preprocessor_get_definition(struct preprocessor *preprocessor, const char *name);
void preprocessor_macro_function_push_something(struct compile_process *compiler, struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments, struct token *arg_token, struct vector *definition_token_vec, struct vector *value_vec_target);

void preprocessor_execute_warning(struct compile_process *compiler, const char *msg)
{
    compiler_warning(compiler, "#warning %s", msg);
}

void preprocessor_execute_error(struct compile_process *compiler, const char *msg)
{
    compiler_error(compiler, "#error %s", msg);
}

struct preprocessor_included_file *preprocessor_add_included_file(struct preprocessor *preprocessor, const char *filename)
{
    struct preprocessor_included_file *included_file = calloc(1, sizeof(struct preprocessor_included_file));
    strncpy(included_file->filename, filename, sizeof(included_file->filename));
    vector_push(preprocessor->includes, &included_file);
    return included_file;
}

void preprocessor_create_static_include(struct preprocessor *preprocessor, const char *filename, PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION creation_handler)
{
    struct preprocessor_included_file *included_file = preprocessor_add_included_file(preprocessor, filename);
    creation_handler(preprocessor, included_file);
}

bool preprocessor_is_keyword(const char *type)
{
    return S_EQ(type, "defined");
}

struct vector *preprocessor_build_value_vector_for_integer(int value)
{
    struct vector *token_vec = vector_create(sizeof(struct token));
    struct token t1 = {};
    t1.type = TOKEN_TYPE_NUMBER;
    t1.llnum = value;
    vector_push(token_vec, &t1);
    return token_vec;
}

void preprocessor_token_vec_push_keyword_and_identifier(struct vector *token_vec, const char *keyword, const char *identifier)
{
    struct token t1 = {};
    t1.type = TOKEN_TYPE_KEYWORD;
    t1.sval = keyword;
    struct token t2 = {};
    t2.type = TOKEN_TYPE_IDENTIFIER;
    t2.sval = identifier;
    vector_push(token_vec, &t1);
    vector_push(token_vec, &t2);
}

void *preprocessor_node_create(struct preprocessor_node *node)
{
    struct preprocessor_node *result = calloc(1, sizeof(struct preprocessor_node));
    memcpy(result, node, sizeof(struct preprocessor_node));
    return result;
}

int preprocessor_definition_argument_exists(struct preprocessor_definition *definition, const char *name)
{
    vector_set_peek_pointer(definition->standard.arguments, 0);
    int i = 0;
    const char *current = vector_peek_ptr(definition->standard.arguments);
    while (current)
    {
        if (S_EQ(current, name))
            return i;

        i++;
        current = vector_peek_ptr(definition->standard.arguments);
    }

    return -1;
}

struct preprocessor_function_argument *preprocessor_function_argument_at(struct preprocessor_function_arguments *arguments, int index)
{
    struct preprocessor_function_argument *argument = vector_at(arguments->arguments, index);
    return argument;
}

void preprocessor_token_push_to_function_arguments(struct preprocessor_function_arguments *arguments, struct token *token)
{
    struct preprocessor_function_argument arg = {};
    arg.tokens = vector_create(sizeof(struct token));
    vector_push(arg.tokens, token);
    vector_push(arguments->arguments, &arg);
}

void preprocessor_function_argument_push_to_vec(struct preprocessor_function_argument *argument, struct vector *vector_out)
{
    struct vector *tokens = argument->expanded ? argument->expanded : argument->tokens;
    vector_set_peek_pointer(tokens, 0);
    struct token *token = vector_peek(tokens);
    while (token)
    {
        vector_push(vector_out, token);
        token = vector_peek(tokens);
    }
}

// The argument as written, for the operands of ##.
static void preprocessor_function_argument_push_raw_to_vec(struct preprocessor_function_argument *argument, struct vector *vector_out)
{
    vector_set_peek_pointer(argument->tokens, 0);
    struct token *token = vector_peek(argument->tokens);
    while (token)
    {
        vector_push(vector_out, token);
        token = vector_peek(argument->tokens);
    }
}

void preprocessor_token_push_to_dst(struct vector *token_vec, struct token *token)
{
    struct token t = *token;
    vector_push(token_vec, &t);
}

void preprocessor_token_push_dst(struct compile_process *compiler, struct token *token)
{
    preprocessor_token_push_to_dst(compiler->token_vec, token);
}

void preprocessor_token_vec_push_src_to_dst(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec)
{
    vector_set_peek_pointer(src_vec, 0);
    struct token *token = vector_peek(src_vec);
    while (token)
    {
        vector_push(dst_vec, token);
        token = vector_peek(src_vec);
    }
}

void preprocessor_token_vec_push_src(struct compile_process *compiler, struct vector *src_vec)
{
    preprocessor_token_vec_push_src_to_dst(compiler, src_vec, compiler->token_vec);
}

void preprocessor_token_vec_push_src_token(struct compile_process *compiler, struct token *token)
{
    vector_push(compiler->token_vec, token);
}

void preprocessor_initialize(struct preprocessor *preprocessor)
{
    memset(preprocessor, 0, sizeof(struct preprocessor));
    preprocessor->definitions = vector_create(sizeof(struct preprocessor_definition *));
    preprocessor->includes = vector_create(sizeof(struct preprocessor_included_file *));
    preprocessor->once_files = vector_create(sizeof(const char *));
    preprocessor_create_definitions(preprocessor);
}

struct preprocessor *preprocessor_create(struct compile_process *compiler)
{
    struct preprocessor *preprocessor = calloc(1, sizeof(struct preprocessor));
    preprocessor_initialize(preprocessor);
    preprocessor->compiler = compiler;
    return preprocessor;
}
struct token *preprocessor_previous_token(struct compile_process *compiler)
{
    return vector_peek_at(compiler->token_vec_original, compiler->token_vec_original->pindex - 1);
}

struct token *preprocessor_next_token(struct compile_process *compiler)
{
    struct token *token = vector_peek(compiler->token_vec_original);
    // Errors point at the token being read; they named the end of the
    // file, where the lexer left the position.
    if (token && token->pos.line)
    {
        compiler->pos = token->pos;
    }
    return token;
}

struct token *preprocessor_next_token_no_increment(struct compile_process *compiler)
{
    return vector_peek_no_increment(compiler->token_vec_original);
}

struct token *preprocessor_peek_next_token_skip_nl(struct compile_process *compiler)
{
    struct token *token = preprocessor_next_token_no_increment(compiler);
    while (token && token->type == TOKEN_TYPE_NEWLINE)
    {
        token = preprocessor_next_token(compiler);
    }
    token = preprocessor_next_token_no_increment(compiler);
    return token;
}

struct token *preprocessor_peek_next_token_with_vector_no_increment(struct compile_process *compiler, struct vector *priority_token_vec, bool overflow_use_compiler_tokens)
{
    struct token *token = vector_peek_no_increment(priority_token_vec);
    if (token == NULL && overflow_use_compiler_tokens)
    {
        token = preprocessor_peek_next_token_skip_nl(compiler);
    }

    return token;
}
struct token *preprocessor_next_token_with_vector(struct compile_process *compiler, struct vector *priority_token_vec, bool overflow_use_compiler_tokens)
{
    struct token *token = vector_peek(priority_token_vec);
    if (token == NULL && overflow_use_compiler_tokens)
    {
        // Past the end of a macro's body the source continues in the file
        // (`#define T typedef` then `T int a_t;`). The token must be taken,
        // not only looked at: peeking returned the same token forever, and
        // the typedef reader pushed it until memory ran out.
        token = preprocessor_peek_next_token_skip_nl(compiler);
        if (token)
        {
            preprocessor_next_token(compiler);
        }
    }

    return token;
}

void *preprocessor_handle_number_token(struct expressionable *expressionable)
{
    struct token *token = expressionable_token_next(expressionable);
    return preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_NUMBER_NODE, .const_val.llnum = token->llnum});
}

void *preprocessor_handle_identifier_token(struct expressionable *expressionable)
{
    struct token *token = expressionable_token_next(expressionable);
    bool is_preprocessor_keyword = preprocessor_is_keyword(token->sval);
    int type = PREPROCESSOR_IDENTIFIER_NODE;
    if (is_preprocessor_keyword)
    {
        type = PREPROCESSOR_KEYWORD_NODE;
    }
    return preprocessor_node_create(&(struct preprocessor_node){.type = type, .sval = token->sval});
}

void preprocessor_make_unary_node(struct expressionable *expressionable, const char *op, void *right_operand_node_ptr)
{
    struct preprocessor_node *right_operand_node = right_operand_node_ptr;
    void *unary_node = preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_UNARY_NODE, .unary_node.op = op, .unary_node.operand_node = right_operand_node});
    expressionable_node_push(expressionable, unary_node);
}

void preprocessor_make_expression_node(struct expressionable *expressionable, void *left_node_ptr, void *right_node_ptr, const char *op)
{
    struct preprocessor_node exp_node;
    exp_node.type = PREPROCESSOR_EXPRESSION_NODE;
    exp_node.exp.left = left_node_ptr;
    exp_node.exp.right = right_node_ptr;
    exp_node.exp.op = op;
    expressionable_node_push(expressionable, preprocessor_node_create(&exp_node));
}

void preprocessor_make_parentheses_node(struct expressionable *expressionable, void *node_ptr)
{
    struct preprocessor_node parentheses_node;
    parentheses_node.type = PREPROCESSOR_PARENTHESES_NODE;
    parentheses_node.parenthesis.exp = node_ptr;
    expressionable_node_push(expressionable, preprocessor_node_create(&parentheses_node));
}
void preprocessor_make_tenary_node(struct expressionable *expressionable, void *true_result_node_ptr, void *false_result_node_ptr)
{
    struct preprocessor_node *true_result_node = true_result_node_ptr;
    struct preprocessor_node *false_result_node = false_result_node_ptr;

    expressionable_node_push(expressionable, preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_TENARY_NODE, .tenary.true_node = true_result_node, .tenary.false_node = false_result_node}));
}

int preprocessor_get_node_type(struct expressionable *expressionable, void *node)
{
    int generic_type = EXPRESSIONABLE_GENERIC_TYPE_NON_GENERIC;
    struct preprocessor_node *preprocessor_node = node;
    switch (preprocessor_node->type)
    {
    case PREPROCESSOR_NUMBER_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
        break;

    case PREPROCESSOR_IDENTIFIER_NODE:
    case PREPROCESSOR_KEYWORD_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_IDENTIFIER;
        break;

    case PREPROCESSOR_UNARY_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_UNARY;
        break;
    case PREPROCESSOR_EXPRESSION_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
        break;

    case PREPROCESSOR_PARENTHESES_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES;
        break;
    }

    return generic_type;
}

void *preprocessor_get_left_node(struct expressionable *expressionable, void *target_node)
{
    struct preprocessor_node *node = target_node;
    return node->exp.left;
}

void *preprocessor_get_right_node(struct expressionable *expressionable, void *target_node)
{
    struct preprocessor_node *node = target_node;
    return node->exp.right;
}

const char *preprocessor_get_node_operator(struct expressionable *expressionable, void *target_node)
{
    struct preprocessor_node *preprocessor_node = target_node;
    return preprocessor_node->exp.op;
}

void **preprocessor_get_left_node_address(struct expressionable *expressionable, void *target_node)
{
    return (void **)&((struct preprocessor_node *)(target_node))->exp.left;
}

void **preprocessor_get_right_node_address(struct expressionable *expressionable, void *target_node)
{
    return (void **)&((struct preprocessor_node *)(target_node))->exp.right;
}

void preprocessor_set_expression_node(struct expressionable *expressionable, void *node, void *left_node, void *right_node, const char *op)
{
    struct preprocessor_node *preprocessor_node = node;
    preprocessor_node->exp.left = left_node;
    preprocessor_node->exp.right = right_node;
    preprocessor_node->exp.op = op;
}

bool preprocessor_should_join_nodes(struct expressionable *expressionable, void *previous_node_ptr, void *node_ptr)
{
    return true;
}

void *preprocessor_join_nodes(struct expressionable *expressionable, void *previous_node_ptr, void *node_ptr)
{
    struct preprocessor_node *previous_node = previous_node_ptr;
    struct preprocessor_node *node = node_ptr;
    return preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_JOINED_NODE, .joined.left = previous_node, .joined.right = node});
}

bool preprocessor_expecting_additional_node(struct expressionable *expressionable, void *node_ptr)
{
    struct preprocessor_node *node = node_ptr;
    return node->type == PREPROCESSOR_KEYWORD_NODE && S_EQ(node->sval, "defined");
}

bool preprocessor_is_custom_operator(struct expressionable *expressionable, struct token *token)
{
    return false;
}

struct expressionable_config preprocessor_expressionable_config =
    {
        .callbacks.handle_number_callback = preprocessor_handle_number_token,
        .callbacks.handle_identifier_callback = preprocessor_handle_identifier_token,
        .callbacks.make_unary_node = preprocessor_make_unary_node,
        .callbacks.make_expression_node = preprocessor_make_expression_node,
        .callbacks.make_parentheses_node = preprocessor_make_parentheses_node,
        .callbacks.make_tenary_node = preprocessor_make_tenary_node,
        .callbacks.get_node_type = preprocessor_get_node_type,
        .callbacks.get_left_node = preprocessor_get_left_node,
        .callbacks.get_right_node = preprocessor_get_right_node,
        .callbacks.get_node_operator = preprocessor_get_node_operator,
        .callbacks.get_left_node_address = preprocessor_get_left_node_address,
        .callbacks.get_right_node_address = preprocessor_get_right_node_address,
        .callbacks.set_exp_node = preprocessor_set_expression_node,
        .callbacks.should_join_nodes = preprocessor_should_join_nodes,
        .callbacks.join_nodes = preprocessor_join_nodes,
        .callbacks.expecting_additional_node = preprocessor_expecting_additional_node,
        .callbacks.is_custom_operator = preprocessor_is_custom_operator,
};

bool preprocessor_is_preprocessor_keyword(const char *value)
{
    return S_EQ(value, "define") ||
           S_EQ(value, "undef") ||
           S_EQ(value, "warning") ||
           S_EQ(value, "error") ||
           S_EQ(value, "if") ||
           // "eleif" in the book, so #elif never matched.
           S_EQ(value, "elif") ||
           S_EQ(value, "else") ||
           S_EQ(value, "pragma") ||
           S_EQ(value, "line") ||
           S_EQ(value, "ifdef") ||
           S_EQ(value, "ifndef") ||
           S_EQ(value, "endif") ||
           S_EQ(value, "include") ||
           S_EQ(value, "typedef");
}
bool preprocessor_token_is_preprocessor_keyword(struct token *token)
{
    return token->type == TOKEN_TYPE_IDENTIFIER || (token->type == TOKEN_TYPE_KEYWORD && preprocessor_is_preprocessor_keyword(token->sval));
}
bool preprocessor_token_is_define(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "define"));
}
bool preprocessor_token_is_undef(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "undef"));
}

bool preprocessor_token_is_warning(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "warning"));
}
bool preprocessor_token_is_error(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "error"));
}

bool preprocessor_token_is_if(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "if"));
}

bool preprocessor_token_is_ifdef(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "ifdef"));
}

bool preprocessor_token_is_ifndef(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "ifndef"));
}

bool preprocessor_token_is_include(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "include"));
}

struct buffer *preprocessor_multi_value_string(struct compile_process *compiler)
{
    struct buffer *buffer = buffer_create();
    struct token *value_token = preprocessor_next_token(compiler);
    while (value_token)
    {
        if (value_token->type == TOKEN_TYPE_NEWLINE)
        {
            break;
        }

        if (token_is_symbol(value_token, '\\'))
        {
            // Skip the new line
            preprocessor_next_token(compiler);
            value_token = preprocessor_next_token(compiler);
            continue;
        }

        buffer_printf(buffer, "%s", value_token->sval);
        value_token = preprocessor_next_token(compiler);
    }

    return buffer;
}

void preprocessor_multi_value_insert_to_vector(struct compile_process *compiler, struct vector *value_token_vec)
{
    struct token *value_token = preprocessor_next_token(compiler);
    while (value_token)
    {
        if (value_token->type == TOKEN_TYPE_NEWLINE)
        {
            break;
        }

        if (token_is_symbol(value_token, '\\'))
        {
            // This allows for another line skip the new line
            preprocessor_next_token(compiler);
            value_token = preprocessor_next_token(compiler);
            continue;
        }

        vector_push(value_token_vec, value_token);
        value_token = preprocessor_next_token(compiler);
    }
}

void preprocessor_definition_remove(struct preprocessor *preprocessor, const char *name)
{
    vector_set_peek_pointer(preprocessor->definitions, 0);
    struct preprocessor_definition *current_definition = vector_peek_ptr(preprocessor->definitions);
    while (current_definition)
    {
        // A typedef is not a macro: `#define` or `#undef` of its name
        // leaves it (preprocessor_get_definition_other_kind).
        if (S_EQ(current_definition->name, name) && current_definition->type != PREPROCESSOR_DEFINITION_TYPEDEF)
        {
            // Remove the definition
            vector_pop_last_peek(preprocessor->definitions);
        }
        current_definition = vector_peek_ptr(preprocessor->definitions);
    }
}
struct preprocessor_definition *preprocessor_definition_create(const char *name, struct vector *value_vec, struct vector *arguments, struct preprocessor *preprocessor)
{
    // Unset the definition if its already created
    preprocessor_definition_remove(preprocessor, name);

    struct preprocessor_definition *definition = calloc(1, sizeof(struct preprocessor_definition));
    definition->type = PREPROCESSOR_DEFINITION_STANDARD;
    definition->name = name;
    definition->standard.value = value_vec;
    definition->standard.arguments = arguments;
    definition->preprocessor = preprocessor;

    if (arguments && vector_count(definition->standard.arguments))
    {
        definition->type = PREPROCESSOR_DEFINITION_MACRO_FUNCTION;
    }

    vector_push(preprocessor->definitions, &definition);
    return definition;
}

struct preprocessor_definition *preprocessor_definition_create_native(const char *name,
                                                                      PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE evaluate,
                                                                      PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE value,
                                                                      struct preprocessor *preprocessor)
{
    struct preprocessor_definition *definition = calloc(sizeof(struct preprocessor_definition), 1);
    definition->type             = PREPROCESSOR_DEFINITION_NATIVE_CALLBACK;
    definition->name             = name;
    definition->native.evaluate  = evaluate;
    definition->native.value     = value;
    definition->preprocessor     = preprocessor;
    vector_push(preprocessor->definitions, &definition);
    return definition;
}

struct preprocessor_definition *preprocessor_definition_create_typedef(const char *name, struct vector *value_vec, struct preprocessor *preprocessor)
{
    struct preprocessor_definition *definition = calloc(1, sizeof(struct preprocessor_definition));
    definition->type = PREPROCESSOR_DEFINITION_TYPEDEF;
    definition->name = name;
    definition->_typedef.value = value_vec;
    definition->preprocessor = preprocessor;
    vector_push(preprocessor->definitions, &definition);
    return definition;
}

struct preprocessor_definition *preprocessor_get_definition(struct preprocessor *preprocessor, const char *name)
{
    vector_set_peek_pointer(preprocessor->definitions, 0);
    struct preprocessor_definition *definition = vector_peek_ptr(preprocessor->definitions);
    while (definition)
    {
        if (S_EQ(definition->name, name))
        {
            break;
        }

        definition = vector_peek_ptr(preprocessor->definitions);
    }

    return definition;
}

// For a typedef, a function-like macro of the same name; for such a
// macro, a typedef of the same name; otherwise NULL.
struct preprocessor_definition *preprocessor_get_definition_other_kind(struct preprocessor *preprocessor, struct preprocessor_definition *definition)
{
    int want;
    if (definition->type == PREPROCESSOR_DEFINITION_TYPEDEF)
    {
        want = PREPROCESSOR_DEFINITION_MACRO_FUNCTION;
    }
    else if (definition->type == PREPROCESSOR_DEFINITION_MACRO_FUNCTION)
    {
        want = PREPROCESSOR_DEFINITION_TYPEDEF;
    }
    else
    {
        return NULL;
    }
    for (int i = 0; i < vector_count(preprocessor->definitions); i++)
    {
        struct preprocessor_definition *other = *(struct preprocessor_definition **)vector_at(preprocessor->definitions, i);
        if (other->type == want && S_EQ(other->name, definition->name))
        {
            return other;
        }
    }
    return NULL;
}

struct vector *preprocessor_definition_value_for_standard(struct preprocessor_definition *definition)
{
    return definition->standard.value;
}

struct vector *preprocessor_definition_value_for_typedef_or_other(struct preprocessor_definition *definition)
{
    if (definition->type != PREPROCESSOR_DEFINITION_TYPEDEF)
    {
        return preprocessor_definition_value(definition);
    }

    return definition->_typedef.value;
}

struct vector *preprocessor_definition_value_for_typedef(struct preprocessor_definition *definition)
{
    return preprocessor_definition_value_for_typedef_or_other(definition);
}

struct vector *preprocessor_definition_value_for_native(struct preprocessor_definition *definition,
                                                        struct preprocessor_function_arguments *arguments)
{
    return definition->native.value(definition, arguments);
}

struct vector *preprocessor_definition_value_with_arguments(struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    if (definition->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK)
    {
        return preprocessor_definition_value_for_native(definition, arguments);
    }
    else if (definition->type == PREPROCESSOR_DEFINITION_TYPEDEF)
    {
        return preprocessor_definition_value_for_typedef(definition);
    }

    return preprocessor_definition_value_for_standard(definition);
}
struct vector *preprocessor_definition_value(struct preprocessor_definition *definition)
{
    return preprocessor_definition_value_with_arguments(definition, NULL);
}

int preprocessor_parse_evaluate_token(struct compile_process *compiler, struct token *token)
{
    struct vector *token_vec = vector_create(sizeof(struct token));
    vector_push(token_vec, token);
    return preprocessor_parse_evaluate(compiler, token_vec);
}

int preprocessor_definition_evaluated_value_for_standard(struct preprocessor_definition *definition)
{
    struct token *token = vector_back(definition->standard.value);
    if (token->type == TOKEN_TYPE_IDENTIFIER)
    {
        return preprocessor_parse_evaluate_token(definition->preprocessor->compiler, token);
    }

    if (token->type != TOKEN_TYPE_NUMBER)
    {
        compiler_error(definition->preprocessor->compiler, "The definition must hold a number value. Unable to use macro IF");
    }
    return token->llnum;
}

int preprocessor_definition_evaluated_value_for_native(struct preprocessor_definition *definition,
                                                       struct preprocessor_function_arguments *arguments)
{
    return definition->native.evaluate(definition, arguments);
}

int preprocessor_definition_evaluated_value(struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    if (definition->type == PREPROCESSOR_DEFINITION_STANDARD)
    {
        return preprocessor_definition_evaluated_value_for_standard(definition);
    }
    else if (definition->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK)
    {
        return preprocessor_definition_evaluated_value_for_native(definition, arguments);
    }

    compiler_error(definition->preprocessor->compiler, "The definition cannot be evaluated into a number");
    return -1;
}
bool preprocessor_is_next_macro_arguments(struct compile_process *compiler)
{
    bool res = false;
    vector_save(compiler->token_vec_original);
    struct token *last_token = preprocessor_previous_token(compiler);
    struct token *current_token = preprocessor_next_token(compiler);

    if (token_is_operator(current_token, "(") && (!last_token || !last_token->whitespace))
    {
        res = true;
    }

    vector_restore(compiler->token_vec_original);
    return res;
}

// The lexer reads `...` as three `.` tokens; this takes the other two.
static bool preprocessor_take_ellipsis(struct compile_process *compiler, struct token *token)
{
    if (!token || !token_is_operator(token, "."))
    {
        return false;
    }
    for (int i = 0; i < 2; i++)
    {
        if (!token_is_operator(preprocessor_next_token(compiler), "."))
        {
            compiler_error(compiler, "Expected ... in the macro arguments");
        }
    }
    return true;
}

// Returns true when the list ends in `...`.
bool preprocessor_parse_macro_argument_declaration(struct compile_process *compiler, struct vector *arguments)
{
    if (token_is_operator(preprocessor_next_token_no_increment(compiler), "("))
    {
        // Skip the (
        preprocessor_next_token(compiler);
        struct token *next_token = preprocessor_next_token(compiler);
        while (next_token && !token_is_symbol(next_token, ')'))
        {
            if (preprocessor_take_ellipsis(compiler, next_token))
            {
                const char *name = "__VA_ARGS__";
                vector_push(arguments, &name);
                if (!token_is_symbol(preprocessor_next_token(compiler), ')'))
                {
                    compiler_error(compiler, "... must end the macro arguments");
                }
                return true;
            }

            if (next_token->type != TOKEN_TYPE_IDENTIFIER)
            {
                compiler_error(compiler, "You must provide an identifier in the preprocessor definition!");
            }

            vector_push(arguments, &next_token->sval);
            next_token = preprocessor_next_token(compiler);
            if (!token_is_operator(next_token, ",") && !token_is_symbol(next_token, ')'))
            {
                compiler_error(compiler, "Incomplete sequence for macro arguments");
            }

            if (token_is_symbol(next_token, ')'))
            {
                break;
            }

            // Skip the "," operator
            next_token = preprocessor_next_token(compiler);
        }
    }
    return false;
}
void preprocessor_handle_definition_token(struct compile_process *compiler)
{
    struct token *name_token = preprocessor_next_token(compiler);
    if (!name_token || (name_token->type != TOKEN_TYPE_IDENTIFIER && name_token->type != TOKEN_TYPE_KEYWORD))
    {
        compiler_error(compiler, "#define needs a macro name");
    }
    struct vector *arguments = vector_create(sizeof(const char *));
    bool variadic = false;
    bool function_like = preprocessor_is_next_macro_arguments(compiler);

    if (function_like)
    {
        variadic = preprocessor_parse_macro_argument_declaration(compiler, arguments);
    }
    // Value can be composed of many tokens
    struct vector *value_token_vec = vector_create(sizeof(struct token));
    preprocessor_multi_value_insert_to_vector(compiler, value_token_vec);

    struct preprocessor *preprocessor = compiler->preprocessor;
    struct preprocessor_definition *definition = preprocessor_definition_create(name_token->sval, value_token_vec, arguments, preprocessor);
    definition->standard.variadic = variadic;
    if (function_like)
    {
        // `#define F() ...` takes no arguments but is still called as F().
        definition->type = PREPROCESSOR_DEFINITION_MACRO_FUNCTION;
    }
}

void preprocessor_handle_undef_token(struct compile_process *compiler)
{
    struct token *name_token = preprocessor_next_token(compiler);
    if (!name_token || (name_token->type != TOKEN_TYPE_IDENTIFIER && name_token->type != TOKEN_TYPE_KEYWORD))
    {
        compiler_error(compiler, "#undef needs a macro name");
    }
    preprocessor_definition_remove(compiler->preprocessor, name_token->sval);
}
void preprocessor_handle_warning_token(struct compile_process *compiler)
{
    struct buffer *str_buf = preprocessor_multi_value_string(compiler);
    preprocessor_execute_warning(compiler, buffer_ptr(str_buf));
}

void preprocessor_handle_error_token(struct compile_process *compiler)
{
    struct buffer *str_buf = preprocessor_multi_value_string(compiler);
    preprocessor_execute_error(compiler, buffer_ptr(str_buf));
}

struct token *preprocessor_hashtag_and_identifier(struct compile_process *compiler, const char *str)
{
    if (!preprocessor_next_token_no_increment(compiler))
    {
        return NULL;
    }

    if (!token_is_symbol(preprocessor_next_token_no_increment(compiler), '#'))
    {
        return NULL;
    }

    vector_save(compiler->token_vec_original);
    // Skip the hashtag symbol
    preprocessor_next_token(compiler);

    struct token *target_token = preprocessor_next_token_no_increment(compiler);
    if ((token_is_identifier(target_token) && S_EQ(target_token->sval, str)) || token_is_keyword(target_token, str))
    {
        // Pop off the target token
        preprocessor_next_token(compiler);
        // Purge the vector save
        vector_save_purge(compiler->token_vec_original);
        return target_token;
    }

    vector_restore(compiler->token_vec_original);
    return NULL;
}

/**
 * @brief Return true if their is a hastag and any type of preprocessor if statement
 * elif is not included.
 *
 * @param compiler
 * @return true
 * @return false
 */
bool preprocessor_is_hashtag_and_any_starting_if(struct compile_process *compiler)
{
    return preprocessor_hashtag_and_identifier(compiler, "if") ||
           preprocessor_hashtag_and_identifier(compiler, "ifdef") ||
           preprocessor_hashtag_and_identifier(compiler, "ifndef");
}

void preprocessor_skip_to_endif(struct compile_process *compiler)
{
    while (!preprocessor_hashtag_and_identifier(compiler, "endif"))
    {
        if (!preprocessor_next_token_no_increment(compiler))
        {
            compiler_error(compiler, "Missing #endif\n");
        }
        if (preprocessor_is_hashtag_and_any_starting_if(compiler))
        {
            preprocessor_skip_to_endif(compiler);
            continue;
        }
        preprocessor_next_token(compiler);
    }
}

int preprocessor_parse_evaluate(struct compile_process *compiler, struct vector *token_vec);
bool preprocessor_if_condition(struct compile_process *compiler);

// Drop the rest of a directive line.
static void preprocessor_skip_line(struct compile_process *compiler)
{
    struct token *token = preprocessor_next_token_no_increment(compiler);
    while (token && token->type != TOKEN_TYPE_NEWLINE)
    {
        preprocessor_next_token(compiler);
        token = preprocessor_next_token_no_increment(compiler);
    }
}

// One conditional group, its first condition already evaluated (C11
// 6.10.1). The tokens of the first branch whose condition holds are
// processed and every other branch is skipped, nested groups inside a
// skipped branch whole. An #elif condition is only evaluated while no
// branch has been taken; #else takes over if none was. Only #endif was
// recognised before, so `#if 0 ... #else B` dropped B and `#elif` lines
// went through to the parser.
void preprocessor_read_to_end_if(struct compile_process *compiler, bool true_clause)
{
    bool taken     = true_clause;
    bool active    = true_clause;
    bool seen_else = false;
    while (true)
    {
        if (!preprocessor_next_token_no_increment(compiler))
        {
            compiler_error(compiler, "Missing #endif\n");
        }
        if (preprocessor_hashtag_and_identifier(compiler, "endif"))
        {
            preprocessor_skip_line(compiler);
            return;
        }
        if (preprocessor_hashtag_and_identifier(compiler, "else"))
        {
            if (seen_else)
            {
                compiler_error(compiler, "A second #else in one conditional\n");
            }
            seen_else = true;
            preprocessor_skip_line(compiler);
            active = !taken;
            taken  = true;
            continue;
        }
        if (preprocessor_hashtag_and_identifier(compiler, "elif"))
        {
            if (seen_else)
            {
                compiler_error(compiler, "#elif after #else\n");
            }
            if (taken)
            {
                preprocessor_skip_line(compiler);
                active = false;
            }
            else
            {
                active = preprocessor_if_condition(compiler);
                taken  = active;
            }
            continue;
        }
        if (active)
        {
            preprocessor_handle_token(compiler, preprocessor_next_token(compiler));
            continue;
        }
        if (preprocessor_is_hashtag_and_any_starting_if(compiler))
        {
            preprocessor_skip_to_endif(compiler);
            continue;
        }
        preprocessor_next_token(compiler);
    }
}
int preprocessor_evaluate_number(struct preprocessor_node *node)
{
    return node->const_val.llnum;
}

int preprocessor_evaluate_identifier(struct compile_process *compiler, struct preprocessor_node *node)
{
    struct preprocessor *preprocessor = compiler->preprocessor;
    struct preprocessor_definition *definition = preprocessor_get_definition(preprocessor, node->sval);
    if (!definition)
    {
        // An identifier that is not a macro is 0 in #if (C11 6.10.1p4).
        // It was true, so `#if FOO` took the branch.
        return false;
    }

    if (vector_count(preprocessor_definition_value(definition)) > 1)
    {
        struct vector *node_vector = vector_create(sizeof(struct preprocessor_node *));
        struct expressionable *expressionable = expressionable_create(&preprocessor_expressionable_config, preprocessor_definition_value(definition), node_vector, EXPRESSIONABLE_FLAG_IS_PREPROCESSOR_EXPRESSION);
        expressionable_parse(expressionable);
        struct preprocessor_node *node = expressionable_node_pop(expressionable);
        int val = preprocessor_evaluate(compiler, node);
        return val;
    }

    if (vector_count(preprocessor_definition_value(definition)) == 0)
    {
        return false;
    }

    return preprocessor_definition_evaluated_value(definition, NULL);
}

int preprocessor_arithmetic(struct compile_process *compiler, long left_operand, long right_operand, const char *op)
{
    bool success = false;
    long result = arithmetic(compiler, left_operand, right_operand, op, &success);
    if (!success)
    {
        compiler_error(compiler, "We do not support the operator %s for preprocessor arithmetic\n", op);
    }
    return result;
}

struct preprocessor_function_arguments *preprocessor_function_arguments_create()
{
    struct preprocessor_function_arguments *args = calloc(1, sizeof(struct preprocessor_function_arguments));
    args->arguments = vector_create(sizeof(struct preprocessor_function_argument));
    return args;
}

void preprocessor_number_push_to_function_arguments(struct preprocessor_function_arguments *arguments, int64_t number)
{
    struct token t;
    t.type = TOKEN_TYPE_NUMBER;
    t.llnum = number;
    preprocessor_token_push_to_function_arguments(arguments, &t);
}

bool preprocessor_exp_is_macro_function_call(struct preprocessor_node *node)
{
    return node->type == PREPROCESSOR_EXPRESSION_NODE && S_EQ(node->exp.op, "()") && node->exp.left->type == PREPROCESSOR_IDENTIFIER_NODE;
}

void preprocessor_evaluate_function_call_argument(struct compile_process *compiler, struct preprocessor_node *node, struct preprocessor_function_arguments *arguments)
{
    if (node->type == PREPROCESSOR_EXPRESSION_NODE && S_EQ(node->exp.op, ","))
    {
        preprocessor_evaluate_function_call_argument(compiler, node->exp.left, arguments);
        preprocessor_evaluate_function_call_argument(compiler, node->exp.right, arguments);
        return;
    }
    else if (node->type == PREPROCESSOR_PARENTHESES_NODE)
    {
        preprocessor_evaluate_function_call_argument(compiler, node->parenthesis.exp, arguments);
        return;
    }

    preprocessor_number_push_to_function_arguments(arguments, preprocessor_evaluate(compiler, node));
}

void preprocessor_evaluate_function_call_arguments(struct compile_process *compiler, struct preprocessor_node *node, struct preprocessor_function_arguments *arguments)
{
    preprocessor_evaluate_function_call_argument(compiler, node, arguments);
}

bool preprocessor_is_macro_function(struct preprocessor_definition *definition)
{
    return definition->type == PREPROCESSOR_DEFINITION_MACRO_FUNCTION || definition->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK;
}
int preprocessor_function_arguments_count(struct preprocessor_function_arguments *arguments)
{
    if (!arguments)
    {
        return 0;
    }

    return vector_count(arguments->arguments);
}

int preprocessor_macro_function_push_argument(struct compile_process *compiler, struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments, const char *arg_name, struct vector *definition_token_vec, struct vector *value_vec_target)
{
    int argument_index = preprocessor_definition_argument_exists(definition, arg_name);
    if (argument_index != -1)
    {
        preprocessor_function_argument_push_to_vec(preprocessor_function_argument_at(arguments, argument_index), value_vec_target);
    }
    return argument_index;
}

void preprocessor_token_vec_push_src_token_to_dst(struct compile_process *compiler, struct token *token, struct vector *dst_vec)
{
    vector_push(dst_vec, token);
}

// a typedef with a struct, union or enum body,
// `typedef struct S { ... } T, *PT;`, comes out as `struct S { ... };`
// and `typedef struct S T, *PT;`, so the parser's typedef
// holds only `struct S`; an untagged body gets the tag `__anonymous_N`.
// This runs over the preprocessed tokens, once a whole file is read,
// so the typedef keyword, the body and the names may each come out of a
// macro (glibc's `__STD_TYPE __FSID_T_TYPE __fsid_t;` with
// `#define __FSID_T_TYPE struct { int __val[2]; }`) and the body may hold
// directives. Reading the declaration ahead from the raw tokens, as this
// used to, saw neither.
static int preprocessor_typedef_anonymous_count;

static bool preprocessor_token_is_tag_keyword(struct token *token)
{
    return token_is_keyword(token, "struct") || token_is_keyword(token, "union") || token_is_keyword(token, "enum");
}

void preprocessor_hoist_typedef_bodies(struct compile_process *compiler)
{
    struct vector *in = compiler->token_vec;
    struct vector *out = vector_create(sizeof(struct token));
    int count = vector_count(in);
    for (int i = 0; i < count; i++)
    {
        struct token *token = vector_at(in, i);
        if (!token_is_keyword(token, "typedef"))
        {
            vector_push(out, token);
            continue;
        }

        // Qualifiers before the keyword stay with the typedef.
        int k = i + 1;
        while (k < count && (token_is_keyword(vector_at(in, k), "const") || token_is_keyword(vector_at(in, k), "volatile")))
        {
            k++;
        }
        int open = k + 1;
        if (k < count && open < count && ((struct token *)vector_at(in, open))->type == TOKEN_TYPE_IDENTIFIER)
        {
            open++;
        }
        if (k >= count || !preprocessor_token_is_tag_keyword(vector_at(in, k)) || open >= count || !token_is_symbol(vector_at(in, open), '{'))
        {
            vector_push(out, token);
            continue;
        }
        int close = open;
        int depth = 0;
        for (; close < count; close++)
        {
            struct token *t = vector_at(in, close);
            if (token_is_symbol(t, '{'))
            {
                depth++;
            }
            else if (token_is_symbol(t, '}') && --depth == 0)
            {
                break;
            }
        }
        if (close >= count)
        {
            // An unclosed body: the parser reports it.
            vector_push(out, token);
            continue;
        }

        struct token *keyword = vector_at(in, k);
        struct token tag;
        if (open == k + 2)
        {
            tag = *(struct token *)vector_at(in, k + 1);
        }
        else
        {
            // Names starting with __ belong to the implementation.
            struct buffer *name = buffer_create();
            buffer_printf(name, "__anonymous_%i", preprocessor_typedef_anonymous_count++);
            memset(&tag, 0, sizeof(tag));
            tag.type = TOKEN_TYPE_IDENTIFIER;
            tag.sval = buffer_ptr(name);
            tag.pos = ((struct token *)vector_at(in, open))->pos;
        }

        // struct S { ... };
        vector_push(out, keyword);
        vector_push(out, &tag);
        for (int j = open; j <= close; j++)
        {
            vector_push(out, vector_at(in, j));
        }
        struct token semicolon = {};
        semicolon.type = TOKEN_TYPE_SYMBOL;
        semicolon.cval = ';';
        vector_push(out, &semicolon);

        // typedef [qualifiers] struct S, then the declarators after the body.
        struct token typedef_token = *token;
        typedef_token.whitespace = true;
        typedef_token.pos = keyword->pos;
        vector_push(out, &typedef_token);
        for (int j = i + 1; j < k; j++)
        {
            vector_push(out, vector_at(in, j));
        }
        struct token named[2] = {*keyword, tag};
        for (int j = 0; j < 2; j++)
        {
            named[j].whitespace = true;
            named[j].pos = keyword->pos;
            vector_push(out, &named[j]);
        }
        i = close;
    }
    compiler->token_vec = out;
}

void preprocessor_token_vec_push_src_resolve_definition(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec, struct token *token)
{
    if (token->type == TOKEN_TYPE_IDENTIFIER)
    {
        preprocessor_handle_identifier_for_token_vector(compiler, src_vec, dst_vec, token);
        return;
    }

    preprocessor_token_vec_push_src_token_to_dst(compiler, token, dst_vec);
}

void preprocessor_token_vec_push_src_resolve_definitions(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec)
{
    assert(src_vec != compiler->token_vec);
    vector_set_peek_pointer(src_vec, 0);
    struct token *token = vector_peek(src_vec);
    while (token)
    {
        preprocessor_token_vec_push_src_resolve_definition(compiler, src_vec, dst_vec, token);
        token = vector_peek(src_vec);
    }
}
int preprocessor_macro_function_push_something_definition(struct compile_process *compiler, struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments, struct token *arg_token, struct vector *definition_token_vec, struct vector *value_vec_target)
{
    if (arg_token->type != TOKEN_TYPE_IDENTIFIER)
    {
        return -1;
    }

    const char *arg_name = arg_token->sval;
    int res = preprocessor_macro_function_push_argument(compiler, definition, arguments, arg_name, definition_token_vec, value_vec_target);
    if (res != -1)
    {
        return 0;
    }

    // Not an argument. Other macro names in the replacement stay as
    // they are here and expand when the result is rescanned.
    return -1;
}
void preprocessor_handle_concat_part(struct compile_process *compiler, struct preprocessor_definition *definition,
                                     struct preprocessor_function_arguments *arguments, struct token *token, struct vector *definition_token_vec,
                                     struct vector *value_vec_target)
{
    int argument_index = token->type == TOKEN_TYPE_IDENTIFIER ? preprocessor_definition_argument_exists(definition, token->sval) : -1;
    if (argument_index != -1)
    {
        preprocessor_function_argument_push_raw_to_vec(preprocessor_function_argument_at(arguments, argument_index), value_vec_target);
        return;
    }
    vector_push(value_vec_target, token);
}

void preprocessor_handle_concat_finalize(struct compile_process *compiler, struct vector *tmp_vec, struct vector *value_vec_target)
{
    struct vector *joined_vec = tokens_join_vector(compiler, tmp_vec);
    preprocessor_token_vec_push_src_to_dst(compiler, joined_vec, value_vec_target);
}

bool preprocessor_is_next_double_hash(struct vector *definition_token_vec);

void preprocessor_handle_concat(struct compile_process *compiler, struct preprocessor_definition *definition,
                                struct preprocessor_function_arguments *arguments, struct token *arg_token, struct vector *definition_token_vec,
                                struct vector *value_vec_target)
{
    // Skip the hashtags ##
    vector_peek(definition_token_vec);
    vector_peek(definition_token_vec);

    struct token *right_token = vector_peek(definition_token_vec);
    if (!right_token)
    {
        compiler_error(compiler, "No right operand provided for concat preprocessor operator ##");
    }

    struct vector *tmp_vec = vector_create(sizeof(struct token));
    preprocessor_handle_concat_part(compiler, definition, arguments, arg_token, definition_token_vec, tmp_vec);
    preprocessor_handle_concat_part(compiler, definition, arguments, right_token, definition_token_vec, tmp_vec);
    // `a ## b ## c` is one token of the three (glibc's
    // __MATH_PRECNAME, `name##f##r`). The third operand's `##` went to
    // the stringizing `#` before.
    while (preprocessor_is_next_double_hash(definition_token_vec))
    {
        vector_peek(definition_token_vec);
        vector_peek(definition_token_vec);
        right_token = vector_peek(definition_token_vec);
        if (!right_token)
        {
            compiler_error(compiler, "No right operand provided for concat preprocessor operator ##");
        }
        preprocessor_handle_concat_part(compiler, definition, arguments, right_token, definition_token_vec, tmp_vec);
    }
    // Empty arguments on both sides leave nothing (C11 6.10.3.3p3).
    if (vector_count(tmp_vec))
    {
        preprocessor_handle_concat_finalize(compiler, tmp_vec, value_vec_target);
    }
}

bool preprocessor_is_next_double_hash(struct vector *definition_token_vec)
{
    bool is_double_hash = true;
    vector_save(definition_token_vec);
    struct token *next_token = vector_peek(definition_token_vec);
    if (!token_is_symbol(next_token, '#'))
    {
        is_double_hash = false;
        goto out;
    }

    next_token = vector_peek(definition_token_vec);
    if (!token_is_symbol(next_token, '#'))
    {
        is_double_hash = false;
        goto out;
    }

out:
    vector_restore(definition_token_vec);
    return is_double_hash;
}

void preprocessor_macro_function_push_something(struct compile_process *compiler, struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments, struct token *arg_token, struct vector *definition_token_vec, struct vector *value_vec_target)
{
    if (preprocessor_is_next_double_hash(definition_token_vec))
    {
        preprocessor_handle_concat(compiler, definition, arguments, arg_token, definition_token_vec, value_vec_target);
        return;
    }

    int res = preprocessor_macro_function_push_something_definition(compiler, definition, arguments, arg_token, definition_token_vec, value_vec_target);
    if (res == -1)
    {
        vector_push(value_vec_target, arg_token);
    }
}

// A string or character value written back as C source, for a literal
// the preprocessor made (its token has no spelling).
static void preprocessor_spell_escaped(struct buffer *out, const char *value, char quote)
{
    buffer_write(out, quote);
    for (const char *c = value; *c; c++)
    {
        unsigned char u = (unsigned char)*c;
        if (u == '\\' || u == (unsigned char)quote)
        {
            buffer_write(out, '\\');
            buffer_write(out, *c);
        }
        else if (u == '\n')
        {
            buffer_printf(out, "\\n");
        }
        else if (u < 32 || u == 127)
        {
            buffer_printf(out, "\\%03o", u);
        }
        else
        {
            buffer_write(out, *c);
        }
    }
    buffer_write(out, quote);
}

// One token as # spells it (C11 6.10.3.2p2): as written and, with
// `escape`, a \ put before each " and \ of a string literal or
// character constant. Without it, the value of the literal # makes.
static void preprocessor_stringify_token(struct buffer *out, struct token *token, bool escape)
{
    struct buffer *spelled = NULL;
    const char *text = token->spelling;
    if (!text)
    {
        switch (token->type)
        {
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_KEYWORD:
        case TOKEN_TYPE_OPERATOR:
            text = token->sval;
            break;
        case TOKEN_TYPE_SYMBOL:
            buffer_write(out, token->cval);
            return;
        case TOKEN_TYPE_STRING:
            spelled = buffer_create();
            preprocessor_spell_escaped(spelled, token->sval ? token->sval : "", '"');
            text = buffer_ptr(spelled);
            break;
        case TOKEN_TYPE_NUMBER:
            spelled = buffer_create();
            if (token->num.flags & NUMBER_FLAG_LONG_DOUBLE)
            {
                buffer_printf(spelled, "%sL", token->num.text);
            }
            else if (token->num.flags & NUMBER_FLAG_FLOATING)
            {
                double value;
                memcpy(&value, &token->llnum, sizeof(value));
                buffer_printf(spelled, "%a%s", value, (token->num.flags & NUMBER_FLAG_FLOAT) ? "f" : "");
            }
            else
            {
                buffer_printf(spelled, "%llu%s%s", token->llnum, (token->num.flags & NUMBER_FLAG_UNSIGNED) ? "u" : "", (token->num.flags & NUMBER_FLAG_LONG) ? "l" : "");
            }
            text = buffer_ptr(spelled);
            break;
        default:
            return;
        }
    }
    bool literal = escape && (token->type == TOKEN_TYPE_STRING || token->type == TOKEN_TYPE_NUMBER);
    for (const char *c = text; *c; c++)
    {
        if (literal && (*c == '"' || *c == '\\'))
        {
            buffer_write(out, '\\');
        }
        buffer_write(out, *c);
    }
    if (spelled)
    {
        buffer_free(spelled);
    }
}

// The text of the string literal #x makes of an argument: its tokens
// spelled one after another, one space where there was whitespace (a
// newline or a comment counts) between two of them, none before the
// first or after the last (C11 6.10.3.2p2).
static void preprocessor_stringify_tokens(struct buffer *out, struct vector *tokens, bool escape)
{
    bool written = false;
    bool space = false;
    for (int i = 0; i < vector_count(tokens); i++)
    {
        struct token *token = vector_at(tokens, i);
        if (token->type == TOKEN_TYPE_NEWLINE || token->type == TOKEN_TYPE_COMMENT)
        {
            space = true;
            continue;
        }
        if (written && space)
        {
            buffer_write(out, ' ');
        }
        preprocessor_stringify_token(out, token, escape);
        written = true;
        space = token->whitespace;
    }
}

void preprocessor_handle_function_argument_to_string(struct compile_process *compiler, struct token *hash_token, struct vector *src_vec, struct vector *value_vec_target, struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    struct token *next_token = vector_peek(src_vec);
    if (!next_token || next_token->type != TOKEN_TYPE_IDENTIFIER)
    {
        compiler_error(compiler, "No macro function argument was provided to convert to a string");
    }

    int argument_index = preprocessor_definition_argument_exists(definition, next_token->sval);
    if (argument_index < 0)
    {
        compiler_error(compiler, "Unexpected macro function argument %s", next_token->sval);
    }

    struct preprocessor_function_argument *argument = preprocessor_function_argument_at(arguments, argument_index);
    if (!argument)
    {
        compiler_error(compiler, "BUG: argument exists but failed to pull");
    }

    // the string is spelled from the argument's tokens as
    // written. It was the text the lexer recorded after the macro's (,
    // which took the ) of an enclosing call (`strlen(STR(abc))` gave
    // "abc)"), stopped at a nested ( (`STR(f(2))` gave "f("), and was
    // NULL for tokens from a macro's replacement (`XSTR(TWO)` crashed).
    struct buffer *value = buffer_create();
    preprocessor_stringify_tokens(value, argument->tokens, false);
    struct buffer *spelling = buffer_create();
    buffer_write(spelling, '"');
    preprocessor_stringify_tokens(spelling, argument->tokens, true);
    buffer_write(spelling, '"');
    struct token str_token = {0};
    str_token.type = TOKEN_TYPE_STRING;
    str_token.pos = hash_token->pos;
    str_token.sval = buffer_ptr(value);
    str_token.spelling = buffer_ptr(spelling);
    vector_push(value_vec_target, &str_token);
}

// Check the argument count. For a variadic macro, the arguments from
// the last named one on become one __VA_ARGS__ argument, commas kept.
static void preprocessor_match_macro_arguments(struct compile_process *compiler, struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    int expected = vector_count(definition->standard.arguments);
    int given = preprocessor_function_arguments_count(arguments);
    if (expected == 0 && given == 1 && vector_count(preprocessor_function_argument_at(arguments, 0)->tokens) == 0)
    {
        // F() for a macro with no parameters.
        vector_pop(arguments->arguments);
        return;
    }

    if (!definition->standard.variadic)
    {
        if (given != expected)
        {
            compiler_error(compiler, "Macro %s takes %i arguments, %i given", definition->name, expected, given);
        }
        return;
    }

    if (given < expected - 1)
    {
        compiler_error(compiler, "Macro %s takes at least %i arguments, %i given", definition->name, expected - 1, given);
    }

    struct preprocessor_function_argument rest = {};
    rest.tokens = vector_create(sizeof(struct token));
    for (int i = expected - 1; i < given; i++)
    {
        if (i > expected - 1)
        {
            struct token comma = {};
            comma.type = TOKEN_TYPE_OPERATOR;
            comma.sval = ",";
            comma.whitespace = preprocessor_function_argument_at(arguments, i - 1)->comma_whitespace;
            vector_push(rest.tokens, &comma);
        }
        preprocessor_function_argument_push_raw_to_vec(preprocessor_function_argument_at(arguments, i), rest.tokens);
    }
    while (preprocessor_function_arguments_count(arguments) > expected - 1)
    {
        vector_pop(arguments->arguments);
    }
    vector_push(arguments->arguments, &rest);
}

// Arguments are macro-expanded before they replace their parameters.
static void preprocessor_expand_macro_arguments(struct compile_process *compiler, struct preprocessor_function_arguments *arguments)
{
    int total = preprocessor_function_arguments_count(arguments);
    for (int i = 0; i < total; i++)
    {
        struct preprocessor_function_argument *argument = preprocessor_function_argument_at(arguments, i);
        argument->expanded = vector_create(sizeof(struct token));
        preprocessor_token_vec_push_src_resolve_definitions(compiler, argument->tokens, argument->expanded);
    }
}

int preprocessor_macro_function_execute(struct compile_process *compiler, const char *function_name, struct preprocessor_function_arguments *arguments, int flags, struct vector *dst_vec)
{
    struct preprocessor *preprocessor = compiler->preprocessor;
    struct preprocessor_definition *definition = preprocessor_get_definition(preprocessor, function_name);
    if (!definition)
    {
        compiler_error(compiler, "Trying to call unknown macro function %s", function_name);
    }
    // A typedef of the same name, defined first, is not the macro.
    if (definition->type == PREPROCESSOR_DEFINITION_TYPEDEF && preprocessor_get_definition_other_kind(preprocessor, definition))
    {
        definition = preprocessor_get_definition_other_kind(preprocessor, definition);
    }

    if (!preprocessor_is_macro_function(definition))
    {
        compiler_error(compiler, "This definition %s is not a macro function", function_name);
    }

    if (definition->type != PREPROCESSOR_DEFINITION_NATIVE_CALLBACK)
    {
        preprocessor_match_macro_arguments(compiler, definition, arguments);
        if (!(flags & PREPROCESSOR_FLAG_EVALUATE_NODE))
        {
            preprocessor_expand_macro_arguments(compiler, arguments);
        }
    }

    struct vector *value_vec_target = vector_create(sizeof(struct token));
    struct vector *definition_token_vec = preprocessor_definition_value_with_arguments(definition, arguments);
    vector_set_peek_pointer(definition_token_vec, 0);
    struct token *token = vector_peek(definition_token_vec);
    while (token)
    {
        if (token_is_symbol(token, '#'))
        {
            preprocessor_handle_function_argument_to_string(compiler, token, definition_token_vec, value_vec_target, definition, arguments);
            token = vector_peek(definition_token_vec);
            continue;
        }

        preprocessor_macro_function_push_something(compiler, definition, arguments, token, definition_token_vec, value_vec_target);
        token = vector_peek(definition_token_vec);
    }

    if (flags & PREPROCESSOR_FLAG_EVALUATE_NODE)
    {
        return preprocessor_parse_evaluate(compiler, value_vec_target);
    }

    // Rescan the result for more macros, with this one switched off.
    definition->expanding++;
    preprocessor_token_vec_push_src_resolve_definitions(compiler, value_vec_target, dst_vec);
    definition->expanding--;
    return 0;
}
int preprocessor_evaluate_function_call(struct compile_process *compiler, struct preprocessor_node *node)
{
    const char *macro_func_name = node->exp.left->sval;
    struct preprocessor_node *call_arguments = node->exp.right->parenthesis.exp;
    struct preprocessor_function_arguments *arguments = preprocessor_function_arguments_create();

    // Evaluate all of the preprocessor arguments
    preprocessor_evaluate_function_call_arguments(compiler, call_arguments, arguments);
    return preprocessor_macro_function_execute(compiler, macro_func_name, arguments, PREPROCESSOR_FLAG_EVALUATE_NODE, NULL);
}

int preprocessor_evaluate_exp(struct compile_process *compiler, struct preprocessor_node *node)
{
    if (preprocessor_exp_is_macro_function_call(node))
    {
        return preprocessor_evaluate_function_call(compiler, node);
    }

    long left_operand = preprocessor_evaluate(compiler, node->exp.left);
    if (node->exp.right->type == PREPROCESSOR_TENARY_NODE)
    {
        if (left_operand)
        {
            return preprocessor_evaluate(compiler, node->exp.right->tenary.true_node);
        }
        else
        {
            return preprocessor_evaluate(compiler, node->exp.right->tenary.false_node);
        }
    }

    long right_operand = preprocessor_evaluate(compiler, node->exp.right);
    return preprocessor_arithmetic(compiler, left_operand, right_operand, node->exp.op);
}
int preprocessor_evaluate(struct compile_process *compiler, struct preprocessor_node *root_node)
{
    struct preprocessor_node *current = root_node;
    int result = 0;
    switch (current->type)
    {
    case PREPROCESSOR_NUMBER_NODE:
        result = preprocessor_evaluate_number(current);
        break;

    case PREPROCESSOR_IDENTIFIER_NODE:
        result = preprocessor_evaluate_identifier(compiler, current);
        break;

    case PREPROCESSOR_UNARY_NODE:
        result = preprocessor_evaluate_unary(compiler, current);
        break;

    case PREPROCESSOR_EXPRESSION_NODE:
        result = preprocessor_evaluate_exp(compiler, current);
        break;

    case PREPROCESSOR_PARENTHESES_NODE:
        result = preprocessor_evaluate_parentheses(compiler, current);
        break;

    case PREPROCESSOR_JOINED_NODE:
        result = preprocessor_evaluate_joined_node(compiler, current);
        break;
    }
    return result;
}

int preprocessor_parse_evaluate(struct compile_process *compiler, struct vector *token_vec)
{
    struct token *first_token = vector_peek_no_increment(token_vec);
    if (!first_token || first_token->type == TOKEN_TYPE_NEWLINE)
    {
        compiler_error(compiler, "Expected an expression after #if or #elif");
    }
    struct vector *node_vector = vector_create(sizeof(struct preprocessor_node *));
    struct expressionable *expressionable = expressionable_create(&preprocessor_expressionable_config, token_vec, node_vector, 0);
    expressionable_parse(expressionable);
    struct preprocessor_node *root_node = vector_count(node_vector) ? expressionable_node_pop(expressionable) : NULL;
    if (!root_node)
    {
        compiler_error(compiler, "Expected an expression after #if or #elif");
    }
    return preprocessor_evaluate(compiler, root_node);
}

// the condition of #if and #elif (C11 6.10.1). The rest of
// the line, with `defined X` and `defined(X)` replaced by 1 or 0, has
// its macros expanded; a `defined` a macro gave is read the same way
// (as gcc does), every identifier or keyword left is 0, and the result
// is worked out in intmax_t or uintmax_t by the usual conversions. A
// name that is not a macro, followed by parentheses (`__has_include(
// <x.h>)`, `__has_attribute(...)`), is 0 too, with what is in them.
// The expression used to be parsed from the raw tokens with macros
// looked up on the way: `!defined A && !defined B` read as
// `!defined (A && ...)`, which glibc's guards are full of, and
// `#if -1` was false.
struct preprocessor_if_value
{
    long long value;
    bool is_unsigned;
};

struct preprocessor_if_reader
{
    struct compile_process *compiler;
    struct vector *tokens;
    int at;
    // Inside the operand of && or || or ?: that is not evaluated, where
    // a division by zero is no error.
    int unevaluated;
};

static struct token *preprocessor_if_peek(struct preprocessor_if_reader *reader)
{
    return reader->at < vector_count(reader->tokens) ? vector_at(reader->tokens, reader->at) : NULL;
}

static bool preprocessor_if_is(struct token *token, const char *op)
{
    if (!token)
    {
        return false;
    }
    if (token->type == TOKEN_TYPE_OPERATOR)
    {
        return S_EQ(token->sval, op);
    }
    return token->type == TOKEN_TYPE_SYMBOL && !op[1] && token->cval == op[0];
}

static bool preprocessor_if_take(struct preprocessor_if_reader *reader, const char *op)
{
    if (!preprocessor_if_is(preprocessor_if_peek(reader), op))
    {
        return false;
    }
    reader->at++;
    return true;
}

static bool preprocessor_if_is_name(struct token *token)
{
    return token && (token->type == TOKEN_TYPE_IDENTIFIER || token->type == TOKEN_TYPE_KEYWORD);
}

// `defined X` or `defined ( X )` at `*at` of `tokens` (the `defined`
// already read): whether X is a macro, and *at past it.
static bool preprocessor_if_defined(struct compile_process *compiler, struct vector *tokens, int *at)
{
    bool parenthesis = *at < vector_count(tokens) && preprocessor_if_is(vector_at(tokens, *at), "(");
    if (parenthesis)
    {
        (*at)++;
    }
    struct token *name = *at < vector_count(tokens) ? vector_at(tokens, *at) : NULL;
    if (!preprocessor_if_is_name(name))
    {
        compiler_error(compiler, "Expected a macro name after defined");
    }
    (*at)++;
    if (parenthesis)
    {
        if (*at >= vector_count(tokens) || !preprocessor_if_is(vector_at(tokens, *at), ")"))
        {
            compiler_error(compiler, "Expected ')' after defined(%s", name->sval);
        }
        (*at)++;
    }
    struct preprocessor_definition *definition = preprocessor_get_definition(compiler->preprocessor, name->sval);
    return definition && definition->type != PREPROCESSOR_DEFINITION_TYPEDEF;
}

static struct token preprocessor_if_number(long long value)
{
    struct token number = {};
    number.type = TOKEN_TYPE_NUMBER;
    number.llnum = value;
    return number;
}

static struct preprocessor_if_value preprocessor_if_conditional(struct preprocessor_if_reader *reader);

static struct preprocessor_if_value preprocessor_if_primary(struct preprocessor_if_reader *reader)
{
    struct preprocessor_if_value result = {0, false};
    struct token *token = preprocessor_if_peek(reader);
    if (!token)
    {
        compiler_error(reader->compiler, "The #if expression ends too early");
    }
    reader->at++;
    if (token->type == TOKEN_TYPE_NUMBER)
    {
        if (token->num.error)
        {
            compiler_error(reader->compiler, "%s", token->num.error);
        }
        if (token->num.flags & NUMBER_FLAG_FLOATING)
        {
            compiler_error(reader->compiler, "A floating constant in an #if expression");
        }
        result.value = token->llnum;
        result.is_unsigned = (token->num.flags & NUMBER_FLAG_UNSIGNED) != 0;
        return result;
    }
    if (preprocessor_if_is(token, "("))
    {
        result = preprocessor_if_conditional(reader);
        while (preprocessor_if_take(reader, ","))
        {
            result = preprocessor_if_conditional(reader);
        }
        if (!preprocessor_if_take(reader, ")"))
        {
            compiler_error(reader->compiler, "Expected ')' in the #if expression");
        }
        return result;
    }
    if (preprocessor_if_is_name(token))
    {
        if (S_EQ(token->sval, "defined"))
        {
            result.value = preprocessor_if_defined(reader->compiler, reader->tokens, &reader->at);
            return result;
        }
        if (preprocessor_if_is(preprocessor_if_peek(reader), "("))
        {
            int depth = 0;
            do
            {
                struct token *t = preprocessor_if_peek(reader);
                if (!t)
                {
                    compiler_error(reader->compiler, "Expected ')' after %s( in the #if expression", token->sval);
                }
                depth += preprocessor_if_is(t, "(") - preprocessor_if_is(t, ")");
                reader->at++;
            } while (depth > 0);
        }
        return result;
    }
    compiler_error(reader->compiler, "Unexpected token in the #if expression");
    return result;
}

static struct preprocessor_if_value preprocessor_if_unary(struct preprocessor_if_reader *reader)
{
    const char *ops[] = {"-", "+", "!", "~"};
    for (int i = 0; i < 4; i++)
    {
        if (preprocessor_if_take(reader, ops[i]))
        {
            struct preprocessor_if_value v = preprocessor_if_unary(reader);
            switch (ops[i][0])
            {
            case '-':
                v.value = (long long)(0ULL - (unsigned long long)v.value);
                break;
            case '!':
                v.value = !v.value;
                v.is_unsigned = false;
                break;
            case '~':
                v.value = ~v.value;
                break;
            }
            return v;
        }
    }
    return preprocessor_if_primary(reader);
}

// One binary operator on a and b, after the usual arithmetic conversions.
static struct preprocessor_if_value preprocessor_if_apply(struct preprocessor_if_reader *reader, const char *op,
                                                          struct preprocessor_if_value a, struct preprocessor_if_value b)
{
    struct preprocessor_if_value r = {0, a.is_unsigned || b.is_unsigned};
    unsigned long long ua = a.value;
    unsigned long long ub = b.value;
    bool u = r.is_unsigned;
    if (S_EQ(op, "<<") || S_EQ(op, ">>"))
    {
        r.is_unsigned = a.is_unsigned;
        int count = (int)(b.value & 63);
        if (S_EQ(op, "<<"))
        {
            r.value = (long long)(ua << count);
        }
        else
        {
            r.value = a.is_unsigned ? (long long)(ua >> count) : a.value >> count;
        }
        return r;
    }
    if ((S_EQ(op, "/") || S_EQ(op, "%")) && b.value == 0)
    {
        if (!reader->unevaluated)
        {
            compiler_error(reader->compiler, "Division by zero in an #if expression");
        }
        return r;
    }
    bool compare = true;
    if (S_EQ(op, "<"))
        r.value = u ? ua < ub : a.value < b.value;
    else if (S_EQ(op, ">"))
        r.value = u ? ua > ub : a.value > b.value;
    else if (S_EQ(op, "<="))
        r.value = u ? ua <= ub : a.value <= b.value;
    else if (S_EQ(op, ">="))
        r.value = u ? ua >= ub : a.value >= b.value;
    else if (S_EQ(op, "=="))
        r.value = ua == ub;
    else if (S_EQ(op, "!="))
        r.value = ua != ub;
    else
        compare = false;
    if (compare)
    {
        r.is_unsigned = false;
        return r;
    }
    switch (op[0])
    {
    case '*':
        r.value = (long long)(ua * ub);
        break;
    case '/':
        r.value = u ? (long long)(ua / ub) : a.value / b.value;
        break;
    case '%':
        r.value = u ? (long long)(ua % ub) : a.value % b.value;
        break;
    case '+':
        r.value = (long long)(ua + ub);
        break;
    case '-':
        r.value = (long long)(ua - ub);
        break;
    case '&':
        r.value = (long long)(ua & ub);
        break;
    case '^':
        r.value = (long long)(ua ^ ub);
        break;
    case '|':
        r.value = (long long)(ua | ub);
        break;
    }
    return r;
}

// The binary operators from the loosest (level 0, |) to the tightest.
static const char *preprocessor_if_levels[][5] = {
    {"|", 0},
    {"^", 0},
    {"&", 0},
    {"==", "!=", 0},
    {"<", ">", "<=", ">=", 0},
    {"<<", ">>", 0},
    {"+", "-", 0},
    {"*", "/", "%", 0},
};

static struct preprocessor_if_value preprocessor_if_binary(struct preprocessor_if_reader *reader, int level)
{
    if (level == (int)(sizeof(preprocessor_if_levels) / sizeof(preprocessor_if_levels[0])))
    {
        return preprocessor_if_unary(reader);
    }
    struct preprocessor_if_value left = preprocessor_if_binary(reader, level + 1);
    while (true)
    {
        const char *op = NULL;
        for (int i = 0; preprocessor_if_levels[level][i]; i++)
        {
            if (preprocessor_if_is(preprocessor_if_peek(reader), preprocessor_if_levels[level][i]))
            {
                op = preprocessor_if_levels[level][i];
            }
        }
        if (!op)
        {
            return left;
        }
        reader->at++;
        struct preprocessor_if_value right = preprocessor_if_binary(reader, level + 1);
        left = preprocessor_if_apply(reader, op, left, right);
    }
}

static struct preprocessor_if_value preprocessor_if_logical_and(struct preprocessor_if_reader *reader)
{
    struct preprocessor_if_value left = preprocessor_if_binary(reader, 0);
    while (preprocessor_if_take(reader, "&&"))
    {
        bool skip = !left.value;
        reader->unevaluated += skip;
        struct preprocessor_if_value right = preprocessor_if_binary(reader, 0);
        reader->unevaluated -= skip;
        left.value = left.value && right.value;
        left.is_unsigned = false;
    }
    return left;
}

static struct preprocessor_if_value preprocessor_if_logical_or(struct preprocessor_if_reader *reader)
{
    struct preprocessor_if_value left = preprocessor_if_logical_and(reader);
    while (preprocessor_if_take(reader, "||"))
    {
        bool skip = left.value != 0;
        reader->unevaluated += skip;
        struct preprocessor_if_value right = preprocessor_if_logical_and(reader);
        reader->unevaluated -= skip;
        left.value = left.value || right.value;
        left.is_unsigned = false;
    }
    return left;
}

static struct preprocessor_if_value preprocessor_if_conditional(struct preprocessor_if_reader *reader)
{
    struct preprocessor_if_value condition = preprocessor_if_logical_or(reader);
    if (!preprocessor_if_take(reader, "?"))
    {
        return condition;
    }
    reader->unevaluated += !condition.value;
    struct preprocessor_if_value yes = preprocessor_if_conditional(reader);
    reader->unevaluated -= !condition.value;
    if (!preprocessor_if_take(reader, ":"))
    {
        compiler_error(reader->compiler, "Expected ':' in the #if expression");
    }
    reader->unevaluated += condition.value != 0;
    struct preprocessor_if_value no = preprocessor_if_conditional(reader);
    reader->unevaluated -= condition.value != 0;
    struct preprocessor_if_value result = condition.value ? yes : no;
    result.is_unsigned = yes.is_unsigned || no.is_unsigned;
    return result;
}

bool preprocessor_if_condition(struct compile_process *compiler)
{
    struct vector *line = vector_create(sizeof(struct token));
    struct token *token = preprocessor_next_token_no_increment(compiler);
    while (token && token->type != TOKEN_TYPE_NEWLINE)
    {
        preprocessor_next_token(compiler);
        if (token_is_symbol(token, '\\'))
        {
            // A backslash at the end of the line goes on on the next.
            struct token *next = preprocessor_next_token_no_increment(compiler);
            if (next && next->type == TOKEN_TYPE_NEWLINE)
            {
                preprocessor_next_token(compiler);
            }
            token = preprocessor_next_token_no_increment(compiler);
            continue;
        }
        if (token->type != TOKEN_TYPE_COMMENT)
        {
            vector_push(line, token);
        }
        token = preprocessor_next_token_no_increment(compiler);
    }
    if (!vector_count(line))
    {
        compiler_error(compiler, "Expected an expression after #if or #elif");
    }

    // defined, before the macros are expanded.
    struct vector *known = vector_create(sizeof(struct token));
    for (int at = 0; at < vector_count(line);)
    {
        struct token *t = vector_at(line, at++);
        if (preprocessor_if_is_name(t) && S_EQ(t->sval, "defined"))
        {
            struct token number = preprocessor_if_number(preprocessor_if_defined(compiler, line, &at));
            vector_push(known, &number);
            continue;
        }
        vector_push(known, t);
    }

    struct vector *expanded = vector_create(sizeof(struct token));
    preprocessor_token_vec_push_src_resolve_definitions(compiler, known, expanded);
    struct preprocessor_if_reader reader = {compiler, expanded, 0, 0};
    struct preprocessor_if_value value = preprocessor_if_conditional(&reader);
    while (preprocessor_if_take(&reader, ","))
    {
        value = preprocessor_if_conditional(&reader);
    }
    if (reader.at != vector_count(expanded))
    {
        compiler_error(compiler, "Unexpected token in the #if expression");
    }
    vector_free(line);
    vector_free(known);
    vector_free(expanded);
    return value.value != 0;
}

void preprocessor_handle_if_token(struct compile_process *compiler)
{
    preprocessor_read_to_end_if(compiler, preprocessor_if_condition(compiler));
}

void preprocessor_handle_ifdef_token(struct compile_process *compiler)
{
    struct token *condition_token = preprocessor_next_token(compiler);
    if (!condition_token)
    {
        compiler_error(compiler, "No condition token was provided.\n");
    }

    struct preprocessor_definition *definition = preprocessor_get_definition(compiler->preprocessor, condition_token->sval);

    // Read the body of the ifdef
    preprocessor_read_to_end_if(compiler, definition != NULL);
}

void preprocessor_handle_ifndef_token(struct compile_process *compiler)
{
    struct token *condition_token = preprocessor_next_token(compiler);
    if (!condition_token)
    {
        compiler_error(compiler, "No condition token was provided\n");
    }
    struct preprocessor_definition *definition = preprocessor_get_definition(compiler->preprocessor, condition_token->sval);
    preprocessor_read_to_end_if(compiler, definition == NULL);
}

struct token* preprocessor_next_token_skip_nl(struct compile_process* compiler)
{
    struct token* token = preprocessor_next_token(compiler);
    while (token && token->type == TOKEN_TYPE_NEWLINE)
    {
        token = preprocessor_next_token(compiler);
    }

    return token;
}

void preprocessor_handle_include_token(struct compile_process* compiler)
{
    struct token* file_path_token = preprocessor_next_token_skip_nl(compiler);
    if (!file_path_token)
    {
        compiler_error(compiler, "No file path provided for include");
    }

    // Each nested #include runs the preprocessor again on the C stack;
    // a file that includes itself without a guard would never end.
    // gcc stops at the same depth.
    static int include_depth = 0;
    if (include_depth >= 200)
    {
        compiler_error(compiler, "#include nested more than 200 deep");
    }
    include_depth++;
    bool quoted = !(file_path_token->flags & TOKEN_FLAG_ANGLED_INCLUDE);
    struct compile_process* new_compile_process = compile_include_search(file_path_token->sval, compiler, quoted);
    include_depth--;
    if (!new_compile_process)
    {
        PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION handler =
            preprocessor_static_include_handler_for(file_path_token->sval);
        if (handler)
        {
            // Handle it
            preprocessor_create_static_include(compiler->preprocessor, file_path_token->sval, handler);
            return;
        }

        compiler_error(compiler, "The file does not exist %s unable to include", file_path_token->sval);
    }

    preprocessor_token_vec_push_src(compiler, new_compile_process->token_vec);
}

static bool preprocessor_is_once_file(struct preprocessor *preprocessor, const char *path)
{
    vector_set_peek_pointer(preprocessor->once_files, 0);
    const char *once_path = vector_peek_ptr(preprocessor->once_files);
    while (once_path)
    {
        if (S_EQ(once_path, path))
        {
            return true;
        }
        once_path = vector_peek_ptr(preprocessor->once_files);
    }
    return false;
}

// `#pragma once` records the file; when the file comes back through a
// second #include, the rest of it is dropped. Any other pragma is
// ignored, as C11 6.10.6 allows for pragmas an implementation does
// not know.
static void preprocessor_handle_pragma_token(struct compile_process *compiler)
{
    struct token *token = preprocessor_next_token_no_increment(compiler);
    if (token && token->type == TOKEN_TYPE_IDENTIFIER && S_EQ(token->sval, "once"))
    {
        const char *path = compiler->cfile.abs_path;
        if (preprocessor_is_once_file(compiler->preprocessor, path))
        {
            while (preprocessor_next_token(compiler))
            {
            }
            vector_clear(compiler->token_vec);
            return;
        }
        vector_push(compiler->preprocessor->once_files, &path);
    }
    preprocessor_skip_line(compiler);
}

int preprocessor_handle_hashtag_token(struct compile_process *compiler, struct token *token)
{
    bool is_preprocessed = false;
    struct token *next_token = preprocessor_next_token(compiler);
    if (!next_token)
    {
        // A `#` alone at the end of the file is a null directive; it
        // read past the end and segfaulted.
        return 1;
    }
    if (preprocessor_token_is_define(next_token))
    {
        // handle the definition token.
        preprocessor_handle_definition_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_undef(next_token))
    {
        preprocessor_handle_undef_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_warning(next_token))
    {
        preprocessor_handle_warning_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_error(next_token))
    {
        preprocessor_handle_error_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_if(next_token))
    {
        preprocessor_handle_if_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_ifdef(next_token))
    {
        preprocessor_handle_ifdef_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_ifndef(next_token))
    {
        preprocessor_handle_ifndef_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_include(next_token))
    {
        preprocessor_handle_include_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_preprocessor_keyword(next_token) && S_EQ(next_token->sval, "pragma"))
    {
        preprocessor_handle_pragma_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_preprocessor_keyword(next_token) && S_EQ(next_token->sval, "line"))
    {
        // Line numbers in diagnostics keep counting source lines.
        preprocessor_skip_line(compiler);
        is_preprocessed = true;
    }

    return is_preprocessed;
}
void preprocessor_handle_symbol(struct compile_process *compiler, struct token *token)
{
    int is_preprocessed = false;
    if (token->cval == '#')
    {
        is_preprocessed = preprocessor_handle_hashtag_token(compiler, token);
    }

    if (!is_preprocessed)
    {
        preprocessor_token_push_dst(compiler, token);
    }
}

struct token *preprocessor_handle_identifier_macro_call_argument_parse_parentheses(struct compile_process *compiler, struct vector *src_vec, struct vector *value_vec, struct preprocessor_function_arguments *arguments, struct token *left_bracket_token)
{
    // Push the  left bracket token to the stack
    vector_push(value_vec, left_bracket_token);

    struct token *next_token = vector_peek(src_vec);
    while (next_token && !token_is_symbol(next_token, ')'))
    {
        if (token_is_operator(next_token, "("))
        {
            // The token after the inner group may itself be ( or ):
            // look at it again rather than copying it, or
            // F((g(x))) ended the argument at the wrong ).
            next_token = preprocessor_handle_identifier_macro_call_argument_parse_parentheses(compiler, src_vec, value_vec, arguments, next_token);
            continue;
        }
        vector_push(value_vec, next_token);
        next_token = vector_peek(src_vec);
    }

    if (!next_token)
    {
        compiler_error(compiler, "You did not end your parentheses expecting a )");
    }
    vector_push(value_vec, next_token);
    return vector_peek(src_vec);
}

void preprocessor_function_argument_push(struct preprocessor_function_arguments *arguments, struct vector *value_vec)
{
    struct preprocessor_function_argument arg = {};
    arg.tokens = vector_clone(value_vec);
    vector_push(arguments->arguments, &arg);
}

void preprocessor_handle_identifier_macro_call_argument(struct preprocessor_function_arguments *arguments, struct vector *token_vec)
{
    preprocessor_function_argument_push(arguments, token_vec);
}

struct token *preprocessor_handle_identifier_macro_call_argument_parse(struct compile_process *compiler, struct vector *src_vec, struct vector *value_vec, struct preprocessor_function_arguments *arguments, struct token *token)
{
    if (token_is_operator(token, "("))
    {
        return preprocessor_handle_identifier_macro_call_argument_parse_parentheses(compiler, src_vec, value_vec, arguments, token);
    }
    if (token_is_symbol(token, ')'))
    {
        // We are done handling the call argument
        preprocessor_handle_identifier_macro_call_argument(arguments, value_vec);
        return NULL;
    }

    if (token_is_operator(token, ","))
    {
        preprocessor_handle_identifier_macro_call_argument(arguments, value_vec);
        struct preprocessor_function_argument *ended = vector_back(arguments->arguments);
        ended->comma_whitespace = token->whitespace;
        // Clear the value vector ready for next argument
        vector_clear(value_vec);
        token = vector_peek(src_vec);
        return token;
    }

    vector_push(value_vec, token);
    token = vector_peek(src_vec);
    return token;
}
struct preprocessor_function_arguments *preprocessor_handle_identifier_macro_call_arguments(struct compile_process *compiler, struct vector *src_vec)
{
    // Skip the left bracket
    vector_peek(src_vec);

    struct preprocessor_function_arguments *arguments = preprocessor_function_arguments_create();
    struct token *token = vector_peek(src_vec);
    struct vector *value_vec = vector_create(sizeof(struct token));
    while (token)
    {
        token = preprocessor_handle_identifier_macro_call_argument_parse(compiler, src_vec, value_vec, arguments, token);
    }

    vector_free(value_vec);
    return arguments;
}

// the whitespace after a macro use (its name, or the ) of a
// call) is after the last token of the expansion, not whatever followed
// that token in the #define: `XSTR(E c)` with `#define E a + b` is
// "a + b c". Only # and -E look at it.
static void preprocessor_expansion_whitespace(struct vector *dst_vec, int before, bool whitespace)
{
    if (vector_count(dst_vec) > before)
    {
        struct token *last = vector_back(dst_vec);
        last->whitespace = whitespace;
    }
}

int preprocessor_handle_identifier_for_token_vector(struct compile_process *compiler, struct vector *src_vec, struct vector *dst_vec, struct token *token)
{
    struct preprocessor_definition *definition = preprocessor_get_definition(compiler->preprocessor, token->sval);
    if (!definition || definition->expanding)
    {
        // Nothing to do with us, maybe a variable of some kind. Not macro
        // related. A macro's own name inside its expansion stays too.
        preprocessor_token_push_to_dst(dst_vec, token);
        return -1;
    }

    // A function-like macro and a typedef may share a name (compiler.h's
    // GENERATOR_END_EXPRESSION): the name followed by `(` is the macro,
    // anywhere else the typedef. They share one table, so look for the
    // other kind too.
    bool is_call = token_is_operator(vector_peek_no_increment(src_vec), "(");
    struct preprocessor_definition *other = preprocessor_get_definition_other_kind(compiler->preprocessor, definition);
    if (other && (other->type == PREPROCESSOR_DEFINITION_TYPEDEF) != is_call)
    {
        definition = other;
    }

    if (definition->type == PREPROCESSOR_DEFINITION_TYPEDEF)
    {
        preprocessor_token_vec_push_src_to_dst(compiler, preprocessor_definition_value(definition), dst_vec);
        return 0;
    }

    if (definition->type == PREPROCESSOR_DEFINITION_MACRO_FUNCTION || (definition->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK && is_call))
    {
        if (!is_call)
        {
            // A function-like macro name without a call is a plain name.
            preprocessor_token_push_to_dst(dst_vec, token);
            return -1;
        }
        struct preprocessor_function_arguments *arguments = preprocessor_handle_identifier_macro_call_arguments(compiler, src_vec);
        struct token *right_bracket = vector_peek_at(src_vec, src_vec->pindex - 1);
        bool after = right_bracket && right_bracket->whitespace;
        const char *function_name = token->sval;
        int before = vector_count(dst_vec);
        preprocessor_macro_function_execute(compiler, function_name, arguments, 0, dst_vec);
        preprocessor_expansion_whitespace(dst_vec, before, after);
        return 0;
    }

    // The replacement is a vector shared by every use of the macro, so
    // rescan a copy: a nested use would move its peek pointer.
    struct vector *definition_val = vector_clone(preprocessor_definition_value(definition));
    int before = vector_count(dst_vec);
    definition->expanding++;
    preprocessor_token_vec_push_src_resolve_definitions(compiler, definition_val, dst_vec);
    definition->expanding--;
    preprocessor_expansion_whitespace(dst_vec, before, token->whitespace);
    return 0;
}
int preprocessor_handle_identifier(struct compile_process *compiler, struct token *token)
{
    return preprocessor_handle_identifier_for_token_vector(compiler, compiler->token_vec_original, compiler->token_vec, token);
}
void preprocessor_handle_keyword(struct compile_process *compiler, struct token *token)
{
    // This is not for us to deal with
    preprocessor_token_push_dst(compiler, token);
}


void preprocessor_handle_token(struct compile_process *compiler, struct token *token)
{
    switch (token->type)
    {
        // Handle all tokens here..

    case TOKEN_TYPE_SYMBOL:
        preprocessor_handle_symbol(compiler, token);
        break;

    case TOKEN_TYPE_KEYWORD:
        preprocessor_handle_keyword(compiler, token);
        break;

    case TOKEN_TYPE_IDENTIFIER:
        preprocessor_handle_identifier(compiler, token);
        break;
    case TOKEN_TYPE_NEWLINE:
        // Ignored
        break;

    default:
        preprocessor_token_push_dst(compiler, token);
    };
}
int preprocessor_run(struct compile_process *compiler)
{
    preprocessor_add_included_file(compiler->preprocessor, compiler->cfile.abs_path);
    vector_set_peek_pointer(compiler->token_vec_original, 0);
    struct token *token = preprocessor_next_token(compiler);
    while (token)
    {
        preprocessor_handle_token(compiler, token);
        token = preprocessor_next_token(compiler);
    }
    // An included file's typedefs are lifted already; doing it again
    // finds no body after `typedef struct S`.
    preprocessor_hoist_typedef_bodies(compiler);

    return 0;
}