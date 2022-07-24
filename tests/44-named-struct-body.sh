#!/usr/bin/env bash
# Ch67: coverage test for the post-ch64 struct path. `struct dog { int x; int y; };`
# parses, registers "dog" as a symbol, and the body size is 8 bytes
# (two ints @ 4 bytes each).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch67_input.XXXXXX)
printf 'struct dog { int x; int y; };' > "$scratch"

probe=$(mktemp /tmp/sam_ch67_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch67_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch67_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct symbol* s = symresolver_get_symbol(cp, "dog");
    if(!s){ printf("FAIL no sym\n"); return 1; }
    struct node* nd = s->data;
    printf("name=%s body_size=%zu stmts=%d\n",
        nd->_struct.name,
        nd->_struct.body_n->body.size,
        vector_count(nd->_struct.body_n->body.statements));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch67 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "name=dog"     "struct registered as 'dog'"
assert_contains "$got" "body_size=8"  "8 bytes for two ints"
assert_contains "$got" "stmts=2"      "two members"
pass
