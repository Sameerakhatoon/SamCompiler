.data
// cell values
.globl values
.balign 8
values: .zero 24
.text
.p2align 2
.globl bubble_sort
.type bubble_sort, %function
// bubble_sort function
bubble_sort:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
sub sp, sp, #32
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
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #12
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #20
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #24
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lfor_loop1
.Lentry_point_3:
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
.Lfor_loop1:
sub x0, x29, #4
ldrsw x0, [x0]
str x0, [sp, #-16]!
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #1
sub w0, w0, w1
sxtw x0, w0
mov x1, x0
ldr x0, [sp], #16
cmp x0, x1
b.ge .Lfor_loop_end2
mov x0, #0
mov x1, x0
sub x0, x29, #8
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lfor_loop5
.Lentry_point_7:
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
.Lfor_loop5:
sub x0, x29, #8
ldrsw x0, [x0]
str x0, [sp, #-16]!
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #1
sub w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
sub w0, w0, w1
sxtw x0, w0
mov x1, x0
ldr x0, [sp], #16
cmp x0, x1
b.ge .Lfor_loop_end6
sub x0, x29, #8
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #12
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
mov x1, x0
sub x0, x29, #20
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
sub x0, x29, #12
ldrsw x0, [x0]
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
mov x1, x0
sub x0, x29, #24
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
sub x0, x29, #20
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #24
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
cmp x0, x1
b.le .Lif_10
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
mov x1, x0
sub x0, x29, #16
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
sub x0, x29, #12
ldrsw x0, [x0]
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #16
ldrsw x0, [x0]
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
sub x0, x29, #12
ldrsw x0, [x0]
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
b .Lif_end_9
.Lif_10:
.Lif_end_9:
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
b .Lfor_loop5
.Lfor_loop_end6:
.Lexit_point_8:
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
b .Lfor_loop1
.Lfor_loop_end2:
.Lexit_point_4:
mov x0, #0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
add sp, sp, #32
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size bubble_sort, .-bubble_sort
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
mov x0, #5
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
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
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
mov x0, #1
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #8
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
mov x0, #2
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #1
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
mov x0, #3
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #7
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
mov x0, #4
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #3
str x0, [sp, #-16]!
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
mov x0, #5
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #6
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, bubble_sort
add x9, x9, :lo12:bubble_sort
blr x9
sxtw x0, w0
adrp x0, values
add x0, x0, :lo12:values
str x0, [sp, #-16]!
mov x0, #0
mov x12, #4
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
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
