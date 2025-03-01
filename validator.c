#include "compiler.h"
#include "helpers/vector.h"

// ch243: scaffolded validator. Sits between parse and codegen so
// later chapters can hang checks (scope, types, statement shape,
// etc.) off this pipeline stage.
// ch244: validator-owned scope + tree iteration. The validator
// borrows the default resolver to track scope state while it walks
// the parse tree.

static struct compile_process* validator_current_compile_process;
static struct node*            current_function;

// ch257: forward decls so validate_statement can call back into
// validate_variable / validate_body without ordering pain.
void validate_variable(struct node* var_node);
void validate_body(struct body* body);

// ch244: open/close a validator scope (delegates to the default
// resolver's scope manager so symbol lookups in the validator agree
// with what codegen will see).
void validation_new_scope(int flags)
{
    resolver_default_new_scope(validator_current_compile_process->resolver, flags);
}

void validation_end_scope(void)
{
    resolver_default_finish_scope(validator_current_compile_process->resolver);
}

// ch244: walk to the next top-level node in the parse tree.
struct node* validation_next_tree_node(void)
{
    return vector_peek_ptr(validator_current_compile_process->node_tree_vec);
}

void validate_initialize(struct compile_process* process)
{
    validator_current_compile_process = process;
    vector_set_peek_pointer(process->node_tree_vec, 0);
    symresolver_new_table(process);
}

void validate_destruct(struct compile_process* process)
{
    symresolver_end_table(process);
    vector_set_peek_pointer(process->node_tree_vec, 0);
}

// ch255: assert that no symbol with `name` is already registered. The
// caller passes a human-readable kind so the diagnostic is useful.
void validate_symbol_unique(const char* name, const char* type_of_symbol, struct node* node)
{
    struct symbol* sym = symresolver_get_symbol(validator_current_compile_process, name);
    if (sym)
    {
        compiler_node_error(node, "Cannot define %s you have already defined a symbol with the name %s", type_of_symbol, name);
    }
}

// ch257: validate an identifier - resolver_follow it; complain
// loudly if it doesn't resolve.
void validate_identifier(struct node* node)
{
    struct resolver_result* result = resolver_follow(validator_current_compile_process->resolver, node);
    if (!resolver_result_ok(result))
    {
        compiler_error(validator_current_compile_process, "The variable does not exist");
    }
}

// ch257: expression-position validation. Only IDENTIFIER is wired
// today; binary expressions / calls land in later validator
// chapters.
void validate_expressionable(struct node* node)
{
    switch (node->type)
    {
        case NODE_TYPE_IDENTIFIER:
            validate_identifier(node);
            break;
    }
}

// ch257: return statement. If the function's return type is void
// (with no pointer indirection) and the return carries a value,
// it's a compile error. Otherwise recurse into the value as an
// expressionable.
void validate_return_node(struct node* node)
{
    if (node->stmt.return_stmt.exp)
    {
        if (datatype_is_void_no_ptr(&current_function->func.rtype))
        {
            compiler_node_error(node, "You are returning a value in a function %s which has a return type of void\n", current_function->func.name);
        }
        validate_expressionable(node->stmt.return_stmt.exp);
    }
}

// ch257: if-statement. Body gets its own scope; condition
// validation lands later.
void validate_if_stmt(struct node* node)
{
    // TODO VALIDATE EXPRESSION OF IF STATEMENT
    validation_new_scope(0);
    validate_body(&node->stmt.if_stmt.body_node->body);
    validation_end_scope();
}

// TODO: validate for / while / switch land in later chapters.

void validate_statement(struct node* node)
{
    switch (node->type)
    {
        case NODE_TYPE_VARIABLE:
            validate_variable(node);
            break;

        case NODE_TYPE_STATEMENT_RETURN:
            validate_return_node(node);
            break;

        case NODE_TYPE_STATEMENT_IF:
            validate_if_stmt(node);
            break;
    }
}

// ch255 (extended in ch257): walk a body's statement vector and
// dispatch each through validate_statement.
void validate_body(struct body* body)
{
    vector_set_peek_pointer(body->statements, 0);
    struct node* statement = vector_peek_ptr(body->statements);
    while (statement)
    {
        validate_statement(statement);
        statement = vector_peek_ptr(body->statements);
    }
}

void validate_function_body(struct node* node)
{
    validate_body(&node->body);
}

// ch256: validate a variable definition. Local-scope lookup first
// to catch shadowing-in-the-same-scope; on miss, register the var
// as a new resolver entity so later references in the same scope
// can find it.
void validate_variable(struct node* var_node)
{
    struct resolver_entity* entity =
        resolver_get_variable_from_local_scope(validator_current_compile_process->resolver, var_node->var.name);
    if (entity)
    {
        compiler_node_error(var_node, "You have already defined the variable %s in the given scope", var_node->var.name);
    }
    resolver_default_new_scope_entity(validator_current_compile_process->resolver, var_node, 0, 0);
}

void validate_function_argument(struct node* func_argument_var_node)
{
    validate_variable(func_argument_var_node);
}

void validate_function_arguments(struct function_arguments* func_arguments)
{
    struct vector* func_arg_vec = func_arguments->vector;
    vector_set_peek_pointer(func_arg_vec, 0);
    struct node* current = vector_peek_ptr(func_arg_vec);
    while (current)
    {
        validate_function_argument(current);
        current = vector_peek_ptr(func_arg_vec);
    }
}

// ch255: validate a function node. Forward decls skip the uniqueness
// check (we expect the real def to come later); everything else
// asserts the name hasn't been registered. Then we register the
// function symbol, open a scope, walk arguments + body, close.
void validate_function_node(struct node* node)
{
    current_function = node;
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
        validate_symbol_unique(node->func.name, "function", node);
    }

    symresolver_register_symbol(validator_current_compile_process, node->func.name, SYMBOL_TYPE_NODE, node);
    validation_new_scope(0);
    validate_function_arguments(&node->func.args);

    if (node->func.body_n)
    {
        validate_function_body(node->func.body_n);
    }
    validation_end_scope();

    current_function = NULL;
}

// ch258: structure validation. Forward decls skip the uniqueness
// check; everything else asserts the name hasn't been registered
// before stamping the struct as SYMBOL_TYPE_NODE.
void validate_structure_node(struct node* node)
{
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
        validate_symbol_unique(node->_struct.name, "struct", node);
    }
    symresolver_register_symbol(validator_current_compile_process, node->_struct.name, SYMBOL_TYPE_NODE, node);
}

void validate_union_node(struct node* node)
{
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
        validate_symbol_unique(node->_union.name, "union", node);
    }
    symresolver_register_symbol(validator_current_compile_process, node->_union.name, SYMBOL_TYPE_NODE, node);
}

void validate_node(struct node* node)
{
    switch (node->type)
    {
        case NODE_TYPE_FUNCTION:
            validate_function_node(node);
            break;

        case NODE_TYPE_STRUCT:
            validate_structure_node(node);
            break;

        case NODE_TYPE_UNION:
            validate_union_node(node);
            break;
    }
}

int validate_tree(struct compile_process* process)
{
    (void)process;
    validation_new_scope(0);
    struct node* node = validation_next_tree_node();
    while (node)
    {
        validate_node(node);
        node = validation_next_tree_node();
    }
    validation_end_scope();
    return VALIDATION_ALL_OK;
}

int validate(struct compile_process* process)
{
    int res = 0;
    validate_initialize(process);
    res = validate_tree(process);
    validate_destruct(process);
    return res;
}
