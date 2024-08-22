section .data
section .text
global pick
; pick function
pick:
push ebp
mov ebp, esp
push ebx
push dword [ebp+8]
push dword 1
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_2
push dword 10
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
jmp .if_end_1
.if_2:
push dword [ebp+8]
push dword 2
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_3
push dword 2
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
jmp .if_end_1
.if_3:
push dword 30
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
.if_end_1:
push dword 0
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
global main
; main function
main:
push ebp
mov ebp, esp
push ebx
lea ebx, [pick]
push ebx
pop ebx
mov dword [function_call_4], ebx
push dword 2
call [function_call_4]
add esp, 4
push eax
pop eax
push eax
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
section .data
function_call_4: dd 0
section .rodata
