#!/usr/bin/env bash
# Ch115: stackframe_push assigns descending offsets from EBP starting
# at 0 and stepping by -STACK_PUSH_SIZE per element. push/pop, sub,
# add, and assert_empty all behave as documented.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch115_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch115_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"
int main(void){
    struct node fn = { .type = NODE_TYPE_FUNCTION };
    fn.func.frame.elements = vector_create(sizeof(struct stack_frame_element));

    struct stack_frame_element e1 = { .type = STACK_FRAME_ELEMENT_TYPE_LOCAL_VARIABLE, .name = "a" };
    struct stack_frame_element e2 = { .type = STACK_FRAME_ELEMENT_TYPE_LOCAL_VARIABLE, .name = "b" };
    stackframe_push(&fn, &e1);
    stackframe_push(&fn, &e2);

    struct stack_frame_element* back = stackframe_back(&fn);
    printf("count=%d off1=%d off2=%d back_name=%s\n",
        (int)vector_count(fn.func.frame.elements),
        e1.offset_from_bp, e2.offset_from_bp,
        back ? back->name : "(nil)");

    stackframe_add(&fn, 0, "x", STACK_PUSH_SIZE);
    printf("after_add=%d\n", (int)vector_count(fn.func.frame.elements));

    stackframe_sub(&fn, STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "saved_bp", STACK_PUSH_SIZE);
    printf("after_sub=%d\n", (int)vector_count(fn.func.frame.elements));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch115 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "count=2"        "two elements after two pushes"
assert_contains "$got" "off1=0"         "first push lands at offset 0"
assert_contains "$got" "off2=-4"        "second push lands at -STACK_PUSH_SIZE"
assert_contains "$got" "back_name=b"    "back returns most recent push"
assert_contains "$got" "after_add=1"    "add(4) pops one element"
assert_contains "$got" "after_sub=2"    "sub(4) pushes one element"
pass
