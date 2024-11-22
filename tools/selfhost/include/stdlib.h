#ifndef SELFHOST_STDLIB_H
#define SELFHOST_STDLIB_H
#include <stddef.h>
void*  malloc(size_t size);
void*  calloc(size_t count, size_t size);
void*  realloc(void* ptr, size_t size);
void   free(void* ptr);
void   exit(int status);
void   abort(void);
long   strtol(const char* in, char** end, int base);
unsigned long long strtoull(const char* in, char** end, int base);
char*  getenv(const char* name);
int    system(const char* command);
int    mkstemp(char* template_name);
char*  realpath(const char* path, char* resolved);
int    atexit(void (*function)(void));
#endif
