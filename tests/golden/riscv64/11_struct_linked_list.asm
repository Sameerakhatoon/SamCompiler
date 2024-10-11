.data
# node nodes
.globl nodes
.balign 8
nodes: .zero 40
.text
.p2align 2
.globl sum_from
.type sum_from, @function
# sum_from function
sum_from:
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
addi a0, s0, 16
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
.Lentry_point_1:
.Lwhile_start_3:
addi a0, s0, -8
lw a0, 0(a0)
li a1, 0
bge a0, a1, .Ltrue_1
j .Lwhile_end_4
.Ltrue_1:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
addi a0, a0, 4
lw a0, 0(a0)
mv a1, a0
addi a0, s0, -8
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lwhile_start_3
.Lwhile_end_4:
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
.size sum_from, .-sum_from
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
li a0, 10
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 0
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 2
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 0
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
addi a0, a0, 4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 20
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 4
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
addi a0, a0, 4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 30
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 4
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
negw a0, a0
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 4
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
addi a0, a0, 4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 999
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 1
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
negw a0, a0
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 1
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
addi a0, a0, 4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 777
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 3
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
negw a0, a0
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, nodes
addi sp, sp, -16
sd a0, 0(sp)
li a0, 3
li t3, 8
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
addi a0, a0, 4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
lla t0, sum_from
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
.section .note.GNU-stack,"",@progbits
