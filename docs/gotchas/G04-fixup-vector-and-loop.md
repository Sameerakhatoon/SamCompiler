# G04 - fixup system was unusable as shipped

Two bugs from the book's ch96.

## 1. Vector element size mismatch

`fixup_sys_new` creates the vector with element size
`sizeof(struct fixup)`, but `fixup_register` pushes a `struct fixup*`.
`vector_push` copies `esize` bytes from the source pointer, so the
full struct is inlined into the vector slot and `fixup_next`
(`vector_peek_ptr`) returns the first 8 bytes of that struct treated
as a pointer. With `flags = 0` and padding/system, the returned
"pointer" is effectively NULL on the first peek and iteration exits
immediately, so unresolved counts read as 0 even when fixups have
been registered.

Fix: create the vector with `sizeof(struct fixup*)` and push
`&fixup`, not `fixup`.

## 2. Infinite loop in fixups_resolve

`fixups_resolve` `continue`s without calling `fixup_next` when it
sees an already-resolved fixup, so after the first successful resolve
the loop spins forever.

Fix: advance the iterator before `continue`.

## Test

`tests/52-fixup-core.sh`:
- Registers two fixups. One resolves on the first try; the other has
  a private `int` countdown that resolves only after the second call.
- Asserts `unresolved=2` before any resolve pass.
- Asserts `unresolved=1` and `done=false` after pass 1.
- Asserts `unresolved=0` and `done=true` after pass 2.
