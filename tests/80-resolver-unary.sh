#!/usr/bin/env bash
# Ch132: resolver_follow_unary dispatches indirection (*) and
# address-of (&). The dispatcher also stamps result + resolver on
# the returned entity.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch132_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_ch132_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch132_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
static void del_scope(struct resolver_scope* s){ (void)s; }
static void del_ent(struct resolver_entity* e){ (void)e; }
static void* mk_priv(struct resolver_entity* e, struct node* n, int o, struct resolver_scope* s){
    (void)e; (void)n; (void)o; (void)s; return NULL;
}
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp); cp->token_vec = lex_process_tokens(lp); parse(cp);

    struct resolver_callbacks cb = {.delete_scope=del_scope,.delete_entity=del_ent,.make_private=mk_priv};
    struct resolver_process* rp = resolver_new_process(cp, &cb);

    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    resolver_new_entity_for_var_node(rp, var, NULL, 8);

    struct node ident = { .type = NODE_TYPE_IDENTIFIER, .sval = "v" };
    struct node star  = { .type = NODE_TYPE_UNARY };
    star.unary.op = "*";
    star.unary.operand = &ident;
    star.unary.indirection.depth = 1;
    struct resolver_result* r = resolver_follow(rp, &star);
    struct resolver_entity* top = resolver_result_peek(r);
    printf("type=%d res_set=%d resv_set=%d\n",
        top ? top->type : -1,
        top ? (top->result != NULL) : -1,
        top ? (top->resolver != NULL) : -1);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch132 probe failed to compile"
got="$("$bin")"
# UNARY_INDIRECTION=8
assert_contains "$got" "type=8 res_set=1 resv_set=1" "follow_unary -> UNARY_INDIRECTION with result+resolver stamped"
pass
