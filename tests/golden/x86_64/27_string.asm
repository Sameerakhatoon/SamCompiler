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
sub rsp, 16
lea rax, [str_1]
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-8]
mov rax, qword [rax]
push rax
mov rax, 1
pop rcx
add rax, rcx
movsx rax, byte [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
str_1: db 'h', 'i', 0
section .note.GNU-stack noalloc noexec nowrite progbits
