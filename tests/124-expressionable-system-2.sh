#!/usr/bin/env bash
# Ch193: expressionable system Part 2. New: parse_identifier (routes
# through handle_identifier_callback), parse_exp / parse_for_operator
# (binary expression: pop left, consume operator token, parse right,
# call make_expression_node), expressionable_error stub,
# expressionable_token_next, and TOKEN_TYPE_IDENTIFIER +
# TOKEN_TYPE_OPERATOR cases in parse_token. Reorder lands in Part 3
# so the chapter stubs out that call.
#
# Test: feed `a + 7` (IDENTIFIER, OPERATOR, NUMBER) and confirm
# make_expression_node is called exactly once with op="+", and
# handle_identifier_callback / handle_number_callback fire once each.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch193_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch193_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

void expressionable_parse(struct expressionable* expressionable);

static int hn_calls = 0, hi_calls = 0, mk_exp_calls = 0;
static char last_op[8] = {0};

void* my_handle_number(struct expressionable* e){
    hn_calls++;
    vector_peek(e->token_vec);
    int* n = malloc(sizeof(int)); *n = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
    return n;
}
void* my_handle_identifier(struct expressionable* e){
    hi_calls++;
    vector_peek(e->token_vec);
    int* n = malloc(sizeof(int)); *n = EXPRESSIONABLE_GENERIC_TYPE_IDENTIFIER;
    return n;
}
void my_make_exp(struct expressionable* e, void* l, void* r, const char* op){
    mk_exp_calls++;
    strncpy(last_op, op, sizeof(last_op)-1);
    int* n = malloc(sizeof(int)); *n = EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
    vector_push(e->node_vec_out, &n);
}
void my_make_unary(struct expressionable* e, const char* op, void* o){}
void my_make_uind(struct expressionable* e, int d, void* o){}
void my_make_tenary(struct expressionable* e, void* t, void* f){}
int  my_get_type(struct expressionable* e, void* n){ return n ? *(int*)n : -1; }
void* my_get_left(struct expressionable* e, void* n){ return NULL; }
void* my_get_right(struct expressionable* e, void* n){ return NULL; }
void** my_get_left_addr(struct expressionable* e, void* n){ return NULL; }
void** my_get_right_addr(struct expressionable* e, void* n){ return NULL; }
const char* my_get_op(struct expressionable* e, void* n){ return NULL; }
void my_set_exp(struct expressionable* e, void* n, void* l, void* r, const char* op){}
bool my_should_join(struct expressionable* e, void* p, void* c){ return false; }
void* my_join(struct expressionable* e, void* p, void* c){ return p; }
bool my_expecting(struct expressionable* e, void* n){ return false; }
bool my_is_custom(struct expressionable* e, struct token* t){ return false; }

int main(void){
    struct expressionable e = {0};
    e.token_vec = vector_create(sizeof(struct token));
    e.node_vec_out = vector_create(sizeof(void*));

    struct token id = {0};
    id.type = TOKEN_TYPE_IDENTIFIER;
    id.sval = "a";
    struct token op = {0};
    op.type = TOKEN_TYPE_OPERATOR;
    op.sval = "+";
    struct token num = {0};
    num.type = TOKEN_TYPE_NUMBER;
    num.llnum = 7;
    vector_push(e.token_vec, &id);
    vector_push(e.token_vec, &op);
    vector_push(e.token_vec, &num);
    vector_set_peek_pointer(e.token_vec, 0);

    e.config.callbacks.handle_number_callback = my_handle_number;
    e.config.callbacks.handle_identifier_callback = my_handle_identifier;
    e.config.callbacks.make_expression_node = my_make_exp;
    e.config.callbacks.make_unary_node = my_make_unary;
    e.config.callbacks.make_unary_indirection_node = my_make_uind;
    e.config.callbacks.make_tenary_node = my_make_tenary;
    e.config.callbacks.get_node_type = my_get_type;
    e.config.callbacks.get_left_node = my_get_left;
    e.config.callbacks.get_right_node = my_get_right;
    e.config.callbacks.get_left_node_address = my_get_left_addr;
    e.config.callbacks.get_right_node_address = my_get_right_addr;
    e.config.callbacks.get_node_operator = my_get_op;
    e.config.callbacks.set_exp_node = my_set_exp;
    e.config.callbacks.should_join_nodes = my_should_join;
    e.config.callbacks.join_nodes = my_join;
    e.config.callbacks.expecting_additional_node = my_expecting;
    e.config.callbacks.is_custom_operator = my_is_custom;

    expressionable_parse(&e);

    printf("hn=%d hi=%d mk=%d op=%s\n", hn_calls, hi_calls, mk_exp_calls, last_op);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch193 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "hn=1 hi=1 mk=1 op=+" "expressionable parses identifier OP number into a binary expression"
pass
