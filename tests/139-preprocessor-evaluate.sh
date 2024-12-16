#!/usr/bin/env bash
# Ch209: evaluating expressions in the preprocessor - Part 1.
# Adds preprocessor_evaluate_number, preprocessor_evaluate
# (switch over the preprocessor_node type with just NUMBER wired
# this round), and preprocessor_parse_evaluate which builds an
# expressionable around the supplied token_vec, parses, pops the
# root, and runs evaluate.
#
# Test: build a single-token vector with NUMBER(42) and confirm
# preprocessor_parse_evaluate returns 42.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch209_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch209_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_parse_evaluate(struct compile_process* compiler, struct vector* token_vec);

int main(void){
    struct vector* tv = vector_create(sizeof(struct token));
    struct token t = {0};
    t.type  = TOKEN_TYPE_NUMBER;
    t.llnum = 42;
    vector_push(tv, &t);
    vector_set_peek_pointer(tv, 0);
    int r = preprocessor_parse_evaluate(NULL, tv);
    printf("eval=%d\n", r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch209 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "eval=42" "preprocessor_parse_evaluate returns the NUMBER value verbatim"
pass
