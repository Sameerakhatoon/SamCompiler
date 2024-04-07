#!/usr/bin/env bash
# Ch9: the lexer reads `"hello" 5838 "abnc494"` as three tokens:
# STRING("hello"), NUMBER(5838), STRING("abnc494"). Self-contained input.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch9_input.XXXXXX)
printf '"hello" 5838 "abnc494"' > "$scratch"

probe=$(mktemp /tmp/sam_ch9_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch9_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch9_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }

    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    printf("count=%d\n", n);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_STRING){
            printf("[%d] STR \"%s\"\n", i, t->sval);
        } else if(t->type == TOKEN_TYPE_NUMBER){
            printf("[%d] NUM %llu\n", i, t->llnum);
        } else {
            printf("[%d] type=%d\n", i, t->type);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" \
    "$REPO_ROOT"/build/compiler.o \
    "$REPO_ROOT"/build/cprocess.o \
    "$REPO_ROOT"/build/lexer.o \
    "$REPO_ROOT"/build/lex_process.o \
    "$REPO_ROOT"/build/token.o \
    "$REPO_ROOT"/build/parser.o \
    "$REPO_ROOT"/build/helpers/buffer.o \
    "$REPO_ROOT"/build/helpers/vector.o \
    -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch9 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "count=3"           "3 tokens"
assert_contains "$got" 'STR "hello"'       "first string"
assert_contains "$got" "NUM 5838"          "middle number"
assert_contains "$got" 'STR "abnc494"'     "second string"
pass
