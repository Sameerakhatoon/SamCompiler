#!/usr/bin/env bash
# Ch42: `int x = 50;` parses to a NODE_TYPE_VARIABLE whose .var.name
# is "x", .var.type.type is DATA_TYPE_INTEGER, .var.val points at a
# NUMBER node carrying 50.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch42_input.XXXXXX)
printf 'int x = 50;' > "$scratch"

probe=$(mktemp /tmp/sam_ch42_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch42_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch42_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    int n = vector_count(cp->node_tree_vec);
    printf("roots=%d\n", n);
    if(n > 0){
        struct node** pp = vector_at(cp->node_tree_vec, 0);
        struct node* nd = *pp;
        printf("type=%d\n", nd->type);
        if(nd->type == NODE_TYPE_VARIABLE){
            printf("name=%s dtype=%d\n", nd->var.name, nd->var.type.type);
            if(nd->var.val){
                printf("val_type=%d val=%llu\n", nd->var.val->type, nd->var.val->llnum);
            }
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch42 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_VARIABLE == 5; DATA_TYPE_INTEGER == 3; NODE_TYPE_NUMBER == 2.
assert_contains "$got" "type=5"             "root is NODE_TYPE_VARIABLE"
assert_contains "$got" "name=x dtype=3"     "var named x of type int"
assert_contains "$got" "val_type=2 val=50"  "init value is NUMBER 50"
pass
