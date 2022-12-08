#!/usr/bin/env bash
# Ch197: expressionable system Part 6 - tenary parsing +
# constructor + public API declarations land in compiler.h.
#
# New: expressionable_init, expressionable_create constructors;
# expressionable_parse_tenary builds COND ? TRUE : FALSE as a
# ? expression node wrapping a tenary node; parse_exp gains the
# if (`(`) / else if (`?`) / else (parse_for_operator) shape.
#
# Test: build via expressionable_create, feed `1 ? 2 : 3` and
# confirm make_tenary_node fires once and a final
# make_expression_node fires with op `?`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch197_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch197_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

struct tn { int type; long long num; void* left; void* right; void* tt; void* tf; char op[4]; };

static int tenary_calls = 0;
static int exp_with_q = 0;

void* my_handle_number(struct expressionable* e){
    struct token* t = vector_peek(e->token_vec);
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
    n->num = t->llnum;
    return n;
}
void* my_handle_identifier(struct expressionable* e){ return NULL; }
void my_make_exp(struct expressionable* e, void* l, void* r, const char* op){
    if (strcmp(op, "?") == 0) exp_with_q++;
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
    n->left = l; n->right = r;
    strncpy(n->op, op, sizeof(n->op)-1);
    vector_push(e->node_vec_out, &n);
}
void my_make_parens(struct expressionable* e, void* exp){}
void my_make_unary(struct expressionable* e, const char* op, void* o){}
void my_make_uind(struct expressionable* e, int d, void* o){}
void my_make_tenary(struct expressionable* e, void* t, void* f){
    tenary_calls++;
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
    n->tt = t; n->tf = f;
    vector_push(e->node_vec_out, &n);
}
int  my_get_type(struct expressionable* e, void* n){ return n ? ((struct tn*)n)->type : -1; }
void* my_get_left(struct expressionable* e, void* n){ return n ? ((struct tn*)n)->left : NULL; }
void* my_get_right(struct expressionable* e, void* n){ return n ? ((struct tn*)n)->right : NULL; }
void** my_get_left_addr(struct expressionable* e, void* n){ return n ? &((struct tn*)n)->left : NULL; }
void** my_get_right_addr(struct expressionable* e, void* n){ return n ? &((struct tn*)n)->right : NULL; }
const char* my_get_op(struct expressionable* e, void* n){ return n ? ((struct tn*)n)->op : NULL; }
void my_set_exp(struct expressionable* e, void* n, void* l, void* r, const char* op){
    struct tn* t = n; t->left = l; t->right = r;
    strncpy(t->op, op, sizeof(t->op)-1); t->op[sizeof(t->op)-1] = 0;
}
bool my_should_join(struct expressionable* e, void* p, void* c){ return false; }
void* my_join(struct expressionable* e, void* p, void* c){ return p; }
bool my_expecting(struct expressionable* e, void* n){ return false; }
bool my_is_custom(struct expressionable* e, struct token* t){ return false; }

int main(void){
    struct expressionable_config cfg = {0};
    cfg.callbacks.handle_number_callback = my_handle_number;
    cfg.callbacks.handle_identifier_callback = my_handle_identifier;
    cfg.callbacks.make_expression_node = my_make_exp;
    cfg.callbacks.make_parentheses_node = my_make_parens;
    cfg.callbacks.make_unary_node = my_make_unary;
    cfg.callbacks.make_unary_indirection_node = my_make_uind;
    cfg.callbacks.make_tenary_node = my_make_tenary;
    cfg.callbacks.get_node_type = my_get_type;
    cfg.callbacks.get_left_node = my_get_left;
    cfg.callbacks.get_right_node = my_get_right;
    cfg.callbacks.get_left_node_address = my_get_left_addr;
    cfg.callbacks.get_right_node_address = my_get_right_addr;
    cfg.callbacks.get_node_operator = my_get_op;
    cfg.callbacks.set_exp_node = my_set_exp;
    cfg.callbacks.should_join_nodes = my_should_join;
    cfg.callbacks.join_nodes = my_join;
    cfg.callbacks.expecting_additional_node = my_expecting;
    cfg.callbacks.is_custom_operator = my_is_custom;

    struct vector* tokens = vector_create(sizeof(struct token));
    struct vector* nodes  = vector_create(sizeof(void*));
    struct token tk;
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 1; vector_push(tokens, &tk);
    tk.type = TOKEN_TYPE_OPERATOR; tk.sval = "?"; vector_push(tokens, &tk);
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 2; vector_push(tokens, &tk);
    tk.type = TOKEN_TYPE_SYMBOL; tk.cval = ':'; vector_push(tokens, &tk);
    tk.type = TOKEN_TYPE_NUMBER; tk.llnum = 3; vector_push(tokens, &tk);
    vector_set_peek_pointer(tokens, 0);

    struct expressionable* e = expressionable_create(&cfg, tokens, nodes, 0);
    expressionable_parse(e);

    printf("t=%d q=%d\n", tenary_calls, exp_with_q);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch197 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "t=1 q=1" "parse_tenary builds 1 ? 2 : 3 with one make_tenary_node + one make_expression_node(?)"
pass
