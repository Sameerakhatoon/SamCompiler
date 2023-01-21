#!/usr/bin/env bash
# Ch232: implementing sizeof. parse_sizeof eats
# `sizeof ( <datatype> )` and emits a NODE_TYPE_NUMBER carrying
# datatype_size(dtype). Compile-time constant - codegen sees an
# ordinary number.
#
# Test: compile `int main() { return sizeof(int); }` and confirm
# the emitted asm contains `push dword 4` (sizeof(int) on our
# 32-bit target).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch232_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch232_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
int main() {
    return sizeof(int);
}
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"push dword 4"*) ;;
    *) fail "expected push dword 4 in emitted asm for sizeof(int); got: $out" ;;
esac
pass
