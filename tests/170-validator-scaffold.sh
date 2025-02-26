#!/usr/bin/env bash
# Ch243: building the foundations (validator). Adds validator.c
# with validate_initialize, validate_destruct, validate_tree
# (returns VALIDATION_ALL_OK), and validate (drives the three).
# compiler.c's compile_file now calls validate() between parse
# and codegen and short-circuits to COMPILER_FAILED_WITH_ERRORS
# if it returns anything other than VALIDATION_ALL_OK.
#
# Test: confirm a trivial source still compiles through the new
# pipeline AND that validate() is linkable + returns ALL_OK on
# its own.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch243_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch243_asm.XXXXXX.asm)
probe=$(mktemp /tmp/sam_ch243_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch243_bin.XXXXXX)
trap 'rm -f "$src" "$asm" "$probe" "$bin"' EXIT

cat > "$src" <<'EOF'
int main() { return 0; }
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "trivial source no longer reaches codegen with validator inserted: $out" ;;
esac

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"

int main(void){
    struct compile_process* cp = compile_process_create("$src", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    int r = validate(cp);
    printf("validate=%d ALL_OK=%d\n", r, VALIDATION_ALL_OK);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch243 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "validate=0 ALL_OK=0" "validate() returns VALIDATION_ALL_OK on a fresh compile_process"
pass
