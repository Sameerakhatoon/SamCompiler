#!/usr/bin/env bash
# Ch18: `0b1110011` is NUMBER(115). Also the dispatch must NOT swallow
# a bare 'x' or 'b' that doesn't follow NUMBER(0) - those should fall
# through to identifier handling.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch18_input.XXXXXX)
printf '0b1110011 0b1 0xFF box ' > "$scratch"

probe=$(mktemp /tmp/sam_ch18_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch18_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch18_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    struct vector* tv = lex_process_tokens(lp);
    int n = vector_count(tv);
    for(int i = 0; i < n; i++){
        struct token* t = vector_at(tv, i);
        if(t->type == TOKEN_TYPE_NUMBER){ printf("NUM %llu\n", t->llnum); }
        if(t->type == TOKEN_TYPE_IDENTIFIER){ printf("ID %s\n", t->sval); }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch18 probe failed to compile"

got="$("$bin")"
assert_contains "$got" "NUM 115"  "0b1110011 = 115"
assert_contains "$got" "NUM 1"    "0b1 = 1"
assert_contains "$got" "NUM 255"  "0xFF still works = 255"
assert_contains "$got" "ID box"   "bare 'box' identifier is not eaten by special-number"
pass
