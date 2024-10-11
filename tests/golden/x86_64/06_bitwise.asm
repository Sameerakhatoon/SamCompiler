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
mov rax, 90
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 15
and eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 255
xor eax, ecx
movsxd rax, eax
mov rcx, 48
and eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
or eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
