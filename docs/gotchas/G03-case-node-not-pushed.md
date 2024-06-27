# G03 - case node never re-pushed after register

## Symptom

Parsing `switch (0) { case 1: }` aborts inside
`vector_back_ptr`: assertion `vector_in_bounds_for_pop(vector, index)`
fails in `node_pop`. The crash happens at the body-statements loop
right after `parse_case` returns.

## Root cause

In the book's ch94 `parse_case`:

```c
make_case_node(case_exp_node);   // pushes the case node
...
struct node* case_node = node_pop();
parser_register_case(history, case_node);
// (no node_push)
```

`make_case_node` pushes a node, and `parse_case` pops it (to register
the index), but never pushes it back. The enclosing
`parse_body_multiple_statements` then does
`stmt_node = node_pop()` and walks off the empty stack.

## Fix

Push `case_node` back after registering so the body sees it as a
statement node like every other statement.

## Test

`tests/51-switch-case-index.sh`: parses
`int main() { switch(0) { case 1: case 2: case 7: } }` and inspects
the switch node's `cases` vector. Expects three entries with indexes
1, 2, 7.
