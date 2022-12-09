#!/usr/bin/env bash
# Ch17: 0xAB75 is one NUMBER token with value 0xAB75. The dispatch sees
# NUMBER(0) then 'x', pops NUMBER(0), consumes 'x', reads the hex digits.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch17_input.XXXXXX)
printf '0xAB75 0xff 0x10' > "$scratch"

probe=$(mktemp /tmp/sam_ch17_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch17_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch17_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    printf("count=%d\n", n);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_NUMBER){ printf("NUM %llu\n", t->llnum); }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch17 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "count=3"     "3 number tokens (one per hex literal)"
assert_contains "$got" "NUM 43893"   "0xAB75 = 43893"
assert_contains "$got" "NUM 255"     "0xff = 255"
assert_contains "$got" "NUM 16"      "0x10 = 16"
pass
