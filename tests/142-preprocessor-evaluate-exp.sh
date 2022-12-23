#!/usr/bin/env bash
# Ch213: evaluating the expressions in the preprocessor part 4.
# Adds the `arithmetic` shim in helper.c (a switch over a fixed
# operator set: * / + - == != > < >= <= << >> && ||) and wires
# preprocessor_arithmetic, preprocessor_evaluate_exp, and the
# EXPRESSION case in preprocessor_evaluate.
#
# Test: feed `#if 1 + 2 \n int x; \n #endif` and confirm the
# body is included (1 + 2 = 3 > 0). Also `#if 1 - 1` skips body.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch213_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch213_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

void push_sym(struct vector* v, char c){ struct token t = {0}; t.type = TOKEN_TYPE_SYMBOL; t.cval = c; vector_push(v, &t); }
void push_id (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_IDENTIFIER; t.sval = s; vector_push(v, &t); }
void push_kw (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_KEYWORD; t.sval = s; vector_push(v, &t); }
void push_op (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_OPERATOR; t.sval = s; vector_push(v, &t); }
void push_num(struct vector* v, long long n){ struct token t = {0}; t.type = TOKEN_TYPE_NUMBER; t.llnum = n; vector_push(v, &t); }
void push_nl (struct vector* v){ struct token t = {0}; t.type = TOKEN_TYPE_NEWLINE; vector_push(v, &t); }

int run_case(long long a, const char* op, long long b){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "if");
    push_num(cp->token_vec_original, a);
    push_op (cp->token_vec_original, op);
    push_num(cp->token_vec_original, b);
    push_kw (cp->token_vec_original, "int");
    push_id (cp->token_vec_original, "x");
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "endif");
    push_nl (cp->token_vec_original);
    preprocessor_run(cp);
    return vector_count(cp->token_vec);
}

int main(void){
    int n_add = run_case(1, "+", 2);
    int n_sub = run_case(1, "-", 1);
    int n_mul = run_case(3, "*", 2);
    int n_gt  = run_case(5, ">", 2);
    int n_lt  = run_case(5, "<", 2);
    printf("add=%d sub=%d mul=%d gt=%d lt=%d\n", n_add, n_sub, n_mul, n_gt, n_lt);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch213 probe failed to compile"
got="$("$bin")"
# 1+2=3>0 include (3 tokens); 1-1=0 skip (0); 3*2=6 include; 5>2=1 include; 5<2=0 skip.
assert_contains "$got" "add=3 sub=0 mul=3 gt=3 lt=0" "preprocessor arithmetic + comparison operators evaluate correctly"
pass
