section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push dword 300
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
; CAST
pop eax
push eax
pop eax
movsx eax, al
push eax
pop eax
mov byte [ebp-5], al
mov eax, [ebp-5]
movsx eax, al
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
