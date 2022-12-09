#!/usr/bin/env bash
# Ch63: bare `{ int x; int y; }` parses to one NODE_TYPE_BODY whose
# .body.statements has 2 variable nodes.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch63_input.XXXXXX)
printf '{ int x; int y; }' > "$scratch"

probe=$(mktemp /tmp/sam_ch63_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch63_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch63_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("type=%d\n", nd->type);
    if(nd->type == NODE_TYPE_BODY){
        int n = vector_count(nd->body.statements);
        printf("stmts=%d\n", n);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch63 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_BODY == 8
assert_contains "$got" "type=8"  "root is NODE_TYPE_BODY"
assert_contains "$got" "stmts=2" "two statements in the body"
pass
