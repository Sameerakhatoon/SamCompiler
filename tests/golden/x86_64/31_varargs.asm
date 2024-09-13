default rel
section .data
section .text
global $sum2
; sum2 function
$sum2:
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
lea rax, [rbp+16]
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-8]
push rax
mov rax, qword [rax]
add rax, 8
pop rcx
mov qword [rcx], rax
mov rax, qword [rax]
movsxd rax, eax
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
push rax
mov rax, qword [rax]
add rax, 8
pop rcx
mov qword [rcx], rax
mov rax, qword [rax]
movsxd rax, eax
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-12]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-16]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
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
mov rax, 20
push rax
mov rax, 10
push rax
mov rax, 2
push rax
pop rdi
pop rsi
pop rdx
lea r11, [$sum2]
xor eax, eax
call r11
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
