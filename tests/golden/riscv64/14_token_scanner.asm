.data
# scanner sc
.globl sc
.balign 8
sc: .zero 24
.text
.p2align 2
.globl is_space
.type is_space, @function
# is_space function
is_space:
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
addi a0, s0, 16
lw a0, 0(a0)
li a1, 32
sub a0, a0, a1
seqz a0, a0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size is_space, .-is_space
.globl scan_step
.type scan_step, @function
# scan_step function
scan_step:
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
lla a0, sc
ld a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 8
lw a0, 0(a0)
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lbu a0, 0(a0)
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -4
lw a0, 0(a0)
li a1, 0
beq a0, a1, .Ltrue_1
j .Lif_2
.Ltrue_1:
li a0, 0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
j .Lif_end_1
.Lif_2:
.Lif_end_1:
.Lentry_point_3:
.Lswitch_stmt_5:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
lla t0, is_space
jalr t0
sext.w a0, a0
li a1, 1
beq a0, a1, .Lswitch_stmt_5_case_1
li a1, 0
beq a0, a1, .Lswitch_stmt_5_case_0
j .Lexit_point_4
.Lswitch_stmt_5_case_1:
# CASE 1
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
j .Lexit_point_4
.Lswitch_stmt_5_case_0:
# CASE 0
lla a0, sc
addi a0, a0, 16
lw a0, 0(a0)
li a1, 0
beq a0, a1, .Ltrue_2
j .Lif_7
.Ltrue_2:
lla a0, sc
addi a0, a0, 12
lw a0, 0(a0)
li a1, 1
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 12
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
j .Lif_end_6
.Lif_7:
.Lif_end_6:
j .Lexit_point_4
.Lswitch_stmt_5_end:
.Lexit_point_4:
lla a0, sc
addi a0, a0, 8
lw a0, 0(a0)
li a1, 1
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
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
.size scan_step, .-scan_step
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
lla a0, str_8
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 12
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
.Lentry_point_9:
.Lwhile_start_11:
lla a0, sc
ld a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, sc
addi a0, a0, 8
lw a0, 0(a0)
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lbu a0, 0(a0)
li a1, 0
sext.w a0, a0
bne a0, a1, .Ltrue_3
j .Lwhile_end_12
.Ltrue_3:
lla t0, scan_step
jalr t0
sext.w a0, a0
j .Lwhile_start_11
.Lwhile_end_12:
.Lexit_point_10:
lla a0, sc
addi a0, a0, 12
lw a0, 0(a0)
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size main, .-main
.data
.section .rodata
str_8: .asciz "one two three four"
