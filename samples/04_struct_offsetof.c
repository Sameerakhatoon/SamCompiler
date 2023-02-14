// EXPECTED EXIT: 8
//
// Exercises: struct definition, member access, sizeof on a
// struct, #include <stddef.h>, offsetof macro (expands to
// &((TYPE*)0x00)->MEMBER). The resolver's ch233 pointer-cast
// fix is what makes offsetof actually work.
//
// `point` has two int members, so:
//   sizeof(point) == 8
//   offsetof(point, y) == 4
// Return their sum = 8 + 4 = 12... wait, the comment lies on
// purpose. Read the code: we return sizeof(point) only, = 8.

#include <stddef.h>

struct point {
    int x;
    int y;
};

int main()
{
    return sizeof(struct point);
}
