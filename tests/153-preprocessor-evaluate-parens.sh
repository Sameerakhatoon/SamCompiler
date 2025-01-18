#!/usr/bin/env bash
# Ch223: implementing the parentheses node in the preprocessor.
# preprocessor_evaluate switch gains PARENTHESES_NODE which
# recurses into preprocessor_evaluate on parenthesis.exp.
#
# Test: feed `#if (1 + 2) > 0` (body included) and `#if !(0)`
# (body included).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch223_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch223_bin.XXXXXX)
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

int main(void){
    // #if ( 1 + 2 ) > 0  int x ;  #endif
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "if");
    push_op (cp->token_vec_original, "(");
    push_num(cp->token_vec_original, 1);
    push_op (cp->token_vec_original, "+");
    push_num(cp->token_vec_original, 2);
    push_sym(cp->token_vec_original, ')');
    push_op (cp->token_vec_original, ">");
    push_num(cp->token_vec_original, 0);
    push_kw (cp->token_vec_original, "int");
    push_id (cp->token_vec_original, "x");
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "endif");
    push_nl (cp->token_vec_original);
    preprocessor_run(cp);
    int n = vector_count(cp->token_vec);
    printf("n=%d\n", n);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch223 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n=3" "#if (1+2) > 0 with parentheses evaluation includes the body"
pass
