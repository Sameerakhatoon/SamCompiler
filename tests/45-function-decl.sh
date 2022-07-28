#!/usr/bin/env bash
# Ch72: `int main() { int x; }` parses to one NODE_TYPE_FUNCTION
# named "main" with body_n -> NODE_TYPE_BODY.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch72_input.XXXXXX)
printf 'int main() { int x; }' > "$scratch"

probe=$(mktemp /tmp/sam_ch72_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch72_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch72_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("type=%d name=%s\n", nd->type, nd->func.name);
    if(nd->func.body_n){
        printf("body_stmts=%d\n", vector_count(nd->func.body_n->body.statements));
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch72 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_FUNCTION == 7
assert_contains "$got" "type=7 name=main"  "NODE_TYPE_FUNCTION named main"
assert_contains "$got" "body_stmts=1"      "body has the one int declaration"
pass
