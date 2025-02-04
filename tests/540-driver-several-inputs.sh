#!/usr/bin/env bash
# several inputs per run on all four
# targets. Three sources linked into one program; .c mixed with .s
# (the target's own assembly) and .o; -c and -S with several sources
# (and -o refused there); a .s input assembled by -c and left alone by
# -S; every source's errors reported when one fails, with nothing
# linked and no object written for the bad file; a duplicate symbol
# reported by the linker; and no temporaries left behind.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_inputs.XXXXXX)
export TMPDIR="$tmp/tmpdir"
mkdir -p "$TMPDIR"
trap 'rm -rf "$tmp"' EXIT
M="$REPO_ROOT/main"
cd "$tmp"

cat > one.c <<'EOF'
int twice(int x){ return 2 * x; }
EOF
cat > two.c <<'EOF'
int twice(int x);
int three(void);
int main(void){ return twice(three()) + 1; }
EOF
cat > three.c <<'EOF'
int three(void){ return 3; }
EOF
cat > bad1.c <<'EOF'
int bad1(void){ return no_such_one; }
EOF
cat > bad2.c <<'EOF'
int bad2(void){ return no_such_two; }
EOF
cat > dup.c <<'EOF'
int twice(int x){ return x; }
EOF

run_on(){
    local run="$1"; shift
    local rc=0
    $run "$@" || rc=$?
    echo "$rc"
}

for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86|x86_64) run="";            tools="nasm" ;;
        aarch64)    run="qemu-aarch64"; tools="aarch64-linux-gnu-as qemu-aarch64" ;;
        riscv64)    run="qemu-riscv64"; tools="riscv64-linux-gnu-as qemu-riscv64" ;;
    esac
    have_tools $tools || continue
    mkdir -p "$target" && cd "$target"

    # Three sources, one program.
    "$M" -target=$target -o p3 ../one.c ../two.c ../three.c 2>log || fail "$target: three sources: $(tail -3 log)"
    assert_eq 7 "$(run_on "$run" ./p3)" "$target: three sources"

    # -S and -c with several sources: one output each, in this directory.
    "$M" -target=$target -S ../one.c ../three.c 2>log || fail "$target: -S of two sources: $(tail -3 log)"
    [ -f one.s ] && [ -f three.s ] || fail "$target: -S wrote $(ls)"
    "$M" -target=$target -c ../one.c ../three.c 2>log || fail "$target: -c of two sources: $(tail -3 log)"
    [ -f one.o ] && [ -f three.o ] || fail "$target: -c wrote $(ls)"
    rc=0; err=$("$M" -target=$target -c ../one.c ../three.c -o both.o 2>&1) || rc=$?
    assert_eq 1 "$rc" "$target: -c -o with two sources"
    assert_contains "$err" "cannot specify -o" "$target: -c -o with two sources"

    # .c with .s and .o: a source, the asm of one, the object of another.
    "$M" -target=$target -o pmix one.s ../two.c three.o 2>log || fail "$target: .s + .c + .o: $(tail -3 log)"
    assert_eq 7 "$(run_on "$run" ./pmix)" "$target: .s + .c + .o"
    # -c assembles a .s input to NAME.o; -S leaves it alone with a warning.
    rm -f one.o
    "$M" -target=$target -c one.s 2>log || fail "$target: -c one.s: $(tail -3 log)"
    [ -f one.o ] || fail "$target: -c one.s wrote no one.o"
    "$M" -target=$target -o pobj one.o ../two.c three.o 2>log || fail "$target: objects from .s: $(tail -3 log)"
    assert_eq 7 "$(run_on "$run" ./pobj)" "$target: object assembled from .s"
    err=$("$M" -target=$target -S one.s 2>&1) || fail "$target: -S one.s failed"
    assert_contains "$err" "assembler input file unused" "$target: -S of a .s file"

    # Errors in the second and third files: both reported, nothing linked.
    rc=0; err=$("$M" -target=$target -o pbad ../one.c ../bad1.c ../bad2.c ../three.c 2>&1) || rc=$?
    assert_eq 1 "$rc" "$target: link with two bad sources"
    assert_contains "$err" "no_such_one" "$target: the first bad source's error"
    assert_contains "$err" "no_such_two" "$target: the second bad source's error"
    [ ! -e pbad ] || fail "$target: a program was linked from bad sources"
    # The same with -c: the good files get objects, the bad ones none.
    rm -f one.o three.o
    rc=0; err=$("$M" -target=$target -c ../one.c ../bad1.c ../three.c ../bad2.c 2>&1) || rc=$?
    assert_eq 1 "$rc" "$target: -c with two bad sources"
    assert_contains "$err" "no_such_two" "$target: -c reports the last bad source"
    [ -f one.o ] && [ -f three.o ] || fail "$target: -c stopped at the first bad source: $(ls)"
    [ ! -e bad1.o ] && [ ! -e bad2.o ] || fail "$target: -c wrote an object for a bad source"

    # A symbol defined twice is the linker's error, and the driver's status.
    rc=0; err=$("$M" -target=$target -o pdup ../one.c ../dup.c ../two.c ../three.c 2>&1) || rc=$?
    [ "$rc" != 0 ] || fail "$target: a duplicate definition linked"
    assert_contains "$err" "multiple definition of \`twice'" "$target: duplicate definition"
    [ ! -e pdup ] || fail "$target: pdup written despite the link error"
    cd ..
done

# The original form is unchanged: in `main in.c out.s [object]` a
# second name ending in .s is the output, not an input.
"$M" -target=x86_64 one.c legacy.s object >/dev/null 2>&1 || fail "the original form with out.s object"
[ -f legacy.s ] && [ -f legacy.s.o ] || fail "the original form wrote $(ls)"
grep -q 'twice' legacy.s || fail "the original form did not write the asm to legacy.s"
rm -f legacy.s legacy.s.o
cat > m.c <<'EOF'
int main(void){ return 5; }
EOF
"$M" -target=x86_64 m.c legacy.s >/dev/null 2>&1 || fail "the original form with out.s"
assert_eq 5 "$(run_on "" ./legacy.s)" "the original form linked over out.s"
rm -f legacy.s legacy.s.o

[ -z "$(ls "$TMPDIR")" ] || fail "temporaries left in TMPDIR: $(ls "$TMPDIR")"
pass
