# ch83-89 - control-flow statements (batched)

These chapters add the rest of C's control flow. I've rolled them into
one commit because they share a single payload (`struct statement`),
several helper functions (`parse_keyword_parentheses_expression`), and
the dispatch in `parse_keyword`.

| ch | feature                                  |
| -- | ---------------------------------------- |
| 83 | `while (exp) body`                       |
| 84 | `do body while (exp);`                   |
| 85 | `switch (exp) body` (body shell)         |
| 86 | `break;` / `continue;`                   |
| 87 | label declarations                       |
| 88 | `goto label;`                            |
| 89 | `case expr:` / `default:` markers        |

What landed:

- `struct statement` gains: `while_stmt`, `do_while_stmt`,
  `switch_stmt { exp, body, cases, has_default_case }`, `_case_stmt`,
  `_goto_stmt`, `_label`.
- `make_while_node`, `make_do_while_node`, `make_switch_node`,
  `make_case_node`, `make_continue_node`, `make_break_node`,
  `make_goto_node`, `make_label_node`, `make_default_node` in
  `node.c`.
- `parse_keyword_parentheses_expression(keyword)` - shared helper:
  consume `keyword ( expr )`, leave expr on the stack.
- `parse_while` / `parse_do_while` / `parse_switch` / `parse_break` /
  `parse_continue` / `parse_goto` / `parse_case` / `parse_default` in
  `parser.c`.
- `parse_keyword` dispatch grows arms for every new keyword.

`switch_stmt.cases` is currently an empty vector populated nowhere -
ch89's case-index registration arrives properly only after the parser
finishes building the body (the book defers it to ch94). For now we
parse `case` markers as standalone statements inside the body.

Smoke coverage: existing parser tests still pass. Dedicated control-
flow tests will land alongside codegen when there's actually something
visible to assert on.
