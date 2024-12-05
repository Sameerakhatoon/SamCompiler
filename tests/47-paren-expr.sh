#!/usr/bin/env bash
# Ch77: `(50 + 20)` parses to a NODE_TYPE_EXPRESSION_PARENTHESES whose
# .parenthesis.exp is the inner `50+20` expression.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch77_input.XXXXXX)
printf 'x = (50 + 20)' > "$scratch"

probe=$(mktemp /tmp/sam_ch77_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch77_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch77_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    if(vector_count(cp->node_tree_vec) == 0){
        printf("roots=0\n");
        return 0;
    }
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("root_type=%d\n", nd->type);
    if(nd->type == NODE_TYPE_EXPRESSION){
        printf("root_op=%s right_type=%d\n", nd->exp.op, nd->exp.right->type);
        if(nd->exp.right->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            struct node* inner = nd->exp.right->parenthesis.exp;
            printf("inner_op=%s\n", inner->exp.op);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch77 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "root_type=0"   "root is NODE_TYPE_EXPRESSION (assignment)"
assert_contains "$got" "root_op="      "assignment operator present"
assert_contains "$got" "right_type=1"  "RHS is NODE_TYPE_EXPRESSION_PARENTHESES"
assert_contains "$got" "inner_op=+"    "inner expression's op is +"
pass
