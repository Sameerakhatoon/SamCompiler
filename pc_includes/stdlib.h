#ifndef STDLIB_H
#define STDLIB_H
// size_t is unsigned and pointer-sized: 4 bytes on x86, 8 on the LP64
// targets.
// Two blocks instead of #else,
#ifndef SAMCC_SIZE_T
#define SAMCC_SIZE_T
#ifndef __LP64__
typedef unsigned int size_t;
#endif
#ifdef __LP64__
typedef unsigned long size_t;
#endif
#endif

// The common C library declarations, as glibc has them, so ordinary
// programs link against the target's libc.
#ifndef NULL
#define NULL 0
#endif
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* ptr, size_t size);
void  free(void* ptr);

void  exit(int status);
void  abort(void);
int   atexit(void (*function)(void));
char* getenv(const char* name);
int   system(const char* command);

int   abs(int value);
long  labs(long value);
int   atoi(const char* s);
long  atol(const char* s);
long  strtol(const char* s, char** end, int base);
unsigned long strtoul(const char* s, char** end, int base);

int   rand(void);
void  srand(unsigned int seed);

void  qsort(void* base, size_t count, size_t size, int (*compare)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t count, size_t size, int (*compare)(const void*, const void*));
#endif
