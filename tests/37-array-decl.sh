#!/usr/bin/env bash
# Ch44: `int x[4][3];` parses to a NODE_TYPE_VARIABLE whose .var.type
# has IS_ARRAY set and .var.type.array.brackets carries two
# NODE_TYPE_BRACKET entries.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch44_input.XXXXXX)
printf 'int x[4][3];' > "$scratch"

probe=$(mktemp /tmp/sam_ch44_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch44_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch44_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("type=%d\n", nd->type);
    printf("is_array=%d\n", (nd->var.type.flags & DATATYPE_FLAG_IS_ARRAY) != 0);
    int bn = vector_count(nd->var.type.array.brackets->n_brackets);
    printf("bracket_count=%d\n", bn);
    for(int i = 0; i < bn; i++){
        struct node** bp = vector_at(nd->var.type.array.brackets->n_brackets, i);
        struct node* b = *bp;
        printf("[%d] btype=%d inner_val=%llu\n", i, b->type, b->bracket.inner->llnum);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch44 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_VARIABLE == 5, NODE_TYPE_BRACKET == 26
assert_contains "$got" "type=5"               "root is variable"
assert_contains "$got" "is_array=1"           "DATATYPE_FLAG_IS_ARRAY set"
assert_contains "$got" "bracket_count=2"      "two bracket pairs"
assert_contains "$got" "btype=26 inner_val=4" "first bracket inner is 4"
assert_contains "$got" "btype=26 inner_val=3" "second bracket inner is 3"
pass
