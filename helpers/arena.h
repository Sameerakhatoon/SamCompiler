#ifndef ARENA_H
#define ARENA_H

// Allocation tracking for one compile. The compiler builds its trees,
// vectors and tables with many small allocations and never knew which
// pass owned what, so nothing was freed and LeakSanitizer reported every
// compile. Every compiler allocation now goes through these functions,
// which keep the block on a list; arena_free_all() releases whatever is
// still on it once the compile is over. free() of a single block still
// works and takes it off the list.
//
// The compiler's sources get the functions through the macros below
// (compiler.h and the helpers include this header). arena.c itself
// defines ARENA_IMPLEMENTATION to reach the C library.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void* arena_malloc(size_t size);
void* arena_calloc(size_t count, size_t size);
void* arena_realloc(void* ptr, size_t size);
void  arena_free(void* ptr);
char* arena_strdup(const char* str);
void  arena_free_all(void);

#ifndef ARENA_IMPLEMENTATION
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#define malloc(size)         arena_malloc(size)
#define calloc(count, size)  arena_calloc(count, size)
#define realloc(ptr, size)   arena_realloc(ptr, size)
#define free(ptr)            arena_free(ptr)
#define strdup(str)          arena_strdup(str)
#endif

#endif
