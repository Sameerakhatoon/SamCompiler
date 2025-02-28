#!/usr/bin/env bash
# Ch256: implementing validation of variables. validate_variable
# checks for a same-scope redeclaration via the new
# resolver_get_variable_from_local_scope and bails via
# compiler_node_error if found; otherwise registers a fresh
# resolver entity via resolver_default_new_scope_entity.
# validate_function_argument now actually calls validate_variable.
#
# Test: a function with two parameters sharing the same name
# should produce a "You have already defined the variable" error.
# A function with distinct parameter names should compile.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src_dup=$(mktemp /tmp/sam_ch256_dup.XXXXXX.c)
src_ok=$(mktemp /tmp/sam_ch256_ok.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch256_asm.XXXXXX.asm)
trap 'rm -f "$src_dup" "$src_ok" "$asm"' EXIT

# Duplicate argument name -> validator should bail.
cat > "$src_dup" <<'EOF'
int foo(int a, int a) { return a; }
int main() { return foo(1, 2); }
EOF

out=$("$REPO_ROOT/main" "$src_dup" "$asm" 2>&1 || true)
case "$out" in
    *"You have already defined the variable"*) ;;
    *) fail "expected duplicate-variable diagnostic; got: $out" ;;
esac

# Distinct argument names -> should reach codegen.
cat > "$src_ok" <<'EOF'
int foo(int a, int b) { return a; }
int main() { return foo(1, 2); }
EOF

out=$("$REPO_ROOT/main" "$src_ok" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "distinct-arg source should reach codegen; got: $out" ;;
esac
pass
