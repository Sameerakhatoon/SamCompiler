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
sub rsp, 48
mov rax, 100
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 30
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 20
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
sub eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
sub eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 64
push rax
lea rax, [rbp-20]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-20]
movsxd rax, dword [rax]
mov rcx, 4
cdq
idiv ecx
movsxd rax, eax
mov rcx, 2
cdq
idiv ecx
movsxd rax, eax
push rax
lea rax, [rbp-24]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cdq
idiv ecx
mov eax, edx
movsxd rax, eax
mov rcx, 7
cdq
idiv ecx
mov eax, edx
movsxd rax, eax
push rax
lea rax, [rbp-28]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
push rax
lea rax, [rbp-32]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-32]
movsxd rax, dword [rax]
mov rcx, 2
shl eax, cl
movsxd rax, eax
mov rcx, 3
shl eax, cl
movsxd rax, eax
push rax
lea rax, [rbp-36]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 256
push rax
lea rax, [rbp-40]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-40]
movsxd rax, dword [rax]
mov rcx, 2
sar eax, cl
movsxd rax, eax
mov rcx, 3
sar eax, cl
movsxd rax, eax
push rax
lea rax, [rbp-44]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-16]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-24]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-28]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-36]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-44]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rcx, 10
add eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 48
pop rbp
ret 48
section .data
section .rodata
