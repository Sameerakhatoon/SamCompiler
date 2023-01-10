#!/usr/bin/env bash
# Ch221b (upstream "Lecture 221 - Implementing preprocessor unary
# not"): preprocessor_evaluate now handles PREPROCESSOR_UNARY_NODE
# via preprocessor_evaluate_unary which supports !, ~, and -.
# Unknown ops compiler_error.
#
# Test: feed `#if !0` (should include body) and `#if !1` (should
# skip body). Then `#if -1 > 0` (-1 > 0 is 0, skip).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch221b_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch221b_bin.XXXXXX)
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

int run_if_unary(const char* op, long long val){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    // # if <op> <val>  int x ;  # endif
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "if");
    push_op (cp->token_vec_original, op);
    push_num(cp->token_vec_original, val);
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
    int n_not0 = run_if_unary("!", 0); // !0 = 1, include
    int n_not1 = run_if_unary("!", 1); // !1 = 0, skip
    int n_neg  = run_if_unary("-", 1); // -1, not > 0, skip
    printf("not0=%d not1=%d neg=%d\n", n_not0, n_not1, n_neg);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch221 unary probe failed to compile"
got="$("$bin")"
assert_contains "$got" "not0=3 not1=0 neg=0" "preprocessor unary operators (!, -) evaluate correctly in #if"
pass
