#!/usr/bin/env bash
# _Thread_local and __thread. tests/diff/thread_local.c and
# threads_*.c check the values against gcc on every target; here:
#   - the sections and the TLS models: .tdata/.tbss, local-exec on
#     aarch64 and riscv64 for a variable the file defines, initial-exec
#     for an extern one, and initial-exec through the hidden per-file
#     alias on the two NASM targets (fs:0, gs:0);
#   - two files linked together on every target: an extern thread-local
#     defined in the other file, and a static one of the same name in
#     each file;
#   - a block-scope _Thread_local without static or extern is an error.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_tls.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/shape.c" <<'EOF'
_Thread_local int here = 3;
static __thread long zero;
extern _Thread_local int there;
int f(void){
    zero++;
    return here + there + (int)zero;
}
EOF

expect_asm(){
    local target="$1"; shift
    samcc -target=$target -S -o "$tmp/shape_$target.s" "$tmp/shape.c" >"$tmp/log" 2>&1 \
        || fail "$target: shape.c did not compile: $(tail -2 "$tmp/log")"
    local text
    for text in "$@"; do
        grep -qF -- "$text" "$tmp/shape_$target.s" || fail "$target: no '$text' in the thread-local code"
    done
}

expect_asm x86     "section .tdata progbits alloc write tls" "section .tbss nobits alloc write tls" \
                   "wrt ..tlsie]" "add eax, [gs:0]" ":hidden"
expect_asm x86_64  "section .tdata progbits alloc write tls" "section .tbss nobits alloc write tls" \
                   "wrt ..gottpoff]" "add rax, [fs:0]" ":hidden"
expect_asm aarch64 '.pushsection .tdata,"awT",@progbits' '.pushsection .tbss,"awT",@nobits' \
                   "mrs x0, tpidr_el0" "#:tprel_hi12:here" ":gottprel:there"
expect_asm riscv64 '.pushsection .tdata,"awT",@progbits' '.pushsection .tbss,"awT",@nobits' \
                   "%tprel_hi(here)" "%tprel_add(here)" "la.tls.ie a0, there"

cat > "$tmp/one.c" <<'EOF'
_Thread_local int shared = 5;
static _Thread_local int hidden = 1;
int bump(void){
    return ++hidden + shared;
}
EOF
cat > "$tmp/two.c" <<'EOF'
extern _Thread_local int shared;
static _Thread_local int hidden = 10;
int bump(void);
int main(void){
    shared += 1;
    hidden++;
    return bump() + hidden;
}
EOF
for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86)     run="" ;;
        x86_64)  run="" ;;
        aarch64) run="qemu-aarch64" ;;
        riscv64) run="qemu-riscv64" ;;
    esac
    have_tools ${run:-true} || continue
    samcc -target=$target -o "$tmp/two_$target" "$tmp/one.c" "$tmp/two.c" >"$tmp/log" 2>&1 \
        || fail "$target: one.c + two.c: $(tail -3 "$tmp/log")"
    set +e
    $run "$tmp/two_$target"
    rc=$?
    set -e
    # bump(): one.c's hidden 2 + shared 6; two.c's hidden 11.
    assert_eq 19 "$rc" "$target: thread-locals across two files"
done

# A thread-local in a shared object: initial-exec, never local-exec
# (the object's block is not at a link-time offset from the thread
# pointer). i386 has no PIC form of it in NASM: a text relocation there.
printf '_Thread_local int t = 5;\nstatic _Thread_local int s = 1;\nint get(void){ s++; return ++t + s; }\n' > "$tmp/tlib.c"
printf 'int get(void);\nint main(void){ return get() == 8 && get() == 10 ? 0 : 1; }\n' > "$tmp/tmain.c"
for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86)     cc="i686-linux-gnu-gcc"; run="/usr/i686-linux-gnu/lib/ld-linux.so.2 --library-path /usr/i686-linux-gnu/lib:$tmp" ;;
        x86_64)  cc="gcc"; run="env LD_LIBRARY_PATH=$tmp" ;;
        aarch64) cc="aarch64-linux-gnu-gcc qemu-aarch64"; run="qemu-aarch64 -L /usr/aarch64-linux-gnu -E LD_LIBRARY_PATH=$tmp" ;;
        riscv64) cc="riscv64-linux-gnu-gcc qemu-riscv64"; run="qemu-riscv64 -L /usr/riscv64-linux-gnu -E LD_LIBRARY_PATH=$tmp" ;;
    esac
    have_tools $cc || continue
    rm -f "$tmp/libtls.so"
    samcc -target=$target -shared "$tmp/tlib.c" -o "$tmp/libtls.so" >"$tmp/log" 2>&1 \
        || fail "$target: thread-local shared object: $(tail -3 "$tmp/log")"
    samcc -target=$target "$tmp/tmain.c" "$tmp/libtls.so" -o "$tmp/tmain_$target" >"$tmp/log" 2>&1 \
        || fail "$target: program with the thread-local shared object: $(tail -3 "$tmp/log")"
    set +e
    $run "$tmp/tmain_$target"
    rc=$?
    set -e
    assert_eq 0 "$rc" "$target: thread-locals in a shared object"
done

printf 'int main(void){ _Thread_local int x = 1; return x; }\n' > "$tmp/bad.c"
for target in x86 x86_64 aarch64 riscv64; do
    if samcc -target=$target "$tmp/bad.c" "$tmp/bad" >"$tmp/log" 2>&1; then
        fail "$target: a block-scope _Thread_local without static was accepted"
    fi
    grep -q "must be a global, static or extern variable" "$tmp/log" || fail "$target: unexpected message: $(tail -2 "$tmp/log")"
done

pass
