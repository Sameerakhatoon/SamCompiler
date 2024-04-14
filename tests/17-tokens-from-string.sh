#!/usr/bin/env bash
# Ch20: tokens_build_for_string lexes a literal string instead of a FILE*.
# Same lex() pipeline, just a swapped-in v-table that reads from a
# buffer in lex_process->private.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

# We still need a (real) compile_process for the lex_process to point at,
# so create a dummy input file but never read from it.
dummy=$(mktemp /tmp/sam_ch20_dummy.XXXXXX)
printf '' > "$dummy"

probe=$(mktemp /tmp/sam_ch20_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch20_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$dummy"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int main(void){
    struct compile_process* cp = compile_process_create("${dummy}", "/tmp/sam_ch20_out", 0);
    if(!cp){ printf("FAIL cp\n"); return 1; }

    struct lex_process* lp = tokens_build_for_string(cp, "int x = 42");
    if(!lp){ printf("FAIL lp\n"); return 1; }

    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    printf("count=%d\n", n);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_KEYWORD)    printf("KW %s\n", t->sval);
        if(t->type == TOKEN_TYPE_IDENTIFIER) printf("ID %s\n", t->sval);
        if(t->type == TOKEN_TYPE_OPERATOR)   printf("OP %s\n", t->sval);
        if(t->type == TOKEN_TYPE_NUMBER)     printf("NUM %llu\n", t->llnum);
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch20 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "KW int" "int is a keyword"
assert_contains "$got" "ID x"   "x is an identifier"
assert_contains "$got" "OP ="   "= is an operator"
assert_contains "$got" "NUM 42" "42 is a number"
pass
