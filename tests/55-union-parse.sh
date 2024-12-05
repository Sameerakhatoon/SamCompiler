#!/usr/bin/env bash
# Ch99: `union foo { int a; int b; };` parses to NODE_TYPE_UNION with
# a body node and the layout uses the largest member's size.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch99_input.XXXXXX)
printf 'union foo { int a; char b; };' > "$scratch"

probe=$(mktemp /tmp/sam_ch99_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch99_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch99_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* un = *pp;
    printf("is_union=%d name=%s body=%p size=%zu\n",
        un->type == NODE_TYPE_UNION,
        un->_union.name ? un->_union.name : "(nil)",
        (void*)un->_union.body_n,
        un->_union.body_n ? un->_union.body_n->body.size : (size_t)0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch99 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "is_union=1" "node is NODE_TYPE_UNION"
assert_contains "$got" "name=foo"   "union name is foo"
assert_contains "$got" "size=4"     "union size is largest field (int = 4)"
pass
