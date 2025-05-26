	bits 16
	org 7c00h
	
	mov ax, cs
	mov es, ax
	mov ds, ax
	mov ss, ax

	; read the second section to memory
	mov ax,0201h    ;02:read;01:sections
	mov cx,2        ;cylinder
	mov dh,0
	mov dl,0        ;driver number
	mov bx,0x7E00   ;es:bx destination
	int 13h

	; ready gdt
	mov cx, (_gdt_end - _gdt0) >> 1
	mov si,_gdt0
	mov di,500h
	rep movsw

	; ready idt
	mov dx,256
	mov di,600h
_loop1:
	mov cx,8>>1
	mov si,_idtx
	rep movsw
	dec dx
	jnz _loop1

	mov cx,8>>1
	mov si,_idtx
	mov di,600h + (6*8);
	rep movsw
	mov cx,8>>1
	mov si,_idt8
	mov di,600h + (8*8);
	rep movsw
	mov cx,8>>1
	mov si,_idtd
	mov di,600h + (0x0d*8);
	rep movsw

	cli
	in al,92h
	or al,0x02
	out 92h,al
	lidt [_lidt]
	lgdt [_lgdt]
	mov eax,cr0;  // PG 00 PE
	or eax,0x01
	mov cr0,eax
	jmp 08h:_prot

	bits 32
_prot:
	mov ax,_gdt2 - _gdt0
	mov ds,ax
	mov es,ax
	mov ss,ax
	mov esp,0x1FFFF
	sti

	mov ax, _tss_dt - _gdt0
	ltr ax

	mov esp,0xFFFF
	push 0x23
	push esp
	push 0x1b
	push _prot_user
	retf

_prot_user:
	mov ax,0x23
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov eax,_puts_user
	mov ecx,_puts_user_end - _puts_user
	call _puts
	;call 0x30 : _prot_kernel;
	;jmp 0x30:_prot_kernel;
	call 0x2b : _prot_kernel
	call _prot_user3;
	jmp $
_puts_user:
	db 'in 3-ring'
_puts_user_end:

_prot_user2:
	mov ax,0x23
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov eax,_puts_user2
	mov ecx,_puts_user_end2 - _puts_user2
	call _puts
	jmp $
_puts_user2:
	db 'in 3-ring again'
_puts_user_end2:

_prot_user3:
	mov ax,0x23
	mov ds, ax
	mov es, ax
	mov eax,_puts_user3
	mov ecx,_puts_user_end3 - _puts_user3
	call _puts
	ret
_puts_user3:
	db 'in 3-ring AAA'
_puts_user_end3:

_prot_kernel:
	mov ax,0x10
	mov ds, ax
	mov es, ax
	mov eax,_puts_kernel
	mov ecx,_puts_kernel_end - _puts_kernel
	call _puts
	mov al,0x0a
	call _putc
	
	mov dword[_puts_ker_buff], esp
	mov eax, _puts_ker_buff
	mov ecx, 4
	call _put_mem
	mov al,0x0a
	call _putc
	mov eax,esp
	;mov dword[_puts_ker_buff],eax
	;mov eax, _puts_ker_buff
	mov ecx, 20
	call _put_mem

	;mov dword [esp],_prot_user2
	;push 0x23
	;push esp
	;push 0x1b
	;push _prot_user2
	
	retf
_puts_kernel:
	db 'in 0-ring'
_puts_kernel_end:
_puts_ker_buff:
	dd 0

_prot_icall:
	mov eax,_puts_comm
	mov ecx,_puts_comm_end - _puts_comm
	call _puts
	iret

_prot_icall8:
	mov al,0x0a
	call _putc
	mov al,0x38
	call _putc
	iret

_prot_icalld:
	mov eax,_puts_gp
	mov ecx,_puts_gp_end - _puts_gp
	call _puts
	iret

_puts_comm:
	db "common interrupt"
_puts_comm_end:
_puts_gp:
	db 'gp exception'
_puts_gp_end:

_paddings_:
  times 510-($-$$) db 0
	db 55h,0aah

_section2:
; eax=string, ecx=length
_put_mem:
	mov dword [_put_mem_buf], eax
	mov dword [_put_mem_buf+4], ecx
_put_mem_1:
	mov ebx, dword [_put_mem_buf]
	mov al, byte [ebx]
	call _putc
	inc byte [_putc_pos+1]
	inc dword [_put_mem_buf]
	dec dword [_put_mem_buf+4]
	jnz short _put_mem_1
	ret
_put_mem_buf:
	dd 0,0

; al=char
_putc:
	cmp al,0x0a
	jz _putc_4
	mov dl,al
	mov edi,0xb8000
	xor ax,ax
	mov al,160
	mul byte [_putc_pos]
	xor ebx,ebx
	mov bl,[_putc_pos+1]
	shl bl,1
	add bx,ax
	mov byte [_putc_pos+2],2
	mov al,dl
_putc_1:
	ror al,4
	mov dl,al
	and al,0x0f
	cmp al,0x0a
	jb short _putc_2
	add al, 'A'-0x0a
	jmp short _putc_3
_putc_2:
	add al,'0'
_putc_3:
	mov byte [ebx + edi],al
	add bx,2
	mov al,dl
	dec byte[_putc_pos+2]
	jnz _putc_1
	add byte [_putc_pos+1],2
	ret
_putc_4:
	inc byte [_putc_pos]
	mov byte [_putc_pos+1], 0
	ret
