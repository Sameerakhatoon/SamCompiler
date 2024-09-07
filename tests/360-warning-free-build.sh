#!/usr/bin/env bash
# Code quality: the Makefile used to hide five
# warning classes, and -Wall -Wextra found about 50 warnings, two of
# them real bugs (a NULL dereference in stackframe_back_expect and a
# missing return in vector_pop_value). Every compiler source must now
# compile with the Makefile's CFLAGS (-Wall -Wextra) and no warning.
# The build itself is untouched: objects go to a temporary directory.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_warnings.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cflags=$(make -s -f Makefile -f - print-cflags <<'EOF'
print-cflags:
	@echo $(CFLAGS)
EOF
)
assert_contains "$cflags" "-Wall -Wextra" "Makefile CFLAGS"
case "$cflags" in
    *-Wno-parentheses*|*-Wno-return-type*|*-Wno-sizeof-pointer-memaccess*|*-Wno-unused-function*|*-Wno-unused-variable*)
        fail "Makefile hides warnings again: $cflags" ;;
esac

count=0
for src in main.c $(ls *.c preprocessor/*.c preprocessor/static-includes/*.c helpers/*.c | grep -v '^test\.c$'); do
    gcc -I./ $cflags -c "$src" -o "$tmp/obj.o" 2>>"$tmp/warnings.txt" || fail "$src does not compile"
    count=$((count + 1))
done
[ "$count" -gt 25 ] || fail "only $count sources found"
if grep -q 'warning:' "$tmp/warnings.txt"; then
    grep 'warning:' "$tmp/warnings.txt" | head -10 >&2
    fail "$(grep -c 'warning:' "$tmp/warnings.txt") warnings"
fi

# The NULL dereference: stackframe_back_expect on an empty ledger used
# to read element->name. It must return 0.
probe="$tmp/probe.c"
cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
int main(void){
    struct node fn = {0};
    fn.type = NODE_TYPE_FUNCTION;
    fn.func.frame.elements = vector_create(sizeof(struct stack_frame_element));
    struct stack_frame_element* e = stackframe_back_expect(&fn, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    int a = 1, b = 2, c = 3;
    struct vector* v = vector_create(sizeof(void*));
    void* pa = &a; void* pb = &b; void* pc = &c;
    vector_push(v, &pa); vector_push(v, &pb);
    int at_b = vector_pop_value(v, pb);
    int at_c = vector_pop_value(v, pc);
    printf("empty=%s at_b=%d at_c=%d count=%d\n", e ? "element" : "null", at_b, at_c, vector_count(v));
    return 0;
}
EOF
gcc -I./ -o "$tmp/probe" "$probe" $LINK_OBJS 2>"$tmp/probe.log" || fail "probe did not build: $(head -5 "$tmp/probe.log")"
got=$("$tmp/probe" 2>&1) || fail "probe crashed: $got"
assert_eq "empty=null at_b=1 at_c=-1 count=1" "$got" "stackframe_back_expect and vector_pop_value"
pass
