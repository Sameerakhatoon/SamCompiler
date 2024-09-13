.data
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
sub sp, sp, #32
mov x0, #1
str x0, [sp, #-16]!
sub x0, x29, #12
str x0, [sp, #-16]!
mov x0, #0
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #2
str x0, [sp, #-16]!
sub x0, x29, #12
str x0, [sp, #-16]!
mov x0, #1
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #3
str x0, [sp, #-16]!
sub x0, x29, #12
str x0, [sp, #-16]!
mov x0, #2
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #12
str x0, [sp, #-16]!
mov x0, #0
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #12
str x0, [sp, #-16]!
mov x0, #1
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #20
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #12
str x0, [sp, #-16]!
mov x0, #2
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #24
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #16
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #20
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #24
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
add sp, sp, #32
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size main, .-main
.data
.section .rodata
