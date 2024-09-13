default rel
section .data
section .text
global $make
; make function
$make:
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
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp+24]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
mov r11, rax
mov rax, qword [r11+0]
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
sub rsp, 16
sub rsp, 8
mov rax, 7
push rax
mov rax, 2
push rax
pop rdi
pop rsi
sub rsp, 8
lea r11, [$make]
xor eax, eax
call r11
add rsp, 8
mov [rsp], rax
mov rax, rsp
mov rcx, rax
lea rax, [rbp-8]
mov r11, qword [rcx+0]
mov qword [rax+0], r11
add rsp, 8
lea rax, [rbp-8]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
add rax, 4
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
section .data
section .rodata