_putc_pos:
	db 1,0,2

;eax=string,ecx=string length
_puts:
	mov esi,eax
	inc byte [_putc_pos]
	mov byte [_putc_pos+1], 0
	xor eax,eax
	mov al,160
	mul byte [_putc_pos]
	xor ebx,ebx
	mov bl,[_putc_pos+1]
	shl bl,1
	add ebx,eax
	mov edi,0xb8000
	add edi,ebx
_puts_1:
	movsb
	inc edi
	loop _puts_1
	ret

_gdt0:
	times 8 db 0
_gdt1:;  first code segment  0x08
	_gdt1_limit     dw 0xFFFF
	_gdt1_base_lo   dw 0
	_gdt1_base_m    db 0
	_gdt1_attrib_lo db 0x9A; P DPL S TYPE
	_gdt1_attrib_hi db 0xCF; G D 0 AVL limit high(16-19)
	_gdt1_base_hi   db 0;    base (24,31)
_gdt2:;  first data segment 0x10
	_gdt2_limit     dw 0xFFFF
	_gdt2_base_lo   dw 0
	_gdt2_base_m    db 0
	_gdt2_attrib_lo db 0x92; P DPL S TYPE
	_gdt2_attrib_hi db 0xCF; G D 0 AVL limit high(16-19)
	_gdt2_base_hi   db 0;    base (24,31)
_gdt3:; first user code segment 0x1b
	_gdt3_limit     dw 0xFFFF
	_gdt3_base_lo   dw 0
	_gdt3_base_m    db 0
	_gdt3_attrib_lo db 0xfA; P DPL S TYPE
	_gdt3_attrib_hi db 0xCF; G D 0 AVL limit high(16-19)
	_gdt3_base_hi   db 0;    base (24,31)
_gdt4:; first user data segment 0x23
	_gdt4_limit     dw 0xFFFF
	_gdt4_base_lo   dw 0
	_gdt4_base_m    db 0
	_gdt4_attrib_lo db 0xf2; P DPL S TYPE
	_gdt4_attrib_hi db 0xCF; G D 0 AVL limit high(16-19)
	_gdt4_base_hi   db 0;    base (24,31)
_gdt5:; user call gate 0x2b or 0x28
	_gdt5_offset_lo dw _prot_kernel
	_gdt5_selector  dw 0x30
	_gdt5_attrib_lo db 0; DWORD COUNT,not used
	_gdt5_attrib_hi db 0xEC; P DPL S TYPE
	_gdt5_offset_hi dw 0;
_gdt6:; call gate code segment 0x33 or 0x30
	_gdt6_limit     dw 0xFFFF
	_gdt6_base_lo   dw 0
	_gdt6_base_m    db 0
	_gdt6_attrib_lo db 0x9A; P DPL S TYPE
	_gdt6_attrib_hi db 0xCF; G D 0 AVL limit high(16-19)
	_gdt6_base_hi   db 0;    base (24,31)
_tss_dt:
	_tss_dt_limit     dw (_tss_end - _tss) - 1
	_tss_dt_base_lo   dw _tss
	_tss_dt_base_m    db 0
	_tss_dt_attrib_lo db 0xE9; P DPL S TYPE (TYPE:1 0 B 1)
	_tss_dt_attrib_hi db 0x40; G D 0 AVL limit high(16-19)
	_tss_dt_base_hi   db 0;    base (24,31)
_gdt_end:
_lgdt:
	_lgdt_limit dw (_gdt_end-_gdt0-1)
	_lgdt_base dd 500h
	
_idtx:
	_idtx_offset_lo dw _prot_icall
	_idtx_selector  dw (_gdt1 - _gdt0)
	_idtx_attrib_lo db 0; DWORD COUNT,not used
	_idtx_attrib_hi db 0x8E; P DPL S TYPE
	_idtx_offset_hi dw 0;
_idt8:
	_idt8_offset_lo dw _prot_icall8
	_idt8_selector  dw (_gdt1 - _gdt0)
	_idt8_attrib_lo db 0; DWORD COUNT
	_idt8_attrib_hi db 0x8E; P DPL S TYPE
	_idt8_offset_hi dw 0;
_idtd:
	_idtd_offset_lo dw _prot_icalld
	_idtd_selector  dw (_gdt1 - _gdt0)
	_idtd_attrib_lo db 0; DWORD COUNT
	_idtd_attrib_hi db 0x8E; P DPL S TYPE
	_idtd_offset_hi dw 0;

_lidt:
	_lidt_limit dw 2047
	_lidt_base dd 600h

_tss:
	dd 0;         back
	dd 0x2FFFF;   0-level stack pointer
	dw 0x10,0;    0-level stack selector
	dw 0, 0;      1-level
	dw 0, 0;
	dw 0, 0;      2-level
	dw 0, 0;
	dd 0;   cr3
	dd 0;   eip
	dd 0;   eflags
	dd 0;   eax
	dd 0;   ecx
	dd 0;   edx
	dd 0;   ebx
	dd 0;   esp
	dd 0;   ebp
	dd 0;   esi
	dd 0;   edi
	dd 0;   es
	dd 0;   cs
	dd 0;   ss
	dd 0;   ds
	dd 0;   fs
	dd 0;   gs
	dd 0;   ldt
	dw 0;
	dw $+2
	db 0xff;   I/O
_tss_end:

_paddings2_:
	times 512-($-_section2) db 0
