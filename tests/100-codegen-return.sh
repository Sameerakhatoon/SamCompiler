#!/usr/bin/env bash
# Ch156: `return N;` emits the expression, pops eax, frees the stack
# without touching the compile-time ledger, and rets.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch156_input.XXXXXX)
printf 'int main() { int a; return 42; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch156_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch156_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch156_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch156 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "push dword 42" "return value pushed"
assert_contains "$got" "pop eax"        "popped into eax"
# return path frees its own stack:
assert_contains "$got" "add esp, 16"    "stack restored on the return path"
assert_contains "$got" "pop ebp"        "ebp restored"
# Two `ret`s expected: one from the return, one from the epilogue.
count=$(grep -c '^ret$' "$outfile" || true)
[ "$count" -ge 2 ] || fail "expected >=2 ret instructions; got $count"
pass
