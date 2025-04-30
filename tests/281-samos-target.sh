#!/usr/bin/env bash
# -target=samos-x86_64, samos-aarch64 and samos-riscv64. Against
# a small sysroot built here (include/, lib/crt0.o, lib/libsamos.a,
# lib/user.ld, the layout SamOS's Makefile exports) each target must
#   - predefine __samos__, its arch macro and __LP64__,
#   - take <stdio.h> from <sysroot>/include and <stdint.h> from
#     pc_includes, and never search /usr/include,
#   - link a static ELF for its machine whose entry is crt0's _start,
#     with no glibc in it,
#   - refuse to run without -sysroot= or SAMOS_SYSROOT.
# Running the programs under SamOS is tests64/arch/*-samcc.sh and
# e2e/208 in the SamOS repo.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_samos.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/prog.c" <<'EOF'
#include <stdio.h>
#include <stdint.h>
#ifndef __samos__
#error no __samos__
#endif
#ifndef __LP64__
#error no __LP64__
#endif
#ifndef SYSROOT_STDIO
#error stdio.h is not from the sysroot
#endif
int main(int argc, char** argv){
    uint32_t n = 3;
    printf("hello %d\n", n);
    return 0;
}
EOF
cat > "$tmp/libc.c" <<'EOF'
int printf(const char* fmt, ...){ return 0; }
EOF
cat > "$tmp/crt0.c" <<'EOF'
int main(int argc, char** argv);
void _start(void){ main(0, 0); for(;;); }
EOF
cat > "$tmp/user.ld" <<'EOF'
ENTRY(_start)
SECTIONS
{
    . = 0x400000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
EOF

checked=0
for t in x86_64 aarch64 riscv64; do
    case $t in
        x86_64)  cc=gcc;                   macro=__x86_64__;  machine="X86-64" ;;
        aarch64) cc=aarch64-linux-gnu-gcc; macro=__aarch64__; machine="AArch64" ;;
        riscv64) cc=riscv64-linux-gnu-gcc; macro=__riscv;     machine="RISC-V" ;;
    esac
    have_tools "$cc" || continue
    sys="$tmp/sysroot-$t"
    mkdir -p "$sys/include" "$sys/lib"
    printf '#define SYSROOT_STDIO 1\nint printf(const char* fmt, ...);\n' > "$sys/include/stdio.h"
    $cc -ffreestanding -fno-pie -fno-stack-protector -O0 -c "$tmp/crt0.c" -o "$sys/lib/crt0.o"
    $cc -ffreestanding -fno-pie -fno-stack-protector -O0 -c "$tmp/libc.c" -o "$tmp/libc-$t.o"
    ar rcs "$sys/lib/libsamos.a" "$tmp/libc-$t.o"
    cp "$tmp/user.ld" "$sys/lib/user.ld"

    out=$(samcc -target=samos-$t "$tmp/prog.c" "$tmp/x" 2>&1 && echo "rc=0" || echo "rc=$?")
    assert_contains "$out" "needs -sysroot=DIR or SAMOS_SYSROOT" "$t without a sysroot"
    assert_contains "$out" "rc=1" "$t without a sysroot exits 1"

    elf="$tmp/prog-$t.elf"
    (cd "$tmp" && samcc -target=samos-$t -sysroot="$sys" "$tmp/prog.c" "$elf" > "$tmp/build-$t.log" 2>&1) || true
    head -c 4 "$elf" 2>/dev/null | grep -q ELF || fail "$t: no ELF: $(grep -i 'error' "$tmp/build-$t.log" | head -3)"
    readelf -h "$elf" | grep -q "Machine:.*$machine" || fail "$t: wrong machine: $(readelf -h "$elf" | grep Machine)"
    readelf -h "$elf" | grep -q "Type: *EXEC" || fail "$t: not a static executable"
    readelf -l "$elf" | grep -q INTERP && fail "$t: has an interpreter"
    entry=$(readelf -h "$elf" | sed -n 's/.*Entry point address: *0x0*//p')
    start=$(nm "$elf" | sed -n 's/^0*\([0-9a-f]*\) T _start$/\1/p')
    assert_eq "$start" "$entry" "$t entry is _start"
    nm "$elf" | grep -qE '__libc_start_main|GLIBC' && fail "$t: glibc linked in"

    SAMOS_SYSROOT="$sys" samcc -target=samos-$t "$tmp/prog.c" "$tmp/env-$t.elf" > /dev/null 2>&1 || true
    head -c 4 "$tmp/env-$t.elf" 2>/dev/null | grep -q ELF || fail "$t: SAMOS_SYSROOT not used"

    # The arch macro, and no glibc headers.
    printf '#ifndef %s\n#error no arch macro\n#endif\nint main(){ return 0; }\n' "$macro" > "$tmp/arch.c"
    samcc -target=samos-$t -sysroot="$sys" "$tmp/arch.c" "$tmp/arch.s" object > "$tmp/arch.log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/arch.log" || fail "$t: $macro not defined"
    printf '#include <unistd.h>\nint main(){ return 0; }\n' > "$tmp/glibc.c"
    samcc -target=samos-$t -sysroot="$sys" "$tmp/glibc.c" "$tmp/glibc.s" object > "$tmp/glibc.log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/glibc.log" && fail "$t: found <unistd.h> (glibc headers searched)"
    checked=$((checked + 1))
done
[ "$checked" -ge 1 ] || fail "no target checked"
pass
