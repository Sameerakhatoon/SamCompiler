#!/usr/bin/env bash
# Ch73: `int add(int a, int b) { }` parses to NODE_TYPE_FUNCTION with
# .func.args.vector containing 2 variable nodes named a and b.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch73_input.XXXXXX)
printf 'int add(int a, int b) { }' > "$scratch"

probe=$(mktemp /tmp/sam_ch73_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch73_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch73_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("name=%s\n", nd->func.name);
    int n = vector_count(nd->func.args.vector);
    printf("args=%d\n", n);
    for(int i = 0; i < n; i++){
        struct node** ap = vector_at(nd->func.args.vector, i);
        struct node* a = *ap;
        printf("[%d] %s\n", i, a->var.name);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch73 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "name=add" "function named add"
assert_contains "$got" "args=2"   "two arguments"
assert_contains "$got" "[0] a"    "first arg is a"
assert_contains "$got" "[1] b"    "second arg is b"
pass
