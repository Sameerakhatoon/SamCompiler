# G01 - parse_identifier asserts against NODE_TYPE instead of TOKEN_TYPE

## Symptom

Any input with an identifier (`gerog`, `int x`, `ABCD`) trips the
assert in `parse_identifier` and aborts. Tests 02, 05, 21, 23 (and
anything else exercising identifiers) fail right after the ch32 commit.

## Root cause

`parser.c::parse_identifier` from ch32, copied verbatim from the book:

```c
static void parse_identifier(struct history* history){
    assert(token_peek_next()->type == NODE_TYPE_IDENTIFIER);
    parse_single_token_to_node();
}
```

`token->type` carries values from the `TOKEN_TYPE_*` enum:

```c
TOKEN_TYPE_IDENTIFIER = 0
TOKEN_TYPE_KEYWORD    = 1
... up to TOKEN_TYPE_NEWLINE = 7
```

`NODE_TYPE_IDENTIFIER` is from a totally separate enum:

```c
NODE_TYPE_EXPRESSION             = 0
NODE_TYPE_EXPRESSION_PARENTHESES = 1
NODE_TYPE_NUMBER                 = 2
NODE_TYPE_IDENTIFIER             = 3
...
```

So the assert is comparing 0 (an identifier token's `type`) with 3
(`NODE_TYPE_IDENTIFIER`) and always failing.

The book most likely had asserts disabled by default and never noticed.

## Fix

Compare the token's `type` with the matching token enum:

```c
assert(token_peek_next()->type == TOKEN_TYPE_IDENTIFIER);
```

## Lesson

Two parallel enums (`TOKEN_TYPE_*`, `NODE_TYPE_*`) with overlapping
spellings is a footgun. Asserting on enum values without a
type-bridge (e.g. `_Static_assert` linking token kind to node kind)
makes the mistake invisible at compile time.
