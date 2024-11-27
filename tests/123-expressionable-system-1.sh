#!/usr/bin/env bash
# Ch192: expressionable system Part 1. The .c body now defines
# parse / parse_single / parse_single_with_flags / parse_token /
# parse_number plus helpers (node_push/pop/peek_or_null, peek_next,
# ignore_nl, callbacks accessor). All routed through the user-supplied
# callbacks struct. This test instantiates a tiny config + token vector
# with a single NUMBER token and confirms expressionable_parse() walks
# the callback for handle_number_callback and stops.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch192_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch192_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

void expressionable_parse(struct expressionable* expressionable);

static int handle_number_calls = 0;
static int is_custom_calls = 0;
static int expecting_calls = 0;

void* my_handle_number(struct expressionable* e){
    handle_number_calls++;
    // Consume the token so the parse loop terminates.
    vector_peek(e->token_vec);
    int* node = malloc(sizeof(int));
    *node = 42;
    return node;
}
void* my_handle_identifier(struct expressionable* e){ return NULL; }
void my_make_expression(struct expressionable* e, void* l, void* r, const char* op){}
void my_make_unary(struct expressionable* e, const char* op, void* operand){}
void my_make_unary_indirection(struct expressionable* e, int depth, void* operand){}
void my_make_tenary(struct expressionable* e, void* tval, void* fval){}
int  my_get_node_type(struct expressionable* e, void* node){ return 0; }
void* my_get_left_node(struct expressionable* e, void* node){ return NULL; }
void* my_get_right_node(struct expressionable* e, void* node){ return NULL; }
void** my_get_left_node_address(struct expressionable* e, void* node){ return NULL; }
void** my_get_right_node_address(struct expressionable* e, void* node){ return NULL; }
const char* my_get_node_operator(struct expressionable* e, void* node){ return NULL; }
void my_set_exp_node(struct expressionable* e, void* node, void* l, void* r, const char* op){}
bool my_should_join(struct expressionable* e, void* prev, void* current){ return false; }
void* my_join_nodes(struct expressionable* e, void* prev, void* current){ return prev; }
bool my_expecting_additional(struct expressionable* e, void* node){
    expecting_calls++;
    return false;
}
bool my_is_custom_op(struct expressionable* e, struct token* token){
    is_custom_calls++;
    return false;
}

int main(void){
    struct expressionable e = {0};
    e.flags = 0;
    e.token_vec = vector_create(sizeof(struct token));
    e.node_vec_out = vector_create(sizeof(void*));

    struct token num = {0};
    num.type = TOKEN_TYPE_NUMBER;
    num.llnum = 7;
    vector_push(e.token_vec, &num);
    vector_set_peek_pointer(e.token_vec, 0);

    e.config.callbacks.handle_number_callback = my_handle_number;
    e.config.callbacks.handle_identifier_callback = my_handle_identifier;
    e.config.callbacks.make_expression_node = my_make_expression;
    e.config.callbacks.make_unary_node = my_make_unary;
    e.config.callbacks.make_unary_indirection_node = my_make_unary_indirection;
    e.config.callbacks.make_tenary_node = my_make_tenary;
    e.config.callbacks.get_node_type = my_get_node_type;
    e.config.callbacks.get_left_node = my_get_left_node;
    e.config.callbacks.get_right_node = my_get_right_node;
    e.config.callbacks.get_left_node_address = my_get_left_node_address;
    e.config.callbacks.get_right_node_address = my_get_right_node_address;
    e.config.callbacks.get_node_operator = my_get_node_operator;
    e.config.callbacks.set_exp_node = my_set_exp_node;
    e.config.callbacks.should_join_nodes = my_should_join;
    e.config.callbacks.join_nodes = my_join_nodes;
    e.config.callbacks.expecting_additional_node = my_expecting_additional;
    e.config.callbacks.is_custom_operator = my_is_custom_op;

    expressionable_parse(&e);

    printf("hn=%d ic=%d ea=%d nodes=%d flag=%d\n",
        handle_number_calls, is_custom_calls, expecting_calls,
        (int)vector_count(e.node_vec_out),
        (int)TOKEN_FLAG_IS_CUSTOM_OPERATOR);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch192 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "hn=1 ic=1 ea=1 nodes=1 flag=1" "expressionable_parse walked single NUMBER token via callbacks"
pass
