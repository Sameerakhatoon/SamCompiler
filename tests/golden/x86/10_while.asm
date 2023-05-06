section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 0
pop eax
mov dword [ebp-4], eax
push dword 0
pop eax
mov dword [ebp-8], eax
.entry_point_1:
.while_start_3:
push dword [ebp-4]
push dword 6
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_4
push dword [ebp-8]
push dword [ebp-4]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
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
