#ifndef STDBOOL_H
#define STDBOOL_H
// C11 7.18: bool is _Bool, which holds 0 or 1 (a conversion to it is
// `!= 0`). It was an unsigned char, so `bool b = flags & 0x800` was 0.
#define bool _Bool
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif
