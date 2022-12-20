#!/usr/bin/env bash
# Ch210: implementing the #if macro. preprocessor_token_is_if
# gates preprocessor_handle_if_token which delegates to
# preprocessor_parse_evaluate on the rest of the input and
# preprocessor_read_to_end_if with true_clause = result > 0.
#
# Only NUMBER eval is wired this round (ch209). Test verifies
# both branches: #if 1 includes body, #if 0 skips body.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch210_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch210_bin.XXXXXX)
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

int run_case(long long cond_val){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    // # if <cond> int x ; # endif
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "if");
    push_num(cp->token_vec_original, cond_val);
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
    int n1 = run_case(1);
    int n0 = run_case(0);
    printf("n1=%d n0=%d\n", n1, n0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch210 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n1=3 n0=0" "#if 1 includes body, #if 0 skips body"
pass
