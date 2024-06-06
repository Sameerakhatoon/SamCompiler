#!/usr/bin/env bash
# Ch64: `struct abc { int a; int b; };` parses to NODE_TYPE_STRUCT with
# .body_n being a NODE_TYPE_BODY of 2 statements. Also gets registered
# as a symbol under the name "abc".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch64_input.XXXXXX)
printf 'struct abc { int a; int b; };' > "$scratch"

probe=$(mktemp /tmp/sam_ch64_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch64_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch64_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* nd = *pp;
    printf("type=%d name=%s\n", nd->type, nd->_struct.name);
    if(nd->_struct.body_n){
        int n = vector_count(nd->_struct.body_n->body.statements);
        printf("body_stmts=%d body_size=%zu\n", n, nd->_struct.body_n->body.size);
    }
    struct symbol* s = symresolver_get_symbol(cp, "abc");
    printf("sym_found=%d\n", s != 0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch64 probe failed to compile"
got="$("$bin")"
# NODE_TYPE_STRUCT == 24
assert_contains "$got" "type=24 name=abc"  "NODE_TYPE_STRUCT named 'abc'"
assert_contains "$got" "body_stmts=2"      "struct body has 2 statements"
assert_contains "$got" "sym_found=1"       "struct registered as a symbol"
pass
