#include "compiler.h"

void preprocessor_stddef_include(struct preprocessor* preprocessor, struct preprocessor_included_file* file)
{
    // ch234: header itself is just the offsetof macro defined in
    // pc_includes/stddef.h. This static stub is a no-op; the real
    // content arrives via the normal `#include <stddef.h>` ->
    // pc_includes/stddef.h path.
}
