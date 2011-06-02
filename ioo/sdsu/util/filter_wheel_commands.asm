; filter_wheel_commands.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_commands.asm,v 1.1 2011-06-02 14:06:16 cjm Exp $
	This include file contains all the commands we can issue to the utility board,
	to start filter wheel operations.
	This source file should be included in utilappl.asm, after all the other commands are defined.
	Version: $Revision: 1.1 $
	Author: $Author: cjm $
	*

; FWM goes here if an error occurs during startup of FWM (the wheel is not starting in a detent).
FWM_ERROR
	MOVE	X:(R4)+,X0			; Get second command parameter (no of positions) into X0 
						; to clear stack
	JMP	<ERROR				; Jump to error routine

; Filter wheel move: FWM <direction> <no of positions>
FILTER_WHEEL_MOVE
; command parameters
	MOVE	X:(R4)+,X0			; Get first command parameter (direction) into X0
; set wheel direction digital output bit
; X0 is wheel direction
; A is loaded with values to test X0 against
; wheel == 0
	JCLR	#FW0_INPUT_DETENT,Y:<DIG_IN,FWM_ERROR ; If the detent bit in Y:DIG_IN is clear, 
						; we are not starting in a detent. Return an error.
	BSET    #FW0_OUTPUT_MOVE,Y:<DIG_OUT 	; switch filter wheel 0 movement on.
	MOVE	X:<ZERO,A			; A = 0
	CMP	X0,A				; Is the direction 0?
	JNE	<FWM_W0D1			; If the direction is NOT 0, go to FWM_W0D1
	BCLR    #FW0_OUTPUT_DIR,Y:<DIG_OUT 	; We are using wheel 0, in direction zero. Clear dir bit accordingly.
	JMP	<FWM_DIR_END			; We have set direction, go to end of direction code
FWM_W0D1
	BSET    #FW0_OUTPUT_DIR,Y:<DIG_OUT 	; We are using wheel 0, in direction one. Set dir bit accordingly.
FWM_DIR_END
	MOVE	X:(R4)+,X0			; Get third command parameter (no of positions) into X0
	MOVE	X0,Y:<FW_POS_MOVE		; Move X0 to Y:<FW_POS_MOVE - parameter is no of positions
	MOVE	X:<ZERO,X0
	MOVE	X0,Y:<FW_DETENT_CHECK_NUM	; Reset num to count to zero.
	BSET    #ST_FW_IN_DETENT,X:<STATUS 	; We are currently in a detent, (see above for test), set 
						; relevant status bit.
	BSET    #ST_FW_MOVE,X:<STATUS 		; We are moving the filter wheel
	JMP	<FINISH				; Issue 'DON' and get next command

; Filter wheel reset: FWR
FILTER_WHEEL_RESET
	BCLR    #FW0_OUTPUT_DIR,Y:<DIG_OUT 	; We are using wheel 0, in direction zero. Clear dir bit accordingly.
	BSET    #FW0_OUTPUT_MOVE,Y:<DIG_OUT 	; switch filter wheel 0 movement on.
	BSET    #ST_FW_RESET,X:<STATUS 		; Put in filter wheel reset mode
	JMP	<FINISH				; Issue 'DON' and get next command

; Filter wheel abort: FWA
; Resets ST_FW_RESET and ST_FW_MOVE STATUS bits (X:<STATUS)
; Clears filter wheel output bits FW0_OUTPUT_MOVE
FILTER_WHEEL_ABORT
	BCLR    #ST_FW_MOVE,X:<STATUS 		; Take out of filter wheel move mode.
	BCLR    #ST_FW_RESET,X:<STATUS 		; Take out of filter wheel reset mode.
	BCLR    #FW0_OUTPUT_MOVE,Y:<DIG_OUT 	; Stop filter wheel 0 move.
	JMP	<FINISH				; Issue 'DON' and get next command

       COMMENT * 
	$Log: not supported by cvs2svn $
	*
