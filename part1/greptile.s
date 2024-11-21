	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 14, 0	sdk_version 14, 4
	.globl	_parse                          ; -- Begin function parse
	.p2align	2
_parse:                                 ; @parse
	.cfi_startproc
; %bb.0:
	stp	x22, x21, [sp, #-48]!           ; 16-byte Folded Spill
	.cfi_def_cfa_offset 48
	stp	x20, x19, [sp, #16]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #32]             ; 16-byte Folded Spill
	add	x29, sp, #32
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	mov	x21, x2
	mov	x19, x1
	mov	x20, x0
Lloh0:
	adrp	x1, l_.str@PAGE
Lloh1:
	add	x1, x1, l_.str@PAGEOFF
	bl	_strstr
	cbz	x0, LBB0_3
; %bb.1:
	mov	x0, x20
	mov	w1, #99
	bl	_strrchr
	cbz	x0, LBB0_6
; %bb.2:
	mov	x22, x0
	add	x1, x0, #1
	mov	x0, x21
	bl	_strcpy
	strb	wzr, [x22]
	b	LBB0_7
LBB0_3:
	mov	w8, #32
	strh	w8, [x21]
	mov	w8, #46
	strh	w8, [x19]
	mov	x0, x19
	mov	x1, x20
	bl	_strcat
	mov	x0, x20
	bl	_strlen
	add	x8, x0, x20
	ldurb	w8, [x8, #-1]
	cmp	w8, #47
	b.ne	LBB0_5
; %bb.4:
	mov	x0, x19
	bl	_strlen
	add	x8, x19, x0
	strb	wzr, [x8, #8]
Lloh2:
	adrp	x9, l_.str.3@PAGE
Lloh3:
	add	x9, x9, l_.str.3@PAGEOFF
Lloh4:
	ldr	x9, [x9]
	str	x9, [x8]
LBB0_5:
	mov	w0, #1
	b	LBB0_8
LBB0_6:
	mov	w8, #32
	strh	w8, [x21]
LBB0_7:
	strb	wzr, [x19, #2]
	mov	w8, #11808
	strh	w8, [x19]
	mov	x0, x19
	mov	x1, x20
	bl	_strcat
	mov	w0, #0
LBB0_8:
	ldp	x29, x30, [sp, #32]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #16]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp], #48             ; 16-byte Folded Reload
	ret
	.loh AdrpAdd	Lloh0, Lloh1
	.loh AdrpAddLdr	Lloh2, Lloh3, Lloh4
	.cfi_endproc
                                        ; -- End function
	.globl	_search_file                    ; -- Begin function search_file
	.p2align	2
_search_file:                           ; @search_file
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #112
	.cfi_def_cfa_offset 112
	stp	x26, x25, [sp, #32]             ; 16-byte Folded Spill
	stp	x24, x23, [sp, #48]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #64]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #80]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #96]             ; 16-byte Folded Spill
	add	x29, sp, #96
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	.cfi_offset w23, -56
	.cfi_offset w24, -64
	.cfi_offset w25, -72
	.cfi_offset w26, -80
	mov	x19, x1
	mov	x20, x0
Lloh5:
	adrp	x21, _buf@GOTPAGE
Lloh6:
	ldr	x21, [x21, _buf@GOTPAGEOFF]
	mov	x1, x21
	bl	_lstat
	tbnz	w0, #31, LBB1_8
; %bb.1:
Lloh7:
	adrp	x1, l_.str.6@PAGE
Lloh8:
	add	x1, x1, l_.str.6@PAGEOFF
	mov	x0, x20
	bl	_fopen
	cbz	x20, LBB1_9
; %bb.2:
	mov	x22, x0
	ldr	x24, [x21, #96]
	mov	x0, x24
	bl	_malloc
	mov	x23, x0
	mov	x1, x24
	mov	x2, x22
	bl	_fgets
	cbz	x0, LBB1_7
; %bb.3:
	mov	w25, #1
Lloh9:
	adrp	x26, ___stdoutp@GOTPAGE
Lloh10:
	ldr	x26, [x26, ___stdoutp@GOTPAGEOFF]
Lloh11:
	adrp	x24, l_.str.8@PAGE
Lloh12:
	add	x24, x24, l_.str.8@PAGEOFF
	b	LBB1_5
LBB1_4:                                 ;   in Loop: Header=BB1_5 Depth=1
	ldr	w1, [x21, #96]
	mov	x0, x23
	mov	x2, x22
	bl	_fgets
	cbz	x0, LBB1_7
LBB1_5:                                 ; =>This Inner Loop Header: Depth=1
	mov	x0, x23
	mov	x1, x19
	bl	_strstr
	cbz	x0, LBB1_4
; %bb.6:                                ;   in Loop: Header=BB1_5 Depth=1
	ldr	x8, [x26]
	stp	x25, x0, [sp, #8]
	str	x20, [sp]
	mov	x0, x8
	mov	x1, x24
	bl	_fprintf
	add	w25, w25, #1
	b	LBB1_4
LBB1_7:
	mov	x0, x22
	bl	_fclose
	mov	x0, x23
	bl	_free
	mov	w0, #0
	ldp	x29, x30, [sp, #96]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #80]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #64]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #48]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #32]             ; 16-byte Folded Reload
	add	sp, sp, #112
	ret
LBB1_8:
	bl	_search_file.cold.1
LBB1_9:
	bl	_search_file.cold.2
	.loh AdrpLdrGot	Lloh5, Lloh6
	.loh AdrpAdd	Lloh7, Lloh8
	.loh AdrpAdd	Lloh11, Lloh12
	.loh AdrpLdrGot	Lloh9, Lloh10
	.cfi_endproc
                                        ; -- End function
	.globl	_helper_func                    ; -- Begin function helper_func
	.p2align	2
_helper_func:                           ; @helper_func
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #96
	.cfi_def_cfa_offset 96
	stp	x26, x25, [sp, #16]             ; 16-byte Folded Spill
	stp	x24, x23, [sp, #32]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #48]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #64]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #80]             ; 16-byte Folded Spill
	add	x29, sp, #80
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	.cfi_offset w23, -56
	.cfi_offset w24, -64
	.cfi_offset w25, -72
	.cfi_offset w26, -80
	mov	x19, x1
	mov	x20, x0
Lloh13:
	adrp	x21, _buf@GOTPAGE
Lloh14:
	ldr	x21, [x21, _buf@GOTPAGEOFF]
	mov	x1, x21
	bl	_lstat
	tbnz	w0, #31, LBB2_4
; %bb.1:
	ldrh	w8, [x21, #4]
	and	w8, w8, #0xf000
	cmp	w8, #4, lsl #12                 ; =16384
	b.eq	LBB2_8
; %bb.2:
	cmp	w8, #8, lsl #12                 ; =32768
	b.ne	LBB2_6
; %bb.3:
	mov	w0, #0
	adrp	x8, _nreg@PAGE
	ldr	x9, [x8, _nreg@PAGEOFF]
	add	x9, x9, #1
	str	x9, [x8, _nreg@PAGEOFF]
	b	LBB2_7
LBB2_4:
Lloh15:
	adrp	x8, ___stderrp@GOTPAGE
Lloh16:
	ldr	x8, [x8, ___stderrp@GOTPAGEOFF]
Lloh17:
	ldr	x0, [x8]
	mov	w8, #4
	stp	x8, x20, [sp]
Lloh18:
	adrp	x1, l_.str.14@PAGE
Lloh19:
	add	x1, x1, l_.str.14@PAGEOFF
LBB2_5:
	bl	_fprintf
LBB2_6:
	mov	w0, #0
LBB2_7:
	ldp	x29, x30, [sp, #80]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #64]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #48]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #16]             ; 16-byte Folded Reload
	add	sp, sp, #96
	ret
LBB2_8:
	adrp	x8, _ndir@PAGE
	ldr	x9, [x8, _ndir@PAGEOFF]
	add	x9, x9, #1
	str	x9, [x8, _ndir@PAGEOFF]
	adrp	x25, _fullpath@PAGE
	ldr	x20, [x25, _fullpath@PAGEOFF]
	mov	x0, x20
	bl	_strlen
	mov	x21, x0
	mov	w8, #47
	strb	w8, [x20, w21, sxtw]
	mov	x0, x20
	bl	_opendir
	cbz	x0, LBB2_17
; %bb.9:
	mov	x20, x0
	bl	_readdir
	cbz	x0, LBB2_15
; %bb.10:
	lsl	x8, x21, #32
	mov	x9, #4294967296
	add	x8, x8, x9
	asr	x26, x8, #32
Lloh20:
	adrp	x21, l_.str.2@PAGE
Lloh21:
	add	x21, x21, l_.str.2@PAGEOFF
Lloh22:
	adrp	x22, l_.str.9@PAGE
Lloh23:
	add	x22, x22, l_.str.9@PAGEOFF
	b	LBB2_12
LBB2_11:                                ;   in Loop: Header=BB2_12 Depth=1
	mov	x0, x20
	bl	_readdir
	cbz	x0, LBB2_15
LBB2_12:                                ; =>This Inner Loop Header: Depth=1
	add	x23, x0, #21
	mov	x0, x23
	mov	x1, x21
	bl	_strcmp
	cbz	w0, LBB2_11
; %bb.13:                               ;   in Loop: Header=BB2_12 Depth=1
	mov	x0, x23
	mov	x1, x22
	bl	_strcmp
	cbz	w0, LBB2_11
; %bb.14:                               ;   in Loop: Header=BB2_12 Depth=1
	ldr	x24, [x25, _fullpath@PAGEOFF]
	add	x0, x24, x26
	mov	x1, x23
	bl	_strcpy
	mov	x0, x24
	mov	x1, x19
	bl	_search_file
	ldr	x0, [x25, _fullpath@PAGEOFF]
	mov	x1, x19
	bl	_helper_func
	cbz	w0, LBB2_11
LBB2_15:
	mov	x0, x20
	bl	_closedir
	tbz	w0, #31, LBB2_6
; %bb.16:
Lloh24:
	adrp	x8, ___stderrp@GOTPAGE
Lloh25:
	ldr	x8, [x8, ___stderrp@GOTPAGEOFF]
Lloh26:
	ldr	x0, [x8]
	ldr	x8, [x25, _fullpath@PAGEOFF]
	str	x8, [sp]
Lloh27:
	adrp	x1, l_.str.10@PAGE
Lloh28:
	add	x1, x1, l_.str.10@PAGEOFF
	b	LBB2_5
LBB2_17:
	mov	w0, #1
	b	LBB2_7
	.loh AdrpLdrGot	Lloh13, Lloh14
	.loh AdrpAdd	Lloh18, Lloh19
	.loh AdrpLdrGotLdr	Lloh15, Lloh16, Lloh17
	.loh AdrpAdd	Lloh22, Lloh23
	.loh AdrpAdd	Lloh20, Lloh21
	.loh AdrpAdd	Lloh27, Lloh28
	.loh AdrpLdrGotLdr	Lloh24, Lloh25, Lloh26
	.cfi_endproc
                                        ; -- End function
	.globl	_main                           ; -- Begin function main
	.p2align	2
_main:                                  ; @main
	.cfi_startproc
; %bb.0:
	stp	x24, x23, [sp, #-64]!           ; 16-byte Folded Spill
	.cfi_def_cfa_offset 64
	stp	x22, x21, [sp, #16]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #32]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #48]             ; 16-byte Folded Spill
	add	x29, sp, #48
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	.cfi_offset w23, -56
	.cfi_offset w24, -64
	cmp	w0, #2
	b.eq	LBB3_3
; %bb.1:
	cmp	w0, #3
	b.ne	LBB3_8
; %bb.2:
	mov	x21, #0
	ldr	x19, [x1, #16]
	b	LBB3_4
LBB3_3:
	mov	w21, w0
Lloh29:
	adrp	x19, l_.str.2@PAGE
Lloh30:
	add	x19, x19, l_.str.2@PAGEOFF
LBB3_4:
	ldr	x20, [x1, #8]
Lloh31:
	adrp	x24, _file_print_offset@GOTPAGE
Lloh32:
	ldr	x24, [x24, _file_print_offset@GOTPAGEOFF]
	str	x21, [x24]
	mov	x0, x20
	bl	_strlen
Lloh33:
	adrp	x8, _pattern_len@GOTPAGE
Lloh34:
	ldr	x8, [x8, _pattern_len@GOTPAGEOFF]
Lloh35:
	str	x0, [x8]
	mov	x0, x21
	bl	_malloc
	mov	x22, x0
	adrp	x23, _fullpath@PAGE
	str	x0, [x23, _fullpath@PAGEOFF]
	mov	x0, x19
	bl	_strlen
	cmp	x21, x0
	b.hi	LBB3_6
; %bb.5:
	lsl	x1, x0, #1
	str	x1, [x24]
	mov	x0, x22
	bl	_realloc
	str	x0, [x23, _fullpath@PAGEOFF]
	cbz	x0, LBB3_7
LBB3_6:
	ldr	x0, [x23, _fullpath@PAGEOFF]
	mov	x1, x19
	bl	_strcpy
	mov	x1, x20
	bl	_helper_func
	cmp	w0, #0
	cset	w0, eq
	ldp	x29, x30, [sp, #48]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #32]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp], #64             ; 16-byte Folded Reload
	ret
LBB3_7:
Lloh36:
	adrp	x0, l_.str.16@PAGE
Lloh37:
	add	x0, x0, l_.str.16@PAGEOFF
	bl	_perror
	b	LBB3_6
LBB3_8:
Lloh38:
	adrp	x8, ___stderrp@GOTPAGE
Lloh39:
	ldr	x8, [x8, ___stderrp@GOTPAGEOFF]
Lloh40:
	ldr	x3, [x8]
Lloh41:
	adrp	x0, l_.str.11@PAGE
Lloh42:
	add	x0, x0, l_.str.11@PAGEOFF
	mov	w1, #38
	mov	w2, #1
	bl	_fwrite
	mov	w0, #2
	bl	_exit
	.loh AdrpAdd	Lloh29, Lloh30
	.loh AdrpLdrGotStr	Lloh33, Lloh34, Lloh35
	.loh AdrpLdrGot	Lloh31, Lloh32
	.loh AdrpAdd	Lloh36, Lloh37
	.loh AdrpAdd	Lloh41, Lloh42
	.loh AdrpLdrGotLdr	Lloh38, Lloh39, Lloh40
	.cfi_endproc
                                        ; -- End function
	.p2align	2                               ; -- Begin function search_file.cold.1
_search_file.cold.1:                    ; @search_file.cold.1
	.cfi_startproc
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	.cfi_def_cfa_offset 16
	mov	x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
Lloh43:
	adrp	x0, l_.str.5@PAGE
Lloh44:
	add	x0, x0, l_.str.5@PAGEOFF
	bl	_perror
	mov	w0, #2
	bl	_exit
	.loh AdrpAdd	Lloh43, Lloh44
	.cfi_endproc
                                        ; -- End function
	.p2align	2                               ; -- Begin function search_file.cold.2
_search_file.cold.2:                    ; @search_file.cold.2
	.cfi_startproc
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	.cfi_def_cfa_offset 16
	mov	x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
Lloh45:
	adrp	x0, l_.str.7@PAGE
Lloh46:
	add	x0, x0, l_.str.7@PAGEOFF
	bl	_perror
	mov	w0, #2
	bl	_exit
	.loh AdrpAdd	Lloh45, Lloh46
	.cfi_endproc
                                        ; -- End function
	.section	__TEXT,__cstring,cstring_literals
l_.str:                                 ; @.str
	.asciz	"/opt/asp/bin/"

l_.str.2:                               ; @.str.2
	.asciz	"."

l_.str.3:                               ; @.str.3
	.asciz	"test.txt"

l_.str.4:                               ; @.str.4
	.asciz	" ."

	.comm	_buf,144,3                      ; @buf
l_.str.5:                               ; @.str.5
	.asciz	"can't stat"

l_.str.6:                               ; @.str.6
	.asciz	"r"

l_.str.7:                               ; @.str.7
	.asciz	"can't open file"

l_.str.8:                               ; @.str.8
	.asciz	"\n%s\n %d:%s"

.zerofill __DATA,__bss,_fullpath,8,3    ; @fullpath
l_.str.9:                               ; @.str.9
	.asciz	".."

l_.str.10:                              ; @.str.10
	.asciz	"can\342\200\231t close directory %s"

	.comm	_file_print_offset,8,3          ; @file_print_offset
l_.str.11:                              ; @.str.11
	.asciz	"usage: greptile <pattern> [directory]\n"

	.comm	_pattern_len,8,3                ; @pattern_len
.zerofill __DATA,__bss,_nreg,8,3        ; @nreg
.zerofill __DATA,__bss,_ndir,8,3        ; @ndir
l_.str.14:                              ; @.str.14
	.asciz	"can't stat %d for pathname %s"

l_.str.16:                              ; @.str.16
	.asciz	"realloc failed"

.subsections_via_symbols
