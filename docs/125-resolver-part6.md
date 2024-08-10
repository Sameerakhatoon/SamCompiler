# ch125 - creating the resolver - Part 6

Finishes `resolver_get_entity_in_scope_with_entity_type` and adds the
public lookup helpers.

What landed in `resolver.c`:
- `resolver_get_entity_in_scope_with_entity_type` body:
  - struct/union branch: if there's a `last_struct_union_entity`,
    compute the field offset via `struct_offset`, zero it for unions
    (all fields share the slot), then `resolver_make_entity` a
    VARIABLE entity guided by the offset.
  - primitive branch: walk the scope's `entities` vector top-down
    via `VECTOR_FLAG_PEEK_DECREMENT`, filtering by entity_type when
    not -1, returning the first match.
- `resolver_get_entity_for_type(result, resolver, name, type)`:
  walks the scope chain via `scope->prev`. Zeroes `last_resolve` on
  success.
- `resolver_get_entity`: type=-1 wrapper.
- `resolver_get_entity_in_scope`: type=-1 wrapper around the
  in-scope helper.
- `resolver_get_variable`: filters by `RESOLVER_ENTITY_TYPE_VARIABLE`.
- `resolver_get_function_in_scope` / `resolver_get_function`:
  filters by `RESOLVER_ENTITY_TYPE_FUNCTION`, rooted at the root
  scope.

Deviation from upstream: book declares
`struct resoler_entity* resolver_get_entity_for_type(struct reoslver_result*, ...)`
with typo'd struct tags. Preserving the typos verbatim would break
every caller in `resolver.c`, so we use the correct struct names.
The function name `resolver_get_entity_for_type` is already correctly
spelled in the book.

Known carry-over bug (see G05): the ch121 var-node factories stamp
`NODE_TYPE_VARIABLE` as the entity type instead of
`RESOLVER_ENTITY_TYPE_VARIABLE`, so `resolver_get_variable` never
matches a variable. ch125 ships this verbatim; G05 patches the
factories and adds the matching test.

Test: `tests/72-resolver-lookup.sh` registers a function (works,
since `resolver_regster_function` uses the right constant) and
confirms `resolver_get_function` finds it and missing names return
NULL. Variable lookup is covered by G05's test.
