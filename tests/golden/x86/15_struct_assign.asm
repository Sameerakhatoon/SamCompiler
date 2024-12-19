extern _GLOBAL_OFFSET_TABLE_
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
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
push dword 8
pop eax
mov dword [esi+$b wrt ..gotoff], eax
push dword 11
pop eax
mov dword [esi+$b+4 wrt ..gotoff], eax
lea ebx, [esi+$b wrt ..gotoff]
push ebx
pop ebx
; STRUCTURE PUSH
push dword [ebx+4]
push dword [ebx+0]
; END STRUCTURE PUSH
pop eax
mov [esi+$a+0 wrt ..gotoff], eax
pop eax
mov [esi+$a+4 wrt ..gotoff], eax
push dword [esi+$a wrt ..gotoff]
pop eax
push eax
push dword [esi+$a+4 wrt ..gotoff]
pop eax
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov ebx, [ebp-4]
mov esi, [ebp-8]
mov edi, [ebp-12]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
pop ebp
ret
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
