.data
.text
.p2align 2
.globl simple_hash
.type simple_hash, @function
# simple_hash function
simple_hash:
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
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
.Lentry_point_1:
.Lwhile_start_3:
addi a0, s0, 16
ld a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lbu a0, 0(a0)
li a1, 0
sext.w a0, a0
bne a0, a1, .Ltrue_1
j .Lwhile_end_4
.Ltrue_1:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, 16
ld a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lbu a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sext.w a1, a1
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
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
j .Lwhile_start_3
.Lwhile_end_4:
.Lexit_point_2:
.Lentry_point_5:
.Lwhile_start_7:
addi a0, s0, -4
lw a0, 0(a0)
li a1, 17
bge a0, a1, .Ltrue_2
j .Lwhile_end_8
.Ltrue_2:
addi a0, s0, -4
lw a0, 0(a0)
li a1, 17
subw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lwhile_start_7
.Lwhile_end_8:
.Lexit_point_6:
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
.size simple_hash, .-simple_hash
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
lla a0, str_9
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
lla t0, simple_hash
jalr t0
sext.w a0, a0
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
str_9: .asciz "ag"
.section .note.GNU-stack,"",@progbits
