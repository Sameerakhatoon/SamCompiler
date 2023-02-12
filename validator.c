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

// ch255: walk a body's statement vector. Per-statement validation
// lands in ch257; today this is just a scaffold loop.
void validate_body(struct body* body)
{
    vector_set_peek_pointer(body->statements, 0);
    struct node* statement = vector_peek_ptr(body->statements);
    while (statement)
    {
        // validate the statement
        statement = vector_peek_ptr(body->statements);
    }
}

void validate_function_body(struct node* node)
{
    validate_body(&node->body);
}

void validate_function_argument(struct node* func_argument_var_node)
{
    (void)func_argument_var_node;
    // validate_variable lands in ch256
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

void validate_node(struct node* node)
{
    switch (node->type)
    {
        case NODE_TYPE_FUNCTION:
            validate_function_node(node);
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
