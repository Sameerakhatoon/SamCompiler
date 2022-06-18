#!/usr/bin/env bash
# Ch30: `1+2*3` should parse as `1 + (2*3)`, NOT `(1+2) * 3`.
# Root op is +, right child is the multiplication.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch30_input.XXXXXX)
printf '1+2*3' > "$scratch"

probe=$(mktemp /tmp/sam_ch30_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch30_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch30_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("root_op=%s\n", nd->exp.op);
    printf("left=%llu\n",  nd->exp.left->llnum);
    printf("right_type=%d\n", nd->exp.right->type);
    if(nd->exp.right->type == NODE_TYPE_EXPRESSION){
        printf("right_op=%s left=%llu right=%llu\n",
            nd->exp.right->exp.op,
            nd->exp.right->exp.left->llnum,
            nd->exp.right->exp.right->llnum);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch30 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "root_op=+"                "root is the + operator"
assert_contains "$got" "left=1"                   "left of root is leaf 1"
assert_contains "$got" "right_type=0"             "right of root is an EXPRESSION (the 2*3)"
assert_contains "$got" "right_op=* left=2 right=3" "right subtree is 2*3"
pass
