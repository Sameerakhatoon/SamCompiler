section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push dword 3
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
push dword 4
pop ecx
pop eax
sal eax, cl
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-8]
push dword 2
pop ecx
pop eax
sar eax, cl
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
