#!/usr/bin/env bash
# Ch100: a function body accumulates each local's size into the
# enclosing function's stack_size field.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch100_input.XXXXXX)
printf 'int main() { int a; int b; }' > "$scratch"

probe=$(mktemp /tmp/sam_ch100_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch100_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch100_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fn = *pp;
    printf("stack_size=%zu\n", fn->func.stack_size);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch100 probe failed to compile"
got="$("$bin")"
# Two ints = 8 bytes. (No alignment padding for same-size primitives.)
assert_contains "$got" "stack_size=8" "two int locals -> stack_size 8"
pass
