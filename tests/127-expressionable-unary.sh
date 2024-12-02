#!/usr/bin/env bash
# Ch196: expressionable system Part 5 - unary parsing.
#
# New: expressionable_token_next_is_operator,
# expressionable_get_pointer_depth,
# expressionable_parse_for_indirection_unary,
# expressionable_parse_for_normal_unary,
# expressionable_parse_unary. Wired into parse_for_operator's
# "no left operand" branch (must be a unary) and the "follow-up
# operator after an operator" branch.
#
# Test: feed `! 1` (OPERATOR `!`, NUMBER 1) and confirm
# make_unary_node fires with op `!` wrapping NUMBER 1. Also
# feed `* 2` and confirm make_unary_indirection_node fires with
# depth 1.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch196_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch196_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

void expressionable_parse(struct expressionable* expressionable);

struct tn { int type; long long num; int depth; void* operand; char op[4]; };

static int unary_calls = 0;
static int indir_calls = 0;
static char last_unary_op[8] = {0};
static int last_indir_depth = -1;

void* my_handle_number(struct expressionable* e){
    struct token* t = vector_peek(e->token_vec);
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
    n->num = t->llnum;
    return n;
}
void* my_handle_identifier(struct expressionable* e){ return NULL; }
void my_make_exp(struct expressionable* e, void* l, void* r, const char* op){}
void my_make_parens(struct expressionable* e, void* exp){}
void my_make_unary(struct expressionable* e, const char* op, void* operand){
    unary_calls++;
    strncpy(last_unary_op, op, sizeof(last_unary_op)-1);
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_UNARY;
    n->operand = operand;
    strncpy(n->op, op, sizeof(n->op)-1);
    vector_push(e->node_vec_out, &n);
}
void my_make_uind(struct expressionable* e, int d, void* operand){
    indir_calls++;
    last_indir_depth = d;
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_UNARY;
    n->depth = d;
    n->operand = operand;
    vector_push(e->node_vec_out, &n);
}
void my_make_tenary(struct expressionable* e, void* t, void* f){}
int  my_get_type(struct expressionable* e, void* n){ return n ? ((struct tn*)n)->type : -1; }
void* my_get_left(struct expressionable* e, void* n){ return NULL; }
void* my_get_right(struct expressionable* e, void* n){ return NULL; }
void** my_get_left_addr(struct expressionable* e, void* n){ return NULL; }
void** my_get_right_addr(struct expressionable* e, void* n){ return NULL; }
const char* my_get_op(struct expressionable* e, void* n){ return n ? ((struct tn*)n)->op : NULL; }
void my_set_exp(struct expressionable* e, void* n, void* l, void* r, const char* op){}
bool my_should_join(struct expressionable* e, void* p, void* c){ return false; }
void* my_join(struct expressionable* e, void* p, void* c){ return p; }
bool my_expecting(struct expressionable* e, void* n){ return false; }
bool my_is_custom(struct expressionable* e, struct token* t){ return false; }

void install_cbs(struct expressionable* e){
    e->config.callbacks.handle_number_callback = my_handle_number;
    e->config.callbacks.handle_identifier_callback = my_handle_identifier;
    e->config.callbacks.make_expression_node = my_make_exp;
    e->config.callbacks.make_parentheses_node = my_make_parens;
    e->config.callbacks.make_unary_node = my_make_unary;
    e->config.callbacks.make_unary_indirection_node = my_make_uind;
    e->config.callbacks.make_tenary_node = my_make_tenary;
    e->config.callbacks.get_node_type = my_get_type;
    e->config.callbacks.get_left_node = my_get_left;
    e->config.callbacks.get_right_node = my_get_right;
    e->config.callbacks.get_left_node_address = my_get_left_addr;
    e->config.callbacks.get_right_node_address = my_get_right_addr;
    e->config.callbacks.get_node_operator = my_get_op;
    e->config.callbacks.set_exp_node = my_set_exp;
    e->config.callbacks.should_join_nodes = my_should_join;
    e->config.callbacks.join_nodes = my_join;
    e->config.callbacks.expecting_additional_node = my_expecting;
    e->config.callbacks.is_custom_operator = my_is_custom;
}

int main(void){
    // ! 1
    struct expressionable e1 = {0};
    e1.token_vec = vector_create(sizeof(struct token));
    e1.node_vec_out = vector_create(sizeof(void*));
    struct token tk;
    tk.type = TOKEN_TYPE_OPERATOR; tk.sval = "!"; vector_push(e1.token_vec, &tk);
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 1; vector_push(e1.token_vec, &tk);
    vector_set_peek_pointer(e1.token_vec, 0);
    install_cbs(&e1);
    expressionable_parse(&e1);

    // * 2 (indirection depth 1)
    struct expressionable e2 = {0};
    e2.token_vec = vector_create(sizeof(struct token));
    e2.node_vec_out = vector_create(sizeof(void*));
    tk.type = TOKEN_TYPE_OPERATOR; tk.sval = "*"; vector_push(e2.token_vec, &tk);
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 2; vector_push(e2.token_vec, &tk);
    vector_set_peek_pointer(e2.token_vec, 0);
    install_cbs(&e2);
    expressionable_parse(&e2);

    printf("u=%d op=%s i=%d d=%d\n", unary_calls, last_unary_op, indir_calls, last_indir_depth);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch196 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "u=1 op=! i=1 d=1" "parse_unary handles ! and *-indirection via the dedicated callbacks"
pass
