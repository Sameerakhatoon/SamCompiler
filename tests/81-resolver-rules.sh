#!/usr/bin/env bash
# Ch133: resolver_execute_rules folds RULE entity flag sets onto its
# left/right neighbors and removes the RULE node from the chain.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch133_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch133_bin.XXXXXX)
scratch=$(mktemp /tmp/sam_ch133_input.XXXXXX)
printf 'int v;' > "$scratch"
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
    struct resolver_callbacks cb = {.delete_scope=del_scope,.delete_entity=del_ent,.make_private=mk_priv};
    struct resolver_process* rp = resolver_new_process(cp, &cb);
    struct resolver_result* r = resolver_new_result(rp);

    // L: GENERAL, R: GENERAL, with a RULE between them setting
    // DO_INDIRECTION on the right and NO_MERGE_WITH_NEXT_ENTITY on
    // the left.
    struct datatype dt = { .type = DATA_TYPE_INTEGER, .size = 4 };
    struct node dummy = { .type = NODE_TYPE_NUMBER };
    struct resolver_entity* L = resolver_create_new_unknown_entity(rp, r, &dt, &dummy, resolver_scope_current(rp), 0);
    resolver_result_entity_push(r, L);
    struct resolver_entity_rule rule = {0};
    rule.left.flags  = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
    rule.right.flags = RESOLVER_ENTITY_FLAG_DO_INDIRECTION;
    resolver_new_entity_for_rule(rp, r, &rule);
    struct resolver_entity* R = resolver_create_new_unknown_entity(rp, r, &dt, &dummy, resolver_scope_current(rp), 0);
    resolver_result_entity_push(r, R);

    // Clear pre-set flags (the unknown factory ORs both NO_MERGE
    // flags by default; we only care about the rule application).
    L->flags &= ~(RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY | RESOLVER_ENTITY_FLAG_DO_INDIRECTION);
    R->flags &= ~(RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY | RESOLVER_ENTITY_FLAG_DO_INDIRECTION);

    size_t before = r->count;
    resolver_execute_rules(rp, r);
    size_t after = r->count;

    int l_no_merge = (L->flags & RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY) != 0;
    int r_indir   = (R->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION) != 0;

    printf("before=%zu after=%zu l_nm=%d r_ind=%d\n", before, after, l_no_merge, r_indir);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch133 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "before=3 after=2 l_nm=1 r_ind=1" "rule folded; L gets left.flags, R gets right.flags"
pass
