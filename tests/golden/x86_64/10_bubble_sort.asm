default rel
section .data
; cell values
global $values
$values: times 24 db  0
section .text
global $bubble_sort
; bubble_sort function
$bubble_sort:
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
sub rsp, 32
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
xor eax, eax
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
lea rax, [rbp-20]
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
lea rax, [rbp-24]
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .for_loop1
.entry_point_3:
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
.for_loop1:
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 1
sub eax, ecx
movsxd rax, eax
mov rcx, rax
pop rax
cmp rax, rcx
jge .for_loop_end2
xor eax, eax
mov rcx, rax
lea rax, [rbp-8]
xchg rax, rcx
mov dword [rcx], eax
jmp .for_loop5
.entry_point_7:
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-8]
xchg rax, rcx
mov dword [rcx], eax
.for_loop5:
lea rax, [rbp-8]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 1
sub eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
sub eax, ecx
movsxd rax, eax
mov rcx, rax
pop rax
cmp rax, rcx
jge .for_loop_end6
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-12]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [$values]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
mov rcx, rax
lea rax, [rbp-20]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [$values]
push rax
lea rax, [rbp-12]
movsxd rax, dword [rax]
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
mov rcx, rax
lea rax, [rbp-24]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [rbp-20]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-24]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
jle .if_10
lea rax, [$values]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
mov rcx, rax
lea rax, [rbp-16]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [$values]
push rax
lea rax, [rbp-12]
movsxd rax, dword [rax]
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
push rax
lea rax, [$values]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-16]
movsxd rax, dword [rax]
push rax
lea rax, [$values]
push rax
lea rax, [rbp-12]
movsxd rax, dword [rax]
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
jmp .if_end_9
.if_10:
.if_end_9:
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-8]
xchg rax, rcx
mov dword [rcx], eax
jmp .for_loop5
.for_loop_end6:
.exit_point_8:
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .for_loop1
.for_loop_end2:
.exit_point_4:
xor eax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 32
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
mov rax, 5
push rax
lea rax, [$values]
push rax
xor eax, eax
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 2
push rax
lea rax, [$values]
push rax
mov rax, 1
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 8
push rax
lea rax, [$values]
push rax
mov rax, 2
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
push rax
lea rax, [$values]
push rax
mov rax, 3
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 7
push rax
lea rax, [$values]
push rax
mov rax, 4
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 3
push rax
lea rax, [$values]
push rax
mov rax, 5
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 6
push rax
pop rdi
lea r11, [$bubble_sort]
xor eax, eax
call r11
movsxd rax, eax
lea rax, [$values]
push rax
xor eax, eax
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
