#!/usr/bin/env bash
# Ch16: lexer handles '//' line comments, '/* ... */' block comments,
# and 'X' char literals (incl. '\n' / '\t' / '\\' / '\'' escapes).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch16_input.XXXXXX)
cat > "$scratch" <<'EOF'
// a one-line comment
/* a
   two-line block comment */
'A' '\n' '\t'
EOF

probe=$(mktemp /tmp/sam_ch16_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch16_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch16_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    int comments = 0, chars = 0;
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_COMMENT){ comments++; printf("CMT [%s]\n", t->sval); }
        if(t->type == TOKEN_TYPE_NUMBER && t->cval){
            chars++;
            printf("CHR %d\n", (int)(unsigned char)t->cval);
        }
    }
    printf("comments=%d chars=%d\n", comments, chars);
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
[ -x "$bin" ] || fail "ch16 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "comments=2"  "one line + one block comment"
assert_contains "$got" "chars=3"     "three char literals"
assert_contains "$got" "CHR 65"      "'A' -> 65"
assert_contains "$got" "CHR 10"      "'\\n' -> 10"
assert_contains "$got" "CHR 9"       "'\\t' -> 9"
pass
