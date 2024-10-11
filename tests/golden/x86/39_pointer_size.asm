section .data
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
push ebx
push dword 4
push dword 4
pop ecx
pop eax
add eax, ecx
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
