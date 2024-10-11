section .data
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
mov eax, str_1
push eax
pop eax
mov dword [ebp-4], eax
mov ebx, [ebp-4]
push ebx
pop ebx
push dword 1
pop eax
add ebx, eax
push ebx
pop eax
mov eax, [eax]
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
str_1: db 'h', 'i', 0
section .note.GNU-stack noalloc noexec nowrite progbits
