#!/usr/bin/env bash
# backend split. codegen() dispatches through the target's
# backend, rdefault takes the frame register from the backend, and
# the stdarg natives reach the target code through the generator
# v-table.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_cm3.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

[ -f codegen_x86.c ] || fail "codegen_x86.c missing"

cat > "$tmp/probe.c" <<EOF2
#include <stdio.h>
#include "compiler.h"
extern struct generator x86_codegen;
char* resolver_default_stack_asm_address(int stack_offset, char* out);
int main(void){
    char a[32], b[32];
    const struct target* t = target_x86();
    resolver_default_stack_asm_address(-4, a);
    resolver_default_stack_asm_address(8, b);
    printf("%s %s %s %s %d %d\n", t->backend->name, t->backend->frame_register, a, b,
        x86_codegen.va_start_store != NULL, x86_codegen.va_arg_fetch != NULL);
    return 0;
}
EOF2
gcc -I"$REPO_ROOT" "$tmp/probe.c" $LINK_OBJS -o "$tmp/probe" 2>/dev/null || fail "probe failed to compile"
assert_eq "x86 ebp ebp-4 ebp+8 1 1" "$("$tmp/probe")" "x86 backend descriptor and hooks"
pass
