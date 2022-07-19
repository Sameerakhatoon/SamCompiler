# ch59 - implementing the variable node scope offset (part 2)

Tiny chapter: introduces the global-scope path.

- New history flag `HISTORY_FLAG_IS_GLOBAL_SCOPE`.
- `parser_scope_offset_for_global(node, history)` - returns 0 for
  now (global vars don't live on the stack).
- `parser_scope_offset` dispatches: GLOBAL -> the new no-op; else
  fall through to the ch58 stack path.

ch60 wires the actual offset stamping; ch61 starts pushing into the
real scope.
