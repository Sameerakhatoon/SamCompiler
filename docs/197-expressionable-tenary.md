# ch197 - creating the expressionable system - Part 6

Adds the tenary operator parse path, the
`expressionable_init` / `expressionable_create` constructors,
and dumps the full expressionable API into `compiler.h` so
external clients (the preprocessor lands next) can just include
the header.

What landed in `compiler.h`:
- A block of forward declarations for every expressionable
  function: callbacks accessor, node stack helpers, peek/next
  variants, init/create, parse_number / parse_identifier, the
  precedence + reorder helpers, the type predicate, expect_op /
  expect_sym, deal_with_additional_expression, parse_parentheses,
  pointer-depth + indirection unary, normal unary, parse_unary,
  parse_for_operator, parse_tenary, parse_exp, parse_token,
  parse_single_with_flags, parse_single, parse.

What landed in `expressionable.c`:
- `expressionable_init`: memset zero, memcpy the
  caller-supplied config, attach the token + node vectors, set
  flags. The standard "fill an existing struct" entry point.
- `expressionable_create`: asserts the token vector's element
  size is `sizeof(struct token)`, calloc's a fresh expressionable,
  calls init, returns it. The "give me a fresh one" entry point.
- `expressionable_parse_tenary`: pops the condition node, eats
  `?`, parses + pops the TRUE branch, eats `:`, parses + pops
  the FALSE branch, calls `make_tenary_node(true, false)`. Pops
  the resulting tenary node and wraps it via
  `make_expression_node(cond, tenary, "?")`.
- `parse_exp` refactored from "if-then-fallthrough" into a
  proper if (`(`) / else if (`?`) / else (parse_for_operator)
  chain. This removes the ch195 deviation guard - the explicit
  else branch makes parse_for_operator only fire when the
  current token is actually the start of a binary expression.

Test: `tests/128-expressionable-tenary.sh` builds the
expressionable through `expressionable_create`, feeds `1 ? 2 :
3`, and confirms `make_tenary_node` fires once and
`make_expression_node` fires once with op `?`.
