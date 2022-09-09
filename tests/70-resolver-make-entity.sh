#!/usr/bin/env bash
# Ch122: resolver_make_entity dispatches by node type. VARIABLE nodes
# get var entities (which carry NODE_TYPE_VARIABLE as type per book);
# anything else becomes a GENERAL unknown. Both inherit offset+flags
# from the guided entity and run make_private to stamp private data.
# Also covers resolver_create_new_entity_for_function_call shape.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch122_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_ch122_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch122_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
static int priv_calls = 0;
static void* mk_priv(struct resolver_entity* e, struct node* n, int o, struct resolver_scope* s){
    (void)e; (void)n; (void)o; (void)s; priv_calls++;
    return (void*)0xCAFE;
}
static void del_scope(struct resolver_scope* s){ (void)s; }
static void del_ent(struct resolver_entity* e){ (void)e; }
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct resolver_callbacks cb = { .delete_scope=del_scope, .delete_entity=del_ent, .make_private=mk_priv };
    struct resolver_process* rp = resolver_new_process(cp, &cb);

    // Synthetic NUMBER node: hits the default unknown branch.
    struct node n = { .type = NODE_TYPE_NUMBER, .llnum = 1 };
    struct datatype dt = { .type = DATA_TYPE_INTEGER, .size = 4, .type_str = "int" };
    struct resolver_entity guided = { .offset = 42, .flags = RESOLVER_ENTITY_FLAG_IS_STACK };
    struct resolver_entity* e = resolver_make_entity(rp, NULL, &dt, &n, &guided, resolver_scope_current(rp));
    printf("type=%d off=%d flag_stack=%d priv=%p calls=%d\n",
        e->type, e->offset,
        (e->flags & RESOLVER_ENTITY_FLAG_IS_STACK) != 0,
        e->private, priv_calls);

    // Function-call entity: dtype copied from the left operand.
    struct resolver_entity left = { .dtype = dt };
    struct resolver_entity* fc = resolver_create_new_entity_for_function_call(NULL, rp, &left, NULL);
    printf("fc_type=%d fc_args=%d\n", fc->type, fc->func_call_data.arguments != NULL);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch122 probe failed to compile"
got="$("$bin")"
# GENERAL = 6; the offset / flags propagate; make_private fires once.
assert_contains "$got" "type=6 off=42 flag_stack=1 priv=0xcafe calls=1" "make_entity NUMBER -> GENERAL with inherited offset/flags + private"
# FUNCTION_CALL = 3.
assert_contains "$got" "fc_type=3 fc_args=1" "function_call entity has args vector"
pass
