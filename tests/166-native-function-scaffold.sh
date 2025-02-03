#!/usr/bin/env bash
# Ch236: implementing native functions - part 1. Lands the
# infrastructure that lets the preprocessor register C-implemented
# functions and the codegen call them through a v-table.
#
# compiler.h: struct generator (asm_push / gen_exp / end_exp /
# entity_address fn-ptrs + compiler + private), struct
# generator_entity_address, struct native_function +
# native_function_callbacks, native_create_function decl,
# symresolver_register_symbol decl.
#
# codegen.c: x86_codegen global v-table instance, codegen_asm_push
# shim (since asm_push is static), codegen_gen_exp / end_exp /
# entity_address, _x86_generator_private; codegen() now stamps
# x86_codegen.compiler = current_process.
#
# native.c: native_create_function callocs + registers via
# symresolver_register_symbol with SYMBOL_TYPE_NATIVE_FUNCTION.
#
# static-includes/stdarg.c: native_test_function emits a tagged
# asm comment; preprocessor_stdarg_internal_include registers it
# as "test".
#
# Test: link the global x86_codegen and confirm its v-table slots
# are wired (non-null function pointers).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch236_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch236_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"

extern struct generator x86_codegen;

int main(void){
    int has_asm_push    = x86_codegen.asm_push       != NULL;
    int has_gen_exp     = x86_codegen.gen_exp        != NULL;
    int has_end_exp     = x86_codegen.end_exp        != NULL;
    int has_ent_addr    = x86_codegen.entity_address != NULL;
    int has_private     = x86_codegen.private        != NULL;
    printf("asm_push=%d gen_exp=%d end_exp=%d ent=%d private=%d\n",
        has_asm_push, has_gen_exp, has_end_exp, has_ent_addr, has_private);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch236 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "asm_push=1 gen_exp=1 end_exp=1 ent=1 private=1" \
    "x86_codegen v-table has all slots populated"
pass
