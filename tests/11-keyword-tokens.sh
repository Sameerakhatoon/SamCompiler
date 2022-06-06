#!/usr/bin/env bash
# Ch13: identifiers whose spelling matches the reserved-word table get
# re-tagged as KEYWORD instead of IDENTIFIER. Spellings outside the
# table stay as IDENTIFIER.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch13_input.XXXXXX)
printf 'gerog erlgermo skgm5845 int long' > "$scratch"

probe=$(mktemp /tmp/sam_ch13_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch13_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch13_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    int ids=0, kws=0;
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_IDENTIFIER){ ids++; printf("ID %s\n", t->sval); }
        if(t->type == TOKEN_TYPE_KEYWORD){    kws++; printf("KW %s\n", t->sval); }
    }
    printf("ids=%d kws=%d\n", ids, kws);
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
[ -x "$bin" ] || fail "ch13 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "ID gerog"     "gerog stays identifier"
assert_contains "$got" "ID erlgermo"  "erlgermo stays identifier"
assert_contains "$got" "ID skgm5845"  "skgm5845 stays identifier"
assert_contains "$got" "KW int"       "int promoted to keyword"
assert_contains "$got" "KW long"      "long promoted to keyword"
assert_contains "$got" "ids=3 kws=2"  "exact tallies"
pass
