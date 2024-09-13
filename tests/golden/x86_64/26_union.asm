default rel
section .data
; u g
global $g
$g: dd 0
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
mov rax, 321
push rax
lea rax, [$g]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [$g]
movsx rax, byte [rax]
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
