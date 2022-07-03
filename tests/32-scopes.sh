#!/usr/bin/env bash
# Ch38: scope_create_root + scope_new + scope_push + scope_finish
# operate on a stack of scopes. Last-entity lookup walks up the chain.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch38_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch38_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

int main(void){
    struct compile_process cp;
    memset(&cp, 0, sizeof cp);
    scope_create_root(&cp);

    int* a = malloc(sizeof(int)); *a = 1;
    int* b = malloc(sizeof(int)); *b = 2;

    scope_push(&cp, a, sizeof(int));
    printf("root_size=%zu\n", cp.scope.current->size);

    // Open a nested scope and push b into it.
    scope_new(&cp, 0);
    scope_push(&cp, b, sizeof(int));

    // scope_last_entity walks back from current; the most-recent push
    // is b.
    int* last = (int*)scope_last_entity(&cp);
    printf("last=%d\n", *last);

    // Close inner scope; current is now root; last should be a.
    scope_finish(&cp);
    int* last2 = (int*)scope_last_entity(&cp);
    printf("after_finish=%d\n", *last2);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch38 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "root_size=4"    "root size accumulates byte tally"
assert_contains "$got" "last=2"         "inner scope's last entity is b"
assert_contains "$got" "after_finish=1" "popping inner scope brings root's a back as last"
pass
