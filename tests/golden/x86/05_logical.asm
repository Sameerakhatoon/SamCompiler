section .data
section .text
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 32
push dword 1
pop eax
mov dword [ebp-4], eax
push dword 0
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
pop eax
cmp eax, 0
je .endc_1
push dword [ebp-8]
pop eax
cmp eax, 0
je .endc_1
; && END CLAUSE
mov eax, 1
jmp .endc_1_positive
.endc_1:
xor eax, eax
.endc_1_positive:
push eax
pop eax
mov dword [ebp-12], eax
push dword [ebp-4]
pop eax
cmp eax, 0
jg .endc_2_positive
push dword [ebp-8]
pop eax
cmp eax, 0
jg .endc_2_positive
; || END CLAUSE
jmp .endc_2
.endc_2_positive:
mov eax, 1
.endc_2:
push eax
pop eax
mov dword [ebp-16], eax
push dword [ebp-8]
pop eax
cmp eax, 0
sete al
movzx eax, al
push eax
pop eax
mov dword [ebp-20], eax
push dword [ebp-12]
push dword [ebp-16]
push dword 2
pop ecx
pop eax
mov ecx, ecx
imul ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
push dword [ebp-20]
push dword 4
pop ecx
pop eax
mov ecx, ecx
imul ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
add esp, 32
pop ebp
ret
add esp, 32
pop ebp
ret
section .data
section .rodata
