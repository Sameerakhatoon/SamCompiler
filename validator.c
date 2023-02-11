#include "compiler.h"

// ch243: scaffolded validator. Sits between parse and codegen so
// later chapters can hang checks (scope, types, statement shape,
// etc.) off this pipeline stage. Today it just returns ALL_OK.

void validate_initialize(struct compile_process* process)
{
    (void)process;
    // todo
}

void validate_destruct(struct compile_process* process)
{
    (void)process;
    // todo
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
