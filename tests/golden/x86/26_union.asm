section .data
; u g
global $g
$g: dd 0
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
push ebx
push dword 321
pop eax
mov dword [$g], eax
push dword [$g]
pop eax
movsx eax, al
push eax
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
