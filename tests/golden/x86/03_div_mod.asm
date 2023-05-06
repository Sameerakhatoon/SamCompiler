section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 47
pop eax
mov dword [ebp-4], eax
push dword 5
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
mov ecx, ecx
cdq
idiv ecx
push eax
pop eax
mov dword [ebp-12], eax
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
mov ecx, ecx
cdq
idiv ecx
mov eax, edx
push eax
pop eax
mov dword [ebp-16], eax
push dword [ebp-12]
push dword [ebp-16]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
section .data
section .rodata
