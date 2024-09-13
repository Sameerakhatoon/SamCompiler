default rel
section .data
section .text
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
mov rax, 8
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
