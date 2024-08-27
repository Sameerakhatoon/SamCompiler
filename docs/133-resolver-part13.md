# ch133 - creating the resolver - Part 13

`resolver_execute_rules` finally gets a body.

What landed in `resolver.c`:
- `resolver_rule_apply_rules(rule, left, right)`: ORs the RULE
  entity's `rule.left.flags` onto `left->flags` and
  `rule.right.flags` onto `right->flags`.
- `resolver_push_vector_of_entities(result, vec)`: pushes every
  element of a stack-shaped helper vector back onto the result
  chain, top-down so order is preserved.
- `resolver_execute_rules`: walks the chain top-down. When it hits
  a RULE entity, pops the left neighbor too and applies the rule's
  flags to (left, last-processed). Survivors go into a helper
  vector and are restored to the result via
  `_push_vector_of_entities`. Net effect: RULE entities are
  consumed; their flag contributions land on the neighboring
  entities.

Test: `tests/81-resolver-rules.sh` builds a 3-entity result
`L | RULE | R` where the rule sets `NO_MERGE_WITH_NEXT_ENTITY` on
the left and `DO_INDIRECTION` on the right. After
`resolver_execute_rules`: count drops to 2, L has the no-merge
flag, R has the indirection flag.
