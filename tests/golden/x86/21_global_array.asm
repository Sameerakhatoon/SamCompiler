section .data
; int g
g: times 16 db  0
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 4
pop eax
mov dword [g+4], eax
push dword 5
pop eax
mov dword [g+12], eax
push dword [g+4]
pop eax
push eax
pop eax
mov dword [ebp-4], eax
push dword [g+12]
pop eax
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword [ebp-8]
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
