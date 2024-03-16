#!/usr/bin/env bash
# Ch8: the lexer reads a stream of integer literals separated by whitespace
# and emits one TOKEN_TYPE_NUMBER per literal. Uses a self-contained
# scratch input file so the test is stable as ./test.c evolves chapter
# by chapter.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch8_input.XXXXXX)
printf "5837 2837 3827 1028 4937" > "$scratch"

probe=$(mktemp /tmp/sam_ch8_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch8_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch8_out", 0);
    if(!cp){ printf("FAIL cp\n"); return 1; }
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(!lp){ printf("FAIL lp\n"); return 1; }
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }

    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    printf("count=%d\n", n);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        printf("[%d] type=%d num=%llu ws=%d\n", i, t->type, t->llnum, (int)t->whitespace);
    }
    return 0;
}
EOF

gcc -I"\$REPO_ROOT" "$probe" \
    "$REPO_ROOT"/build/compiler.o \
    "$REPO_ROOT"/build/cprocess.o \
    "$REPO_ROOT"/build/lexer.o \
    "$REPO_ROOT"/build/lex_process.o \
    "$REPO_ROOT"/build/helpers/buffer.o \
    "$REPO_ROOT"/build/helpers/vector.o \
    -I"$REPO_ROOT" \
    -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch8 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "count=5"        "5 tokens"
assert_contains "$got" "num=5837"       "first token value"
assert_contains "$got" "num=4937"       "last token value"
assert_contains "$got" "type=4"         "TOKEN_TYPE_NUMBER == 4"
assert_contains "$got" "ws=1"           "whitespace flag set on at least one token"
pass
