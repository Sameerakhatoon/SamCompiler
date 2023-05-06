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
push dword 20
pop ecx
pop eax
add eax, ecx
push eax
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
