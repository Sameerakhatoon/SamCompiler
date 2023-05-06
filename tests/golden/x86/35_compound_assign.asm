section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 5
pop eax
mov dword [ebp-4], eax
push dword 10
pop eax
add dword [ebp-4], eax
push dword [ebp-4]
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
section .data
section .rodata
