#!/usr/bin/env bash
# Ch231: implementing includes - part 3. Adds the static-include
# fallback: when compile_include fails to find a real file, the
# preprocessor checks preprocessor_static_include_handler_for() -
# currently registers stddef-internal.h and stdarg-internal.h as
# stubs - and if a handler exists calls preprocessor_create_
# static_include with it (registers an included_file entry +
# invokes the handler).
#
# Adds static-include.c (handler lookup), static-includes/stddef.c
# + stdarg.c (handler stubs with TODO warnings). Wires the
# handle_include_token fallback.
#
# Test: confirm preprocessor_static_include_handler_for returns
# non-NULL for "stddef-internal.h" / "stdarg-internal.h" and NULL
# otherwise.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch231_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch231_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"

int main(void){
    int has_stddef = preprocessor_static_include_handler_for("stddef-internal.h") != NULL;
    int has_stdarg = preprocessor_static_include_handler_for("stdarg-internal.h") != NULL;
    int has_bogus  = preprocessor_static_include_handler_for("nope.h") != NULL;
    printf("stddef=%d stdarg=%d bogus=%d\n", has_stddef, has_stdarg, has_bogus);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch231 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "stddef=1 stdarg=1 bogus=0" "static-include handler lookup recognizes stddef + stdarg, rejects unknown"
pass
