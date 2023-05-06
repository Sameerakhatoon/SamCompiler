section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 5
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
push dword 3
pop ecx
pop eax
cmp eax, ecx
setg al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .tenary_false_2
.tenary_true_1:
push dword 8
pop eax
jmp .tenary_end_3
.tenary_false_2:
push dword 1
pop eax
.tenary_end_3:
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-8]
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
section .data
section .rodata
