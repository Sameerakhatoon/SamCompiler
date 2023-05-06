section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 32
push dword 1
pop eax
mov dword [ebp-12], eax
push dword 2
pop eax
mov dword [ebp-8], eax
push dword 3
pop eax
mov dword [ebp-4], eax
push dword [ebp-12]
pop eax
push eax
pop eax
mov dword [ebp-16], eax
push dword [ebp-8]
pop eax
push eax
pop eax
mov dword [ebp-20], eax
push dword [ebp-4]
pop eax
push eax
pop eax
mov dword [ebp-24], eax
push dword [ebp-16]
push dword [ebp-20]
push dword [ebp-24]
pop ecx
pop eax
add eax, ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
add esp, 32
pop ebp
ret
add esp, 32
pop ebp
ret
section .data
section .rodata
