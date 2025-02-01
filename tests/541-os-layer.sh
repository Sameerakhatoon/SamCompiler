#!/usr/bin/env bash
# the driver without fork. With SAMCC_NO_FORK set,
# os_run_forked declines and the driver runs itself again for each
# input (`--samcc-compile-child`), as it does on Windows. Here: the
# options reach the child (-D with a space and a quote in the value, -I
# with a space and a quote in the directory, -target=), standard input
# is inherited, an error in one input is still that input's alone, and
# the asm is the same as with fork. Then the driver tests 310 and 313
# run on that path.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_oslayer.XXXXXX)
export TMPDIR="$tmp/tmpdir"
mkdir -p "$TMPDIR"
trap 'rm -rf "$tmp"' EXIT
M="$REPO_ROOT/main"
cd "$tmp"

mkdir -p "inc dir'q"
cat > "inc dir'q/val.h" <<'EOF'
#define VAL 40
EOF
cat > prog.c <<'EOF'
#include "val.h"
int printf(const char* fmt, ...);
int main(void){
    printf("%s\n", MSG);
    return VAL + EXTRA;
}
EOF
cat > bad.c <<'EOF'
int f(void){ return missing_name; }
EOF

for target in x86_64 aarch64; do
    "$M" -target=$target -S prog.c -o fork.s "-Iinc dir'q" "-DMSG=\"it's here\"" -DEXTRA=2 2>log \
        || fail "$target: fork path: $(tail -2 log)"
    SAMCC_NO_FORK=1 "$M" -target=$target -S prog.c -o spawn.s "-Iinc dir'q" "-DMSG=\"it's here\"" -DEXTRA=2 2>log \
        || fail "$target: spawn path: $(tail -2 log)"
    cmp -s fork.s spawn.s || fail "$target: the spawned compile gives other asm"
    SAMCC_NO_FORK=1 "$M" -target=$target -S - -o stdin.s "-Iinc dir'q" "-DMSG=\"it's here\"" -DEXTRA=2 <prog.c 2>log \
        || fail "$target: standard input on the spawn path: $(tail -2 log)"
    cmp -s fork.s stdin.s || fail "$target: standard input did not reach the child"
done

if have_tools nasm; then
    out=$(SAMCC_NO_FORK=1 "$M" -target=x86_64 -o prog prog.c "-Iinc dir'q" "-DMSG=\"it's here\"" -DEXTRA=2 2>&1) \
        || fail "spawn path link: $out"
    rc=0; text=$(./prog) || rc=$?
    assert_eq 42 "$rc" "program built on the spawn path"
    assert_eq "it's here" "$text" "-D through the spawn path"
fi

rc=0; err=$(SAMCC_NO_FORK=1 "$M" -target=x86_64 -S prog.c bad.c -DMSG='"m"' -DEXTRA=0 "-Iinc dir'q" 2>&1) || rc=$?
assert_eq 1 "$rc" "a bad input on the spawn path"
assert_contains "$err" "missing_name" "the bad input's error"
[ -f prog.s ] || fail "the good input got no asm on the spawn path"
[ -z "$(ls "$TMPDIR")" ] || fail "temporaries left in TMPDIR: $(ls "$TMPDIR")"

cd "$REPO_ROOT"
for t in 310 313; do
    SAMCC_NO_FORK=1 bash tests/$t-*.sh >"$tmp/log_$t" 2>&1 \
        || fail "test $t without fork: $(tail -2 "$tmp/log_$t")"
done
pass
