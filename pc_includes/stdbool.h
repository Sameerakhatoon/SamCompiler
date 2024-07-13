#ifndef STDBOOL_H
#define STDBOOL_H
// no _Bool in SamCompiler; a one-byte unsigned type has the
// same size and the same 0 / 1 values gcc's bool passes.
typedef unsigned char bool;
#define true 1
#define false 0
#endif
