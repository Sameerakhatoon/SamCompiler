#!/usr/bin/env bash
# Ch164: switch-statement bookkeeping vector lives on the code
# generator. Nothing actually emits a switch yet (ch165+); we just
# confirm the generator's switch.swtiches vector is allocated.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch164_input.XXXXXX)
printf 'int x;' > "$scratch"
probe=$(mktemp /tmp/sam_ch164_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch164_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    int has = cp && cp->generator && cp->generator->_switch.swtiches != NULL;
    int empty = has && vector_count(cp->generator->_switch.swtiches) == 0;
    printf("has=%d empty=%d id=%d\n", has, empty, cp->generator->_switch.current.id);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch164 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "has=1 empty=1 id=0" "switch nest vector allocated and empty"
pass
