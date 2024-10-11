default rel
section .data
; int g
global $g
$g: dd 20
; char k
global $k
$k: db 100
; char msg
global $msg
$msg: dq $str_1
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
lea rax, [$g]
movsxd rax, dword [rax]
push rax
lea rax, [$k]
movsx rax, byte [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [$msg]
mov rax, qword [rax]
push rax
mov rax, 1
pop rcx
add rax, rcx
movsx rax, byte [rax]
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
str_1: db 'x', 0
section .note.GNU-stack noalloc noexec nowrite progbits
