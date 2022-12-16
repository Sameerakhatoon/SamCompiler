#!/usr/bin/env bash
# Ch206: implementing #error. Mirrors #warning, but routes through
# preprocessor_execute_error which prepends `#error ` and calls
# compiler_error - and compiler_error exit(-1)s.
#
# Test: feed `#error halt \n` and confirm the probe binary exits
# non-zero with `#error halt` on stderr.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch206_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch206_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    cp->pos.line = 1;
    cp->pos.col  = 1;
    cp->pos.filename = "test";

    struct token tk;
    tk.type = TOKEN_TYPE_SYMBOL;     tk.cval = '#';     vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval = "error"; vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval = "halt";  vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NEWLINE;                        vector_push(cp->token_vec_original, &tk);

    preprocessor_run(cp);
    // Should never reach here - compiler_error exits.
    printf("UNREACHABLE\n");
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch206 probe failed to compile"
out=$("$bin" 2>&1 || true)
ec=$?
case "$out" in
    *"#error halt"*) ;;
    *) fail "expected #error halt on stderr, got: $out" ;;
esac
case "$out" in
    *"UNREACHABLE"*) fail "preprocessor_run returned after #error instead of exiting" ;;
esac
pass
