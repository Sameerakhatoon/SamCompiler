#ifndef SELFHOST_STDIO_H
#define SELFHOST_STDIO_H
#include <stddef.h>
#include <stdarg.h>
struct _IO_FILE {
    int _flags;
};
typedef struct _IO_FILE FILE;
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
#define EOF (-1)
FILE*  fopen(const char* path, const char* mode);
int    fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int    getc(FILE* stream);
int    ungetc(int c, FILE* stream);
int    printf(const char* fmt, ...);
int    fprintf(FILE* stream, const char* fmt, ...);
int    sprintf(char* out, const char* fmt, ...);
int    snprintf(char* out, size_t size, const char* fmt, ...);
int    vfprintf(FILE* stream, const char* fmt, va_list args);
int    vsprintf(char* out, const char* fmt, va_list args);
int    vsnprintf(char* out, size_t size, const char* fmt, va_list args);
int    sscanf(const char* in, const char* fmt, ...);
int    rename(const char* from, const char* to);
#endif
