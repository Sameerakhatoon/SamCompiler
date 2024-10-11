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
sub x0, x29, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #4
ldrsw x0, [x0]
cbz x0, .Lexp_1
sub x0, x29, #8
ldrsw x0, [x0]
cbz x0, .Lexp_1
mov x0, #1
b .Lexp_2
.Lexp_1:
mov x0, #0
.Lexp_2:
str x0, [sp, #-16]!
sub x0, x29, #12
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #4
ldrsw x0, [x0]
cbnz x0, .Lexp_3
sub x0, x29, #8
ldrsw x0, [x0]
cbnz x0, .Lexp_3
mov x0, #0
b .Lexp_4
.Lexp_3:
mov x0, #1
.Lexp_4:
str x0, [sp, #-16]!
sub x0, x29, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #8
ldrsw x0, [x0]
cmp x0, #0
cset w0, eq
str x0, [sp, #-16]!
sub x0, x29, #20
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #12
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #16
ldrsw x0, [x0]
mov x1, #2
mul w0, w0, w1
sxtw x0, w0
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #20
ldrsw x0, [x0]
mov x1, #4
mul w0, w0, w1
sxtw x0, w0
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
.section .note.GNU-stack,"",@progbits
