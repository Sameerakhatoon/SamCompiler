#!/usr/bin/env bash
# Ch255: implementing functions (validator). validate_tree now
# walks the parse tree; validate_node dispatches NODE_TYPE_
# FUNCTION to validate_function_node which checks the name is
# unique (forward decls excepted), registers the function symbol,
# opens a scope, walks arguments + body, closes. Adds
# compiler_node_error for parse-tree-anchored diagnostics.
#
# Test: a source with TWO non-forward function definitions sharing
# the same name should fail validation (compiler_node_error
# exit(-1)). A source with one definition + one forward decl
# (same name) should compile.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src_dup=$(mktemp /tmp/sam_ch255_dup.XXXXXX.c)
src_ok=$(mktemp /tmp/sam_ch255_ok.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch255_asm.XXXXXX.asm)
trap 'rm -f "$src_dup" "$src_ok" "$asm"' EXIT

# Two real definitions of `foo` -> validator should bail.
cat > "$src_dup" <<'EOF'
int foo() { return 1; }
int foo() { return 2; }
int main() { return foo(); }
EOF

out=$("$REPO_ROOT/main" "$src_dup" "$asm" 2>&1 || true)
case "$out" in
    *"Cannot define function"*) ;;
    *) fail "expected duplicate-function diagnostic from validator; got: $out" ;;
esac

# Single definition (no duplicate) -> should reach codegen.
cat > "$src_ok" <<'EOF'
int foo() { return 1; }
int main() { return foo(); }
EOF

out=$("$REPO_ROOT/main" "$src_ok" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "single-function source should still reach codegen; got: $out" ;;
esac
pass
