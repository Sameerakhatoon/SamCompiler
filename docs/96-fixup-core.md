# ch96 - fixup core functionality

New module `fixup.c` and matching declarations in `compiler.h`.

Idea: callers register a unit of deferred work (`struct fixup`) with a
`fix` callback and an `end` callback. `fixups_resolve` walks every
registered fixup that isn't `FIXUP_FLAG_RESOLVED` yet and calls its
fix. ch97 wires this into the parser for forward declarations.

What landed:
- `Makefile`: new `fixup.o` target, added to `OBJECTS`.
- `compiler.h`:
  - `struct fixup`, `struct fixup_system`, `struct fixup_config`.
  - `FIXUP_FIX` / `FIXUP_END` function-pointer types.
  - `FIXUP_FLAG_RESOLVED`.
  - Decls for `fixup_sys_new`, `fixup_register`, `fixup_resolve`,
    `fixups_resolve`, `fixup_sys_free`,
    `fixup_sys_unresolved_fixups_count`, `fixup_private`,
    `fixup_config`, `fixup_start_iteration`, `fixup_next`.
- `fixup.c`: implementations, mirroring the book verbatim.

No test on this commit: the book ships a vector-element-size bug that
makes the system unusable until it's patched. The fix and its test
land in the next gotcha commit.
