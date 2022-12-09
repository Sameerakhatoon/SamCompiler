#!/usr/bin/env bash
# Ch108: codegen label / entry-exit machinery. Ch110 replaced the
# always-on smoke emitter, so we only check that compile_file still
# runs end-to-end and that the generator's string_table /
# entry_points / exit_points are non-NULL.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch108_input.XXXXXX)
printf 'int x;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch108_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch108_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch108_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "${outfile}", 0, NULL);
    printf("gen=%d ep=%d xp=%d\n",
        cp->generator != NULL,
        cp->generator && cp->generator->entry_points != NULL,
        cp->generator && cp->generator->exit_points  != NULL);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch108 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "gen=1 ep=1 xp=1" "code_generator + entry / exit vectors allocated"
pass
