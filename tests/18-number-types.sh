#!/usr/bin/env bash
# Ch21: number tokens record their L / f / d suffix in num.type.
# `42` -> NORMAL, `42L` -> LONG, `42f` -> FLOAT.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch21_input.XXXXXX)
printf '42 5837L 7f' > "$scratch"

probe=$(mktemp /tmp/sam_ch21_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch21_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch21_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_NUMBER){
            printf("NUM val=%llu nt=%d\n", t->llnum, t->num.type);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch21 probe failed to compile"

got="$("$bin")"
# NUMBER_TYPE_NORMAL=0, LONG=1, FLOAT=2, DOUBLE=3
assert_contains "$got" "NUM val=42 nt=0"   "42 -> NORMAL"
assert_contains "$got" "NUM val=5837 nt=1" "5837L -> LONG"
assert_contains "$got" "NUM val=7 nt=2"    "7f -> FLOAT"
pass
