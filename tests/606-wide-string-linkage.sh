#!/usr/bin/env bash
# The array behind a wide string literal (L"", u"", U"") has
# internal linkage, as a string literal does. Every file numbers its own
# `__wide_string_N` from 0, and exported ones failed to link two files
# that both used one ("multiple definition of `__wide_string_0'"). Two
# files with wide literals are linked and run on all four targets.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_wide_link.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# No headers: a wchar_t and a char32_t are 4 bytes and a char16_t 2 on
# all four targets; the units are read through void pointers.
cat > "$tmp/a.c" <<'EOF'
int other(void);
int units4(const void* p){ const int* s = p; int n = 0; while(s[n]) n++; return n; }
static const void* greeting = L"hello";
int main(void){
    const void* w = L"wide";
    return units4(greeting) + units4(w) * 10 + other() * 100 - 345;
}
EOF
cat > "$tmp/b.c" <<'EOF'
int units4(const void* p);
int other(void){
    const void* w = L"ab";
    const unsigned short* s = (const void*)u"xyz";
    const void* t = U"q";
    int n = 0;
    while(s[n]) n++;
    return units4(w) + n - 3 + (*(const unsigned*)t == 'q');
}
EOF

for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86)     have_tools nasm || continue; need_x86_link; runner="" ;;
        x86_64)  have_tools nasm gcc || continue; runner="" ;;
        aarch64) have_tools aarch64-linux-gnu-gcc qemu-aarch64 || continue; runner="qemu-aarch64" ;;
        riscv64) have_tools riscv64-linux-gnu-gcc qemu-riscv64 || continue; runner="qemu-riscv64" ;;
    esac
    samcc -target=$target "$tmp/a.c" "$tmp/b.c" -o "$tmp/prog-$target" > "$tmp/$target.log" 2>&1 \
        || fail "$target: two files with wide literals did not link: $(grep -iE 'error|multiple' "$tmp/$target.log" | head -3)"
    rc=0
    timeout 60 $runner "$tmp/prog-$target" || rc=$?
    assert_eq 0 "$rc" "$target exit status"
    # Each file keeps its literals to itself.
    samcc -target=$target -S "$tmp/a.c" -o "$tmp/a-$target.s" >/dev/null 2>&1 || fail "$target: -S a.c"
    if grep -qE '^[[:space:]]*(global|\.globl|\.global)[[:space:]]+\$?__wide_string' "$tmp/a-$target.s"; then
        fail "$target: a wide string literal is exported"
    fi
done
pass
