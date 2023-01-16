#!/usr/bin/env bash
# Ch228: creating native definitions - part 2. Wires the
# NATIVE_CALLBACK dispatch in preprocessor_definition_value_with_
# arguments and preprocessor_definition_evaluated_value via new
# helpers preprocessor_definition_value_for_native /
# evaluated_value_for_native. Native macros now actually evaluate
# instead of returning the prior TODO #warning stubs (NULL / -1).
#
# Test: feed a source containing `int x = __LINE__;` and confirm
# the resulting token_vec contains a NUMBER token with value 1
# (the line __LINE__ appeared on).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch228_src.XXXXXX.c)
probe=$(mktemp /tmp/sam_ch228_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch228_bin.XXXXXX)
trap 'rm -f "$src" "$probe" "$bin"' EXIT

cat > "$src" <<'EOF'
int x = __LINE__;
EOF

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;
int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("$src", NULL, 0, NULL);
    if (!cp) return 1;
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if (!lp) return 1;
    if (lex(lp) != LEXICAL_ANALYSIS_ALL_OK) return 1;
    cp->token_vec_original = lex_process_tokens(lp);
    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    long long num_val = -1;
    int saw_line_ident = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (!t) continue;
        if (t->type == TOKEN_TYPE_NUMBER && num_val == -1) num_val = t->llnum;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "__LINE__")) saw_line_ident = 1;
    }
    printf("num=%lld line_ident=%d\n", num_val, saw_line_ident);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch228 probe failed to compile"
got="$("$bin")"
# Expected: __LINE__ expands to a NUMBER token (line 1 here);
# the __LINE__ identifier itself should be gone from token_vec.
assert_contains "$got" "num=1 line_ident=0" "__LINE__ expands to NUMBER(line) via the NATIVE_CALLBACK path"
pass
