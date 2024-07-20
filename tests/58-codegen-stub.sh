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
# ch105 superseded the ch104 placeholder `jmp label_name` with the
# real section walk, so we only check that codegen ran end-to-end and
# wrote something to the outfile.
assert_contains "$got_stdout"  "res=0"     "compile_file returns OK"
[ -s "$outfile" ] || fail "outfile is empty after codegen"
pass
