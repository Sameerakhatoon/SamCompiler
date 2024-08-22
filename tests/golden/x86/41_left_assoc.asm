section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 48
push ebx
push dword 100
pop eax
mov dword [ebp-4], eax
push dword 30
pop eax
mov dword [ebp-8], eax
push dword 20
pop eax
mov dword [ebp-12], eax
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
sub eax, ecx
push eax
push dword [ebp-12]
pop ecx
pop eax
sub eax, ecx
push eax
pop eax
mov dword [ebp-16], eax
push dword 64
pop eax
mov dword [ebp-20], eax
push dword [ebp-20]
push dword 4
pop ecx
pop eax
mov ecx, ecx
cdq
idiv ecx
push eax
push dword 2
pop ecx
pop eax
mov ecx, ecx
cdq
idiv ecx
push eax
pop eax
mov dword [ebp-24], eax
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
mov ecx, ecx
cdq
idiv ecx
mov eax, edx
push eax
push dword 7
pop ecx
pop eax
mov ecx, ecx
cdq
idiv ecx
mov eax, edx
push eax
pop eax
mov dword [ebp-28], eax
push dword 1
pop eax
mov dword [ebp-32], eax
push dword [ebp-32]
push dword 2
pop ecx
pop eax
sal eax, cl
push eax
push dword 3
pop ecx
pop eax
sal eax, cl
push eax
pop eax
mov dword [ebp-36], eax
push dword 256
pop eax
mov dword [ebp-40], eax
push dword [ebp-40]
push dword 2
pop ecx
pop eax
sar eax, cl
push eax
push dword 3
pop ecx
pop eax
sar eax, cl
push eax
pop eax
mov dword [ebp-44], eax
push dword [ebp-16]
push dword [ebp-24]
pop ecx
pop eax
add eax, ecx
push eax
push dword [ebp-28]
pop ecx
pop eax
add eax, ecx
push eax
push dword [ebp-36]
pop ecx
pop eax
add eax, ecx
push eax
push dword [ebp-44]
pop ecx
pop eax
add eax, ecx
push eax
push dword 10
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov ebx, [ebp-52]
mov esp, ebp
pop ebp
ret
pop ebx
add esp, 48
pop ebp
ret
section .data
section .rodata
