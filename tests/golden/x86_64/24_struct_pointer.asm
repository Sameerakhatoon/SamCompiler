default rel
section .data
section .text
global $sum
; sum function
$sum:
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
lea rax, [rbp+16]
mov rax, qword [rax]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
mov rax, qword [rax]
add rax, 4
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
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
mov rax, 5
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 10
push rax
lea rax, [rbp-8]
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
push rax
pop rdi
lea r11, [$sum]
xor eax, eax
call r11
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
