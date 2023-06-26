#!/usr/bin/env bash
# put SamCompiler's output next to gcc -O0 and gcc -O2 for the
# same target, one sample at a time.
#
# Usage:
#   bash tools/compare_gcc.sh ARCH=x86_64              # every sample
#   bash tools/compare_gcc.sh ARCH=aarch64 09_factorial_recursion
#   OUT=/tmp/cmp bash tools/compare_gcc.sh ARCH=riscv64
#
# For each sample it writes, under $OUT/<arch>/ (default
# /tmp/sam_compare):
#   <sample>.sam.s      SamCompiler (NASM for x86/x86_64, GNU as otherwise)
#   <sample>.O0.s       gcc -O0 -S
#   <sample>.O2.s       gcc -O2 -S
#   <sample>.side.txt   the three in columns, in that order
# and prints a table of instruction counts. x86 and x86_64 gcc output
# uses -masm=intel so it reads like the NASM side.

set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

arch=""
only=""
for arg in "$@"; do
    case "$arg" in
        ARCH=*) arch="${arg#ARCH=}" ;;
        *)      only="$arg" ;;
    esac
done

case "$arch" in
    x86)     cc="i686-linux-gnu-gcc";    flags="-masm=intel" ;;
    x86_64)  cc="gcc";                   flags="-masm=intel" ;;
    aarch64) cc="aarch64-linux-gnu-gcc"; flags="" ;;
    riscv64) cc="riscv64-linux-gnu-gcc"; flags="" ;;
    *) echo "usage: bash tools/compare_gcc.sh ARCH=x86|x86_64|aarch64|riscv64 [sample]" >&2; exit 2 ;;
esac
command -v "$cc" >/dev/null || { echo "$cc not found; see docs/guides/targets/$arch.md" >&2; exit 1; }

[ -x ./main ] || ./build.sh >/dev/null 2>&1 || { echo "build.sh failed" >&2; exit 1; }

out="${OUT:-/tmp/sam_compare}/$arch"
mkdir -p "$out"

# Instruction lines only: no blank lines, labels, directives,
# comments (`;` NASM, `//` aarch64, `#` riscv) or NASM keywords.
count_ins(){
    grep -vE '^\s*$|^\s*[.;#/]|^\s*[A-Za-z_.$][A-Za-z0-9_.$]*:\s*$|^\s*(section|global|extern|default)\b' "$1" | wc -l
}

# gcc -S for one sample. pc_includes goes after the system headers so
# gcc keeps its own stdarg.h (SamCompiler's needs the compiler's
# natives); if that fails (sample 07's <values.h> is also a glibc
# header) retry with pc_includes first.
gcc_s(){
    local src=$1 opt=$2 dst=$3
    $cc "$opt" -S $flags -fno-asynchronous-unwind-tables -idirafter pc_includes "$src" -o "$dst" 2>/dev/null && return
    $cc "$opt" -S $flags -fno-asynchronous-unwind-tables -I pc_includes "$src" -o "$dst" 2>/dev/null && return
    echo "; gcc could not compile $src" > "$dst"
}

printf "%-32s %8s %8s %8s\n" "sample ($arch)" "sam" "gcc -O0" "gcc -O2"
for src in samples/*.c; do
    base=$(basename "$src" .c)
    [ -z "$only" ] || [ "$only" = "$base" ] || continue
    ./main -target="$arch" "$src" "$out/$base.sam.s" object >/dev/null 2>&1 || true
    rm -f "$out/$base.sam.s.o"
    gcc_s "$src" -O0 "$out/$base.O0.s"
    gcc_s "$src" -O2 "$out/$base.O2.s"
    # gcc indents with tabs; expand them so the columns line up.
    pr -m -t -w 180 <(expand -t 4 "$out/$base.sam.s") <(expand -t 4 "$out/$base.O0.s") <(expand -t 4 "$out/$base.O2.s") > "$out/$base.side.txt"
    printf "%-32s %8s %8s %8s\n" "$base" "$(count_ins "$out/$base.sam.s")" "$(count_ins "$out/$base.O0.s")" "$(count_ins "$out/$base.O2.s")"
done
echo
echo "files in $out"
