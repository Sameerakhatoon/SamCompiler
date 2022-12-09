#!/usr/bin/env bash
# Ch43: `int x, e, d, ii = 50;` parses to one NODE_TYPE_VARIABLE_LIST
# whose .var_list.list vector contains 4 NODE_TYPE_VARIABLE entries.
# (The trailing `= 50` initializer is only attached to the last one
# in upstream's grammar, matching C semantics.)
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch43_input.XXXXXX)
printf 'int x, e, d, ii = 50;' > "$scratch"

probe=$(mktemp /tmp/sam_ch43_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch43_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch43_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    int n = vector_count(cp->node_tree_vec);
    printf("roots=%d\n", n);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("type=%d\n", nd->type);
    if(nd->type == NODE_TYPE_VARIABLE_LIST){
        int m = vector_count(nd->var_list.list);
        printf("count=%d\n", m);
        for(int i = 0; i < m; i++){
            struct node** vp = vector_at(nd->var_list.list, i);
            struct node* v = *vp;
            printf("[%d] name=%s\n", i, v->var.name);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch43 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_VARIABLE_LIST == 6.
assert_contains "$got" "type=6"      "root is NODE_TYPE_VARIABLE_LIST"
assert_contains "$got" "count=4"     "4 variables in the list"
assert_contains "$got" "name=x"      "first is x"
assert_contains "$got" "name=ii"     "last is ii"
pass
