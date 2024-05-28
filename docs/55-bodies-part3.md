# ch55 - implementing bodies (part 3)

`parser_finalize_body` is now real:

1. If the history says we're inside a union, the total size is just
   the size of the largest member (unions overlap).
2. Sum every variable's `.padding` byte count and add it to the
   running total.
3. Realign the total up to the largest align-eligible member's
   natural size, so the body's tail is on a clean boundary.
4. Stamp `body_node` with the computed `size`, `padded` flag, and
   `largest_var_node`.

New flag in `parser.c`: `HISTORY_FLAG_INSIDE_UNION` (bit 0 of history
flags). The unfinished union path will set this when ch99 lands.

No new test - existing struct/variable tests cover the change
silently (body sizes change but no test asserts on them yet; codegen
tests will catch breakage when we get there).
