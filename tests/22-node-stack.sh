#!/usr/bin/env bash
# Ch26: node.c exposes node_set_vector / push / peek / pop. Verify they
# stack and unstack node pointers correctly, including the
# "popping a root also pops from node_vector_root" invariant.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch26_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch26_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

int main(void){
    struct vector* v    = vector_create(sizeof(struct node*));
    struct vector* vroot= vector_create(sizeof(struct node*));
    node_set_vector(v, vroot);

    struct node* a = calloc(1, sizeof(struct node)); a->type = NODE_TYPE_NUMBER; a->llnum = 1;
    struct node* b = calloc(1, sizeof(struct node)); b->type = NODE_TYPE_NUMBER; b->llnum = 2;

    node_push(a);
    node_push(b);

    struct node* peeked = node_peek();
    printf("peek=%llu\n", peeked->llnum);

    // Push b also into root, so pop of b should drop it from root too.
    vector_push(vroot, &b);
    int root_before = vector_count(vroot);
    struct node* popped = node_pop();
    int root_after  = vector_count(vroot);
    printf("pop=%llu root_before=%d root_after=%d\n",
        popped->llnum, root_before, root_after);

    // After popping b, peek should return a.
    struct node* a2 = node_peek_or_null();
    printf("after_pop_peek=%llu\n", a2->llnum);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch26 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "peek=2"                                     "peek returns top"
assert_contains "$got" "pop=2 root_before=1 root_after=0"           "pop drops from root when matched"
assert_contains "$got" "after_pop_peek=1"                           "stack returns to previous top"
pass
