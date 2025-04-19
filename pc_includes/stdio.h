#ifndef STDIO_H
#define STDIO_H
#include <stdlib.h>

// FILE is only ever used through a pointer; its layout is the C
// library's business. stdin, stdout and stderr
// are glibc's variables of those names.
typedef struct _IO_FILE FILE;

#define EOF (-1)
#define BUFSIZ 8192
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE*  fopen(const char* filename, const char* mode);
FILE*  freopen(const char* filename, const char* mode, FILE* stream);
int    fclose(FILE* stream);
int    fflush(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int    fseek(FILE* stream, long offset, int whence);
long   ftell(FILE* stream);
void   rewind(FILE* stream);
int    feof(FILE* stream);
int    ferror(FILE* stream);
void   clearerr(FILE* stream);

int    printf(const char* fmt, ...);
int    fprintf(FILE* stream, const char* fmt, ...);
int    sprintf(char* str, const char* fmt, ...);
int    snprintf(char* str, size_t size, const char* fmt, ...);
int    scanf(const char* fmt, ...);
int    fscanf(FILE* stream, const char* fmt, ...);
int    sscanf(const char* str, const char* fmt, ...);
// No vprintf family: SamCompiler's va_list is not the C library's on
// aarch64 and riscv64, so passing one on would read the wrong memory.
// On x86_64 it is; -glibc gives glibc's own stdio.h there.

int    fgetc(FILE* stream);
int    getc(FILE* stream);
int    getchar(void);
char*  fgets(char* str, int size, FILE* stream);
int    ungetc(int c, FILE* stream);
int    fputc(int c, FILE* stream);
int    putc(int c, FILE* stream);
int    putchar(int c);
int    fputs(const char* str, FILE* stream);
int    puts(const char* str);

void   perror(const char* str);
int    remove(const char* path);
int    rename(const char* from, const char* to);
#endif
