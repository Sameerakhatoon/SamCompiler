.data
.text
.p2align 2
.globl main
.type main, @function
# main function
main:
addi sp, sp, -64
addi sp, sp, -16
sd ra, 8(sp)
sd s0, 0(sp)
mv s0, sp
sd a0, 16(s0)
sd a1, 24(s0)
sd a2, 32(s0)
sd a3, 40(s0)
sd a4, 48(s0)
sd a5, 56(s0)
sd a6, 64(s0)
sd a7, 72(s0)
addi sp, sp, -16
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
.Ldo_while_start_3:
.Lentry_point_4:
.Lswitch_stmt_6:
addi a0, s0, -8
lw a0, 0(a0)
li a1, 1
beq a0, a1, .Lswitch_stmt_6_case_1
li a1, 2
beq a0, a1, .Lswitch_stmt_6_case_2
li a1, 3
beq a0, a1, .Lswitch_stmt_6_case_3
li a1, 4
beq a0, a1, .Lswitch_stmt_6_case_4
li a1, 5
beq a0, a1, .Lswitch_stmt_6_case_5
li a1, 6
beq a0, a1, .Lswitch_stmt_6_case_6
j .Lexit_point_5
.Lswitch_stmt_6_case_1:
# CASE 1
addi a0, s0, -4
lw a0, 0(a0)
li a1, 1
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lexit_point_5
.Lswitch_stmt_6_case_2:
# CASE 2
addi a0, s0, -4
lw a0, 0(a0)
li a1, 4
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lexit_point_5
.Lswitch_stmt_6_case_3:
# CASE 3
addi a0, s0, -4
lw a0, 0(a0)
li a1, 9
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lexit_point_5
.Lswitch_stmt_6_case_4:
# CASE 4
addi a0, s0, -4
lw a0, 0(a0)
li a1, 4
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lexit_point_5
.Lswitch_stmt_6_case_5:
# CASE 5
addi a0, s0, -4
lw a0, 0(a0)
li a1, 5
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lexit_point_5
.Lswitch_stmt_6_case_6:
# CASE 6
addi a0, s0, -4
lw a0, 0(a0)
li a1, 6
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lexit_point_5
.Lswitch_stmt_6_end:
.Lexit_point_5:
addi a0, s0, -8
lw a0, 0(a0)
li a1, 1
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -8
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
.Lentry_point_1:
addi a0, s0, -8
lw a0, 0(a0)
li a1, 6
slt a0, a1, a0
xori a0, a0, 1
bnez a0, .Ldo_while_start_3
.Lexit_point_2:
addi a0, s0, -4
lw a0, 0(a0)
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
addi sp, sp, 16
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
