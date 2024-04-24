#!/usr/bin/env bash
# Ch31: `50*30+20` should parse as `(50*30) + 20`, i.e. root op is +,
# left subtree is an EXPRESSION (50*30), right is the leaf 20.
# Same parser, different shape than ch30's `1+2*3`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch31_input.XXXXXX)
printf '50*30+20' > "$scratch"

probe=$(mktemp /tmp/sam_ch31_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch31_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch31_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("root_op=%s\n", nd->exp.op);
    printf("right=%llu\n", nd->exp.right->llnum);
    printf("left_type=%d\n", nd->exp.left->type);
    if(nd->exp.left->type == NODE_TYPE_EXPRESSION){
        printf("left_op=%s left=%llu right=%llu\n",
            nd->exp.left->exp.op,
            nd->exp.left->exp.left->llnum,
            nd->exp.left->exp.right->llnum);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch31 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "root_op=+"                "root is the + operator"
assert_contains "$got" "right=20"                 "right of root is leaf 20"
assert_contains "$got" "left_type=0"              "left of root is EXPRESSION (the 50*30)"
assert_contains "$got" "left_op=* left=50 right=30" "left subtree is 50*30"
pass
