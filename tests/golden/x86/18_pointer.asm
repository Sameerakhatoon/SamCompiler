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
lea ebx, [ebp-4]
push ebx
pop ebx
; PUSH ADDRESS &
push ebx
pop eax
mov dword [ebp-8], eax
push dword 7
push dword [ebp-8]
; INDIRECTION
pop ebx
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword [ebp-4]
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
