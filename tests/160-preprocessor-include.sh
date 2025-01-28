#!/usr/bin/env bash
# Ch230: implementing includes - part 2. Wires `#include`. New
# helpers preprocessor_token_is_include, preprocessor_next_token_
# skip_nl, preprocessor_handle_include_token. handle_hashtag_token
# gains an else-if arm dispatching to handle_include_token.
#
# The include path is the next non-newline token; compile_include
# walks the parent's include_dirs trying `<dir>/<file>`. The new
# child compile_process's token_vec is appended to the parent via
# preprocessor_token_vec_push_src.
#
# Test: create an include dir + header file, register the dir on
# the parent's include_dirs, feed `#include "header.h"` then a
# trailing token; confirm the header's tokens land in the parent
# token_vec.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

incdir=$(mktemp -d /tmp/sam_ch230_inc.XXXXXX)
src=$(mktemp /tmp/sam_ch230_src.XXXXXX.c)
probe=$(mktemp /tmp/sam_ch230_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch230_bin.XXXXXX)
trap 'rm -rf "$incdir" "$src" "$probe" "$bin"' EXIT

cat > "$incdir/header.h" <<'EOF'
int included_var;
EOF

cat > "$src" <<EOF
#include "header.h"
int main() {}
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
    // Prepend the test include dir so it gets searched first.
    const char* dir = "$incdir";
    vector_set_peek_pointer(cp->include_dirs, 0);
    vector_push_at(cp->include_dirs, 0, &dir);

    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if (lex(lp) != LEXICAL_ANALYSIS_ALL_OK) return 1;
    cp->token_vec_original = lex_process_tokens(lp);
    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    int saw_included_var = 0;
    int saw_main = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (!t) continue;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "included_var")) saw_included_var = 1;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "main")) saw_main = 1;
    }
    printf("included_var=%d main=%d\n", saw_included_var, saw_main);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch230 probe failed to compile"
got="$("$bin" 2>&1 || true)"
case "$got" in
    *"included_var=1 main=1"*) ;;
    *) fail "expected included_var=1 main=1 in token_vec; got: $got" ;;
esac
pass
