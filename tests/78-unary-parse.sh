#!/usr/bin/env bash
# Ch130: `int b = -5;` parses with a NODE_TYPE_UNARY ("-") wrapping the
# number. Also covers `*p` indirection depth.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch130_input.XXXXXX)
printf 'int b = -5;' > "$scratch"
probe=$(mktemp /tmp/sam_ch130_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch130_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp); cp->token_vec = lex_process_tokens(lp); parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    struct node* val = var->var.val;
    printf("val_type=%d op=%s operand_type=%d operand_num=%llu\n",
        val ? val->type : -1,
        (val && val->type == NODE_TYPE_UNARY) ? val->unary.op : "(nil)",
        (val && val->type == NODE_TYPE_UNARY) ? val->unary.operand->type : -1,
        (val && val->type == NODE_TYPE_UNARY) ? val->unary.operand->llnum : 0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch130 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_UNARY = 4; NODE_TYPE_NUMBER = 1.
assert_contains "$got" "op=-"            "unary op is -"
assert_contains "$got" "operand_num=5"   "unary operand is 5"
assert_contains "$got" "val_type=21"     "value node is NODE_TYPE_UNARY (=21)"
assert_contains "$got" "operand_type=2"  "unary operand is NODE_TYPE_NUMBER (=2)"
pass
