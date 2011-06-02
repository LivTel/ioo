; filter_wheel_isr.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_isr.asm,v 1.1 2011-06-02 14:06:08 cjm Exp $
	This include file is called from the 0.8ms SERVICE routine.
	It evaluates the digital inputs DIG_IN, and may set digital outputs DIG_OUT.
	It runs only if we are currently doing a filter wheel operation.
	This source file should be included in utilappl.asm, near "UPD_DIG". Ideally, it should
	come after the digital inputs are moved from RD_DIG to DIG_IN, but before
	the outputs are moved from DIG_OUT to WR_DIG.
	Version: $Revision: 1.1 $
	Author: $Author: cjm $
	*

; Filter wheel move
	JCLR    #ST_FW_MOVE,X:<STATUS,FW_ISR_FWR 	; If the move bit in status is clear, go to FW_ISR_FWR
; FW_DETENT_CHECK_NUM = (FW_DETENT_CHECK_NUM+1)
	MOVE	Y:<FW_DETENT_CHECK_NUM,A		; A = FW_DETENT_CHECK_NUM
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_DETENT_CHECK_NUM+1
	MOVE	A,Y:<FW_DETENT_CHECK_NUM		; FW_DETENT_CHECK_NUM = A (FW_DETENT_CHECK_NUM+1)
; if FW_DETENT_CHECK_NUM >= FW_DETENT_CHECK_COUNT enable detent checking
	MOVE	Y:<FW_DETENT_CHECK_NUM,A		; A = FW_DETENT_CHECK_NUM
	MOVE	Y:<FW_DETENT_CHECK_COUNT,X0		; X0 = FW_DETENT_CHECK_COUNT
	CMP	X0,A					; Is FW_DETENT_CHECK_NUM < FW_DETENT_CHECK_COUNT?
	JLT	<FW_ISR_END				; If (FW_DETENT_CHECK_NUM < FW_DETENT_CHECK_COUNT) 
							; 	goto FW_ISR_END
	BSET	#ST_FW_DETENT_CHECK,X:<STATUS		; We are detent checking
; wheel 0
; check whether we are in a detent or not and jump accordingly
	JSET	#FW0_INPUT_DETENT,Y:<DIG_IN,FW_ISR_FWM_IN_DETENT ; If the detent bit in Y:DIG_IN is set,
							; goto FW_ISR_FWM_IN_DETENT.
							; If the bit is set the wheel is in a detent.
	JMP	<FW_ISR_FWM_OUT_DETENT			; Go to FW_ISR_FWM_OUT_DETENT (end of FW ISR)
FW_ISR_FWM_IN_DETENT
	JSET	#ST_FW_IN_DETENT,X:<STATUS,FW_ISR_END 	; If the status detent bit is set,
							; goto end of ISR (no detent state change).
	BSET	#ST_FW_IN_DETENT,X:<STATUS		; Set ST_FW_IN_DETENT status bit.
; reset FW_DETENT_CHECK_NUM to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:<FW_DETENT_CHECK_NUM		; FW_DETENT_CHECK_NUM = 0
	BCLR	#ST_FW_DETENT_CHECK,X:<STATUS		; We are not detent checking - wait until out of detent.
; FW_POS_MOVE--
	MOVE	X:<ZERO,A				; A = 0 - test for CMP below (ie A1 also 0)
	MOVE	Y:<FW_POS_MOVE,A0			; A = FW_POS_MOVE
	DEC	A					; A-- -> FW_POS_MOVE--
	MOVE 	A0,Y:<FW_POS_MOVE			; FW_POS_MOVE = A e.g. FW_POS_MOVE--
	MOVE	X:<ZERO,X0				; X0 = 0
	CMP	X0,A					; Is FW_POS_MOVE (A) zero (X0)?
	JNE 	<FW_ISR_END				; If (FW_POS_MOVE != 0) goto end of ISR
; finish move
	BCLR    #ST_FW_MOVE,X:<STATUS			; Clear ST_FW_MOVE status bit.
	BCLR    #FW0_OUTPUT_MOVE,Y:<DIG_OUT		; Clear filter wheel 0 MOVE bit in Y:DIG_OUT.
	JMP 	<FW_ISR_END				; goto end of ISR
FW_ISR_FWM_OUT_DETENT
	JCLR	#ST_FW_IN_DETENT,X:<STATUS,FW_ISR_END 	; If the status detent bit is clear,
							; goto end of ISR (no detent state change).
	BCLR	#ST_FW_IN_DETENT,X:<STATUS		; Clear ST_FW_IN_DETENT status bit.
;	JMP 	<FW_ISR_END				; goto end (not nessassary, next line will do same)
; Filter wheel reset
FW_ISR_FWR
	JCLR    #ST_FW_RESET,X:<STATUS,FW_ISR_END 	; If the reset bit in status is clear, go to FW_ISR_END
; wheel 0
	; check detent
	JCLR	#FW0_INPUT_DETENT,Y:<DIG_IN,FW_ISR_END 	; If the detent bit in Y:DIG_IN is clear,
							; continue reset (go to end).
							; If the bit is set the wheel is in a detent position.
	; check home
	JCLR	#FW0_INPUT_HOME,Y:<DIG_IN,FW_ISR_END ; If the home bit in Y:DIG_IN is clear,continue reset (go to end).
							; If the bit is set the wheel is in the home position.
	BCLR    #ST_FW_RESET,X:<STATUS			; Clear ST_FW_RESET status bit.
	BCLR    #FW0_OUTPUT_MOVE,Y:<DIG_OUT		; Clear filter wheel MOVE bit in Y:DIG_OUT.
FW_ISR_END

       COMMENT * 
	$Log: not supported by cvs2svn $
	*
