#!/usr/bin/env bash
# Ch79: `if (1) {} else if (2) {} else {}` parses with the .next field
# of each IF pointing at the next IF (else-if) or an ELSE.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch79_input.XXXXXX)
printf 'int main() { if(1) {} else if(2) {} else {} }' > "$scratch"

probe=$(mktemp /tmp/sam_ch79_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch79_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch79_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fn = *pp;
    struct node** sp = vector_at(fn->func.body_n->body.statements, 0);
    struct node* ifn = *sp;
    printf("if=%d\n", ifn->type);
    struct node* n2 = ifn->stmt.if_stmt.next;
    printf("n2=%d\n", n2 ? n2->type : -1);
    if(n2 && n2->type == NODE_TYPE_STATEMENT_IF){
        struct node* n3 = n2->stmt.if_stmt.next;
        printf("n3=%d\n", n3 ? n3->type : -1);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch79 probe failed to compile"
got="$("$bin")"
# IF == 10, ELSE == 11
assert_contains "$got" "if=10"  "first stmt is IF"
assert_contains "$got" "n2=10"  "second link is another IF (the else-if)"
assert_contains "$got" "n3=11"  "third link is the final ELSE"
pass
