default rel
section .data
section .text
global $simple_hash
; simple_hash function
$simple_hash:
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
xor eax, eax
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
.entry_point_1:
.while_start_3:
lea rax, [rbp+16]
mov rax, qword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
pop rcx
add rax, rcx
movsx rax, byte [rax]
xor ecx, ecx
cmp rax, rcx
je .while_end_4
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
mov rax, qword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
pop rcx
add rax, rcx
movsx rax, byte [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-8]
xchg rax, rcx
mov dword [rcx], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
.entry_point_5:
.while_start_7:
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 17
cmp rax, rcx
jl .while_end_8
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 17
sub eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .while_start_7
.while_end_8:
.exit_point_6:
lea rax, [rbp-4]
movsxd rax, dword [rax]
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
lea rax, [str_9]
push rax
pop rdi
lea r11, [$simple_hash]
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
str_9: db 'a', 'g', 0
