#!/usr/bin/env bash
# Ch22: after compile_file runs, compile_process.token_vec holds the
# vector of tokens lex() produced.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch22_input.XXXXXX)
printf 'int x = 42;' > "$scratch"

probe=$(mktemp /tmp/sam_ch22_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch22_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;

int main(void){
    // Replicate what compile_file does, end-to-end, then poke at
    // process->token_vec.
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch22_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if(lex(lp) != LEXICAL_ANALYSIS_ALL_OK){ printf("FAIL lex\n"); return 1; }
    cp->token_vec = lex_process_tokens(lp);

    if(!cp->token_vec){ printf("FAIL: no token_vec\n"); return 1; }
    int n = vector_count(cp->token_vec);
    printf("count=%d\n", n);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -8
[ -x "$bin" ] || fail "ch22 probe failed to compile"

got="$("$bin")"
# tokens: int, x, =, 42, ;  -> 5
assert_contains "$got" "count=5" "token vector reachable via compile_process"
pass
