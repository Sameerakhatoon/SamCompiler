#!/usr/bin/env bash
# Shared objects: tests/shared/lib.c built with
# `main -shared` on all four targets, with a gcc-built program and a
# SamCompiler-built one linked against it (`main main.c lib.so`, a
# dynamic PIE), and the library built by gcc under a SamCompiler
# program. The program changes the library's exported data and
# interposes one of its functions, so the library must reach its
# exported symbols through the GOT. No text relocations, no linker
# warnings. Cross targets run under qemu with the cross sysroot, i386
# with the cross loader.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_shared.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

targets="x86_64"
have_tools i686-linux-gnu-gcc && targets="$targets x86"
have_tools aarch64-linux-gnu-gcc qemu-aarch64 && targets="$targets aarch64"
have_tools riscv64-linux-gnu-gcc qemu-riscv64 && targets="$targets riscv64"

for target in $targets; do
    dir="$tmp/$target"
    mkdir -p "$dir"
    case $target in
        x86)     cc="i686-linux-gnu-gcc"
                 run="/usr/i686-linux-gnu/lib/ld-linux.so.2 --library-path /usr/i686-linux-gnu/lib:$dir" ;;
        x86_64)  cc="gcc"; run="env LD_LIBRARY_PATH=$dir" ;;
        aarch64) cc="aarch64-linux-gnu-gcc"; run="qemu-aarch64 -L /usr/aarch64-linux-gnu -E LD_LIBRARY_PATH=$dir" ;;
        riscv64) cc="riscv64-linux-gnu-gcc"; run="qemu-riscv64 -L /usr/riscv64-linux-gnu -E LD_LIBRARY_PATH=$dir" ;;
    esac

    ./main -target=$target -shared tests/shared/lib.c -o "$dir/libsam.so" 2>"$dir/log" \
        || fail "$target: -shared failed: $(tail -3 "$dir/log")"
    [ ! -s "$dir/log" ] || fail "$target: -shared warned: $(cat "$dir/log")"
    file "$dir/libsam.so" | grep -q 'shared object' || fail "$target: not a shared object: $(file "$dir/libsam.so")"
    if readelf -d "$dir/libsam.so" | grep -q TEXTREL; then
        fail "$target: the library has text relocations"
    fi
    # Exported functions and data are dynamic symbols, static ones not.
    syms=$(readelf --dyn-syms -W "$dir/libsam.so")
    for s in counter table lib_get lib_hook; do
        echo "$syms" | awk '{ print $NF }' | grep -qx "$s" || fail "$target: $s is not exported"
    done
    if echo "$syms" | awk '{ print $NF }' | grep -qx 'helper\|hidden'; then
        fail "$target: a static symbol is exported"
    fi
    $cc -w -shared -fPIC tests/shared/lib.c -o "$dir/libgcc.so" || fail "$target: gcc rejected lib.c"

    # gcc's program with SamCompiler's library.
    $cc -w tests/shared/main.c "$dir/libsam.so" -o "$dir/gcc_main" || fail "$target: gcc link against libsam.so failed"
    # SamCompiler's program with SamCompiler's and with gcc's library.
    ./main -target=$target tests/shared/main.c "$dir/libsam.so" -o "$dir/sam_main" 2>"$dir/log" \
        || fail "$target: main.c with libsam.so failed: $(tail -3 "$dir/log")"
    [ ! -s "$dir/log" ] || fail "$target: the link warned: $(cat "$dir/log")"
    mkdir -p "$dir/g"
    cp "$dir/libgcc.so" "$dir/g/libsam.so"

    for prog in gcc_main sam_main; do
        rc=0
        out=$($run "$dir/$prog") || rc=$?
        assert_eq 0 "$rc" "$target: $prog with SamCompiler's library (bad bits)"
        assert_eq "lib: counter=105
bad=0" "$out" "$target: $prog output"
    done
    # The same SamCompiler program, run against gcc's build of the
    # library (same soname-less file name, another directory).
    rc=0
    out=$(${run/$dir/$dir/g} "$dir/sam_main") || rc=$?
    assert_eq 0 "$rc" "$target: sam_main with gcc's library (bad bits)"
    assert_eq "lib: counter=105
bad=0" "$out" "$target: sam_main with gcc's library"
done

# -fPIC objects linked into a shared object by hand, and the SamOS
# targets refuse -shared.
./main -target=x86_64 -fPIC -c tests/shared/lib.c -o "$tmp/lib_pic.o" || fail "x86_64: -fPIC -c failed"
gcc -shared "$tmp/lib_pic.o" -o "$tmp/lib_pic.so" 2>"$tmp/log" || fail "x86_64: -fPIC object did not link: $(cat "$tmp/log")"
[ ! -s "$tmp/log" ] || fail "x86_64: -fPIC link warned: $(cat "$tmp/log")"
# The exported counter is reached by its symbol (a word the dynamic
# linker fills), not through its section as NASM would write it.
readelf -rW "$tmp/lib_pic.o" | grep -q 'R_X86_64_64 .* counter' \
    || fail "x86_64: -fPIC does not reach counter by its symbol"
if ./main -target=samos-x86_64 -shared tests/shared/lib.c -o "$tmp/x.so" 2>"$tmp/log"; then
    fail "samos-x86_64 accepted -shared"
fi
assert_contains "$(cat "$tmp/log")" "no shared objects"

pass
