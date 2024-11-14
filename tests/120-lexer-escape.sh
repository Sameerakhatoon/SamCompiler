#!/usr/bin/env bash
# Ch182: escapes inside string literals are now consumed properly.
# `"a\tb"` should produce a 3-char string (a, tab, b) rather than
# dropping the backslash.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch182_input.XXXXXX)
printf 'char* s = "a\\tb";' > "$scratch"
probe=$(mktemp /tmp/sam_ch182_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch182_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    const char* s = var->var.val->sval;
    printf("len=%d c0=%d c1=%d c2=%d\n", (int)strlen(s), s[0], s[1], s[2]);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch182 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "len=3 c0=97 c1=9 c2=98" "string is a (97), tab (9), b (98)"
pass
