; filter_wheel_isr.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_isr.asm,v 1.6 2022-03-23 15:29:11 cjm Exp $
	This include file is called from the 0.8ms SERVICE routine.
	It evaluates the digital inputs DIG_IN, and may set digital outputs DIG_OUT.
	It runs only if we are currently doing a filter wheel operation.
	This source file should be included in utilappl.asm, near "UPD_DIG". Ideally, it should
	come after the digital inputs are moved from RD_DIG to DIG_IN, but before
	the outputs are moved from DIG_OUT to WR_DIG.
	Version: $Revision: 1.6 $
	Author: $Author: cjm $
	*

; Filter wheel move
	JSET    #ST_FW_MOVE,X:<STATUS,FW_ISR_START 	; If the move bit in status is set, goto FW_ISR_START
	JCLR    #ST_FW_RESET,X:<STATUS,FW_ISR_END 	; If the reset bit in status is clear, goto FW_ISR_END
FW_ISR_START
; We are in a filer wheel move or reset operation
; If we are not moving the locators out goto FW_ISR_MOVING_OUT
	JCLR	#ST_FW_LOCATORS_OUT,X:<STATUS,FW_ISR_MOVING_OUT
; set digital output bit to move locators out
	BCLR	#DIG_OUT_BIT_LOCATORS_IN,Y:<DIG_OUT	; move locators out
; check whether the locators are out
; if locator 1 not out goto FW_ISR_LOCATOR_NOT_OUT
	JSET	#DIG_IN_BIT_LOCATOR_1_OUT,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_OUT
; if locator 2 not out goto FW_ISR_LOCATOR_NOT_OUT
	JSET	#DIG_IN_BIT_LOCATOR_2_OUT,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_OUT 
; if locator 3 not out goto FW_ISR_LOCATOR_NOT_OUT
	JSET	#DIG_IN_BIT_LOCATOR_3_OUT,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_OUT 
; all three locators are out at this point, goto next stage
	BCLR    #ST_FW_LOCATORS_OUT,X:<STATUS		; We have finished moving locators out
	BSET    #ST_FW_MOVE_OUT_POSITION,X:<STATUS	; Start moving wheel out of a position
; reset move timeout FW_TIMEOUT_INDEX to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:FW_TIMEOUT_INDEX			; FW_TIMEOUT_INDEX = 0
	JMP	FW_ISR_END
FW_ISR_LOCATOR_NOT_OUT
; FW_TIMEOUT_INDEX = (FW_TIMEOUT_INDEX+1)
	JSR	FW_ISR_TIMEOUT_INDEX_INC
