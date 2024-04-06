#!/usr/bin/env bash
# Ch24: struct node + NODE_TYPE_* are compilable and usable. Sanity-only.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch24_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch24_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "compiler.h"
int main(void){
    struct node n;
    memset(&n, 0, sizeof n);
    n.type     = NODE_TYPE_NUMBER;
    n.llnum    = 42;
    n.pos.line = 7;
    printf("type=%d num=%llu line=%d size=%zu\n",
        n.type, n.llnum, n.pos.line, sizeof n);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch24 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "type=2 num=42 line=7" "node fields wired"
pass
