#!/usr/bin/env bash
# Ch244: implementing validator scopes. validator.c gains module-
# static validator_current_compile_process + current_function,
# plus validation_new_scope / _end_scope (delegate to the default
# resolver's scope manager) and validation_next_tree_node
# (peek_ptr the parse tree). validate_initialize stamps the
# process, resets the tree peek pointer, and starts a fresh
# symresolver table. validate_destruct ends that table and resets
# the peek pointer.
#
# Test: still a smoke test - confirm a trivial source survives
# the new init/destruct round-trip and reaches codegen, and that
# validate() called against a fresh compile_process still returns
# ALL_OK.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch244_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch244_asm.XXXXXX.asm)
probe=$(mktemp /tmp/sam_ch244_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch244_bin.XXXXXX)
trap 'rm -f "$src" "$asm" "$probe" "$bin"' EXIT

cat > "$src" <<'EOF'
int x;
int main() { return 0; }
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "trivial source did not reach codegen with validator scopes: $out" ;;
esac

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"

int main(void){
    struct compile_process* cp = compile_process_create("$src", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    int r = validate(cp);
    printf("validate=%d\n", r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch244 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "validate=0" "validate() still returns ALL_OK with scope init/destruct wired"
pass
