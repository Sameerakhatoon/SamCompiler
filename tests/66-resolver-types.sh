#!/usr/bin/env bash
# Ch117: resolver type declarations land in compiler.h - flags / enums
# / structs / callback typedefs. No implementation yet; just check the
# decls compile and have the expected shape.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch117_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch117_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stddef.h>
#include "compiler.h"
int main(void){
    struct resolver_result r = {0};
    struct resolver_entity e = {0};
    struct resolver_scope  s = {0};
    e.type  = RESOLVER_ENTITY_TYPE_VARIABLE;
    e.flags = RESOLVER_ENTITY_FLAG_IS_STACK;
    s.flags = RESOLVER_SCOPE_FLAG_IS_STACK;
    r.flags = RESOLVER_RESULT_FLAG_FAILED;
    printf("addr_len=%zu base_len=%zu entity_off=%zu\n",
        sizeof(r.base.address),
        sizeof(r.base.base_address),
        (size_t)e.offset);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch117 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "addr_len=60"  "result.base.address is char[60]"
assert_contains "$got" "base_len=60"  "result.base.base_address is char[60]"
pass
