#!/usr/bin/env bash
# Ch225: evaluating tenaries in the preprocessor. evaluate_exp's
# right-side TENARY_NODE check now actually evaluates the true /
# false branches based on the left operand's truthiness instead
# of falling through with a TODO #warning.
#
# Test: `#if 1 ? 2 : 0` includes body; `#if 0 ? 1 : 0` skips.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch225_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch225_bin.XXXXXX)
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

int run_if(long long cond, long long t, long long f){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    // # if <cond> ? <t> : <f>  int x ;  # endif
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "if");
    push_num(cp->token_vec_original, cond);
    push_op (cp->token_vec_original, "?");
    push_num(cp->token_vec_original, t);
    push_sym(cp->token_vec_original, ':');
    push_num(cp->token_vec_original, f);
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
    int n_true  = run_if(1, 2, 0); // 1 ? 2 : 0 = 2 > 0 include
    int n_false = run_if(0, 2, 0); // 0 ? 2 : 0 = 0, skip
    printf("t=%d f=%d\n", n_true, n_false);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch225 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "t=3 f=0" "#if cond ? t : f evaluates the chosen branch"
pass
