#!/usr/bin/env bash
# Ch140: assignment-as-statement (`b = 20;`) now generates code,
# resolving the LHS via resolver_follow and emitting push/pop/mov to
# the variable's stack slot.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch140_input.XXXXXX)
printf 'int main() { int b = 50; b = 20; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch140_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch140_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch140_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch140 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
# Initializer path:
assert_contains "$got" "push dword 50" "initializer literal pushed"
# Reassignment path: push 20, pop eax, store at the same slot.
assert_contains "$got" "push dword 20"            "rhs of reassignment pushed"
# Two `mov dword [ebp-4], eax` instances expected (init + reassign).
count=$(grep -c "mov dword \[ebp-4\], eax" "$outfile" || true)
[ "$count" -ge 2 ] || fail "expected at least 2 stores to [ebp-4]; got $count"
pass
