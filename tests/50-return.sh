#!/usr/bin/env bash
# Ch81: `return 42;` and bare `return;` parse to NODE_TYPE_STATEMENT_RETURN.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch81_input.XXXXXX)
printf 'int main() { return 42; }' > "$scratch"

probe=$(mktemp /tmp/sam_ch81_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch81_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch81_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fn = *pp;
    struct node** sp = vector_at(fn->func.body_n->body.statements, 0);
    struct node* rn = *sp;
    printf("type=%d exp_val=%llu\n", rn->type, rn->stmt.return_stmt.exp->llnum);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch81 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_STATEMENT_RETURN == 9
assert_contains "$got" "type=9"       "stmt is NODE_TYPE_STATEMENT_RETURN"
assert_contains "$got" "exp_val=42"   "return expression is 42"
pass
