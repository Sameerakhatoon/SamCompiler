#!/usr/bin/env bash
# Ch11: SYMBOL_CASE emits TOKEN_TYPE_SYMBOL with the literal char in cval.
# ')' also drops the paren counter; using ')' without a matching '('
# triggers compiler_error.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch11_input.XXXXXX)
# Mix: operators + symbols + balanced parens.
printf '50+20+50+39+28+18*5  ++ (50+20) [#]' > "$scratch"

probe=$(mktemp /tmp/sam_ch11_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch11_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;

int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch11_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }

    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv), syms = 0;
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_SYMBOL){
            printf("SYM '%c'\n", t->cval);
            syms++;
        }
    }
    printf("count=%d syms=%d\n", n, syms);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch11 probe failed to compile"

got="$("$bin")"
# Expected symbols from "(50+20) [#]": ')', '#', ']'  (and '(', '[' are operators).
assert_contains "$got" "SYM ')'"  "closing paren is a symbol"
assert_contains "$got" "SYM '#'"  "hash is a symbol"
assert_contains "$got" "SYM ']'"  "closing bracket is a symbol"
assert_contains "$got" "syms=3"   "exactly 3 symbol tokens"
pass
