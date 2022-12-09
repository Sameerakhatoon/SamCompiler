#!/usr/bin/env bash
# Ch144: when a declarator carries pointer stars, the parsed datatype
# now stamps DATATYPE_FLAG_IS_POINTER + the actual depth.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch144_input.XXXXXX)
printf 'int** p;' > "$scratch"
probe=$(mktemp /tmp/sam_ch144_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch144_bin.XXXXXX)
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
    printf("is_ptr=%d depth=%d\n",
        (var->var.type.flags & DATATYPE_FLAG_IS_POINTER) != 0,
        var->var.type.pointer_depth);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch144 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "is_ptr=1 depth=2" "int** stamps IS_POINTER + depth 2"
pass
