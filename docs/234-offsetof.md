# ch234 - implementing offsetof

Adds `<stddef.h>` with the classic `offsetof` macro, leveraging
the ch233 resolver fix for `&((T*)0)->member` patterns.

What landed:
- `pc_includes/stddef.h` (new): wraps with
  `#ifndef STDDEF_H / #define STDDEF_H`, pulls in the static
  `stddef-internal.h`, defines
  `offsetof(TYPE, MEMBER) &((TYPE*)0x00)->MEMBER`.
- `preprocessor/static-includes/stddef.c`: empties the stub.
  The static include exists only so `#include <stddef-internal.h>`
  doesn't fail; the actual contents live in
  pc_includes/stddef.h above.

Test: `tests/164-offsetof.sh` compiles
```
#include <stddef.h>
struct dog { int x; int y; };
int main() { return offsetof(struct dog, y); }
```
and confirms main reaches codegen and the output references
the byte offset (4).
