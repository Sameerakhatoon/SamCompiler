.data
.text
.p2align 2
.globl make
.type make, %function
// make function
make:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
sub sp, sp, #16
add x0, x29, #16
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
add x0, x29, #24
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #8
mov x11, x0
ldr x0, [x11, #0]
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
add sp, sp, #16
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size make, .-make
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
sub sp, sp, #16
sub sp, sp, #16
mov x0, #7
str x0, [sp, #-16]!
mov x0, #2
str x0, [sp, #-16]!
ldr x0, [sp], #16
ldr x1, [sp], #16
adrp x9, make
add x9, x9, :lo12:make
blr x9
str x0, [sp]
mov x0, sp
mov x1, x0
sub x0, x29, #8
ldr x11, [x1, #0]
str x11, [x0, #0]
add sp, sp, #16
sub x0, x29, #8
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
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
add sp, sp, #16
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