; A = Y:FW_TIMEOUT_INDEX in FW_ISR_TIMEOUT_INDEX_INC
; if FW_TIMEOUT_INDEX >= FW_TIMEOUT_COUNT move timeout
	MOVE	Y:FW_TIMEOUT_COUNT,X0
	CMP	X0,A					; Is FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT
	JLT	FW_ISR_END				; If (FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_RESET,X:<STATUS			; Take out of filter wheel reset mode.
	BCLR    #ST_FW_LOCATORS_OUT,X:<STATUS		; We are no longer moving the locators out
	MOVE	#>6,X0					; X0 = 6
	MOVE	X0,Y:FW_ERROR_CODE			; Y:FW_ERROR_CODE = X0 = 6
	JMP	FW_ISR_END
FW_ISR_MOVING_OUT
; If we are not moving the wheel and moving out of a proximity sensor goto FW_ISR_MOVING_IN
	JCLR	#ST_FW_MOVE_OUT_POSITION,X:<STATUS,FW_ISR_MOVING_IN 
	BSET	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor on
; FW_TIMEOUT_INDEX = (FW_TIMEOUT_INDEX+1)
	JSR	FW_ISR_TIMEOUT_INDEX_INC
; A = Y:FW_TIMEOUT_INDEX in FW_ISR_TIMEOUT_INDEX_INC
; de bounce, dont't check proximity for 10 loops (0.01s)
	MOVE	#>10,X0
	CMP	X0,A					; Is FW_TIMEOUT_INDEX < #100
	JLT	FW_ISR_END				; If (FW_TIMEOUT_INDEX < #100) goto FW_ISR_END
; check proximity sensor pattern
	JCLR	#DIG_IN_BIT_PROXIMITY_1_ON,Y:<DIG_IN,FW_ISR_MOVING_OUT_STILL
	JCLR	#DIG_IN_BIT_PROXIMITY_2_ON,Y:<DIG_IN,FW_ISR_MOVING_OUT_STILL
	JCLR	#DIG_IN_BIT_PROXIMITY_3_ON,Y:<DIG_IN,FW_ISR_MOVING_OUT_STILL
	JCLR	#DIG_IN_BIT_PROXIMITY_4_ON,Y:<DIG_IN,FW_ISR_MOVING_OUT_STILL
	JCLR	#DIG_IN_BIT_PROXIMITY_5_ON,Y:<DIG_IN,FW_ISR_MOVING_OUT_STILL
	JCLR	#DIG_IN_BIT_PROXIMITY_6_ON,Y:<DIG_IN,FW_ISR_MOVING_OUT_STILL
; If all proximity bits are set we have moved out of a position
FW_ISR_MOVED_OUT
	BCLR    #ST_FW_MOVE_OUT_POSITION,X:<STATUS	; We have finished moving out of a position
	BSET    #ST_FW_MOVE_IN_POSITION,X:<STATUS	; Start moving wheel in to a position
; reset move timeout FW_TIMEOUT_INDEX to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:FW_TIMEOUT_INDEX			; FW_TIMEOUT_INDEX = 0
	JMP	FW_ISR_END
FW_ISR_MOVING_OUT_STILL
; if FW_TIMEOUT_INDEX >= Y:FW_TIMEOUT_COUNT move timeout
	MOVE	Y:FW_TIMEOUT_INDEX,A			; A = FW_TIMEOUT_INDEX
	MOVE	Y:FW_TIMEOUT_COUNT,X0
	CMP	X0,A					; Is FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT
	JLT	FW_ISR_END				; If (FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT) goto FW_ISR_END
; timeout error Y:FW_ERROR_CODE = 7 : We have timed out moving out of a detent position
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_RESET,X:<STATUS			; Take out of filter wheel reset mode.
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor off
	BCLR    #ST_FW_MOVE_OUT_POSITION,X:<STATUS	; We are no longer moving out of a position
	MOVE	#>7,X0					; X0 = 7
	MOVE	X0,Y:FW_ERROR_CODE			; Y:FW_ERROR_CODE = X0 = 7
	JMP	FW_ISR_END
FW_ISR_MOVING_IN
; If we are not moving the wheel into a proximity sensor position goto FW_ISR_LOCATORS_IN
	JCLR	#ST_FW_MOVE_IN_POSITION,X:<STATUS,FW_ISR_LOCATORS_IN
	BSET	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor on
; FW_TIMEOUT_INDEX = (FW_TIMEOUT_INDEX+1)
	JSR	FW_ISR_TIMEOUT_INDEX_INC
; A = Y:FW_TIMEOUT_INDEX in FW_ISR_TIMEOUT_INDEX_INC
; de bounce, dont't check proximity for 10 loops (0.01s)
	MOVE	#>10,X0
	CMP	X0,A					; Is FW_TIMEOUT_INDEX < #100
	JLT	FW_ISR_END				; If (FW_TIMEOUT_INDEX < #100) goto FW_ISR_END
; check proximity sensor pattern 
; If one or more proximity bits are clear we have moved in to a position
	JCLR	#DIG_IN_BIT_PROXIMITY_1_ON,Y:<DIG_IN,FW_ISR_MOVED_IN
	JCLR	#DIG_IN_BIT_PROXIMITY_2_ON,Y:<DIG_IN,FW_ISR_MOVED_IN
	JCLR	#DIG_IN_BIT_PROXIMITY_3_ON,Y:<DIG_IN,FW_ISR_MOVED_IN
	JCLR	#DIG_IN_BIT_PROXIMITY_4_ON,Y:<DIG_IN,FW_ISR_MOVED_IN
	JCLR	#DIG_IN_BIT_PROXIMITY_5_ON,Y:<DIG_IN,FW_ISR_MOVED_IN
	JCLR	#DIG_IN_BIT_PROXIMITY_6_ON,Y:<DIG_IN,FW_ISR_MOVED_IN
; if FW_TIMEOUT_INDEX >= Y:FW_TIMEOUT_COUNT move timeout
	MOVE	Y:FW_TIMEOUT_INDEX,A			; A = FW_TIMEOUT_INDEX
	MOVE	Y:FW_TIMEOUT_COUNT,X0
	CMP	X0,A					; Is FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT
	JLT	FW_ISR_END				; If (FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_RESET,X:<STATUS			; Take out of filter wheel reset mode.
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor off
	BCLR    #ST_FW_MOVE_IN_POSITION,X:<STATUS	; We are no longer moving in to a position
	MOVE	#>11,X0					; X0 = 11
	MOVE	X0,Y:FW_ERROR_CODE			; Y:FW_ERROR_CODE = X0 = 11
	JMP	FW_ISR_END
FW_ISR_MOVED_IN
	BCLR    #ST_FW_MOVE_IN_POSITION,X:<STATUS	; We have finished moving in to a position
; reset move timeout FW_TIMEOUT_INDEX to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:FW_TIMEOUT_INDEX			; FW_TIMEOUT_INDEX = 0
; decrement Y:FW_OFFSET_POS
  	MOVE	Y:<FW_OFFSET_POS,A			; A = FW_OFFSET_POS
	MOVE	X:<ONE,X0				; X0 = 1
	SUB	X0,A					; A = FW_OFFSET_POS-1
	MOVE	A,Y:<FW_OFFSET_POS			; FW_OFFSET_POS = A (FW_OFFSET_POS-1)
	TST	A					; Is A (Y:FW_OFFSET_POS) zero
	JEQ	FW_ISR_STOP_MOVING			; If Y:FW_OFFSET_POS is zero goto FW_ISR_STOP_MOVING
; Y:FW_OFFSET_POS > 0, we still have positions to move to
	BSET    #ST_FW_MOVE_OUT_POSITION,X:<STATUS	; Start moving wheel out of a position
	JMP	FW_ISR_END
FW_ISR_STOP_MOVING
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor off NOW rather than later
	BSET    #ST_FW_LOCATORS_IN,X:<STATUS		; Start moving locators in
	JMP	FW_ISR_END
FW_ISR_LOCATORS_IN
; If we are not moving the locators in goto FW_ISR_END
	JCLR	#ST_FW_LOCATORS_IN,X:<STATUS,FW_ISR_END
; move locators in
	BSET	#DIG_OUT_BIT_LOCATORS_IN,Y:<DIG_OUT	; move locators in
; check whether the locators are in
; if locator 1 not in goto FW_ISR_LOCATOR_NOT_IN
	JSET	#DIG_IN_BIT_LOCATOR_1_IN,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_IN
; if locator 2 not in goto FW_ISR_LOCATOR_NOT_IN
	JSET	#DIG_IN_BIT_LOCATOR_2_IN,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_IN
; if locator 3 not in goto FW_ISR_LOCATOR_NOT_IN
	JSET	#DIG_IN_BIT_LOCATOR_3_IN,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_IN 
; all three locators are in at this point, check we are in the right position
	BCLR    #ST_FW_LOCATORS_IN,X:<STATUS		; We have finished moving locators in
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
; reset move timeout FW_TIMEOUT_INDEX to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:FW_TIMEOUT_INDEX			; FW_TIMEOUT_INDEX = 0
; If we are moving check proximity pattern against target
	JSET    #ST_FW_RESET,X:<STATUS,FW_ISR_RESET_LAST_POS 	; If the reset bit in status is set, 
								; goto FW_ISR_RESET_LAST_POS
; check proximity sensor pattern against target
	MOVE	Y:<FW_TARGET_PROXIMITY_PATTERN,X0	; X0 = Y:FW_TARGET_PROXIMITY_PATTERN
	MOVE	Y:<DIG_IN,A1				; A1 = Y:<DIG_IN
	MOVE	#DIG_IN_PROXIMITY_MASK,X1		; X1 = #DIG_IN_PROXIMITY_MASK
	AND	X1,A				; A1 = A1 & X1 (#DIG_IN_PROXIMITY_MASK), isolate DIG_IN proximity bits
						; NB AND uses A1 rather than A0
	CMP	X0,A					; Does X0 (FW_TARGET_PROXIMITY_PATTERN) == 
							; A (DIG_IN & PROXIMITY_MASK)
	JNE	FW_ISR_MOVE_WRONG_POS
; If we are at the right proximity location, we have finished!
	MOVE	Y:<FW_TARGET_POS,X0			; X0 = Y:FW_TARGET_POS
	MOVE	X0,Y:<FW_LAST_POS			; Y:FW_LAST_POS = X0 = Y:FW_TARGET_POS
	JMP	FW_ISR_END
FW_ISR_MOVE_WRONG_POS
; We have stopped moving in the wrong position, set an error
	MOVE	#>10,X0					; X0 = 10
	MOVE	X0,Y:FW_ERROR_CODE			; Y:FW_ERROR_CODE = X0 = 10
	JMP	FW_ISR_END
FW_ISR_RESET_LAST_POS
	BCLR    #ST_FW_RESET,X:<STATUS			; Take out of filter wheel reset mode.
; lookup current proximity setting in FW_PROXIMITY_PATTERN and set FW_LAST_POS accordingly
	MOVE	 X:<ZERO,X0				; X0 = 0
	MOVE	 X0,Y:<FW_LAST_POS			; Y:FW_LAST_POS = X0 = 0
        MOVE	#FW_PROXIMITY_PATTERN,R5	; R5 = Address of FW_PROXIMITY_PATTERN
FW_ISR_RESET_LAST_POS_LOOP
; isolate proximity bits in Y:<DIG_IN
	MOVE	Y:<DIG_IN,A1				; A1 = Y:<DIG_IN
	MOVE	#DIG_IN_PROXIMITY_MASK,X1		; X1 = #DIG_IN_PROXIMITY_MASK
	AND	X1,A				; A = A1 & X1 (#DIG_IN_PROXIMITY_MASK), isolate DIG_IN proximity bits
						; NB AND uses A1 rather than A0
 	MOVE    Y:(R5)+,X0			; X0 = proximity pattern at Y:(FW_PROXIMITY_PATTERN++)
; Does current proximity pattern in index Y:FW_LAST_POS == current DIG_IN proximity bits
	CMP	X0,A		; Does X0 Y:(FW_PROXIMITY_PATTERN+Y:FW_LAST_POS) == A (DIG_IN & PROXIMITY_MASK)
	JEQ	FW_ISR_END	; if it does finish, Y:FW_LAST_POS is correct
; add 1 to Y:FW_LAST_POS
	MOVE	Y:<FW_LAST_POS,A			; A = Y:FW_LAST_POS
	MOVE	X:<ONE,X0			; X0 = 1
	ADD	X0,A				; A  = Y:FW_LAST_POS + 1
	MOVE	A,Y:<FW_LAST_POS		; Y:FW_LAST_POS = A = Y:FW_LAST_POS + 1
; check Y:FW_LAST_POS < FW_POS_COUNT
	MOVE	Y:<FW_POS_COUNT,X0		; X0 = Y:FW_POS_COUNT
	CMP	X0,A				
	JLT	FW_ISR_RESET_LAST_POS_LOOP      
; we couldn't find the right proximity pattern - error
	MOVE	#>3,X0					; X0 = 3
	MOVE	X0,Y:FW_ERROR_CODE			; Y:FW_ERROR_CODE = X0 = 3
	JMP	FW_ISR_END
FW_ISR_LOCATOR_NOT_IN
; FW_TIMEOUT_INDEX = (FW_TIMEOUT_INDEX+1)
	JSR	FW_ISR_TIMEOUT_INDEX_INC
; A = Y:FW_TIMEOUT_INDEX in FW_ISR_TIMEOUT_INDEX_INC
; if FW_TIMEOUT_INDEX >= Y:FW_TIMEOUT_COUNT move timeout
	MOVE	Y:FW_TIMEOUT_COUNT,X0
	CMP	X0,A					; Is FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT
	JLT	FW_ISR_END				; If (FW_TIMEOUT_INDEX < Y:FW_TIMEOUT_COUNT) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_RESET,X:<STATUS			; Take out of filter wheel reset mode.
	BCLR    #ST_FW_LOCATORS_IN,X:<STATUS		; We are no longer moving the locators in
	MOVE	#>8,X0					; X0 = 8
	MOVE	X0,Y:FW_ERROR_CODE			; Y:FW_ERROR_CODE = X0 = 8 : Failure to move locators in.
	JMP	FW_ISR_END
; increment timeout index subroutine
; FW_TIMEOUT_INDEX = (FW_TIMEOUT_INDEX+1)
FW_ISR_TIMEOUT_INDEX_INC
	MOVE	Y:FW_TIMEOUT_INDEX,A			; A = FW_TIMEOUT_INDEX
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_TIMEOUT_INDEX+1
	MOVE	A,Y:FW_TIMEOUT_INDEX			; FW_TIMEOUT_INDEX = A (FW_TIMEOUT_INDEX+1)
	RTS
; end of filter wheel part of the ISR
FW_ISR_END

       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.5  2011/12/01 12:08:24  cjm
	Fixed comment.

	Revision 1.4  2011/08/18 16:43:34  cjm
	First working version of proximity counting software.

	Revision 1.3  2011/07/26 14:00:21  cjm
	Rewritten. Clutch permanently engaged, position not checked.
	Now counting in and out of proximity patterns, absolute and relative
	position variables set.

	Revision 1.2  2011/07/21 10:51:03  cjm
	First O version, with clutch engage/disengage and too many locator outputs.
	This ISR version did not wotk and caused FWM commands to return ERR and not set variables.

	Revision 1.1  2011/06/02 14:06:08  cjm
	Initial revision

	*
