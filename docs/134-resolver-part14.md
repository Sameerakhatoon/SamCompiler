# ch134 - creating the resolver - Part 14

`resolver_merge_compile_times` gets a body.

What landed in `resolver.c`:
- `resolver_merge_compile_time_result(resolver, result, L, R)`:
  hands the pair off to the user's `callbacks.merge_entities` unless
  NO_MERGE_WITH_NEXT_ENTITY on L or NO_MERGE_WITH_LEFT_ENTITY on R
  forbids it.
- `_resolver_merge_compile_times(resolver, result)`: one pass.
  Repeatedly pop (R, L); merge if possible; otherwise mark R
  no-merge-with-left, stash it, and put L back so it can try with
  its predecessor next iteration. Survivors get pushed back.
- `resolver_merge_compile_times(resolver, result)`: repeat the
  one-pass loop until either the chain collapses to a single entity
  or a pass makes no progress.

Test: `tests/82-resolver-merge.sh` pushes three GENERAL entities
with all NO_MERGE flags cleared and a `merge_entities` callback that
always succeeds. Confirms count drops from 3 to 1 with exactly 2
callback invocations.
