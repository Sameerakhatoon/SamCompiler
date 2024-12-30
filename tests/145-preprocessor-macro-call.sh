#!/usr/bin/env bash
# Ch217: implementing macro functions part 2. Wires up the call
# path - identifier with `(` directly after now parses the call
# arguments (handle_identifier_macro_call_arguments using
# handle_identifier_macro_call_argument_parse + _parentheses for
# nesting) and invokes preprocessor_macro_function_execute.
#
# macro_function_execute itself still iterates the definition
# body and calls macro_function_push_something which (per
# upstream) just pushes the raw definition token verbatim
# without doing argument substitution. So `DBL(7)` with body
# `x + x` pushes literally `x + x` into token_vec rather than
# `7 + 7`. Real substitution lands in part 3.
#
# Test: feed `#define DBL(x) x + x \n int y = DBL(7);` and
# confirm the post-preprocessor token_vec contains the literal
# `x + x` (3 tokens) where the call was. No crash, no leftover
# DBL identifier.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch217_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch217_bin.XXXXXX)
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
    // #define DBL(x) x + x
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "define");
    push_id (cp->token_vec_original, "DBL");
    push_op (cp->token_vec_original, "(");
    push_id (cp->token_vec_original, "x");
    push_sym(cp->token_vec_original, ')');
    push_id (cp->token_vec_original, "x");
    push_op (cp->token_vec_original, "+");
    push_id (cp->token_vec_original, "x");
    push_nl (cp->token_vec_original);
    // int y = DBL(7);
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
    int x_count = 0, plus_count = 0, dbl_count = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (!t) continue;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval){
            if (S_EQ(t->sval, "x"))   x_count++;
            if (S_EQ(t->sval, "DBL")) dbl_count++;
        }
        if (t->type == TOKEN_TYPE_OPERATOR && t->sval && S_EQ(t->sval, "+")) plus_count++;
    }
    printf("n=%d x=%d plus=%d dbl=%d\n", n, x_count, plus_count, dbl_count);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch217 probe failed to compile"
got="$("$bin")"
# Expect: int y = x + x ; -> 6 tokens, x ident appears twice, + once, no DBL.
assert_contains "$got" "n=7 x=2 plus=1 dbl=0" "DBL(7) macro call expands to body x + x (substitution lands in part 3)"
pass
