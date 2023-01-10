#!/usr/bin/env bash
# Ch19: tokens born inside ( ) have a non-NULL between_brackets pointer
# whose buffer contains the chars read so far inside the paren group.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch19_input.XXXXXX)
printf '(50+20)' > "$scratch"

probe=$(mktemp /tmp/sam_ch19_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch19_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch19_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    int with_bb = 0;
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->between_brackets){
            with_bb++;
            printf("BB [%s]\n", t->between_brackets);
        }
    }
    printf("count=%d with_bb=%d\n", n, with_bb);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch19 probe failed to compile"

got="$("$bin")"
# Tokens for "(50+20)": (, 50, +, 20, ).  '(' is created BEFORE
# lex_new_expression bumps the counter, so '(' itself has no
# between_brackets. After ch220b's token_make_symbol change (peek-
# then-finish-then-consume), the closing ')' fires lex_finish_
# expression before nextc, so ')' is no longer appended to the
# parentheses buffer - the recorded substring is just "50+20".
# ')' itself still gets a between_brackets pointer because
# token_create captures the buffer before the bump-down takes
# effect token-build wise. So 4 tokens (50, +, 20, ')') carry BB
# and the last one shows the buffer up to but not including ')'.
assert_contains "$got" "count=5"      "5 tokens total"
assert_contains "$got" "BB [50+20]"   "buffer contains the raw inside-paren text (no trailing ')' since ch220b)"
pass
