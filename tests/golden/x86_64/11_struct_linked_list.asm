default rel
section .data
; node nodes
global $nodes
$nodes: times 40 db  0
section .text
global $sum_from
; sum_from function
$sum_from:
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
lea rax, [rbp+16]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
.entry_point_1:
.while_start_3:
lea rax, [rbp-8]
movsxd rax, dword [rax]
xor ecx, ecx
cmp rax, rcx
jl .while_end_4
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [$nodes]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
imul rax, rax, 8
pop rcx
add rax, rcx
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [$nodes]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
imul rax, rax, 8
pop rcx
add rax, rcx
add rax, 4
movsxd rax, dword [rax]
mov rcx, rax
lea rax, [rbp-8]
xchg rax, rcx
mov dword [rcx], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
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
mov rax, 10
push rax
lea rax, [$nodes]
push rax
xor eax, eax
imul rax, rax, 8
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 2
push rax
lea rax, [$nodes]
push rax
xor eax, eax
imul rax, rax, 8
pop rcx
add rax, rcx
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 20
push rax
lea rax, [$nodes]
push rax
mov rax, 2
imul rax, rax, 8
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 4
push rax
lea rax, [$nodes]
push rax
mov rax, 2
imul rax, rax, 8
pop rcx
add rax, rcx
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 30
push rax
lea rax, [$nodes]
push rax
mov rax, 4
imul rax, rax, 8
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
neg eax
movsxd rax, eax
push rax
lea rax, [$nodes]
push rax
mov rax, 4
imul rax, rax, 8
pop rcx
add rax, rcx
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 999
push rax
lea rax, [$nodes]
push rax
mov rax, 1
imul rax, rax, 8
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
neg eax
movsxd rax, eax
push rax
lea rax, [$nodes]
push rax
mov rax, 1
imul rax, rax, 8
pop rcx
add rax, rcx
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 777
push rax
lea rax, [$nodes]
push rax
mov rax, 3
imul rax, rax, 8
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
neg eax
movsxd rax, eax
push rax
lea rax, [$nodes]
push rax
mov rax, 3
imul rax, rax, 8
pop rcx
add rax, rcx
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
pop rdi
lea r11, [$sum_from]
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
