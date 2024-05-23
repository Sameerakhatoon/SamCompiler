# ch52 - implementing variable size functions

Two helpers in a new `helper.c`:

- `variable_size(var_node)` - the byte size of one
  NODE_TYPE_VARIABLE, computed via `datatype_size(&var.type)`.
- `variable_size_for_list(var_list_node)` - sum of the sizes of every
  variable in a NODE_TYPE_VARIABLE_LIST (`int a, b, c;`).

`Makefile` gets a `helper.o` build rule. Smoke test piggybacks on
existing variable / variable-list tests; no new dedicated test
because the behaviour is just a sum of already-tested datatype_size
calls.
