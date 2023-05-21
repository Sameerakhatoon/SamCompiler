#!/usr/bin/env bash
# struct target. The x86 descriptor must carry exactly the
# constants the front end used before it, x86 must be the current
# target by default, and a compile_process must pick it up.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_cm1_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_cm1_bin.XXXXXX)
src=$(mktemp /tmp/sam_cm1_src.XXXXXX.c)
trap 'rm -f "$probe" "$bin" "$src"' EXIT

printf 'int main(){ return 0; }\n' > "$src"

cat > "$probe" <<EOF2
#include <stdio.h>
#include "compiler.h"
int main(void){
    const struct target* t = target_x86();
    struct compile_process* p = compile_process_create("${src}", NULL, 0, NULL);
    printf("%s %zu %zu %zu %zu %zu %zu %d %d %d %d\n", t->name,
        t->pointer_size, t->long_size, t->stack_slot_size,
        t->frame_header_size, t->arg_slot_size, t->stack_align,
        t->max_reg_args, target_current() == t, p->target == t,
        t->backend->generate == codegen_x86);
    return 0;
}
EOF2

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "probe failed to compile"
got="$("$bin")"
assert_eq "x86 4 4 4 8 4 16 0 1 1 1" "$got" "x86 target descriptor"
pass
