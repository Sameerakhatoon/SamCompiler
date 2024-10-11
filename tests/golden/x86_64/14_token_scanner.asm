default rel
section .data
; scanner sc
global $sc
$sc: times 24 db  0
section .text
global $is_space
; is_space function
$is_space:
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
movsxd rax, dword [rax]
mov rcx, 32
cmp rax, rcx
sete al
movzx eax, al
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
global $scan_step
; scan_step function
$scan_step:
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
lea rax, [$sc]
mov rax, qword [rax]
push rax
lea rax, [$sc]
add rax, 8
movsxd rax, dword [rax]
pop rcx
add rax, rcx
movsx rax, byte [rax]
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
xor ecx, ecx
cmp rax, rcx
jne .if_2
xor eax, eax
mov rsp, rbp
pop rbp
ret 48
jmp .if_end_1
.if_2:
.if_end_1:
.entry_point_3:
.switch_stmt_5:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
pop rdi
lea r11, [$is_space]
xor eax, eax
call r11
movsxd rax, eax
cmp eax, 1
je .switch_stmt_5_case_1
cmp eax, 0
je .switch_stmt_5_case_0
jmp .exit_point_4
.switch_stmt_5_case_1:
; CASE 1
xor eax, eax
push rax
lea rax, [$sc]
add rax, 16
mov rcx, rax
pop rax
mov dword [rcx], eax
jmp .exit_point_4
.switch_stmt_5_case_0:
; CASE 0
lea rax, [$sc]
add rax, 16
movsxd rax, dword [rax]
xor ecx, ecx
cmp rax, rcx
jne .if_7
lea rax, [$sc]
add rax, 12
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
push rax
lea rax, [$sc]
add rax, 12
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
push rax
lea rax, [$sc]
add rax, 16
mov rcx, rax
pop rax
mov dword [rcx], eax
jmp .if_end_6
.if_7:
.if_end_6:
jmp .exit_point_4
.switch_stmt_5_end:
.exit_point_4:
lea rax, [$sc]
add rax, 8
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
push rax
lea rax, [$sc]
add rax, 8
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
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
lea rax, [str_8]
push rax
lea rax, [$sc]
mov rcx, rax
pop rax
mov qword [rcx], rax
xor eax, eax
push rax
lea rax, [$sc]
add rax, 8
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
lea rax, [$sc]
add rax, 12
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
lea rax, [$sc]
add rax, 16
mov rcx, rax
pop rax
mov dword [rcx], eax
.entry_point_9:
.while_start_11:
lea rax, [$sc]
mov rax, qword [rax]
push rax
lea rax, [$sc]
add rax, 8
movsxd rax, dword [rax]
pop rcx
add rax, rcx
movsx rax, byte [rax]
xor ecx, ecx
cmp rax, rcx
je .while_end_12
lea r11, [$scan_step]
xor eax, eax
call r11
movsxd rax, eax
jmp .while_start_11
.while_end_12:
.exit_point_10:
lea rax, [$sc]
add rax, 12
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
str_8: db 'o', 'n', 'e', ' ', 't', 'w', 'o', ' ', 't', 'h', 'r', 'e', 'e', ' ', 'f', 'o', 'u', 'r', 0
section .note.GNU-stack noalloc noexec nowrite progbits
