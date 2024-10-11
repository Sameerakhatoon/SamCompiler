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
li a0, 3
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 5
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
blt a0, a1, .Ltrue_1
j .Lif_2
.Ltrue_1:
addi a0, s0, -12
lw a0, 0(a0)
li a1, 1
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -12
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lif_end_1
.Lif_2:
.Lif_end_1:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
blt a1, a0, .Ltrue_2
j .Lif_4
.Ltrue_2:
addi a0, s0, -12
lw a0, 0(a0)
li a1, 2
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -12
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lif_end_3
.Lif_4:
.Lif_end_3:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
bge a1, a0, .Ltrue_3
j .Lif_6
.Ltrue_3:
addi a0, s0, -12
lw a0, 0(a0)
li a1, 4
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -12
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lif_end_5
.Lif_6:
.Lif_end_5:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
bge a0, a1, .Ltrue_4
j .Lif_8
.Ltrue_4:
addi a0, s0, -12
lw a0, 0(a0)
li a1, 8
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -12
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lif_end_7
.Lif_8:
.Lif_end_7:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
beq a0, a1, .Ltrue_5
j .Lif_10
.Ltrue_5:
addi a0, s0, -12
lw a0, 0(a0)
li a1, 16
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -12
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lif_end_9
.Lif_10:
.Lif_end_9:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
bne a0, a1, .Ltrue_6
j .Lif_12
.Ltrue_6:
addi a0, s0, -12
lw a0, 0(a0)
li a1, 32
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -12
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lif_end_11
.Lif_12:
.Lif_end_11:
addi a0, s0, -12
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
