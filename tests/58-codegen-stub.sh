#!/usr/bin/env bash
# Ch104: codegen() exists, is wired into compile_file, and writes its
# placeholder `jmp label_name` line both to stdout and to the
# compile_process output file.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch104_input.XXXXXX)
printf 'int x;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch104_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch104_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch104_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    int res = compile_file("${scratch}", "${outfile}", 0);
    printf("res=%d\n", res);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch104 probe failed to compile"
got_stdout="$("$bin")"
got_outfile="$(cat "$outfile" 2>/dev/null || echo)"
assert_contains "$got_stdout"  "jmp label_name" "stdout has the placeholder asm"
assert_contains "$got_stdout"  "res=0"          "compile_file returns OK"
assert_contains "$got_outfile" "jmp label_name" "outfile has the placeholder asm"
pass
