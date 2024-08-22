section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push dword 0
pop eax
mov dword [ebp-4], eax
push dword 1
pop eax
mov dword [ebp-8], eax
push dword 1
pop eax
mov dword [ebp-8], eax
jmp .for_loop1
.entry_point_3:
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
.for_loop1:
push dword [ebp-8]
push dword 5
pop ecx
pop eax
cmp eax, ecx
setle al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .for_loop_end2
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
jmp .for_loop1
.for_loop_end2:
.exit_point_4:
push dword 6
pop eax
mov dword [ebp-8], eax
.entry_point_5:
.while_start_7:
push dword [ebp-8]
push dword 10
pop ecx
pop eax
cmp eax, ecx
setle al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_8
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
jmp .while_start_7
.while_end_8:
.exit_point_6:
push dword [ebp-4]
pop eax
mov ebx, [ebp-20]
mov esp, ebp
pop ebp
ret
pop ebx
add esp, 16
pop ebp
ret
section .data
section .rodata
