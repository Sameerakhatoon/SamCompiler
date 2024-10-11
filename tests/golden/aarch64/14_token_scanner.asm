.data
// scanner sc
.globl sc
.balign 8
sc: .zero 24
.text
.p2align 2
.globl is_space
.type is_space, %function
// is_space function
is_space:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #32
cmp x0, x1
cset w0, eq
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size is_space, .-is_space
.globl scan_step
.type scan_step, %function
// scan_step function
scan_step:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
sub sp, sp, #16
adrp x0, sc
add x0, x0, :lo12:sc
ldr x0, [x0]
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #8
ldrsw x0, [x0]
ldr x1, [sp], #16
add x0, x0, x1
ldrb w0, [x0]
sxtw x0, w0
str x0, [sp, #-16]!
sub x0, x29, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #0
cmp x0, x1
b.ne .Lif_2
mov x0, #0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
b .Lif_end_1
.Lif_2:
.Lif_end_1:
.Lentry_point_3:
.Lswitch_stmt_5:
sub x0, x29, #4
ldrsw x0, [x0]
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, is_space
add x9, x9, :lo12:is_space
blr x9
sxtw x0, w0
mov w1, #1
cmp w0, w1
b.eq .Lswitch_stmt_5_case_1
mov w1, #0
cmp w0, w1
b.eq .Lswitch_stmt_5_case_0
b .Lexit_point_4
.Lswitch_stmt_5_case_1:
// CASE 1
mov x0, #0
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
b .Lexit_point_4
.Lswitch_stmt_5_case_0:
// CASE 0
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #16
ldrsw x0, [x0]
mov x1, #0
cmp x0, x1
b.ne .Lif_7
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #12
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #12
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #1
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
b .Lif_end_6
.Lif_7:
.Lif_end_6:
b .Lexit_point_4
.Lswitch_stmt_5_end:
.Lexit_point_4:
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #8
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
add sp, sp, #16
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size scan_step, .-scan_step
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
adrp x0, str_8
add x0, x0, :lo12:str_8
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
mov x0, #0
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #12
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #16
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
.Lentry_point_9:
.Lwhile_start_11:
adrp x0, sc
add x0, x0, :lo12:sc
ldr x0, [x0]
str x0, [sp, #-16]!
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #8
ldrsw x0, [x0]
ldr x1, [sp], #16
add x0, x0, x1
ldrb w0, [x0]
mov x1, #0
sxtw x0, w0
cmp x0, x1
b.eq .Lwhile_end_12
adrp x9, scan_step
add x9, x9, :lo12:scan_step
blr x9
sxtw x0, w0
b .Lwhile_start_11
.Lwhile_end_12:
.Lexit_point_10:
adrp x0, sc
add x0, x0, :lo12:sc
add x0, x0, #12
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
str_8: .asciz "one two three four"
.section .note.GNU-stack,"",@progbits
