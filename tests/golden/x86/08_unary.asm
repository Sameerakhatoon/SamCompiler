section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push dword 5
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
pop eax
neg eax
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
pop eax
not eax
push eax
pop eax
mov dword [ebp-12], eax
push dword [ebp-8]
push dword [ebp-12]
pop ecx
pop eax
add eax, ecx
push eax
push dword 20
pop ecx
pop eax
add eax, ecx
push eax
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
