#!/usr/bin/env bash
# Ch28: `58272+2000` parses to one NODE_TYPE_EXPRESSION whose .exp.left
# is the left NUMBER, .exp.right is the right NUMBER, .exp.op is "+".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch28_input.XXXXXX)
printf '58272+2000' > "$scratch"

probe=$(mktemp /tmp/sam_ch28_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch28_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch28_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    int n = vector_count(cp->node_tree_vec);
    printf("roots=%d\n", n);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("root_type=%d\n", nd->type);
    if(nd->type == NODE_TYPE_EXPRESSION){
        printf("op=%s left=%llu right=%llu\n",
            nd->exp.op,
            nd->exp.left  ? nd->exp.left->llnum  : 0,
            nd->exp.right ? nd->exp.right->llnum : 0);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch28 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "root_type=0"        "root is NODE_TYPE_EXPRESSION (0)"
assert_contains "$got" "op=+ left=58272 right=2000"  "exp wired left+op+right"
pass
