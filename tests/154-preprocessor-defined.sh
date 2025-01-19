#!/usr/bin/env bash
# Ch224: implementing joined nodes in the preprocessor. Adds the
# JOINED_NODE evaluator. Only the `defined IDENTIFIER` pattern is
# wired this round: the left side must be the keyword `defined`,
# the right side is pulled as a string via preprocessor_pull_
# string_from (handles IDENTIFIER, KEYWORD, EXPRESSION (recurses
# left), PARENTHESES (recurses inner)), and the result is whether
# the name has a preprocessor definition.
#
# Test: feed `#define FOO 1 \n #if defined FOO body #endif` and
# confirm the body is included. Then `#if defined BAR body
# #endif` (BAR undefined) skips.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch224_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch224_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

void push_sym(struct vector* v, char c){ struct token t = {0}; t.type = TOKEN_TYPE_SYMBOL; t.cval = c; vector_push(v, &t); }
void push_id (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_IDENTIFIER; t.sval = s; vector_push(v, &t); }
void push_kw (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_KEYWORD; t.sval = s; vector_push(v, &t); }
void push_num(struct vector* v, long long n){ struct token t = {0}; t.type = TOKEN_TYPE_NUMBER; t.llnum = n; vector_push(v, &t); }
void push_nl (struct vector* v){ struct token t = {0}; t.type = TOKEN_TYPE_NEWLINE; vector_push(v, &t); }

int main(void){
    // #define FOO 1 \n #if defined FOO int x ; #endif
    struct compile_process* cp1 = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "define");
    push_id (cp1->token_vec_original, "FOO"); push_num(cp1->token_vec_original, 1);
    push_nl (cp1->token_vec_original);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "if");
    push_id (cp1->token_vec_original, "defined");
    push_id (cp1->token_vec_original, "FOO");
    push_kw (cp1->token_vec_original, "int");
    push_id (cp1->token_vec_original, "x");
    push_sym(cp1->token_vec_original, ';');
    push_nl (cp1->token_vec_original);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "endif");
    push_nl (cp1->token_vec_original);
    preprocessor_run(cp1);
    int n1 = vector_count(cp1->token_vec);

    // #if defined BAR int y ; #endif
    struct compile_process* cp2 = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp2->token_vec_original, '#'); push_id(cp2->token_vec_original, "if");
    push_id (cp2->token_vec_original, "defined");
    push_id (cp2->token_vec_original, "BAR");
    push_kw (cp2->token_vec_original, "int");
    push_id (cp2->token_vec_original, "y");
    push_sym(cp2->token_vec_original, ';');
    push_nl (cp2->token_vec_original);
    push_sym(cp2->token_vec_original, '#'); push_id(cp2->token_vec_original, "endif");
    push_nl (cp2->token_vec_original);
    preprocessor_run(cp2);
    int n2 = vector_count(cp2->token_vec);

    printf("n1=%d n2=%d\n", n1, n2);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch224 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n1=3 n2=0" "#if defined NAME includes body when defined, skips otherwise (via JOINED_NODE)"
pass
