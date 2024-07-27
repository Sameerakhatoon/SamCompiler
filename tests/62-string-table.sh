#!/usr/bin/env bash
# Ch110/111: string-table machinery shipped in ch110 but its only
# live caller from inside codegen was a smoke test that ch111
# removed. Once ch112 wires real string literals through codegen
# we'll test the emit path end-to-end; for now confirm the API
# exists and code_generator owns a string_table vector.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch110u_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch110u_bin.XXXXXX)
scratch=$(mktemp /tmp/sam_ch110u_input.XXXXXX)
printf 'int x;' > "$scratch"
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    int has_table = cp && cp->generator && cp->generator->string_table != NULL;
    int empty     = has_table && vector_count(cp->generator->string_table) == 0;
    printf("table=%d empty=%d\n", has_table, empty);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch110 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "table=1 empty=1" "code_generator.string_table is allocated and empty"
pass
