#!/usr/bin/env bash
# Ch195: expressionable system Part 4. Adds parentheses parsing.
# New helpers: expressionable_generic_type_is_value_expressionable,
# expressionable_expect_op, expressionable_expect_sym,
# expressionable_deal_with_additional_expression,
# expressionable_parse_parentheses. Wires parse_for_operator's
# `(` branch + parse_exp's leading `(` branch to call it, and
# routes is_custom_operator path to the real parse_exp.
# Adds make_parentheses_node callback slot + is_operator_token
# token helper.
#
# Test: feed `( 1 ) + 2` through expressionable_parse and confirm
# make_parentheses_node is invoked exactly once, and the
# resulting root is a PLUS expression whose left child is a
# PARENTHESES node wrapping NUMBER 1 and whose right child is
# NUMBER 2. (A trailing operator is required: parse_exp falls
# through to parse_for_operator after parse_parentheses, and
# parse_for_operator would null-deref otherwise. Same as
# upstream; the preprocessor will only ever call this on full
# expressions.)
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch195_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch195_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

void expressionable_parse(struct expressionable* expressionable);

struct tn { int type; long long num; void* left; void* right; void* inner; char op[4]; };

static int paren_calls = 0;

void* my_handle_number(struct expressionable* e){
    struct token* t = vector_peek(e->token_vec);
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
    n->num = t->llnum;
    return n;
}
void* my_handle_identifier(struct expressionable* e){ return NULL; }
void my_make_exp(struct expressionable* e, void* l, void* r, const char* op){
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
    n->left = l; n->right = r;
    strncpy(n->op, op, sizeof(n->op)-1);
    vector_push(e->node_vec_out, &n);
}
void my_make_parens(struct expressionable* e, void* exp){
    paren_calls++;
    struct tn* n = calloc(1, sizeof(*n));
    n->type = EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES;
    n->inner = exp;
    vector_push(e->node_vec_out, &n);
}
void my_make_unary(struct expressionable* e, const char* op, void* o){}
void my_make_uind(struct expressionable* e, int d, void* o){}
void my_make_tenary(struct expressionable* e, void* t, void* f){}
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

void push_num(struct vector* v, long long n){ struct token t = {0}; t.type = TOKEN_TYPE_NUMBER; t.llnum = n; vector_push(v, &t); }
void push_op (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_OPERATOR; t.sval = s; vector_push(v, &t); }
void push_sym(struct vector* v, char c){ struct token t = {0}; t.type = TOKEN_TYPE_SYMBOL; t.cval = c; vector_push(v, &t); }

int main(void){
    struct expressionable e = {0};
    e.token_vec = vector_create(sizeof(struct token));
    e.node_vec_out = vector_create(sizeof(void*));
    // ( 1 ) + 2
    push_op (e.token_vec, "(");
    push_num(e.token_vec, 1);
    {
        struct token t = {0};
        t.type = TOKEN_TYPE_SYMBOL;
        t.cval = ')';
        vector_push(e.token_vec, &t);
    }
    push_op (e.token_vec, "+");
    push_num(e.token_vec, 2);
    vector_set_peek_pointer(e.token_vec, 0);

    e.config.callbacks.handle_number_callback = my_handle_number;
    e.config.callbacks.handle_identifier_callback = my_handle_identifier;
    e.config.callbacks.make_expression_node = my_make_exp;
    e.config.callbacks.make_parentheses_node = my_make_parens;
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

    struct tn* root = vector_back_ptr(e.node_vec_out);
    int is_plus = root && strcmp(root->op, "+") == 0;
    int left_parens = root && root->left && ((struct tn*)root->left)->type == EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES;
    int inner_one = left_parens && ((struct tn*)root->left)->inner && ((struct tn*)((struct tn*)root->left)->inner)->num == 1;
    int right_two = root && root->right && ((struct tn*)root->right)->type == EXPRESSIONABLE_GENERIC_TYPE_NUMBER && ((struct tn*)root->right)->num == 2;
    printf("paren=%d +=%d Lp=%d L1=%d R2=%d\n", paren_calls, is_plus, left_parens, inner_one, right_two);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch195 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "paren=1 +=1 Lp=1 L1=1 R2=1" "parse_parentheses builds (1) + 2 with the parens node on the left"
pass
