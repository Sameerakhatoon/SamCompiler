.data
.text
.p2align 2
// extern printf
.globl main
.type main, %function
// main function
main:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
adrp x0, str_1
add x0, x0, :lo12:str_1
str x0, [sp, #-16]!
mov x0, #42
str x0, [sp, #-16]!
adrp x0, str_2
add x0, x0, :lo12:str_2
str x0, [sp, #-16]!
ldr x0, [sp], #16
ldr x1, [sp], #16
ldr x2, [sp], #16
adrp x9, :got:printf
ldr x9, [x9, :got_lo12:printf]
blr x9
sxtw x0, w0
mov x0, #0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size main, .-main
.data
.section .rodata
str_1: .asciz "hi"
str_2: .asciz "v=%d s=%s\012"
.section .note.GNU-stack,"",@progbits
