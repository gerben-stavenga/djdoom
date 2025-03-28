;
; Copyright (C) 1993-1996 Id Software, Inc.
; Copyright (C) 2023 Frenkel Smeijers
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version 2
; of the License, or (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program. If not, see <https://www.gnu.org/licenses/>.
;

cpu 386

PLANEWIDTH		equ	80
SCREENHEIGHT	equ 200

%ifidn __OUTPUT_FORMAT__, coff
section .text public class=CODE USE32
%elifidn __OUTPUT_FORMAT__, obj
section _TEXT public class=CODE USE32
%endif

%assign ROW SCREENHEIGHT
%rep SCREENHEIGHT / 2
	lea ecx, [edx + ebx]
	shr edx, 25
	mov al, [esi + edx]
	mov al, [eax]
	mov [edi - PLANEWIDTH * ROW], al
	lea edx, [ecx + ebx]
	shr ecx, 25
	mov al, [esi + ecx]
	mov al, [eax]
	mov [edi - PLANEWIDTH * ROW + PLANEWIDTH], al
	%assign ROW ROW - 2
 %endrep
global _R_ScaleColumnAsm
_R_ScaleColumnAsm:
	ret

%assign COL PLANEWIDTH
%rep PLANEWIDTH / 2
	lea ecx, [edx + ebx]
	shr edx, 26
	shld dx, cx, 6
	mov al, [esi + edx]
	mov al, [eax]
	mov [edi - COL], al
	lea edx, [ecx + ebx]
	shr ecx, 26
	shld cx, dx, 6
	mov al, [esi + ecx]
	mov al, [eax]
	mov [edi - COL + 1], al
	%assign COL COL - 2
 %endrep
global _R_ScaleRowAsm
_R_ScaleRowAsm:
	ret
