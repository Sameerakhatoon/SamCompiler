section .data
; u g
g: dd 0
section .text
global main
; main function
main:
push ebp
mov ebp, esp
push dword 321
pop eax
mov dword [g], eax
push dword [g]
pop eax
movsx eax, al
push eax
pop eax
pop ebp
ret
pop ebp
ret
section .data
section .rodata
