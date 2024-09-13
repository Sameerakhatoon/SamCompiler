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
mov rax, 42
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 21
push rax
mov rax, 21
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
