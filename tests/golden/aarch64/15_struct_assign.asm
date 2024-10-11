.data
// pt a
.globl a
.balign 8
a: .quad 0
// pt b
.globl b
.balign 8
b: .quad 0
.text
.p2align 2
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
mov x0, #8
str x0, [sp, #-16]!
adrp x0, b
add x0, x0, :lo12:b
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #11
str x0, [sp, #-16]!
adrp x0, b
add x0, x0, :lo12:b
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
adrp x0, b
add x0, x0, :lo12:b
mov x1, x0
adrp x0, a
add x0, x0, :lo12:a
ldr x11, [x1, #0]
str x11, [x0, #0]
adrp x0, a
add x0, x0, :lo12:a
ldrsw x0, [x0]
str x0, [sp, #-16]!
adrp x0, a
add x0, x0, :lo12:a
add x0, x0, #4
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
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
.section .note.GNU-stack,"",@progbits
