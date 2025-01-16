#!/usr/bin/env bash
# Ch222: fixing a mistake with logical not in the code generator.
# codegen_generate_normal_unary's switch gains an `!` arm that
# emits `cmp eax, 0; sete al; movzx eax, al` so a C-level `!x`
# expression produces the canonical 0/1 result.
#
# Test: compile a source snippet containing `!0` and confirm the
# emitted asm contains the cmp + sete sequence.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch222_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch222_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
int main() {
    return !0;
}
EOF

"$REPO_ROOT/main" "$src" "$asm" object >/dev/null 2>&1 || true
out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"sete al"*|*"sete  al"*) ;;
    *) fail "expected sete al instruction emitted for !0; got: $out" ;;
esac
case "$out" in
    *"cmp eax, 0"*) ;;
    *) fail "expected cmp eax, 0 instruction emitted for !0; got: $out" ;;
esac
pass
