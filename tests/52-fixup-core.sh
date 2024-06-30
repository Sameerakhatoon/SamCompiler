#!/usr/bin/env bash
# G04: fixup_sys_new / fixup_register / fixups_resolve. We register
# two fixups: one resolves immediately, one has a countdown that needs
# two passes. Confirms both vector-slot-size and resolve-loop fixes.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_g04_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_g04_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

static bool always_fix(struct fixup* f){ (void)f; return true; }
static void noop_end(struct fixup* f){ (void)f; }

static bool count_fix(struct fixup* f){
    int* c = fixup_private(f);
    (*c)--;
    return *c <= 0;
}
static void free_end(struct fixup* f){ free(fixup_private(f)); }

int main(void){
    struct fixup_system* sys = fixup_sys_new();
    int* c = malloc(sizeof(int));
    *c = 2;

    struct fixup_config cfg_a = { .fix = always_fix, .end = noop_end, .private = NULL };
    struct fixup_config cfg_b = { .fix = count_fix,  .end = free_end, .private = c };

    fixup_register(sys, &cfg_a);
    fixup_register(sys, &cfg_b);

    printf("before=%d\n", fixup_sys_unresolved_fixups_count(sys));
    bool done1 = fixups_resolve(sys);
    printf("after1=%d done1=%d\n", fixup_sys_unresolved_fixups_count(sys), done1);
    bool done2 = fixups_resolve(sys);
    printf("after2=%d done2=%d\n", fixup_sys_unresolved_fixups_count(sys), done2);
    fixup_sys_free(sys);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "g04 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "before=2"           "two unresolved at start"
assert_contains "$got" "after1=1 done1=0"   "one resolves after first pass"
assert_contains "$got" "after2=0 done2=1"   "all resolve after second pass"
pass
