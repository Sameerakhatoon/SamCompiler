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
// SamCompiler's va_list is not glibc's: its vprintf family is
// tools/selfhost/samvprintf.c.
#define vfprintf  sam_vfprintf
#define vsnprintf sam_vsnprintf
#define vsprintf  sam_vsprintf
#define vprintf   sam_vprintf
FILE*  fopen(const char* path, const char* mode);
int    fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
char*  fgets(char* s, int size, FILE* stream);
int    getc(FILE* stream);
int    fgetc(FILE* stream);
int    fputc(int c, FILE* stream);
int    fputs(const char* s, FILE* stream);
int    puts(const char* s);
int    fflush(FILE* stream);
FILE*  fmemopen(void* buffer, size_t size, const char* mode);
int    ungetc(int c, FILE* stream);
int    printf(const char* fmt, ...);
int    fprintf(FILE* stream, const char* fmt, ...);
int    sprintf(char* out, const char* fmt, ...);
int    snprintf(char* out, size_t size, const char* fmt, ...);
int    vfprintf(FILE* stream, const char* fmt, va_list args);
int    vprintf(const char* fmt, va_list args);
int    vsprintf(char* out, const char* fmt, va_list args);
int    vsnprintf(char* out, size_t size, const char* fmt, va_list args);
int    sscanf(const char* in, const char* fmt, ...);
int    rename(const char* from, const char* to);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int    fseek(FILE* stream, long offset, int whence);
long   ftell(FILE* stream);
void   rewind(FILE* stream);
#endif
