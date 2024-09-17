#!/usr/bin/env bash
# There was no include-guard test. A header guarded
# with #ifndef/#define, and one guarded with #if !defined(...), are
# included twice directly and once more through another guarded header.
# Each body records whether it was already seen; the program returns 99
# if a guarded body ran twice. An unguarded header included twice must
# run twice (so the check itself works), and pc_includes' own guarded
# headers can be included twice. Built and run for x86_64 and x86.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
need_tools nasm
need_x86_link

tmp=$(mktemp -d /tmp/sam_guards.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/inc"

cat > "$tmp/inc/point.h" <<'EOF'
#ifndef POINT_H
#define POINT_H
#ifdef POINT_BODY_SEEN
#define POINT_BODY_TWICE 1
#endif
#define POINT_BODY_SEEN 1
struct point { int x; int y; };
typedef int coord_t;
int point_sum(struct point* p){ return p->x + p->y; }
#endif
EOF
cat > "$tmp/inc/shape.h" <<'EOF'
#if !defined(SHAPE_H)
#define SHAPE_H
#include "point.h"
#ifdef SHAPE_BODY_SEEN
#define SHAPE_BODY_TWICE 1
#endif
#define SHAPE_BODY_SEEN 1
struct box { struct point lo; struct point hi; };
int box_width(struct box* b){ return b->hi.x - b->lo.x; }
#endif
EOF
cat > "$tmp/inc/plain.h" <<'EOF'
#ifdef PLAIN_SEEN
#define PLAIN_TWICE 1
#endif
#ifndef PLAIN_SEEN
#define PLAIN_SEEN 1
#endif
EOF
cat > "$tmp/prog.c" <<'EOF'
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdbool.h>
#include "point.h"
#include "shape.h"
#include "point.h"
#include "shape.h"
#include "plain.h"
#include "plain.h"
int main(void){
#ifdef POINT_BODY_TWICE
    return 99;
#endif
#ifdef SHAPE_BODY_TWICE
    return 98;
#endif
#ifndef PLAIN_TWICE
    return 97;
#endif
    struct box b;
    coord_t one = 1;
    b.lo.x = one; b.lo.y = 2; b.hi.x = 11; b.hi.y = 4;
    int32_t w = box_width(&b);
    bool ok = point_sum(&b.lo) == 3;
    return ok ? w + 30 : 1;
}
EOF

for target in x86_64 x86; do
    "$REPO_ROOT/main" -target=$target -I"$tmp/inc" "$tmp/prog.c" "$tmp/prog-$target" >"$tmp/build-$target.log" 2>&1 \
        || fail "$target build failed: $(grep -v '^\s*$' "$tmp/build-$target.log" | tail -3)"
    "$tmp/prog-$target" && rc=0 || rc=$?
    assert_eq 40 "$rc" "$target exit status (99/98: a guarded body ran twice, 97: unguarded ran once)"
done
pass
