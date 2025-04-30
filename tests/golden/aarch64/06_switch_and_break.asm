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
mov x0, #1
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
.Ldo_while_start_3:
.Lentry_point_4:
.Lswitch_stmt_6:
sub x0, x29, #8
ldrsw x0, [x0]
mov w1, #1
cmp w0, w1
b.eq .Lswitch_stmt_6_case_1
mov w1, #2
cmp w0, w1
b.eq .Lswitch_stmt_6_case_2
mov w1, #3
cmp w0, w1
b.eq .Lswitch_stmt_6_case_3
mov w1, #4
cmp w0, w1
b.eq .Lswitch_stmt_6_case_4
mov w1, #5
cmp w0, w1
b.eq .Lswitch_stmt_6_case_5
mov w1, #6
cmp w0, w1
b.eq .Lswitch_stmt_6_case_6
b .Lexit_point_5
.Lswitch_stmt_6_case_1:
// CASE 1
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
b .Lexit_point_5
.Lswitch_stmt_6_case_2:
// CASE 2
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #4
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_5
.Lswitch_stmt_6_case_3:
// CASE 3
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #9
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_5
.Lswitch_stmt_6_case_4:
// CASE 4
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #4
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_5
.Lswitch_stmt_6_case_5:
// CASE 5
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #5
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_5
.Lswitch_stmt_6_case_6:
// CASE 6
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #6
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_5
.Lswitch_stmt_6_end:
.Lexit_point_5:
sub x0, x29, #8
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #8
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
.Lentry_point_1:
sub x0, x29, #8
ldrsw x0, [x0]
mov x1, #6
cmp x0, x1
cset w0, le
cbnz x0, .Ldo_while_start_3
.Lexit_point_2:
sub x0, x29, #4
ldrsw x0, [x0]
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
