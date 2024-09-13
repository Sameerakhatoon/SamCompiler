default rel
section .data
section .text
extern $printf
global $main
; main function
$main:
pop r11
push r9
push r8
push rcx
push rdx
push rsi
push rdi
push r11
push rbp
mov rbp, rsp
lea rax, [str_1]
push rax
mov rax, 42
push rax
lea rax, [str_2]
push rax
pop rdi
pop rsi
pop rdx
lea r11, [$printf]
xor eax, eax
call r11
movsxd rax, eax
xor eax, eax
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
str_1: db 'h', 'i', 0
str_2: db 'v', '=', '%', 'd', ' ', 's', '=', '%', 's', 10, 0
