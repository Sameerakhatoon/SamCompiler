#!/usr/bin/env bash
# Ch119: array_multiplier / array_offset compute the byte offset for
# the i-th access into an array datatype using the bracket vector
# carried on the parsed datatype.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch119_input.XXXXXX)
printf 'int x[4][3];' > "$scratch"
probe=$(mktemp /tmp/sam_ch119_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch119_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    struct datatype* d = &var->var.type;
    // x[4][3] of int (4 bytes). Offset for x[i][0]:
    //   index=0, idx_val=i -> array_multiplier walks bracket[1]=3,
    //   so size_sum = i * 3 = 3i; element size = 4 -> offset 12*i.
    //   For i=2: 24. For index=1 (last bracket), offset = j * 4.
    printf("a=%d b=%d c=%d d=%d\n",
        array_offset(d, 0, 0),
        array_offset(d, 0, 1),
        array_offset(d, 0, 2),
        array_offset(d, 1, 2));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch119 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "a=0 b=12 c=24 d=8" "x[0][_]=0  x[1][_]=12  x[2][_]=24  x[_][2]=8"
pass
