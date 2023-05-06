section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 4
pop eax
mov dword [ebp-8], eax
push dword 7
pop eax
mov dword [ebp-4], eax
push dword [ebp-8]
pop eax
push eax
push dword [ebp-4]
pop eax
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
