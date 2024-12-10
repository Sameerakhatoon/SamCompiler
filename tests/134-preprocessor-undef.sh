#!/usr/bin/env bash
# Ch204: implementing #undef. preprocessor_token_is_undef gates
# preprocessor_handle_undef_token which reads the next identifier
# and calls preprocessor_definition_remove on the preprocessor.
# Hashtag dispatcher learns the new directive after define.
#
# Test: feed `#define FOO 1 \n #undef FOO \n` and confirm the
# preprocessor->definitions vector is empty afterwards.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch204_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch204_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }

    struct token tk;
    tk.type = TOKEN_TYPE_SYMBOL;     tk.cval  = '#';     vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval  = "define";vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval  = "FOO";   vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NUMBER;     tk.llnum = 1;        vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NEWLINE;                          vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_SYMBOL;     tk.cval  = '#';     vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval  = "undef"; vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval  = "FOO";   vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NEWLINE;                          vector_push(cp->token_vec_original, &tk);

    preprocessor_run(cp);

    int n = vector_count(cp->preprocessor->definitions);
    printf("after_undef_defs=%d\n", n);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch204 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "after_undef_defs=0" "#undef removes a prior #define from the definitions vector"
pass
