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
sub sp, sp, #16
mov x0, #0
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
.Lentry_point_1:
.Lwhile_start_3:
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #100
cmp x0, x1
b.ge .Lwhile_end_4
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #2
cmp x0, x1
b.ne .Lif_6
b .Lentry_point_1
b .Lif_end_5
.Lif_6:
.Lif_end_5:
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #4
cmp x0, x1
b.le .Lif_8
b .Lexit_point_2
b .Lif_end_7
.Lif_8:
.Lif_end_7:
sub x0, x29, #8
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #8
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lwhile_start_3
.Lwhile_end_4:
.Lexit_point_2:
sub x0, x29, #8
ldrsw x0, [x0]
mov x1, #1
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
