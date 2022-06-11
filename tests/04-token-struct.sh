#!/usr/bin/env bash
# Ch6: compile a tiny probe that uses struct token + struct pos + the
# token-type enum, to confirm the new declarations are wired correctly.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch6_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch6_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "compiler.h"

int main(void){
    struct token t;
    memset(&t, 0, sizeof t);
    t.type        = TOKEN_TYPE_NUMBER;
    t.inum        = 42;
    t.whitespace  = true;

    struct pos p = { .line = 3, .col = 7, .filename = "x.c" };

    printf("type=%d num=%u ws=%d line=%d col=%d file=%s\n",
        t.type, t.inum, (int)t.whitespace, p.line, p.col, p.filename);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "probe failed to compile"
got="$("$bin")"
assert_contains "$got" "type=4 num=42 ws=1 line=3 col=7 file=x.c" "probe output"
pass
