#!/usr/bin/env bash
# Ch208: creating #ifndef. Mirror of ifdef but read_to_end_if is
# called with true_clause = (definition == NULL).
#
# Test: feed `#define A 1 \n #ifndef A int x; #endif` and confirm
# the body is skipped (token_vec count 0). Then feed
# `#ifndef B int y; #endif` (B not defined) and confirm the body
# reaches token_vec (3 tokens).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch208_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch208_bin.XXXXXX)
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
    // CASE 1: A defined + #ifndef A body #endif -> body skipped.
    struct compile_process* cp1 = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "define");
    push_id (cp1->token_vec_original, "A"); push_num(cp1->token_vec_original, 1);
    push_nl (cp1->token_vec_original);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "ifndef");
    push_id (cp1->token_vec_original, "A");
    push_nl (cp1->token_vec_original);
    push_kw (cp1->token_vec_original, "int");
    push_id (cp1->token_vec_original, "x");
    push_sym(cp1->token_vec_original, ';');
    push_nl (cp1->token_vec_original);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "endif");
    push_nl (cp1->token_vec_original);
    preprocessor_run(cp1);
    int n1 = vector_count(cp1->token_vec);

    // CASE 2: B undefined + #ifndef B body #endif -> body included.
    struct compile_process* cp2 = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp2->token_vec_original, '#'); push_id(cp2->token_vec_original, "ifndef");
    push_id (cp2->token_vec_original, "B");
    push_nl (cp2->token_vec_original);
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

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch208 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n1=0 n2=3" "#ifndef skips body when name is defined, includes when not"
pass
