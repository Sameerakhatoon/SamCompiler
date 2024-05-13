#!/usr/bin/env bash
# Ch40: symresolver_register_symbol stashes by name; get_symbol finds.
# new_table / end_table stack-saves the active table so nested scopes
# can shadow without losing the outer set.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch40_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch40_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

int main(void){
    struct compile_process cp;
    memset(&cp, 0, sizeof cp);
    symresolver_initialize(&cp);
    // initial table must be created via new_table before we register.
    symresolver_new_table(&cp);

    int v1 = 42;
    int v2 = 99;
    struct symbol* a = symresolver_register_symbol(&cp, "foo", SYMBOL_TYPE_NATIVE_FUNCTION, &v1);
    struct symbol* b = symresolver_register_symbol(&cp, "bar", SYMBOL_TYPE_NODE,            &v2);
    printf("a_ok=%d b_ok=%d\n", a != 0, b != 0);

    // Duplicate registration must return NULL.
    struct symbol* dup = symresolver_register_symbol(&cp, "foo", SYMBOL_TYPE_NODE, &v2);
    printf("dup=%d\n", dup == 0);

    struct symbol* f = symresolver_get_symbol(&cp, "foo");
    printf("found=%d type=%d val=%d\n", f != 0, f ? f->type : -1, f ? *(int*)f->data : -1);

    // Push a fresh nested table; foo should now be invisible.
    symresolver_new_table(&cp);
    struct symbol* miss = symresolver_get_symbol(&cp, "foo");
    printf("nested_miss=%d\n", miss == 0);

    symresolver_end_table(&cp);
    struct symbol* refound = symresolver_get_symbol(&cp, "foo");
    printf("refound=%d\n", refound != 0);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch40 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "a_ok=1 b_ok=1"                      "register two distinct symbols"
assert_contains "$got" "dup=1"                              "duplicate name returns NULL"
assert_contains "$got" "found=1 type=1 val=42"              "lookup foo returns native-function symbol"
assert_contains "$got" "nested_miss=1"                      "fresh table doesn't see outer symbols"
assert_contains "$got" "refound=1"                          "end_table restores outer view"
pass
