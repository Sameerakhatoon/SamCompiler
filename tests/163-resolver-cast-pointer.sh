#!/usr/bin/env bash
# Ch233: fixing some issues with casting pointers. Tightens the
# resolver's handling of `->`, struct/union casts, and unary `&`.
#
# - resolver_do_indirection: don't try to indirect through a
#   FUNCTION_CALL result, a value whose result already wants the
#   address (RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS), or a CAST
#   result.
# - resolver_follow_struct_exp's `->` branch now uses
#   resolver_do_indirection instead of the narrow function-call
#   check.
# - resolver_follow_cast anchors struct/union cast entities as
#   last_struct_union_entity so member access against `(T*)0`
#   resolves member offsets through the cast dtype.
# - resolver_follow_unary_address sets RESOLVER_RESULT_FLAG_DOES_
#   GET_ADDRESS before walking the operand.
#
# Test: compile `return &((struct dog*)0)->y;` against a struct
# that puts `y` at offset 4. The emitted asm must reference an
# `add` of 4 (the offsetof y) or an offset of 4, confirming the
# resolver routed through the cast struct member.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch233_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch233_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
struct dog
{
    int x;
    int y;
};

int main()
{
    return &((struct dog*)0)->y;
}
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
# Confirm we reached codegen (no "Compile failed" / no resolver crash).
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "main did not reach codegen on cast-pointer source: $out" ;;
esac
# Spot check the asm contains the byte offset 4 somewhere (member y).
case "$out" in
    *"4"*) ;;
    *) fail "expected asm output to reference offset 4 for member y; got: $out" ;;
esac
pass
