#!/usr/bin/env bash
# Ch134: resolver_merge_compile_times calls back into merge_entities
# for each adjacent pair and folds them when the callback returns
# non-NULL. NO_MERGE flags block fusion.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch134_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch134_bin.XXXXXX)
scratch=$(mktemp /tmp/sam_ch134_input.XXXXXX)
printf 'int v;' > "$scratch"
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
static int merge_calls = 0;
static struct resolver_entity* my_merge(struct resolver_process* p, struct resolver_result* r,
                                         struct resolver_entity* L, struct resolver_entity* R){
    (void)p; (void)r; merge_calls++;
    L->offset += R->offset;     // pretend we summed something
    return L;
}
static void del_scope(struct resolver_scope* s){ (void)s; }
static void del_ent(struct resolver_entity* e){ (void)e; }
static void* mk_priv(struct resolver_entity* e, struct node* n, int o, struct resolver_scope* s){
    (void)e; (void)n; (void)o; (void)s; return NULL;
}
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct resolver_callbacks cb = {.delete_scope=del_scope,.delete_entity=del_ent,
                                     .merge_entities=my_merge, .make_private=mk_priv};
    struct resolver_process* rp = resolver_new_process(cp, &cb);
    struct resolver_result* r = resolver_new_result(rp);

    struct datatype dt = { .type = DATA_TYPE_INTEGER, .size = 4 };
    struct node dummy = { .type = NODE_TYPE_NUMBER };

    // Three GENERAL entities, all mergeable.
    struct resolver_entity* A = resolver_create_new_unknown_entity(rp, r, &dt, &dummy, resolver_scope_current(rp), 4);
    struct resolver_entity* B = resolver_create_new_unknown_entity(rp, r, &dt, &dummy, resolver_scope_current(rp), 8);
    struct resolver_entity* C = resolver_create_new_unknown_entity(rp, r, &dt, &dummy, resolver_scope_current(rp), 16);
    A->flags = 0; B->flags = 0; C->flags = 0;
    resolver_result_entity_push(r, A);
    resolver_result_entity_push(r, B);
    resolver_result_entity_push(r, C);
    size_t before = r->count;
    resolver_merge_compile_times(rp, r);
    printf("before=%zu after=%zu merge_calls=%d\n", before, r->count, merge_calls);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch134 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "before=3 after=1 merge_calls=2" "merge collapses 3 entities into 1 with 2 callback calls"
pass
