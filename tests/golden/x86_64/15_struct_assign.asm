default rel
section .data
; pt a
global $a
$a: dq 0
; pt b
global $b
$b: dq 0
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
push rax
lea rax, [$b]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 11
push rax
lea rax, [$b]
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [$b]
mov rcx, rax
lea rax, [$a]
mov r11, qword [rcx+0]
mov qword [rax+0], r11
lea rax, [$a]
movsxd rax, dword [rax]
push rax
lea rax, [$a]
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
section .data
section .rodata
