#!/usr/bin/env bash
# Ch229: implementing includes - part 1. Lays down the include
# infrastructure: compiler.h gains compile_include / include_dir
# helpers + file_exists; cprocess.c adds default include dirs
# (./pc_includes, ../pc_includes, /usr/include/peach-includes,
# /usr/include) and the begin/next iterators; helper.c gets
# file_exists; compiler.c gets compile_include + _for_include_dir
# that lex + preprocess an include's file but don't parse it.
#
# The #include directive itself isn't wired yet (part 2). This
# test just confirms the include-dir vector is populated and
# file_exists / compile_include link.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch229_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch229_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    int n = vector_count(cp->include_dirs);
    int has_existing = file_exists("/dev/null");
    int has_bogus    = file_exists("/this/path/does/not/exist/at/all");
    const char* first = compiler_include_dir_begin(cp);
    int has_first = first != NULL;
    printf("n=%d existing=%d bogus=%d first=%d\n", n, has_existing, has_bogus, has_first);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch229 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n=4 existing=1 bogus=0 first=1" \
    "compile_process_create populates 4 default include dirs and file_exists works"
pass
