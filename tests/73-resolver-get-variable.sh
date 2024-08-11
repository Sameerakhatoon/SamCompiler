#!/usr/bin/env bash
# G05: resolver_get_variable now finds variables registered via
# resolver_new_entity_for_var_node (the factory used to stamp
# NODE_TYPE_VARIABLE instead of RESOLVER_ENTITY_TYPE_VARIABLE).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_g05_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_g05_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_g05_bin.XXXXXX)
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
    resolver_new_entity_for_var_node(rp, var, NULL, 16);

    struct resolver_entity* got = resolver_get_variable(NULL, rp, "v");
    printf("got=%s off=%d type=%d\n",
        got ? got->name : "(nil)",
        got ? got->offset : -1,
        got ? got->type : -1);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "g05 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "got=v off=16 type=0" "resolver_get_variable finds var, offset and type preserved"
pass
