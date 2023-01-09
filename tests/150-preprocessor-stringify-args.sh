#!/usr/bin/env bash
# Ch220 (numbered as "Lecture 220 - Implementing macro strings -
# Part 2" upstream, distinct from ch220 typedef part 2): wires
# the lexer to track per-argument substrings via a new
# argument_string_buffer, exposes them on tokens as
# between_arguments, and switches the stringification path to
# use between_arguments instead of between_brackets so the
# captured value is the raw call argument substring rather than
# the whole paren contents.
#
# Test: lex and run the actual #define STR(x) #x ; STR(hello)
# pipeline by routing input through the real lexer, then check
# that the resulting STRING token has the expected sval.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch220strings_src.XXXXXX.c)
probe=$(mktemp /tmp/sam_ch220strings_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch220strings_bin.XXXXXX)
trap 'rm -f "$src" "$probe" "$bin"' EXIT

cat > "$src" <<'EOF'
#define STR(x) #x
int y = STR(hello);
EOF

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;
int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("$src", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if (!lp){ printf("lp=null\n"); return 0; }
    if (lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("lex=fail\n"); return 0; }
    cp->token_vec_original = lex_process_tokens(lp);
    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    int found_hello = 0;
    int strings = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (!t) continue;
        if (t->type == TOKEN_TYPE_STRING){
            strings++;
            if (t->sval && strstr(t->sval, "hello")) found_hello = 1;
        }
    }
    printf("strings=%d hello=%d\n", strings, found_hello);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch220 strings probe failed to compile"
got="$("$bin")"
assert_contains "$got" "strings=1 hello=1" "STR(hello) stringifies via between_arguments to a STRING token containing 'hello'"
pass
