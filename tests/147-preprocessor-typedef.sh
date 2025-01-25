#!/usr/bin/env bash
# Ch219: finishing the typedef directive - part 1.
#
# Adds: preprocessor_peek_next_token_with_vector_no_increment,
# preprocessor_next_token_with_vector (priority vector with
# overflow fallback to compiler tokens),
# preprocessor_definition_create_typedef,
# preprocessor_definition_value_for_typedef(_or_other),
# preprocessor_token_is_typedef,
# preprocessor_handle_typedef_body_for_non_struct_or_union,
# preprocessor_handle_typedef_body (with TODO for struct),
# preprocessor_handle_typedef_token,
# preprocessor_handle_keyword. Wires definition_value_with_
# arguments TYPEDEF case to return _typedef.value. Wires the
# token_vec_push_src_resolve_definition typedef branch to call
# handle_typedef_token. handle_token switch gains
# TOKEN_TYPE_KEYWORD routing to handle_keyword (which dispatches
# typedef).
#
# Test: feed `typedef int ABC; ABC x = 50;` and confirm the
# preprocessor->definitions vector grows by one TYPEDEF
# definition named ABC with value-token-vector containing the
# `int` keyword, and the resulting token_vec has the typedef
# stripped and ABC expanded back to `int` for the variable
# declaration.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch219_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch219_bin.XXXXXX)
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
    // typedef int ABC ;
    push_kw (cp->token_vec_original, "typedef");
    push_kw (cp->token_vec_original, "int");
    push_id (cp->token_vec_original, "ABC");
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);
    // ABC x = 50;
    push_id (cp->token_vec_original, "ABC");
    push_id (cp->token_vec_original, "x");
    push_op (cp->token_vec_original, "=");
    push_num(cp->token_vec_original, 50);
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);

    preprocessor_run(cp);

    // ch227's __LINE__ native lives at index 0 so user defs land at >0.
    int n_defs = vector_count(cp->preprocessor->definitions) - 1;
    vector_set_peek_pointer(cp->preprocessor->definitions, 1);
    struct preprocessor_definition* d = vector_peek_ptr(cp->preprocessor->definitions);
    int name_ok = d && S_EQ(d->name, "ABC");
    int type_ok = d && d->type == PREPROCESSOR_DEFINITION_TYPEDEF;

    int n = vector_count(cp->token_vec);
    int saw_abc = 0;
    int saw_int = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (!t) continue;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "ABC")) saw_abc = 1;
        if (t->type == TOKEN_TYPE_KEYWORD    && t->sval && S_EQ(t->sval, "int")) saw_int = 1;
    }
    printf("defs=%d name=%d type=%d n=%d abc=%d int=%d\n", n_defs, name_ok, type_ok, n, saw_abc, saw_int);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch219 probe failed to compile"
got="$("$bin")"
# Expected: 1 def, name=ABC, type=TYPEDEF; token_vec has `int x = 50 ;` (5 tokens), no ABC, has int.
assert_contains "$got" "defs=1 name=1 type=1 n=5 abc=0 int=1" \
    "typedef int ABC; ABC x = 50; registers a TYPEDEF and expands ABC back to int"
pass
