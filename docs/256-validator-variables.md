# ch256 - implementing validation of variables

Wires same-scope variable redeclaration checks into the
validator. After this, function parameters or locals that
collide within the same scope produce a "You have already
defined the variable" diagnostic.

What landed:
- `resolver.c`: `resolver_get_variable_from_local_scope(
  resolver, var_name)`: searches ONLY the current resolver scope
  (via `resolver_get_entity_in_scope` + `resolver_scope_current`)
  so the validator doesn't false-positive on shadowing in an
  outer scope.
- `compiler.h`: forward decl for the new helper.
- `validator.c`:
  - `validate_variable(var_node)`: local-scope lookup;
    `compiler_node_error` if the name is already there;
    otherwise `resolver_default_new_scope_entity` to register
    the var as a fresh resolver entity.
  - `validate_function_argument` now actually calls
    `validate_variable`.

Test: `tests/173-validator-variables.sh` confirms:
- `int foo(int a, int a)` produces the
  "You have already defined the variable" diagnostic.
- `int foo(int a, int b)` compiles normally.
