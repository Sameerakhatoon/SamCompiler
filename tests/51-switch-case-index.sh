#!/usr/bin/env bash
# G03: switch case registration assigns each case's literal value to
# parsed_switch_case.index. Verifies that parse_case re-pushes the case
# node so the body doesn't crash, and that the switch.cases vector
# carries indexes 1, 2, 7 in order.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_g03_input.XXXXXX)
printf 'int main() { switch(0) { case 1: case 2: case 7: } }' > "$scratch"

probe=$(mktemp /tmp/sam_g03_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_g03_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
// ch166: parsed_switch_case now lives in compiler.h; no local redef.
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_g03_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fn = *pp;
    struct node** sp = vector_at(fn->func.body_n->body.statements, 0);
    struct node* sw = *sp;
    if(sw->type != NODE_TYPE_STATEMENT_SWITCH){ printf("not switch: %d\n", sw->type); return 1; }
    struct vector* cs = sw->stmt.switch_stmt.cases;
    int n = (int)vector_count(cs);
    printf("n=%d", n);
    for(int i = 0; i < n; i++){
        struct parsed_switch_case* c = vector_at(cs, i);
        printf(" i%d=%d", i, c->index);
    }
    printf("\n");
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "g03 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n=3"   "three cases registered"
assert_contains "$got" "i0=1"  "first case index is 1"
assert_contains "$got" "i1=2"  "second case index is 2"
assert_contains "$got" "i2=7"  "third case index is 7"
pass
