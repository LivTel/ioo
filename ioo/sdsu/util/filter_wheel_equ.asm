; filter_wheel_equ.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_equ.asm,v 1.1 2011-06-02 14:06:13 cjm Exp $
	This include file defines EQUates used by the filter wheel.
	Some of these define what we are using the digital input and output bits for,
	located at DIG_IN (Y:0) and DIG_OUT (Y:1).
	Some define bits for the status word STATUS (X:0). Note the first five (0..4) bits are already used.
	This source file should be included after the "Bit definitions of STATUS word" in utilappl.asm.
	Note the values of the bit definitions will need updating in libo_ccd:ccd_filter_wheel.c if they change.
	Version: $Revision: 1.1 $
	Author: $Author: cjm $
	*
;Note the values of the bit definitions will need updating in libo_ccd:ccd_filter_wheel.c if they change.

;  Extra Bit definitions of STATUS word
ST_FW_MOVE		EQU	5	; Set if a filter wheel move (FWM) operation is in progress.
ST_FW_RESET		EQU	6	; Set if a filter wheel reset (FWR) operation is in progress.
ST_FW_IN_DETENT		EQU	7	; Last known state of the filter wheel detent. Used to detect
					; filter wheel detent state changes.
ST_FW_DETENT_CHECK	EQU	8	; Set if we are detent checking, clear if we are de-bouncing.

; Bit definitions of the digital input word DIG_IN (Y:0)
; Note these values take the form of a bit number i.e. '4' means the fourth bit is set,
; rather than a bit mask number (1<<'4') == 16
; This is so they can be used with BSET,BCLR,JSET,JCLR
FW0_INPUT_DETENT	EQU	0	; (1<<0) Set when wheel 0 is in a detent position.
FW0_INPUT_HOME		EQU	1	; (1<<1) Set when wheel 0 is in it's home position.

; Bit definitions of the digital output word DIG_OUT (Y:1)
; DIG_OUT bit 0 was for output enable when using stepper motors, this is no longer needed.
FW0_OUTPUT_MOVE		EQU	1	; (1<<1) Set to tell filter wheel 0 to move.
FW0_OUTPUT_DIR		EQU	2	; (1<<2) Sets filter wheel's 0 direction.

       COMMENT * 
	$Log: not supported by cvs2svn $
	*
