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
sub rsp, 32
mov rax, 1000
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov qword [rcx], rax
mov rax, 950
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-8]
mov rax, qword [rax]
push rax
lea rax, [rbp-16]
mov rax, qword [rax]
mov rcx, rax
pop rax
sub rax, rcx
push rax
lea rax, [rbp-24]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-24]
mov rax, qword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 32
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
