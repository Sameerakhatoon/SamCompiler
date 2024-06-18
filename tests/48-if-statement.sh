#!/usr/bin/env bash
# Ch78: `int main() { if(1) { int y = 20; } }` parses cleanly. The
# function body contains one NODE_TYPE_STATEMENT_IF whose cond_node
# is a NUMBER(1) and whose body_node is a BODY of 1 stmt.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch78_input.XXXXXX)
printf 'int main() { if(1) { int y = 20; } }' > "$scratch"

probe=$(mktemp /tmp/sam_ch78_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch78_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch78_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fn = *pp;
    if(fn->type != NODE_TYPE_FUNCTION){ printf("FAIL not function\n"); return 1; }
    struct node** sp = vector_at(fn->func.body_n->body.statements, 0);
    struct node* ifn = *sp;
    printf("if_type=%d cond_type=%d cond_val=%llu\n",
        ifn->type, ifn->stmt.if_stmt.cond_node->type,
        ifn->stmt.if_stmt.cond_node->llnum);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch78 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_STATEMENT_IF == 10
assert_contains "$got" "if_type=10"   "first statement is NODE_TYPE_STATEMENT_IF"
assert_contains "$got" "cond_val=1"   "cond is the literal 1"
pass
