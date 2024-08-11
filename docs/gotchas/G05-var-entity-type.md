# G05 - var-node factory stamps the parser enum

## Symptom

`resolver_get_variable(result, rp, "v")` returns NULL even when the
caller registered a variable with that name via
`resolver_new_entity_for_var_node`.

## Root cause

`resolver_create_new_entity_for_var_node_custom_scope` passes
`NODE_TYPE_VARIABLE` (the parser enum) as the entity type. The
resolver lookup filters by `RESOLVER_ENTITY_TYPE_VARIABLE` (= 0),
which is a different value. So the var entity's `type` never matches
the filter and the walk skips it.

## Fix

Stamp `RESOLVER_ENTITY_TYPE_VARIABLE` instead.

## Test

`tests/73-resolver-get-variable.sh`: parses `int v;`, registers it
with the resolver via `resolver_new_entity_for_var_node`, and
confirms `resolver_get_variable` finds it by name.
