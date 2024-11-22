#!/usr/bin/env bash
# Compile time and memory grow about linearly with the size of the
# program. 20,000 functions took 138 s and 2.4 GB (linear symbol and
# scope lookups, vectors grown 20 elements at a time) and one
# 4,000-term expression 24 s and 1.7 GB (parser_reorder_expression);
# The limits here are loose enough for
# a loaded machine and far below the old cost.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_scale.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

python3 - "$tmp" <<'EOF'
import sys
tmp = sys.argv[1]
with open(tmp + "/funcs.c", "w") as f:
    f.write("int g0;\nint f0(int x){ return x + g0; }\n")
    for i in range(1, 20000):
        f.write("int g%d;\nint f%d(int x){ int l = g%d; return f%d(x) + l; }\n" % (i, i, i, i - 1))
    f.write("int main(){ return f19999(1) & 255; }\n")
with open(tmp + "/sum.c", "w") as f:
    f.write("int main(){ int a = 1; int b = 2; return (" + " + ".join(["a", "b"] * 4000) + ") & 255; }\n")
EOF

# 1.5 GB of address space: the old compiler needed more than that for
# either file.
for t in x86 x86_64; do
    rc=0
    (ulimit -v 1500000; timeout 120 ./main -target=$t "$tmp/funcs.c" "$tmp/out.s" object) >/dev/null 2>"$tmp/err.txt" || rc=$?
    assert_eq 0 "$rc" "20,000 functions on $t ($(head -c 200 "$tmp/err.txt"))"
    rc=0
    (ulimit -v 1500000; timeout 60 ./main -target=$t "$tmp/sum.c" "$tmp/out.s" object) >/dev/null 2>"$tmp/err.txt" || rc=$?
    assert_eq 0 "$rc" "8,000-term sum on $t ($(head -c 200 "$tmp/err.txt"))"
done
pass
