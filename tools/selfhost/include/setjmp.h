#ifndef SELFHOST_SETJMP_H
#define SELFHOST_SETJMP_H
/* Declarations only, with glibc's x86_64 layout of jmp_buf (200
   bytes): the diagnostics' error recovery in compiler.h needs it. */
typedef long __jmp_buf[8];
struct __jmp_buf_tag {
    __jmp_buf __jmpbuf;
    int __mask_was_saved;
    struct { unsigned long __val[16]; } __saved_mask;
};
typedef struct __jmp_buf_tag jmp_buf[1];
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
#endif
