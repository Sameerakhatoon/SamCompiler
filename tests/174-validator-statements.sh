#!/usr/bin/env bash
# Ch257: implementing the validation of statements. validate_body
# now dispatches each statement via validate_statement. Handles:
# NODE_TYPE_VARIABLE (validate_variable), NODE_TYPE_STATEMENT_
# RETURN (rejects returning a value from a void function;
# recursively validates the value as an expressionable), NODE_
# TYPE_STATEMENT_IF (opens scope, validates body, closes scope).
# validate_identifier resolver_follows and bails on miss.
#
# Test: a void function that returns a value should produce the
# "returning a value in a function ... void" diagnostic. A void
# function with `return;` should compile.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src_bad=$(mktemp /tmp/sam_ch257_bad.XXXXXX.c)
src_ok=$(mktemp /tmp/sam_ch257_ok.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch257_asm.XXXXXX.asm)
trap 'rm -f "$src_bad" "$src_ok" "$asm"' EXIT

# Void return with a value -> validator should bail.
cat > "$src_bad" <<'EOF'
void foo() { return 5; }
int main() { foo(); return 0; }
EOF

out=$("$REPO_ROOT/main" "$src_bad" "$asm" 2>&1 || true)
case "$out" in
    *"returning a value"*|*"void"*) ;;
    *) fail "expected void-return diagnostic; got: $out" ;;
esac

# Bare `return;` in a void function -> should compile.
cat > "$src_ok" <<'EOF'
void foo() { return; }
int main() { foo(); return 0; }
EOF

out=$("$REPO_ROOT/main" "$src_ok" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "void function with bare return should compile; got: $out" ;;
esac
pass
