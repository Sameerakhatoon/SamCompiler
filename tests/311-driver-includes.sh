#!/usr/bin/env bash
# -I, -D and -U; pc_includes found next to
# the compiler binary (also through a symlink) wherever it runs; quoted
# includes searched next to the including file first, angled ones not;
# the current directory not searched.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_includes.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
M="$REPO_ROOT/main"

# From another directory: sample 07 includes <values.h> from pc_includes.
mkdir -p "$tmp/elsewhere"
(cd "$tmp/elsewhere" && "$M" -target=x86_64 "$REPO_ROOT/samples/07_include_user_header.c" -o s07 2>/dev/null) \
    || fail "sample 07 did not compile outside the repository"
rc=0; "$tmp/elsewhere/s07" || rc=$?
assert_eq 4 "$rc" "sample 07 exit status"

# Through a symlink to the binary, from yet another directory.
ln -s "$M" "$tmp/samcc"
(cd / && "$tmp/samcc" -target=x86_64 -E "$REPO_ROOT/samples/07_include_user_header.c" > "$tmp/s07.i") \
    || fail "the symlinked compiler did not find pc_includes"

# -I DIR and -IDIR, in order, before pc_includes.
mkdir -p "$tmp/inc1" "$tmp/inc2" "$tmp/src/lib"
echo '#define FIRST 10' > "$tmp/inc1/first.h"
echo '#define SECOND 20' > "$tmp/inc2/second.h"
echo '#define SECOND 99' > "$tmp/inc1/second.h.unused"
cat > "$tmp/src/main.c" <<'EOF'
#include <first.h>
#include <second.h>
#include "lib/local.h"
int main(){ return FIRST + SECOND + LOCAL + EXTRA - GONE; }
EOF
# A quoted include inside lib/ finds its neighbour in lib/.
echo '#include "near.h"' > "$tmp/src/lib/local.h"
echo '#define LOCAL 5' > "$tmp/src/lib/near.h"
(cd / && "$M" -target=x86_64 -I "$tmp/inc1" -I"$tmp/inc2" -DEXTRA=7 -DGONE -UGONE -DGONE=0 \
    "$tmp/src/main.c" -o "$tmp/m" 2>/dev/null) || fail "the -I / -D build failed"
rc=0; "$tmp/m" || rc=$?
assert_eq 42 "$rc" "-I/-D program exit status"

# -D NAME defines it as 1; -U removes it again; -U removes a predefined macro.
cat > "$tmp/d.c" <<'EOF'
#ifdef FLAG
int flag = FLAG;
#endif
#ifndef __x86_64__
int no_x86_64;
#endif
EOF
out=$("$M" -target=x86_64 -E -D FLAG "$tmp/d.c")
assert_contains "$out" "int flag = 1;" "-D FLAG"
out=$("$M" -target=x86_64 -E -DFLAG -UFLAG -U__x86_64__ "$tmp/d.c")
case "$out" in *flag*) fail "-U FLAG did not undefine it: $out";; esac
assert_contains "$out" "int no_x86_64;" "-U of a predefined macro"
# -D with a function-like macro.
echo 'int x = SQ(6);' > "$tmp/f.c"
out=$("$M" -E '-DSQ(a)=((a)*(a))' "$tmp/f.c")
assert_contains "$out" "6" "-D of a function-like macro"
case "$out" in *SQ*) fail "SQ was not expanded: $out";; esac

# An angled include does not look next to the including file: a local
# stdbool.h must not hide pc_includes' one, while "stdbool.h" finds it.
mkdir -p "$tmp/shadow"
echo '#define SHADOWED 1' > "$tmp/shadow/stdbool.h"
printf '#include <stdbool.h>\nbool b = true;\n#ifdef SHADOWED\nint shadowed;\n#endif\n' > "$tmp/shadow/angled.c"
out=$("$M" -E "$tmp/shadow/angled.c")
case "$out" in *shadowed*) fail "<stdbool.h> found the file next to the source: $out";; esac
printf '#include "stdbool.h"\n#ifdef SHADOWED\nint shadowed;\n#endif\n' > "$tmp/shadow/quoted.c"
out=$("$M" -E "$tmp/shadow/quoted.c")
assert_contains "$out" "int shadowed;" "quoted include next to the source"

# The current directory is not an include directory.
mkdir -p "$tmp/cwd"
echo '#define FROM_CWD 1' > "$tmp/cwd/cwdonly.h"
printf '#include <cwdonly.h>\nint x;\n' > "$tmp/src/cwd.c"
rc=0; (cd "$tmp/cwd" && "$M" -E "$tmp/src/cwd.c" >/dev/null 2>&1) || rc=$?
[ "$rc" != 0 ] || fail "an include was found in the current directory"
pass
