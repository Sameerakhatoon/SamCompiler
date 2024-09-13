#!/usr/bin/env bash
# Regenerate the golden asm files for one target from the current
# compiler. Only run this when an output change is intended; the
# golden tests exist to catch the unintended ones.
#
# Usage: bash tests/golden/regen.sh [x86|x86_64|aarch64|riscv64]
set -euo pipefail

cd "$(dirname "$0")/../.."

target="${1:-x86}"
case "$target" in
    x86) target_flag="" ;;
    x86_64|aarch64|riscv64) target_flag="-target=$target" ;;
    *) echo "regen: unknown target $target" >&2; exit 2 ;;
esac
out_dir="tests/golden/$target"
tmp=$(mktemp -d /tmp/sam_golden_regen.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

./build.sh >/dev/null 2>&1
mkdir -p "$out_dir"

for src in samples/*.c tests/golden/inputs/*.c; do
    base=$(basename "$src" .c)
    # object mode stops after the assembler; we only want the .asm,
    # so a missing assembler is not an error here.
    log=$(./main $target_flag "$src" "$tmp/$base.asm" object 2>&1 || true)
    case "$log" in
        *"everything compiled fine"*) ;;
        *) echo "regen: $src did not compile:" >&2; echo "$log" | tail -3 >&2; exit 1 ;;
    esac
    cp "$tmp/$base.asm" "$out_dir/$base.asm"
done

echo "regenerated $(ls "$out_dir"/*.asm | wc -l) files in $out_dir"
