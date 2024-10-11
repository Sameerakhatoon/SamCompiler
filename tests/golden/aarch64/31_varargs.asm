.data
.text
.p2align 2
.globl sum2
.type sum2, %function
// sum2 function
sum2:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
sub sp, sp, #16
add x0, x29, #16
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #8
str x0, [sp, #-16]!
ldr x0, [x0]
add x0, x0, #8
ldr x1, [sp], #16
str x0, [x1]
ldr x0, [x0]
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #12
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #8
str x0, [sp, #-16]!
ldr x0, [x0]
add x0, x0, #8
ldr x1, [sp], #16
str x0, [x1]
ldr x0, [x0]
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #12
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #16
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
.size sum2, .-sum2
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
mov x0, #20
str x0, [sp, #-16]!
mov x0, #10
str x0, [sp, #-16]!
mov x0, #2
str x0, [sp, #-16]!
ldr x0, [sp], #16
ldr x1, [sp], #16
ldr x2, [sp], #16
adrp x9, sum2
add x9, x9, :lo12:sum2
blr x9
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
