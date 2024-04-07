#!/usr/bin/env bash
# Ch12: default-case dispatch hands off to read_special_token, which spawns
# an IDENTIFIER token for [A-Za-z_][A-Za-z0-9_]*. Numbers inside the run
# of identifier chars still count as identifier chars (so "skgm5845" is
# one identifier, not "skgm" + 5845).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch12_input.XXXXXX)
printf 'gerog erlgermo skgm5845' > "$scratch"

probe=$(mktemp /tmp/sam_ch12_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch12_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch12_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }

    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    printf("count=%d\n", n);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_IDENTIFIER){
            printf("ID %s\n", t->sval);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" \
    "$REPO_ROOT"/build/compiler.o "$REPO_ROOT"/build/cprocess.o \
    "$REPO_ROOT"/build/lexer.o "$REPO_ROOT"/build/lex_process.o \
    "$REPO_ROOT"/build/token.o \
    "$REPO_ROOT"/build/parser.o \
    "$REPO_ROOT"/build/helpers/buffer.o "$REPO_ROOT"/build/helpers/vector.o \
    -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch12 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "count=3"        "3 identifier tokens"
assert_contains "$got" "ID gerog"       "gerog"
assert_contains "$got" "ID erlgermo"    "erlgermo"
assert_contains "$got" "ID skgm5845"    "skgm5845 (digits inside identifier)"
pass
