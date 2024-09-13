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
mov rax, 3
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 5
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
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
cmp rax, rcx
jge .if_2
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
jmp .if_end_1
.if_2:
.if_end_1:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
jle .if_4
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, 2
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
jmp .if_end_3
.if_4:
.if_end_3:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
jg .if_6
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, 4
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
jmp .if_end_5
.if_6:
.if_end_5:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
jl .if_8
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, 8
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
jmp .if_end_7
.if_8:
.if_end_7:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
jne .if_10
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, 16
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
jmp .if_end_9
.if_10:
.if_end_9:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
je .if_12
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rcx, 32
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
jmp .if_end_11
.if_12:
.if_end_11:
lea rax, [rbp-12]
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
