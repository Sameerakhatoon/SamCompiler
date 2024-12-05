#!/usr/bin/env bash
# Ch14: '\n' produces TOKEN_TYPE_NEWLINE instead of falling through to
# whitespace handling. With four lines of content the lexer should emit
# at least three newline tokens between them.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch14_input.XXXXXX)
printf 'gerog erlgermo \nskgm5845 \nint \nlong ' > "$scratch"

probe=$(mktemp /tmp/sam_ch14_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch14_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch14_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    int nls = 0;
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_NEWLINE){ nls++; }
    }
    printf("count=%d nls=%d\n", n, nls);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch14 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "nls=3"  "3 newline tokens between 4 lines"
pass
