section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
push dword 4
push dword 3
pop ecx
pop eax
add eax, ecx
push eax
pop eax
pop ebp
ret
pop ebp
ret
section .data
section .rodata
