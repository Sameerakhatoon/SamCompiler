section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 90
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
push dword 15
pop ecx
pop eax
and eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword 255
pop ecx
pop eax
xor eax, ecx
push eax
push dword 48
pop ecx
pop eax
and eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
push dword [ebp-8]
push dword [ebp-12]
pop ecx
pop eax
or eax, ecx
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
