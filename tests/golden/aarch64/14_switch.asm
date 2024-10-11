.data
.text
.p2align 2
.globl classify
.type classify, %function
// classify function
classify:
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
.Lentry_point_1:
.Lswitch_stmt_3:
add x0, x29, #16
ldrsw x0, [x0]
mov w1, #1
cmp w0, w1
b.eq .Lswitch_stmt_3_case_1
mov w1, #2
cmp w0, w1
b.eq .Lswitch_stmt_3_case_2
b .Lswitch_stmt_3_case_default
.Lswitch_stmt_3_case_1:
// CASE 1
mov x0, #10
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_2
.Lswitch_stmt_3_case_2:
// CASE 2
mov x0, #20
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_2
// DEFAULT CASE
.Lswitch_stmt_3_case_default:
mov x0, #3
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lexit_point_2
.Lswitch_stmt_3_end:
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
.size classify, .-classify
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
mov x0, #2
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, classify
add x9, x9, :lo12:classify
blr x9
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #1
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, classify
add x9, x9, :lo12:classify
blr x9
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #4
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
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
