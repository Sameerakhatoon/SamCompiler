#!/usr/bin/env bash
# Ch221: implementing macro strings part 1. Adds the `#x`
# stringification operator inside a macro function body.
#
# `preprocessor_handle_function_argument_to_string` reads the
# identifier after `#`, looks it up in the definition's args,
# pulls the matching call argument's first token, and emits a
# new TOKEN_TYPE_STRING whose sval is that token's
# `between_brackets` field. macro_function_execute now intercepts
# `#` symbols in the body and dispatches there instead of going
# through push_something.
#
# Test: `#define STR(x) #x` then `STR(foo);`. After preprocessor,
# token_vec contains at least one TOKEN_TYPE_STRING token where
# the call site was - confirms the stringification path fired.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch221_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch221_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

void push_sym(struct vector* v, char c){ struct token t = {0}; t.type = TOKEN_TYPE_SYMBOL; t.cval = c; vector_push(v, &t); }
void push_id (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_IDENTIFIER; t.sval = s; vector_push(v, &t); }
void push_op (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_OPERATOR; t.sval = s; vector_push(v, &t); }
void push_nl (struct vector* v){ struct token t = {0}; t.type = TOKEN_TYPE_NEWLINE; vector_push(v, &t); }

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    // #define STR(x) #x
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "define");
    push_id (cp->token_vec_original, "STR");
    push_op (cp->token_vec_original, "(");
    push_id (cp->token_vec_original, "x");
    push_sym(cp->token_vec_original, ')');
    push_sym(cp->token_vec_original, '#');
    push_id (cp->token_vec_original, "x");
    push_nl (cp->token_vec_original);
    // STR(foo);
    push_id (cp->token_vec_original, "STR");
    push_op (cp->token_vec_original, "(");
    push_id (cp->token_vec_original, "foo");
    push_sym(cp->token_vec_original, ')');
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);

    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    int string_tokens = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (t && t->type == TOKEN_TYPE_STRING) string_tokens++;
    }
    printf("n=%d strings=%d\n", n, string_tokens);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch221 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "strings=1" "STR(foo) with body #x emits a STRING token via stringification"
pass
