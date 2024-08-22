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
push dword 4
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword [ebp-8]
push dword 5
pop ecx
pop eax
mov ecx, ecx
imul ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
push dword 2
pop ecx
pop eax
sub eax, ecx
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
