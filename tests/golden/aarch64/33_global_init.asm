.data
// int g
.globl g
.balign 4
g: .word 20
// char k
.globl k
.balign 1
k: .byte 100
// char msg
.globl msg
.balign 8
msg: .quad str_1
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
adrp x0, g
add x0, x0, :lo12:g
ldrsw x0, [x0]
str x0, [sp, #-16]!
adrp x0, k
add x0, x0, :lo12:k
ldrb w0, [x0]
mov x1, x0
ldr x0, [sp], #16
sxtw x1, w1
add w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
adrp x0, msg
add x0, x0, :lo12:msg
ldr x0, [x0]
str x0, [sp, #-16]!
mov x0, #1
ldr x1, [sp], #16
add x0, x0, x1
ldrb w0, [x0]
mov x1, x0
ldr x0, [sp], #16
sxtw x1, w1
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
str_1: .asciz "x"
.section .note.GNU-stack,"",@progbits
