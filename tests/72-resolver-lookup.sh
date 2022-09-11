#!/usr/bin/env bash
# Ch125: resolver_get_entity walks the scope chain looking up names.
# Register a function via resolver_regster_function (root scope) and
# a variable via resolver_new_entity_for_var_node (current scope),
# then look both up.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch125_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_ch125_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch125_bin.XXXXXX)
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

    // Register a synthetic function entity at root scope.
    struct datatype rdt = { .type = DATA_TYPE_INTEGER, .size = 4, .type_str = "int" };
    struct node fn = { .type = NODE_TYPE_FUNCTION };
    fn.func.name = "foo";
    fn.func.rtype = rdt;
    resolver_regster_function(rp, &fn, NULL);

    // Register the parsed variable in the current scope.
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    resolver_new_entity_for_var_node(rp, var, NULL, 0);

    // ch125 ships var lookup broken (see G05) because the var-node
    // factory stamps NODE_TYPE_VARIABLE instead of
    // RESOLVER_ENTITY_TYPE_VARIABLE. fn lookup works because
    // resolver_regster_function uses the right constant. Test only
    // what's working at this chapter; G05 adds the var test.
    struct resolver_entity* got_fn   = resolver_get_function(NULL, rp, "foo");
    struct resolver_entity* missing  = resolver_get_entity(NULL, rp, "nope");
    printf("fn=%s missing=%d\n",
        got_fn  ? got_fn->name  : "(nil)",
        missing == NULL);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch125 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "fn=foo missing=1" "function lookup finds foo, misses nope"
pass
