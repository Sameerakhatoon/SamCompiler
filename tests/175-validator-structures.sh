#!/usr/bin/env bash
# Ch258: implementing the validation of structures and unions.
# validate_node now dispatches NODE_TYPE_STRUCT to validate_
# structure_node and NODE_TYPE_UNION to validate_union_node.
# Both check the tag name is unique (forward decls excepted) and
# register the node as SYMBOL_TYPE_NODE.
#
# Test: two distinct `struct foo` definitions should bail. One
# definition + use compiles.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src_dup=$(mktemp /tmp/sam_ch258_dup.XXXXXX.c)
src_ok=$(mktemp /tmp/sam_ch258_ok.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch258_asm.XXXXXX.asm)
trap 'rm -f "$src_dup" "$src_ok" "$asm"' EXIT

cat > "$src_dup" <<'EOF'
struct foo { int a; };
struct foo { int b; };
int main() { return 0; }
EOF

out=$("$REPO_ROOT/main" "$src_dup" "$asm" 2>&1 || true)
case "$out" in
    *"Cannot define struct"*) ;;
    *) fail "expected duplicate-struct diagnostic; got: $out" ;;
esac

cat > "$src_ok" <<'EOF'
struct foo { int a; };
int main() { return 0; }
EOF

out=$("$REPO_ROOT/main" "$src_ok" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "single struct def should compile; got: $out" ;;
esac
pass
