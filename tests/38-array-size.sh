#!/usr/bin/env bash
# Ch46: `int x[4][3];` -> .var.type.array.size == 4 * 4 * 3 == 48.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch46_input.XXXXXX)
printf 'int x[4][3];' > "$scratch"

probe=$(mktemp /tmp/sam_ch46_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch46_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch46_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("array_size=%zu\n", nd->var.type.array.size);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch46 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "array_size=48" "int x[4][3] = 4 * 4 * 3 = 48 bytes"
pass
