#!/usr/bin/env bash
# Ch53: padding / align_value primitives.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
probe=$(mktemp /tmp/sam_ch53_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch53_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
int main(void){
    printf("p_5_4=%d p_8_4=%d\n", padding(5, 4), padding(8, 4));
    printf("a_5_4=%d a_8_4=%d a_15_8=%d\n",
        align_value(5, 4), align_value(8, 4), align_value(15, 8));
    return 0;
}
EOF
gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch53 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "p_5_4=3 p_8_4=0"          "padding 5->8 is 3, 8 is already aligned"
assert_contains "$got" "a_5_4=8 a_8_4=8 a_15_8=16" "align_value rounds up to next multiple"
pass
