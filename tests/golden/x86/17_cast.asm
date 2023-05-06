section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
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
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
section .data
section .rodata
