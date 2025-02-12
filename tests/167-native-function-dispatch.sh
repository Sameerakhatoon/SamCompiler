#!/usr/bin/env bash
# Ch237: implementing native functions - part 2. Wires the
# resolver + codegen so an identifier that names a previously-
# registered native function (e.g. `test` from
# preprocessor_stdarg_internal_include) actually dispatches to
# its registered callback at codegen time.
#
# Test: compile a source that #include <stdarg-internal.h> +
# calls `test();` and confirm the emitted asm contains both
# `; NATIVE FUNCTION test` (codegen dispatch tag) and the
# native callback's `; TEST FUNCTION ACTIVATED!` payload.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch237_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch237_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
#include <stdarg-internal.h>
int main() {
    test();
}
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"; NATIVE FUNCTION test"*) ;;
    *) fail "expected '; NATIVE FUNCTION test' tag in asm output; got: $out" ;;
esac
case "$out" in
    *"; TEST FUNCTION ACTIVATED!"*) ;;
    *) fail "expected native callback to emit '; TEST FUNCTION ACTIVATED!'; got: $out" ;;
esac
pass
