#!/usr/bin/env bash
# Ch98: `struct dog;` parses as a struct forward declaration; the
# emitted NODE_TYPE_STRUCT carries NODE_FLAG_IS_FORWARD_DECLARATION
# and has a NULL body.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch98_input.XXXXXX)
printf 'struct dog; struct dog { int x; };' > "$scratch"

probe=$(mktemp /tmp/sam_ch98_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch98_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch98_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    int n = (int)vector_count(cp->node_tree_vec);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* fwd = *pp;
    printf("n=%d fwd_type=%d fwd_body=%p fwd_flags=%d\n",
        n, fwd->type, (void*)fwd->_struct.body_n, fwd->flags);
    if(n >= 2){
        struct node** pp2 = vector_at(cp->node_tree_vec, 1);
        struct node* full = *pp2;
        printf("full_type=%d full_has_body=%d\n",
            full->type, full->_struct.body_n != NULL);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch98 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n=2"            "two top-level nodes (fwd + full)"
assert_contains "$got" "fwd_body=(nil)"  "forward decl has NULL body"
assert_contains "$got" "fwd_flags=2"     "fwd flags has IS_FORWARD_DECLARATION bit"
assert_contains "$got" "full_has_body=1" "later definition has a body"
pass
