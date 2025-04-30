#!/usr/bin/env bash
# SamCompiler's x86 code linked with gcc's.
# gcc code calls SamCompiler functions with ebx set and checks it comes
# back, gets a struct returned through the hidden pointer, and checks
# from its own frames that every call SamCompiler makes is 16-byte
# aligned. SamCompiler code also calls a gcc function returning a struct.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

command -v i686-linux-gnu-gcc >/dev/null || fail "i686-linux-gnu-gcc is missing"

tmp=$(mktemp -d /tmp/sam_x86abi.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/g.c" <<'EOF'
struct big { int a, b, c; };
extern struct big sam_make(int x);
extern int sam_plain(int x);
extern int sam_calls_back(void);
static int misaligned;
int gcc_align_check(int x){
    unsigned long fp = (unsigned long)__builtin_frame_address(0);
    if((fp + 8) % 16 != 0) misaligned++;
    return x + 1;
}
struct big gcc_make(int x){ struct big r = { x, x * 2, x * 3 }; return r; }
/* ebx = 12345, call sam_plain(3), store its result in *v, return ebx. */
__asm__(
    ".globl ebx_call\n"
    "ebx_call:\n"
    "    push ebx\n"
    "    mov ebx, 12345\n"
    "    sub esp, 4\n"
    "    push 3\n"
    "    call sam_plain\n"
    "    add esp, 8\n"
    "    mov ecx, [esp+8]\n"
    "    mov [ecx], eax\n"
    "    mov eax, ebx\n"
    "    pop ebx\n"
    "    ret\n");
int ebx_call(int* v);
int gcc_main(void){
    int fail = 0;
    int v = 0;
    if(ebx_call(&v) != 12345) fail |= 1;
    if(v != 4) fail |= 2;
    struct big b = sam_make(5);
    if(b.a != 5 || b.b != 10 || b.c != 15) fail |= 4;
    if(sam_calls_back() != 6 + 7 + 14 + 21) fail |= 8;
    if(misaligned) fail |= 16;
    return fail;
}
int main(void){ return gcc_main(); }
EOF

cat > "$tmp/s.c" <<'EOF'
struct big { int a; int b; int c; };
int gcc_align_check(int x);
struct big gcc_make(int x);
int sam_plain(int x){ int a[3]; a[0] = x; return gcc_align_check(a[0]); }
struct big sam_make(int x){
    struct big r;
    r.a = x; r.b = x * 2; r.c = x * 3;
    gcc_align_check(1);
    return r;
}
int sam_calls_back(void){
    int t = gcc_align_check(5);
    struct big b = gcc_make(7);
    int u = gcc_align_check(1) + gcc_align_check(2);
    return t + b.a + b.b + b.c + u - 5;
}
EOF

samcc -target=x86 "$tmp/s.c" "$tmp/s.asm" object >"$tmp/s.log" 2>&1 \
    || fail "s.c did not compile: $(tail -2 "$tmp/s.log")"
i686-linux-gnu-gcc -O1 -masm=intel -fno-omit-frame-pointer -c "$tmp/g.c" -o "$tmp/g.o" \
    || fail "gcc could not build the gcc side"
i686-linux-gnu-gcc -static "$tmp/g.o" "$tmp/s.asm.o" -o "$tmp/abi" 2>/dev/null \
    || fail "linking the two sides failed"
rc=0
"$tmp/abi" || rc=$?
# Bits: 1 ebx clobbered (B3), 2 wrong value, 4 struct return from
# SamCompiler (B2), 8 struct return from gcc (B2), 16 misaligned (B4).
assert_eq 0 "$rc" "x86 calls between SamCompiler and gcc code"

pass
