# ch65 - implementing structures (part 4)

A second `struct foo` declarator now resolves against the previously-
defined struct. `struct abc { int a; int b; }; struct abc x;` works
end to end.

What landed in `node.c`:

- `node_from_sym(sym)` - unwrap a SYMBOL_TYPE_NODE symbol's `data`.
- `node_from_symbol(process, name)` - same but starts from a name.
- `struct_node_for_name(process, name)` - same but only succeeds for
  NODE_TYPE_STRUCT nodes.

`parser.c`:
- `size_of_struct(name)` - look up the struct by name, return
  `body_n->body.size`.
- `parser_datatype_init_type_and_size`'s `DATA_TYPE_EXPECT_STRUCT`
  case now sets `.size` via `size_of_struct(...)` and stamps
  `.struct_node` so the resolver can walk back to the definition.
- The union arm now errors with "Union types are currently
  unsupported" (was previously a shared error with struct; ch99 lands
  unions properly).

No new test - the existing struct test plus ch64's
`tests/43-struct-with-body.sh` already exercises the path; the next
chapter adds a multi-declaration test.
