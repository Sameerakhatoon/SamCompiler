#!/usr/bin/env bash
# B1: struct arguments by value and struct results against gcc on the
# three 64-bit targets. tests/abi64/gcc.c (built by gcc, has main) and
# tests/abi64/sam.c (built by SamCompiler) call each other both ways
# with structs of 4 to 40 bytes, by value and returned, next to
# scalars that use up the argument registers; the exit status is a bit
# mask of the failed checks (see gcc.c). tests/diff only shows that
# SamCompiler agrees with itself. tests/abi64/self.c holds the struct
# calls the x86 backend cannot build yet (so tests/diff cannot), run
# against gcc's exit status.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_abi64.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for target in x86_64 aarch64 riscv64; do
    case $target in
        x86_64)  cc="gcc -no-pie";                   run="" ;;
        aarch64) cc="aarch64-linux-gnu-gcc -static"; run="qemu-aarch64" ;;
        riscv64) cc="riscv64-linux-gnu-gcc -static"; run="qemu-riscv64" ;;
    esac
    command -v "${cc%% *}" >/dev/null || fail "$target: ${cc%% *} is missing"
    [ -z "$run" ] || command -v "$run" >/dev/null || fail "$target: $run is missing"

    # object mode: the asm goes to the named file, the object next to it.
    ./main -target=$target tests/abi64/sam.c "$tmp/sam_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: sam.c did not compile: $(tail -2 "$tmp/log")"
    $cc -c tests/abi64/gcc.c -o "$tmp/gcc_$target.o" || fail "$target: gcc rejected gcc.c"
    $cc "$tmp/gcc_$target.o" "$tmp/sam_$target.s.o" -o "$tmp/interop_$target" 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    rc=0
    $run "$tmp/interop_$target" || rc=$?
    assert_eq 0 "$rc" "$target: failed check bits of gcc.c <-> sam.c"

    $cc -w tests/abi64/self.c -o "$tmp/self_gcc_$target" || fail "$target: gcc rejected self.c"
    want=0
    $run "$tmp/self_gcc_$target" || want=$?
    ./main -target=$target tests/abi64/self.c "$tmp/self_$target" >"$tmp/log" 2>&1 \
        || fail "$target: self.c did not build: $(tail -2 "$tmp/log")"
    got=0
    $run "$tmp/self_$target" || got=$?
    assert_eq "$want" "$got" "$target: self.c exit status against gcc's"
done

pass
