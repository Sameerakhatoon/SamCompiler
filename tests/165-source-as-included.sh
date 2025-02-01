#!/usr/bin/env bash
# Ch235: adding our source file as an included file. compile_process
# _create now resolves the source filename to absolute via realpath
# and stores it on cfile.abs_path, plus calls node_set_vector to
# bind the global node module to this process's vectors. preprocessor
# _run now starts by adding the source as an included file via
# preprocessor_add_included_file, replacing the prior `#warning "add
# our source file as an included file"`.
#
# Test: create a real source file, run a probe that builds a
# compile_process + runs lex + preprocessor_run, then confirms the
# preprocessor->includes vector contains exactly one entry whose
# filename is the absolute path to the source.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch235_src.XXXXXX.c)
probe=$(mktemp /tmp/sam_ch235_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch235_bin.XXXXXX)
trap 'rm -f "$src" "$probe" "$bin"' EXIT

cat > "$src" <<'EOF'
int main() {}
EOF

cat > "$probe" <<EOF
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

extern struct lex_process_functions compiler_lex_functions;
int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("$src", NULL, 0, NULL);
    if (!cp) return 1;
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    if (lex(lp) != LEXICAL_ANALYSIS_ALL_OK) return 1;
    cp->token_vec_original = lex_process_tokens(lp);
    preprocessor_run(cp);

    int n = vector_count(cp->preprocessor->includes);
    int abs_match = 0;
    if (n > 0){
        vector_set_peek_pointer(cp->preprocessor->includes, 0);
        struct preprocessor_included_file* f = vector_peek_ptr(cp->preprocessor->includes);
        if (f && strstr(f->filename, "sam_ch235_src")) abs_match = 1;
    }
    printf("includes=%d abs_match=%d\n", n, abs_match);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch235 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "includes=1 abs_match=1" \
    "preprocessor_run registers the source file as an included file with an absolute path"
pass
