#ifndef STDALIGN_H
#define STDALIGN_H
// C11 7.15. alignof is _Alignof. _Alignas is not supported
// yet, so alignas names it for the error to say so.
#define alignof _Alignof
#define alignas _Alignas
#define __alignof_is_defined 1
#define __alignas_is_defined 1
#endif
