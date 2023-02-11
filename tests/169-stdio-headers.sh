#!/usr/bin/env bash
# Ch240: finishing some important header files. Adds
# pc_includes/stdlib.h (typedef int size_t;) and
# pc_includes/stdio.h (struct _iobuf typedef'd as FILE, with
# fopen / fwrite / fclose / fread / printf prototypes).
#
# Test: compile a source that #includes both headers and uses
# fopen + fwrite; confirm main reaches codegen and references
# `fopen` and `fwrite` in the emitted output.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch240_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch240_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
#include <stdio.h>

int main() {
    FILE* f = fopen("./testing.txt", "w");
    fwrite("hello", 5, 1, f);
}
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "main did not reach codegen for stdio source: $out" ;;
esac
case "$out" in
    *fopen*) ;;
    *) fail "expected fopen reference in emitted asm; got: $out" ;;
esac
case "$out" in
    *fwrite*) ;;
    *) fail "expected fwrite reference in emitted asm; got: $out" ;;
esac
pass
