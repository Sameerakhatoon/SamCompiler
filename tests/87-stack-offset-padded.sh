#!/usr/bin/env bash
# Ch139: stack offsets correctly fold variable padding into aoffset
# for primitive locals. Two ints + a char arrange as -4, -8, -12 (no
# padding); int then char then int forces alignment to -12 for the
# second int.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch139_input.XXXXXX)
printf 'int main() { int a; char b; int c; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch139_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch139_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch139_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp); cp->token_vec = lex_process_tokens(lp); parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fn = *pp;
    struct vector* body = fn->func.body_n->body.statements;
    vector_set_peek_pointer(body, 0);
    struct node* a = vector_peek_ptr(body);
    struct node* b = vector_peek_ptr(body);
    struct node* c = vector_peek_ptr(body);
    printf("a=%d b=%d c=%d\n",
        a->var.aoffset, b->var.aoffset, c->var.aoffset);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch139 probe failed to compile"
got="$("$bin")"
# int a -> -4; char b -> -5 (no padding, 1-byte); int c -> -12
# (offset = -5 + -1 - 4 = -10 raw, padded up to -12 for int alignment).
assert_contains "$got" "a=-4 b=-5 c=-12" "stack offsets with int/char/int locals"
pass
