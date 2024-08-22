section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
push ebx
push dword 1
push dword 3
pop ecx
pop eax
add eax, ecx
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
section .rodata
