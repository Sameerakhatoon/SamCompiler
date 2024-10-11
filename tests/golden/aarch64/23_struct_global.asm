.data
// pt g
.globl g
.balign 8
g: .quad 0
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
mov x0, #6
str x0, [sp, #-16]!
adrp x0, g
add x0, x0, :lo12:g
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #7
str x0, [sp, #-16]!
adrp x0, g
add x0, x0, :lo12:g
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
adrp x0, g
add x0, x0, :lo12:g
ldrsw x0, [x0]
str x0, [sp, #-16]!
adrp x0, g
add x0, x0, :lo12:g
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
