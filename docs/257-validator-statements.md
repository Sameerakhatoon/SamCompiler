# ch257 - implementing the validation of statements

Wires statement-level dispatch into the validator. Today the
validator catches `return <value>;` inside a void function and
verifies identifiers in expressionable positions resolve.

What landed:
- `datatype.c`: `datatype_is_void_no_ptr(dtype)` returns
  `S_EQ(type_str, "void") && !(flags & IS_POINTER)`.
- `compiler.h`: forward decl for the new helper.
- `validator.c`:
  - Forward decls for `validate_variable` + `validate_body`
    (so validate_statement can call them).
  - `validate_identifier(node)`: `resolver_follow` and
    `compiler_error` if the result isn't OK.
  - `validate_expressionable(node)` switch - only IDENTIFIER
    wired this round.
  - `validate_return_node(node)`: if `stmt.return_stmt.exp`
    is set, error when current_function->func.rtype is void
    (no pointer); otherwise recurse via validate_expressionable.
  - `validate_if_stmt(node)`: opens a fresh scope, validates
    the body, closes. Condition validation TODO.
  - `validate_statement(node)` switch - routes
    NODE_TYPE_VARIABLE / _STATEMENT_RETURN / _STATEMENT_IF.
  - `validate_body(body)` now actually calls validate_statement
    on each statement.

Test: `tests/174-validator-statements.sh` confirms:
- `void foo() { return 5; }` produces the void-return
  diagnostic.
- `void foo() { return; }` compiles.
