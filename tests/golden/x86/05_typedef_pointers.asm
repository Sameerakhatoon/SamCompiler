section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push dword 0
pop eax
mov dword [ebp-4], eax
lea ebx, [ebp-4]
push ebx
pop ebx
; PUSH ADDRESS &
push ebx
pop eax
mov dword [ebp-8], eax
push dword 99
push dword [ebp-8]
; INDIRECTION
pop ebx
push ebx
pop edx
pop eax
mov dword [edx], eax
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
