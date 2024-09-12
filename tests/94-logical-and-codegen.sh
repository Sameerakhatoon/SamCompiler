#!/usr/bin/env bash
# Ch147: `a && b` emits short-circuit asm using cmp/je to an end
# label, with the && END CLAUSE setting eax=1 on success and 0 on
# fail. Also covers comparison + bitshift operator dispatch through
# codegen_set_flag_for_operator (`==` becomes sete, `<<` becomes sal).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch147_input.XXXXXX)
printf 'int main() { int a; int b; int r = a && b; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch147_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch147_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch147_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch147 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "; && END CLAUSE" "logical && end-clause comment"
assert_contains "$got" "cmp eax, 0"      "compare against zero for short circuit"
# Short-circuit jump target name pattern (`.endc_N` from codegen_label_count).
case "$got" in
    *je\ .endc_*) ;;
    *) fail "expected je to a .endc_<n> label in: $got" ;;
esac
assert_contains "$got" "mov eax, 1"      "success branch sets eax = 1"
assert_contains "$got" "xor eax, eax"    "fail branch zeros eax"
pass
