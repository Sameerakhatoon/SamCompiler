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
    ./main -target=$target -S -o "$tmp/shape_$target.s" "$tmp/shape.c" >"$tmp/log" 2>&1 \
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
    ./main -target=$target -o "$tmp/two_$target" "$tmp/one.c" "$tmp/two.c" >"$tmp/log" 2>&1 \
        || fail "$target: one.c + two.c: $(tail -3 "$tmp/log")"
    set +e
    $run "$tmp/two_$target"
    rc=$?
    set -e
    # bump(): one.c's hidden 2 + shared 6; two.c's hidden 11.
    assert_eq 19 "$rc" "$target: thread-locals across two files"
done

printf 'int main(void){ _Thread_local int x = 1; return x; }\n' > "$tmp/bad.c"
for target in x86 x86_64 aarch64 riscv64; do
    if ./main -target=$target "$tmp/bad.c" "$tmp/bad" >"$tmp/log" 2>&1; then
        fail "$target: a block-scope _Thread_local without static was accepted"
    fi
    grep -q "must be a global, static or extern variable" "$tmp/log" || fail "$target: unexpected message: $(tail -2 "$tmp/log")"
done

pass
