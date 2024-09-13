.data
.text
.p2align 2
# extern printf
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
lla a0, str_1
addi sp, sp, -16
sd a0, 0(sp)
li a0, 42
addi sp, sp, -16
sd a0, 0(sp)
lla a0, str_2
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
ld a1, 0(sp)
addi sp, sp, 16
ld a2, 0(sp)
addi sp, sp, 16
lla t0, printf
jalr t0
sext.w a0, a0
li a0, 0
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
str_1: .asciz "hi"
str_2: .asciz "v=%d s=%s\012"
