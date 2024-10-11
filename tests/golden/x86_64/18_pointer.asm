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
xor eax, eax
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov qword [rcx], rax
mov rax, 7
push rax
lea rax, [rbp-12]
mov rax, qword [rax]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
