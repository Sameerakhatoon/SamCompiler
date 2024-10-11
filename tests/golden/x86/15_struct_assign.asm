section .data
; pt a
global $a
$a: dq 0
; pt b
global $b
$b: dq 0
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
push ebx
push dword 8
pop eax
mov dword [$b], eax
push dword 11
pop eax
mov dword [$b+4], eax
lea ebx, [$b]
push ebx
pop ebx
; STRUCTURE PUSH
push dword [ebx+4]
push dword [ebx+0]
; END STRUCTURE PUSH
pop eax
mov [$a+0], eax
pop eax
mov [$a+4], eax
push dword [$a]
pop eax
push eax
push dword [$a+4]
pop eax
push eax
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
