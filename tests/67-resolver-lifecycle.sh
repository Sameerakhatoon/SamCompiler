#!/usr/bin/env bash
# Ch118: resolver process / scope / result lifecycle. Create a
# resolver_process with a minimal callbacks table, push a child scope,
# pop it back to root. Allocate a result, push two entities, peek/pop,
# free.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch118_input.XXXXXX)
printf 'int x;' > "$scratch"
probe=$(mktemp /tmp/sam_ch118_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch118_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"
static int delete_scope_calls = 0;
static void del_scope(struct resolver_scope* s){ (void)s; delete_scope_calls++; }
static void del_entity(struct resolver_entity* e){ (void)e; }
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct resolver_callbacks cb = { .delete_scope = del_scope, .delete_entity = del_entity };
    struct resolver_process* rp = resolver_new_process(cp, &cb);
    printf("root=%d cur_is_root=%d\n",
        resolver_scope_root(rp) != NULL,
        resolver_scope_current(rp) == resolver_scope_root(rp));

    resolver_new_scope(rp, NULL, RESOLVER_SCOPE_FLAG_IS_STACK);
    printf("after_new=%d\n", resolver_scope_current(rp) != resolver_scope_root(rp));
    resolver_finish_scope(rp);
    printf("after_finish=%d scope_dels=%d\n",
        resolver_scope_current(rp) == resolver_scope_root(rp),
        delete_scope_calls);

    struct resolver_result* r = resolver_new_result(rp);
    struct resolver_entity* e1 = resolver_create_new_entity(r, RESOLVER_ENTITY_TYPE_VARIABLE, NULL);
    struct resolver_entity* e2 = resolver_create_new_entity(r, RESOLVER_ENTITY_TYPE_FUNCTION, NULL);
    resolver_result_entity_push(r, e1);
    resolver_result_entity_push(r, e2);
    printf("count=%zu peek_type=%d ok=%d\n",
        r->count,
        resolver_result_peek(r)->type,
        resolver_result_ok(r));
    struct resolver_entity* popped = resolver_result_pop(r);
    printf("popped_type=%d new_count=%zu\n", popped->type, r->count);
    free(popped);
    resolver_result_free(r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch118 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "root=1 cur_is_root=1"          "fresh process has root scope as current"
assert_contains "$got" "after_new=1"                   "new scope advances current"
assert_contains "$got" "after_finish=1 scope_dels=1"   "finish returns to root and calls delete_scope"
assert_contains "$got" "count=2 peek_type=1 ok=1"      "push 2 entities, peek is most recent (FUNCTION=1)"
assert_contains "$got" "popped_type=1 new_count=1"     "pop returns the last-pushed entity"
pass
