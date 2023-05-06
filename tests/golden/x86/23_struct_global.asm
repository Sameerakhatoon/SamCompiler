section .data
; pt g
g: dq 0
section .text
global main
; main function
main:
push ebp
mov ebp, esp
push dword 6
pop eax
mov dword [g], eax
push dword 7
pop eax
mov dword [g+4], eax
push dword [g]
pop eax
push eax
push dword [g+4]
pop eax
push eax
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
