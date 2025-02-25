# ch240 - finishing some important header files

Lands `pc_includes/stdio.h` and `pc_includes/stdlib.h` so
sources can `#include <stdio.h>` and call fopen / fwrite /
fclose / fread / printf against the platform libc at link time.

What landed:
- `pc_includes/stdlib.h` (new): include guards plus
  `typedef int size_t;`. Just the bits that stdio.h needs for
  its prototypes.
- `pc_includes/stdio.h` (new): include guards, pulls in
  `<stdlib.h>`, typedefs `struct _iobuf` (with the classic
  windows-libc-style member names) as `FILE`, declares
  `fopen`, `fwrite`, `fclose`, `fread`, `printf`.

Note: these are pure declarations - the actual symbols come
from the C runtime the linker pulls in.

Test: `tests/169-stdio-headers.sh` compiles a source that
`#include <stdio.h>` and uses `fopen` + `fwrite`; confirms main
reaches codegen and the emitted asm references both
identifiers.
