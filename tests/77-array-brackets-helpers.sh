#!/usr/bin/env bash
# Ch129: array_brackets_count returns dim count; datatype_decrement_pointer
# drops depth and clears IS_POINTER at 0.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch129_input.XXXXXX)
printf 'int x[4][3];' > "$scratch"
probe=$(mktemp /tmp/sam_ch129_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch129_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp); cp->token_vec = lex_process_tokens(lp); parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    printf("brackets=%zu\n", array_brackets_count(&var->var.type));

    struct datatype d = { .flags = DATATYPE_FLAG_IS_POINTER, .pointer_depth = 2 };
    datatype_decrement_pointer(&d);
    printf("dec1_depth=%d dec1_ptr=%d\n", d.pointer_depth, (d.flags & DATATYPE_FLAG_IS_POINTER) != 0);
    datatype_decrement_pointer(&d);
    printf("dec2_depth=%d dec2_ptr=%d\n", d.pointer_depth, (d.flags & DATATYPE_FLAG_IS_POINTER) != 0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch129 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "brackets=2"          "int x[4][3] has 2 brackets"
assert_contains "$got" "dec1_depth=1 dec1_ptr=1" "depth 2 -> 1, still pointer"
assert_contains "$got" "dec2_depth=0 dec2_ptr=0" "depth 1 -> 0, pointer flag cleared"
pass
