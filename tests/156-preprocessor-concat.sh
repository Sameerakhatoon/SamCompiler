#!/usr/bin/env bash
# Ch226: processing concat directive in the preprocessor. Adds
# the ## (token-paste) operator inside a macro function body.
# preprocessor_macro_function_push_something now checks for a
# trailing ## via preprocessor_is_next_double_hash; if so calls
# preprocessor_handle_concat which pushes both sides through a
# temp vector, runs tokens_join_vector (serialize -> re-lex) to
# collapse the adjacent tokens into one, then inserts the
# result back at position 0 of value_vec_target.
#
# Adds tokens_join_buffer_write_token, tokens_join_vector in
# token.c and tokens_build_for_string (+ string-buffer lex
# v-table) in lexer.c.
#
# Test: feed `#define CAT(a, b) a ## b` then `CAT(foo, bar);`
# through the real lex + preprocessor pipeline; confirm the
# resulting token_vec contains a single IDENTIFIER `foobar`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch226_src.XXXXXX.c)
probe=$(mktemp /tmp/sam_ch226_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch226_bin.XXXXXX)
trap 'rm -f "$src" "$probe" "$bin"' EXIT

cat > "$src" <<'EOF'
#define CAT(a, b) a ## b
CAT(foo, bar);
EOF

cat > "$probe" <<EOF
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;
int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("$src", NULL, 0, NULL);
    if (!cp) return 1;
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if (!lp) return 1;
    if (lex(lp) != LEXICAL_ANALYSIS_ALL_OK) return 1;
    cp->token_vec_original = lex_process_tokens(lp);
    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    int found_foobar = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (t && t->type == TOKEN_TYPE_IDENTIFIER && t->sval && strcmp(t->sval, "foobar") == 0){
            found_foobar = 1;
        }
    }
    printf("n=%d foobar=%d\n", n, found_foobar);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch226 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "foobar=1" "CAT(foo, bar) ## concat produces a single IDENTIFIER 'foobar'"
pass
