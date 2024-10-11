section .data
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push dword 3
pop eax
mov dword [ebp-4], eax
push dword 5
pop eax
mov dword [ebp-8], eax
push dword 0
pop eax
mov dword [ebp-12], eax
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_2
push dword [ebp-12]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .if_end_1
.if_2:
.if_end_1:
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
cmp eax, ecx
setg al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_4
push dword [ebp-12]
push dword 2
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .if_end_3
.if_4:
.if_end_3:
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
cmp eax, ecx
setle al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_6
push dword [ebp-12]
push dword 4
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .if_end_5
.if_6:
.if_end_5:
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
cmp eax, ecx
setge al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_8
push dword [ebp-12]
push dword 8
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .if_end_7
.if_8:
.if_end_7:
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_10
push dword [ebp-12]
push dword 16
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .if_end_9
.if_10:
.if_end_9:
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
cmp eax, ecx
setne al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_12
push dword [ebp-12]
push dword 32
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .if_end_11
.if_12:
.if_end_11:
push dword [ebp-12]
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
section .note.GNU-stack noalloc noexec nowrite progbits
