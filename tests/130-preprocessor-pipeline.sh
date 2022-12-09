#!/usr/bin/env bash
# Ch200: beginning the preprocessor logic. compile_process_create
# gains a parent_process param; compile_file now stashes lex output
# in token_vec_original and runs preprocessor_run, which copies
# tokens into token_vec via the default switch case (deviation from
# upstream verbatim, see docs/200).
#
# Adds compile_process->token_vec_original, ->include_dirs,
# ->preprocessor; struct preprocessor / preprocessor_definition /
# preprocessor_included_file; helpers preprocessor_create,
# preprocessor_initialize, preprocessor_add_included_file,
# preprocessor_build_value_vector_for_integer, etc.
#
# Test: confirm a tiny .c file still compiles end-to-end through
# the new pipeline (lex -> preprocessor_run -> parse -> codegen),
# and that preprocessor_create returns a non-null preprocessor
# with empty definitions + includes vectors.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

# End-to-end: main still compiles a trivial source.
src=$(mktemp /tmp/sam_ch200_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch200_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
int main(){
    return 0;
}
EOF

# Run in "object" mode so nasm runs but gcc link step is skipped.
# We just want to confirm the new lex -> preprocessor -> parse ->
# codegen pipeline still produces output - skip cleanly if nasm /
# 32-bit gcc are not installed.
if command -v nasm >/dev/null; then
    "$REPO_ROOT/main" "$src" "$asm" object >/dev/null 2>&1 || true
fi
out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "main did not reach codegen on a trivial source after preprocessor wiring: $out" ;;
esac

# Probe the preprocessor struct shape.
probe=$(mktemp /tmp/sam_ch200_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch200_bin.XXXXXX)
trap 'rm -f "$src" "$asm" "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int main(void){
    struct compile_process* p = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!p){ printf("create=null\n"); return 0; }
    int has_pp = p->preprocessor != NULL;
    int has_dirs = p->include_dirs != NULL;
    int has_orig = p->token_vec_original != NULL;
    int has_tok  = p->token_vec != NULL;
    int defs_empty = p->preprocessor && vector_count(p->preprocessor->definitions) == 0;
    int inc_empty  = p->preprocessor && vector_count(p->preprocessor->includes) == 0;
    printf("pp=%d dirs=%d orig=%d tok=%d defs0=%d inc0=%d\n",
        has_pp, has_dirs, has_orig, has_tok, defs_empty, inc_empty);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch200 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "pp=1 dirs=1 orig=1 tok=1 defs0=1 inc0=1" \
    "compile_process_create wires the preprocessor + include_dirs + token vectors"
pass
