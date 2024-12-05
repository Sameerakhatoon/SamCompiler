#!/usr/bin/env bash
# Ch7: build + run still works after wiring in lex_process_create + the
# (still empty) lex() stub. ./main should still report success on
# ./test.c because lex() returns LEXICAL_ANALYSIS_ALL_OK.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
out="$(./main)"
assert_contains "$out" "everything compiled fine" "main output (lexer stub)"

# Probe: confirm we can create a lex_process and that pos starts at 1/1.
probe=$(mktemp /tmp/sam_ch7_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch7_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int main(void){
    struct compile_process* cp = compile_process_create("./test.c", "/tmp/sam_ch7_out", 0, NULL);
    if(!cp){ printf("FAIL cp\n"); return 1; }
    struct lex_process* lp = lex_process_create(cp, 0, 0);
    if(!lp){ printf("FAIL lp\n"); return 1; }
    printf("line=%d col=%d tokens=%d\n",
        lp->pos.line, lp->pos.col, vector_count(lp->token_vec));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch7 probe failed to compile"

got="$("$bin")"
assert_eq "line=1 col=1 tokens=0" "$got" "lex_process initial state"
pass
