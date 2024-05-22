#!/usr/bin/env bash
# Ch51: datatype_size / _no_ptr / _element_size / _for_array_access
# return the right thing for plain ints, pointers, and arrays.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch51_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch51_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "compiler.h"

int main(void){
    // plain int
    struct datatype d = {0};
    d.type = DATA_TYPE_INTEGER; d.size = 4;
    printf("plain: size=%zu elem=%zu noptr=%zu\n",
        datatype_size(&d), datatype_element_size(&d), datatype_size_no_ptr(&d));

    // int*
    struct datatype p = d;
    p.flags |= DATATYPE_FLAG_IS_POINTER; p.pointer_depth = 1;
    printf("ptr:   size=%zu elem=%zu noptr=%zu\n",
        datatype_size(&p), datatype_element_size(&p), datatype_size_no_ptr(&p));

    // int [4][3] (total array size = 48)
    struct datatype a = d;
    a.flags |= DATATYPE_FLAG_IS_ARRAY;
    a.array.size = 48;
    printf("arr:   size=%zu elem=%zu noptr=%zu\n",
        datatype_size(&a), datatype_element_size(&a), datatype_size_no_ptr(&a));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch51 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "plain: size=4 elem=4 noptr=4"   "plain int: 4-byte everywhere"
assert_contains "$got" "ptr:   size=4 elem=4 noptr=4"   "int*: pointer is DWORD"
assert_contains "$got" "arr:   size=48 elem=4 noptr=48" "int[4][3]: total 48, element 4"
pass
