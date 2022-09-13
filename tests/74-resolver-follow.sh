#!/usr/bin/env bash
# Ch126: resolver_follow on an IDENTIFIER node clones the matching
# entity into the result, sets `identifier` to it, and stamps
# referencing_node.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch126_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_ch126_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch126_bin.XXXXXX)
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
    resolver_new_entity_for_var_node(rp, var, NULL, 12);

    // Synthetic IDENTIFIER node pointing at "v".
    struct node id = { .type = NODE_TYPE_IDENTIFIER, .sval = "v" };
    struct resolver_result* r = resolver_follow(rp, &id);
    printf("ok=%d ident=%s root_off=%d ref=%p\n",
        resolver_result_ok(r),
        r->identifier ? r->identifier->name : "(nil)",
        r->identifier ? r->identifier->offset : -1,
        r->identifier ? (void*)r->identifier->last_resolve.referencing_node : NULL);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch126 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "ok=1 ident=v root_off=12" "follow finds v, identifier set, offset preserved"
case "$got" in
    *"ref=(nil)"*) fail "referencing_node should be non-NULL: $got" ;;
esac
pass
