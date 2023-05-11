#!/usr/bin/env bash
# -target= flag, per-target descriptors and predefined macros.
#   - -target=x86 gives the same asm as no flag at all
#   - an unknown name is rejected
#   - each target predefines its own macro and none of the others
#   - the LP64 descriptors have 8-byte long and pointers
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_cm2.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

"$REPO_ROOT/main" -target=x86 samples/09_factorial_recursion.c "$tmp/flag.asm" object >/dev/null 2>&1 || true
cmp -s "$tmp/flag.asm" tests/golden/x86/09_factorial_recursion.asm || fail "-target=x86 output differs from the default"

out=$("$REPO_ROOT/main" -target=sparc samples/09_factorial_recursion.c "$tmp/x" 2>&1 && echo "rc=0" || echo "rc=$?")
assert_contains "$out" "Unknown target sparc" "unknown target message"
assert_contains "$out" "rc=1" "unknown target exit code"

printf 'int main(){ return 0; }\n' > "$tmp/src.c"
cat > "$tmp/probe.c" <<EOF2
#include <stdio.h>
#include "compiler.h"
struct preprocessor_definition* preprocessor_get_definition(struct preprocessor* preprocessor, const char* name);
static const char* names[] = { "__i386__", "__LP64__", "__x86_64__", "__aarch64__", "__riscv", 0 };
int main(void){
    const char* targets[] = { "x86", "x86_64", "aarch64", "riscv64", 0 };
    for(int i = 0; targets[i]; i++){
        const struct target* t = target_for_name(targets[i]);
        target_set_current(t);
        struct compile_process* p = compile_process_create("$tmp/src.c", NULL, 0, NULL);
        printf("%s long=%zu ptr=%zu slot=%zu regs=%d", t->name, t->long_size, t->pointer_size, t->stack_slot_size, t->max_reg_args);
        for(int n = 0; names[n]; n++){
            if(preprocessor_get_definition(p->preprocessor, names[n])){
                printf(" %s", names[n]);
            }
        }
        printf("\n");
    }
    return 0;
}
EOF2
gcc -I"$REPO_ROOT" "$tmp/probe.c" $LINK_OBJS -o "$tmp/probe" 2>/dev/null || fail "probe failed to compile"
got="$("$tmp/probe")"
assert_contains "$got" "x86 long=4 ptr=4 slot=4 regs=0"$'\n' "x86 descriptor and macros"
assert_contains "$got" "x86_64 long=8 ptr=8 slot=8 regs=6 __LP64__ __x86_64__"$'\n' "x86_64 descriptor and macros"
assert_contains "$got" "aarch64 long=8 ptr=8 slot=16 regs=8 __LP64__ __aarch64__"$'\n' "aarch64 descriptor and macros"
assert_contains "$got" "riscv64 long=8 ptr=8 slot=16 regs=8 __LP64__ __riscv" "riscv64 descriptor and macros"
pass
