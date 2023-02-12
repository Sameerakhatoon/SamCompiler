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

int validate_tree(struct compile_process* process)
{
    (void)process;
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
