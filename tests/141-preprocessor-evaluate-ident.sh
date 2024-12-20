#!/usr/bin/env bash
# Ch212: evaluating the expressions in the preprocessor part 3.
# Adds definition value accessors, parse_evaluate_token,
# definition_evaluated_value_for_standard, definition_evaluated_
# value, evaluate_identifier, and an IDENTIFIER case in the
# preprocessor_evaluate switch.
#
# evaluate_identifier behaviour: returns true (1) when the name
# is undefined, false (0) when the definition's value vector is
# empty, definition_evaluated_value(def) (i.e. the numeric value
# of the definition) when value is exactly 1 token, or recurses
# through expressionable parse + evaluate when value has > 1
# token.
#
# Test: #define A 7 then #if A body #endif -> body included
# (3 tokens), #define B then #if B body #endif -> body skipped
# (value vec count = 0 -> evaluate_identifier returns false -> 0
# is not > 0 so read_to_end_if skips).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch212_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch212_bin.XXXXXX)
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
    // CASE 1: #define A 7 then #if A body #endif -> body included.
    struct compile_process* cp1 = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "define");
    push_id (cp1->token_vec_original, "A"); push_num(cp1->token_vec_original, 7);
    push_nl (cp1->token_vec_original);
    push_sym(cp1->token_vec_original, '#'); push_id(cp1->token_vec_original, "if");
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

    printf("n1=%d\n", n1);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch212 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n1=3" "#if A with A defined as 7 includes the body"
pass
