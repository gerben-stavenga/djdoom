; SPDX-License-Identifier: GPL-2.0-or-later
; Shared 32-bit cdecl scalers. The backend supplies destination/stride;
; no plane, bank or page selection occurs in these loops.
bits 32
cpu 386
%ifidn __OUTPUT_FORMAT__,coff
%define CSYM(x) _ %+ x
%else
%define CSYM(x) x
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
section .text

%macro PROLOGUE 0
    push ebp
    push ebx
    push esi
    push edi
    mov edi, [esp+20]           ; destination
    mov esi, [esp+32]           ; source
    mov ebx, [esp+36]           ; colormap
%endmacro
%macro EPILOGUE 0
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
%endmacro

%macro COLUMN_PIXEL 1
    mov eax, edx
    shr eax, 16
    and eax, 127                ; ordinary 128-texel column addressing
    movzx eax, byte [esi+eax]
    mov al, [ebx+eax]           ; lighting and player translation are precomposed
%if %1 == 2
    mov ah, al
    mov [edi], ax
%else
    mov [edi], al
%endif
    add edi, [esp+24]           ; row stride
    add edx, [esp+44]           ; texture step
%endmacro

%macro COLUMN_SCALER 1
    PROLOGUE
    mov ecx, [esp+28]           ; pixel count
    mov edx, [esp+40]           ; texture fraction
.batch:
    cmp ecx, 8
    jb .tail
%rep 8
    COLUMN_PIXEL %1
%endrep
    sub ecx, 8
    jmp .batch
.tail:
    test ecx, ecx
    jz .done
.pixel:
    COLUMN_PIXEL %1
    dec ecx
    jnz .pixel
.done:
    EPILOGUE

%endmacro

global CSYM(R_ScaleColumn)
CSYM(R_ScaleColumn):
    COLUMN_SCALER 1

global CSYM(R_ScaleColumnWord)
CSYM(R_ScaleColumnWord):
    COLUMN_SCALER 2

%macro SPAN_PIXEL 1
    mov eax, edx
    shr eax, 10
    and eax, 63                 ; X integer (bits 10..15)
    mov ecx, edx
    shr ecx, 20
    and ecx, 4032               ; Y integer (bits 26..31), times 64
    or eax, ecx
    movzx eax, byte [esi+eax]
    mov al, [ebx+eax]
%if %1 == 2
    mov ah, al
    mov [edi], ax
%else
    mov [edi], al
%endif
    add edi, [esp+24]           ; destination advance
    add edx, ebp                ; packed texture step
%endmacro

%macro SPAN_SCALER 1
    PROLOGUE
    mov edx, [esp+40]           ; packed Y6.10:X6.10 fraction
    mov ebp, [esp+44]           ; packed texture step
.batch:
    cmp dword [esp+28], 8
    jb .tail
%rep 8
    SPAN_PIXEL %1
%endrep
    sub dword [esp+28], 8
    jmp .batch
.tail:
    cmp dword [esp+28], 0
    je .done
.pixel:
    SPAN_PIXEL %1
    dec dword [esp+28]
    jnz .pixel
.done:
    EPILOGUE
%endmacro

global CSYM(R_ScaleSpan)
CSYM(R_ScaleSpan):
    SPAN_SCALER 1

global CSYM(R_ScaleSpanWord)
CSYM(R_ScaleSpanWord):
    SPAN_SCALER 2
