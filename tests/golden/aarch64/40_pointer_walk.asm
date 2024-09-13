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
adrp x0, str_1
add x0, x0, :lo12:str_1
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #8
ldr x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #16
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #20
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
.Lentry_point_2:
.Lwhile_start_4:
sub x0, x29, #16
ldr x0, [x0]
ldrb w0, [x0]
mov x1, #103
sxtw x0, w0
cmp x0, x1
b.eq .Lwhile_end_5
sub x0, x29, #16
ldr x0, [x0]
mov x1, #1
add x0, x0, x1
mov x1, x0
sub x0, x29, #16
mov x12, x0
mov x0, x1
mov x1, x12
str x0, [x1]
sub x0, x29, #20
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #20
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lwhile_start_4
.Lwhile_end_5:
.Lexit_point_3:
sub x0, x29, #20
ldrsw x0, [x0]
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
str_1: .asciz "abcdefg"
