.data
# cell values
.globl values
.balign 8
values: .zero 24
.text
.p2align 2
.globl bubble_sort
.type bubble_sort, @function
# bubble_sort function
bubble_sort:
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
addi sp, sp, -32
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
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -20
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -24
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
mv a1, a0
addi a0, s0, -4
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lfor_loop1
.Lentry_point_3:
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
.Lfor_loop1:
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, 16
lw a0, 0(a0)
li a1, 1
subw a0, a0, a1
sext.w a0, a0
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
blt a0, a1, .Ltrue_1
j .Lfor_loop_end2
.Ltrue_1:
li a0, 0
mv a1, a0
addi a0, s0, -8
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
j .Lfor_loop5
.Lentry_point_7:
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
.Lfor_loop5:
addi a0, s0, -8
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, 16
lw a0, 0(a0)
li a1, 1
subw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -4
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
subw a0, a0, a1
sext.w a0, a0
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
blt a0, a1, .Ltrue_2
j .Lfor_loop_end6
.Ltrue_2:
addi a0, s0, -8
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
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
mv a1, a0
addi a0, s0, -20
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
lw a0, 0(a0)
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
mv a1, a0
addi a0, s0, -24
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
addi a0, s0, -20
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -24
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
blt a1, a0, .Ltrue_3
j .Lif_10
.Ltrue_3:
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
mv a1, a0
addi a0, s0, -16
mv t3, a0
mv a0, a1
mv a1, t3
sw a0, 0(a1)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
lw a0, 0(a0)
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -16
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
lw a0, 0(a0)
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
j .Lif_end_9
.Lif_10:
.Lif_end_9:
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
j .Lfor_loop5
.Lfor_loop_end6:
.Lexit_point_8:
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
j .Lfor_loop1
.Lfor_loop_end2:
.Lexit_point_4:
li a0, 0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
addi sp, sp, 32
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size bubble_sort, .-bubble_sort
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
li a0, 5
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 0
li t3, 4
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
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 1
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 8
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 3
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 7
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 4
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 3
addi sp, sp, -16
sd a0, 0(sp)
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 5
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 6
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
lla t0, bubble_sort
jalr t0
sext.w a0, a0
lla a0, values
addi sp, sp, -16
sd a0, 0(sp)
li a0, 0
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
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
.section .note.GNU-stack,"",@progbits
