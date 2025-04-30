#!/usr/bin/env bash
# -O1. An explicit -O0 gives the golden output
# byte for byte on all four targets (the default is checked by 176 and
# 362-364), -O1 shortens `int add(int a, int b)`, and the gcc
# differential passes with SAMCC_FLAGS=-O1 on all four targets, which
# includes tests/diff/opt_fold.c and opt_dead.c for stage 1's rules.
. "$(dirname "$0")/lib.sh"

need_tools nasm gcc i686-linux-gnu-gcc aarch64-linux-gnu-gcc riscv64-linux-gnu-gcc qemu-aarch64 qemu-riscv64
tmp=$(mktemp -d /tmp/sam_opt.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for target in x86 x86_64 aarch64 riscv64; do
    for src in samples/*.c tests/golden/inputs/*.c; do
        base=$(basename "$src" .c)
        samcc -target="$target" -O0 -S "$src" -o "$tmp/$base.s" >"$tmp/log" 2>&1 || fail "-O0 $src ($target): $(head -2 "$tmp/log")"
        cmp -s "$tmp/$base.s" "tests/golden/$target/$base.asm" || fail "-O0 output for $src ($target) differs from the golden file"
    done
done

# Instructions of a function: lines between its label and the next
# function, without labels, directives and comments.
count_add(){
    awk '/^\$?add:$/{on=1; next} on && /(;|\/\/|#) [a-z_]+ function$/{on=0} on && !/^[.$A-Za-z_0-9]*:$/ && !/^[.;#\/]/ && !/^(global|section)/{n++} END{print n+0}' "$1"
}
printf 'int add(int a, int b){ return a + b; }\n' > "$tmp/add.c"
for target in x86 x86_64 aarch64 riscv64; do
    samcc -target="$target" -S "$tmp/add.c" -o "$tmp/add0.s" >/dev/null 2>&1 || fail "add.c -O0 ($target)"
    samcc -target="$target" -O1 -S "$tmp/add.c" -o "$tmp/add1.s" >/dev/null 2>&1 || fail "add.c -O1 ($target)"
    o0=$(count_add "$tmp/add0.s")
    o1=$(count_add "$tmp/add1.s")
    [ "$o1" -lt "$o0" ] || fail "-O1 does not shorten add on $target ($o0 -> $o1)"
done

# Code a goto reaches stays, even in an if(0).
cat > "$tmp/goto.c" <<'EOF'
int f(int n){ int r = 0; if(n > 100){ goto inside; } if(0){ inside: r += 50; } return r; }
EOF
for target in aarch64 riscv64; do
    samcc -target="$target" -O1 -S "$tmp/goto.c" -o "$tmp/goto.s" >/dev/null 2>&1 || fail "goto.c -O1 ($target)"
    # The label is .Llabel_f_inside on both.
    grep -q 'label_f_inside:$' "$tmp/goto.s" || fail "-O1 dropped the goto label ($target)"
    grep -q '#\?50$' "$tmp/goto.s" || fail "-O1 dropped r += 50 after a goto label ($target)"
done

# -O on its own and the later options win.
samcc -target=x86_64 -O -O0 -S "$tmp/add.c" -o "$tmp/a.s" >/dev/null 2>&1 || fail "-O -O0"
samcc -target=x86_64 -S "$tmp/add.c" -o "$tmp/b.s" >/dev/null 2>&1 || fail "default"
cmp -s "$tmp/a.s" "$tmp/b.s" || fail "-O -O0 is not -O0"

SAMCC_FLAGS=-O1 bash tools/gcc_differential.sh tests/diff > "$tmp/diff" 2>&1 || {
    grep -E '\*|mismatches' "$tmp/diff" | head -20 >&2
    fail "gcc differential at -O1"
}
pass
