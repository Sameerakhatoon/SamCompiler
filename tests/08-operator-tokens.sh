#!/usr/bin/env bash
# Ch10: the lexer reads `50+20+50+39+28+18*5  ++` as a mix of NUMBER and
# OPERATOR tokens, with the greedy two-char operator "++" at the end.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch10_input.XXXXXX)
printf '50+20+50+39+28+18*5  ++' > "$scratch"

probe=$(mktemp /tmp/sam_ch10_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch10_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch10_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }

    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    printf("count=%d\n", n);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_NUMBER){
            printf("[%d] NUM %llu\n", i, t->llnum);
        } else if(t->type == TOKEN_TYPE_OPERATOR){
            printf("[%d] OP %s\n", i, t->sval);
        } else {
            printf("[%d] type=%d\n", i, t->type);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch10 probe failed to compile"

got="$("$bin")"
# Expect: 50, +, 20, +, 50, +, 39, +, 28, +, 18, *, 5, ++  -> 14 tokens
assert_contains "$got" "count=14"   "14 tokens"
assert_contains "$got" "NUM 50"     "first number 50"
assert_contains "$got" "OP +"       "+ operator present"
assert_contains "$got" "OP *"       "* operator present"
assert_contains "$got" "OP ++"      "greedy two-char ++"
pass
