#!/usr/bin/env bash
# Ch201: preprocessor's expressionable configuration. Wires every
# expressionable callback to a preprocessor_node-aware shim and
# exports `preprocessor_expressionable_config`. Also inserts a
# new enum value PREPROCESSOR_PARENTHESES_NODE between EXPRESSION
# and JOINED.
#
# Test: build a fresh expressionable_create with
# preprocessor_expressionable_config, feed `1 + 2`, and confirm
# the resulting top-of-stack is a PREPROCESSOR_EXPRESSION_NODE
# (preprocessor's tag) and the get_node_type callback maps it to
# EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch201_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch201_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct expressionable_config preprocessor_expressionable_config;

// Local mirror of preprocessor_node's head fields so we can poke
// the tag without including preprocessor.c. Layout matches the
// shipped struct.
struct ppnode_head {
    int type;
    struct { long long llnum; } const_val;
    void* a;
    void* b;
    const char* c;
    void* d;
    void* e;
    void* f;
    const char* sval;
};

int main(void){
    struct vector* tokens = vector_create(sizeof(struct token));
    struct vector* nodes  = vector_create(sizeof(void*));
    struct token tk;
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 1; vector_push(tokens, &tk);
    tk.type = TOKEN_TYPE_OPERATOR; tk.sval = "+"; vector_push(tokens, &tk);
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 2; vector_push(tokens, &tk);
    vector_set_peek_pointer(tokens, 0);

    struct expressionable* e = expressionable_create(&preprocessor_expressionable_config, tokens, nodes, 0);
    expressionable_parse(e);

    void* top = vector_back_ptr(nodes);
    int top_tag = top ? ((struct ppnode_head*)top)->type : -1;
    int generic = preprocessor_expressionable_config.callbacks.get_node_type(e, top);
    printf("tag=%d generic=%d EXP_GEN=%d\n", top_tag, generic, EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch201 probe failed to compile"
got="$("$bin")"
# tag=4 PREPROCESSOR_EXPRESSION_NODE; generic=4 EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION
assert_contains "$got" "tag=4 generic=4 EXP_GEN=4" \
    "preprocessor_expressionable_config wires get_node_type to map PREPROCESSOR_EXPRESSION_NODE -> EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION"
pass
