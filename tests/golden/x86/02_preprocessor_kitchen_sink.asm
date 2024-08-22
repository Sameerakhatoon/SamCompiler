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
push dword 42
pop eax
mov dword [ebp-8], eax
push dword 21
push dword 21
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
push dword [ebp-8]
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
