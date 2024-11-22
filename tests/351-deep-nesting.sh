#!/usr/bin/env bash
# E7: 50,000 nested parentheses overflowed the stack and the compiler
# died with SIGSEGV. Deep nesting of any kind (parentheses, blocks,
# unary operators, one long expression) now stops with an error that
# says what happened, on every target, and the limit follows the stack
# limit: what fails at the default 8 MB compiles with a larger one.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_nest.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

gen() {
    python3 - "$1" "$2" "$tmp/$1$2.c" <<'EOF'
import sys
kind, n, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
with open(out, "w") as f:
    if kind == "paren":
        f.write("int main(){ return " + "(" * n + "1" + ")" * n + "; }\n")
    elif kind == "blocks":
        f.write("int main(){ int x = 0; " + "{" * n + " x = 1; " + "}" * n + " return x; }\n")
    elif kind == "unary":
        f.write("int main(){ int a = 1; return " + "- " * n + "a; }\n")
    elif kind == "sum":
        f.write("int main(){ int a = 1; return (" + " + ".join(["a"] * n) + ") & 255; }\n")
EOF
}

compile() {
    # 8 MB: the usual default, and what the numbers in the note assume.
    (ulimit -s 8192; ./main -target="$1" "$2" "$tmp/out.s" object) >/dev/null 2>"$tmp/err.txt"
}

for kind in paren blocks unary sum; do
    gen "$kind" 100000
    for t in x86 x86_64 aarch64 riscv64; do
        rc=0
        compile "$t" "$tmp/${kind}100000.c" || rc=$?
        [ "$rc" -ne 139 ] || fail "$kind on $t: segfault"
        [ "$rc" -ne 0 ] || fail "$kind on $t: 100,000 levels compiled"
        assert_contains "$(cat "$tmp/err.txt")" "nested too deeply" "$kind on $t message"
    done
done

# Ordinary depths still compile: far past C's minimum of 63 levels.
gen paren 1000
gen sum 4000
for t in x86 x86_64 aarch64 riscv64; do
    compile "$t" "$tmp/paren1000.c" || fail "1,000 parentheses on $t: $(head -2 "$tmp/err.txt")"
    compile "$t" "$tmp/sum4000.c" || fail "4,000-term sum on $t: $(head -2 "$tmp/err.txt")"
done

# The limit is the stack limit, not a fixed depth.
gen paren 40000
compile x86 "$tmp/paren40000.c" && fail "40,000 parentheses compiled with an 8 MB stack"
(ulimit -s 65536; ./main "$tmp/paren40000.c" "$tmp/out.s" object) >/dev/null 2>"$tmp/err.txt" \
    || fail "40,000 parentheses with a 64 MB stack: $(head -2 "$tmp/err.txt")"
pass
