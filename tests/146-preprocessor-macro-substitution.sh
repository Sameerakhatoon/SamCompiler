#!/usr/bin/env bash
# Ch218: implementing macro functions part 3. Wires macro
# function argument substitution. preprocessor_macro_function_
# push_something now calls push_something_definition first and
# only falls back to verbatim push when that returns -1.
# token_vec_push_src_resolve_definition handles IDENTIFIER by
# routing back through handle_identifier_for_token_vector.
# preprocessor_evaluate_exp's macro-function-call path now
# actually invokes preprocessor_evaluate_function_call.
#
# Test: feed `#define DBL(x) x + x \n int y = DBL(7);` and
# confirm DBL(7) expands to NUMBER(7) + NUMBER(7) (substitution
# worked) - i.e. two NUMBER tokens with value 7 land in
# token_vec.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch218_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch218_bin.XXXXXX)
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
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "define");
    push_id (cp->token_vec_original, "DBL");
    push_op (cp->token_vec_original, "(");
    push_id (cp->token_vec_original, "x");
    push_sym(cp->token_vec_original, ')');
    push_id (cp->token_vec_original, "x");
    push_op (cp->token_vec_original, "+");
    push_id (cp->token_vec_original, "x");
    push_nl (cp->token_vec_original);
    push_kw (cp->token_vec_original, "int");
    push_id (cp->token_vec_original, "y");
    push_op (cp->token_vec_original, "=");
    push_id (cp->token_vec_original, "DBL");
    push_op (cp->token_vec_original, "(");
    push_num(cp->token_vec_original, 7);
    push_sym(cp->token_vec_original, ')');
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);

    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    int seven_count = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (t && t->type == TOKEN_TYPE_NUMBER && t->llnum == 7) seven_count++;
    }
    printf("n=%d sevens=%d\n", n, seven_count);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch218 probe failed to compile"
got="$("$bin")"
# Substitution gives `int y = 7 + 7 ;` -> 7 tokens, NUMBER 7 appears twice.
assert_contains "$got" "n=7 sevens=2" "DBL(7) expands to NUMBER(7) + NUMBER(7) - argument substitution wired"
pass
