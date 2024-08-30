#!/usr/bin/env bash
# Ch135: resolver_finalize_result invokes set_result_base on the
# first entity and classifies result flags. For a single VARIABLE
# entity the FIRST_ENTITY_PUSH_VALUE flag stays set.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch135_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_ch135_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch135_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
static int base_calls = 0;
static void my_base(struct resolver_result* r, struct resolver_entity* e){
    (void)r; (void)e; base_calls++;
}
static void del_scope(struct resolver_scope* s){ (void)s; }
static void del_ent(struct resolver_entity* e){ (void)e; }
static void* mk_priv(struct resolver_entity* e, struct node* n, int o, struct resolver_scope* s){
    (void)e; (void)n; (void)o; (void)s; return NULL;
}
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp); cp->token_vec = lex_process_tokens(lp); parse(cp);

    struct resolver_callbacks cb = {
        .delete_scope=del_scope, .delete_entity=del_ent,
        .set_result_base=my_base, .make_private=mk_priv,
    };
    struct resolver_process* rp = resolver_new_process(cp, &cb);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    resolver_new_entity_for_var_node(rp, *pp, NULL, 4);

    struct node id = { .type = NODE_TYPE_IDENTIFIER, .sval = "v" };
    struct resolver_result* r = resolver_follow(rp, &id);
    int push_value = (r->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE) != 0;
    int load_ebx   = (r->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX) != 0;
    printf("base_calls=%d push=%d load=%d\n", base_calls, push_value, load_ebx);

    // Synthetic struct/union variable should flip to LOAD_TO_EBX.
    struct resolver_result* r2 = resolver_new_result(rp);
    struct datatype dt = { .type = DATA_TYPE_STRUCT, .size = 8, .type_str = "x" };
    struct resolver_entity solo = {
        .type = RESOLVER_ENTITY_TYPE_VARIABLE, .dtype = dt,
    };
    resolver_result_entity_push(r2, &solo);
    resolver_finalize_result(rp, r2);
    int p2 = (r2->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE) != 0;
    int l2 = (r2->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX) != 0;
    printf("struct_push=%d struct_load=%d\n", p2, l2);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch135 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "base_calls=1 push=1 load=0" "set_result_base called once; PUSH_VALUE set for simple int var"
assert_contains "$got" "struct_push=0 struct_load=1" "struct-value variable flips to LOAD_TO_EBX"
pass
