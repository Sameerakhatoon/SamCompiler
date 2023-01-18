#!/usr/bin/env bash
# Ch202: creating the #define macro. preprocessor_handle_token now
# routes TOKEN_TYPE_SYMBOL through preprocessor_handle_symbol; `#`
# kicks off preprocessor_handle_hashtag_token which dispatches to
# preprocessor_handle_definition_token when it sees `define`.
#
# preprocessor_definition is created with the (name, value-token-
# vector, args-vector, preprocessor) tuple. With 0 args, type stays
# PREPROCESSOR_DEFINITION_STANDARD; with >0, becomes
# PREPROCESSOR_DEFINITION_MACRO_FUNCTION.
#
# Definition also gains a back-pointer to the preprocessor.
#
# Test: feed `#define FOO 42` through preprocessor_run; confirm
# the preprocessor's definitions vector grows by one and the
# stored definition has name=FOO, type=STANDARD, value vector
# contains one NUMBER token with llnum=42.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch202_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch202_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }

    // Hand-build a token vector representing: # define FOO 42 \n
    struct token tk;
    tk.type = TOKEN_TYPE_SYMBOL;     tk.cval = '#';   vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval = "define"; vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval = "FOO"; vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NUMBER;     tk.llnum = 42;   vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NEWLINE;                      vector_push(cp->token_vec_original, &tk);

    preprocessor_run(cp);

    // ch227's __LINE__ native lives at index 0 so user defs land at >0.
    int n_defs = vector_count(cp->preprocessor->definitions);
    int user_defs = n_defs - 1;
    vector_set_peek_pointer(cp->preprocessor->definitions, 1);
    struct preprocessor_definition* d = vector_peek_ptr(cp->preprocessor->definitions);
    int name_ok = d && S_EQ(d->name, "FOO");
    int type_ok = d && d->type == PREPROCESSOR_DEFINITION_STANDARD;
    int back_ok = d && d->preprocessor == cp->preprocessor;
    int val_n   = d && d->standard.value ? vector_count(d->standard.value) : -1;
    long long val0 = -1;
    if (d && d->standard.value && vector_count(d->standard.value) >= 1){
        vector_set_peek_pointer(d->standard.value, 0);
        struct token* t = vector_peek(d->standard.value);
        if (t) val0 = t->llnum;
    }
    printf("defs=%d name=%d type=%d back=%d valN=%d v0=%lld\n", user_defs, name_ok, type_ok, back_ok, val_n, val0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch202 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "defs=1 name=1 type=1 back=1 valN=1 v0=42" \
    "#define FOO 42 registers a standard preprocessor definition with value-token NUMBER(42)"
pass
