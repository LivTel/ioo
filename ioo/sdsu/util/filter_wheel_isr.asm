; filter_wheel_isr.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_isr.asm,v 1.2 2011-07-21 10:51:03 cjm Exp $
	This include file is called from the 0.8ms SERVICE routine.
	It evaluates the digital inputs DIG_IN, and may set digital outputs DIG_OUT.
	It runs only if we are currently doing a filter wheel operation.
	This source file should be included in utilappl.asm, near "UPD_DIG". Ideally, it should
	come after the digital inputs are moved from RD_DIG to DIG_IN, but before
	the outputs are moved from DIG_OUT to WR_DIG.
	Version: $Revision: 1.2 $
	Author: $Author: cjm $
	*

; Filter wheel move
	JCLR    #ST_FW_MOVE,X:<STATUS,FW_ISR_END 	; If the move bit in status is clear, go to FW_ISR_END
; We are in a filer wheel move operation
; If we are not engaging the clutch goto FW_ISR_LOCATORS_OUT
	JCLR	#ST_FW_ENGAGE_CLUTCH,X:<STATUS,FW_ISR_LOCATORS_OUT 
; we are engaging the clutch at this point
	BSET	#DIG_OUT_BIT_CLUTCH_ENGAGE,Y:<DIG_OUT ; set engage clutch on Y:DIG_OUT
; Have we engaged the clutch yet
; if clutch engaged goto FW_ISR_ENGAGE_CLUTCH_DONE
	JSET	#DIG_IN_BIT_CLUTCH_ENGAGED,Y:<DIG_IN,FW_ISR_ENGAGE_CLUTCH_DONE 
; FW_MOVE_TIMEOUT = (FW_MOVE_TIMEOUT+1)
	MOVE	Y:<FW_MOVE_TIMEOUT,A			; A = FW_MOVE_TIMEOUT
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_MOVE_TIMEOUT+1
	MOVE	A,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = A (FW_MOVE_TIMEOUT+1)
; if FW_MOVE_TIMEOUT >= #100000 move timeout
	MOVE	#>100000,X0
	CMP	X0,A					; Is FW_MOVE_TIMEOUT < #100000
	JLT	<FW_ISR_END				; If (FW_MOVE_TIMEOUT < #100000) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_ENGAGE_CLUTCH,X:<STATUS		; We are no longer engaging the clutch
	MOVE	#>5,X0					; X0 = 5
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 5
	JMP	<FW_ISR_END
FW_ISR_ENGAGE_CLUTCH_DONE
	BCLR    #ST_FW_ENGAGE_CLUTCH,X:<STATUS		; We have finished engaging the clutch
	BSET    #ST_FW_LOCATORS_OUT,X:<STATUS		; Start moving the locators out
; reset move timeout FW_MOVE_TIMEOUT to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = 0
	JMP	<FW_ISR_END
FW_ISR_LOCATORS_OUT
; If we are not moving the locators out goto FW_ISR_READING_PROXIMITY
	JCLR	#ST_FW_LOCATORS_OUT,X:<STATUS,FW_ISR_READING_PROXIMITY 
; set digital output bits to move locators out
	BCLR	#DIG_OUT_BIT_LOCATOR_1_IN,Y:<DIG_OUT	; move locator 1 out
	BCLR	#DIG_OUT_BIT_LOCATOR_2_IN,Y:<DIG_OUT	; move locator 2 out
	BCLR	#DIG_OUT_BIT_LOCATOR_3_IN,Y:<DIG_OUT	; move locator 3 out
; check whether the locators are out
; if locator 1 not out goto FW_ISR_LOCATOR_NOT_OUT
	JCLR	#DIG_IN_BIT_LOCATOR_1_OUT,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_OUT
; if locator 2 not out goto FW_ISR_LOCATOR_NOT_OUT
	JCLR	#DIG_IN_BIT_LOCATOR_2_OUT,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_OUT 
; if locator 3 not out goto FW_ISR_LOCATOR_NOT_OUT
	JCLR	#DIG_IN_BIT_LOCATOR_3_OUT,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_OUT 
; all three locators are out at this point, goto next stage
	BCLR    #ST_FW_LOCATORS_OUT,X:<STATUS		; We have finished moving locators out
	BSET    #ST_FW_READING_PROXIMITY,X:<STATUS	; Start moving wheel/checking proximity sensors
; reset move timeout FW_MOVE_TIMEOUT to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = 0
	JMP	<FW_ISR_END
FW_ISR_LOCATOR_NOT_OUT
; FW_MOVE_TIMEOUT = (FW_MOVE_TIMEOUT+1)
	MOVE	Y:<FW_MOVE_TIMEOUT,A			; A = FW_MOVE_TIMEOUT
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_MOVE_TIMEOUT+1
	MOVE	A,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = A (FW_MOVE_TIMEOUT+1)
; if FW_MOVE_TIMEOUT >= #100000 move timeout
	MOVE	#>100000,X0
	CMP	X0,A					; Is FW_MOVE_TIMEOUT < #100000
	JLT	<FW_ISR_END				; If (FW_MOVE_TIMEOUT < #100000) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_LOCATORS_OUT,X:<STATUS		; We are no longer moving the locators out
	MOVE	#>6,X0					; X0 = 6
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 6
	JMP	<FW_ISR_END
FW_ISR_READING_PROXIMITY
; If we are not moving the wheel and checking proximity sensors goto FW_ISR_LOCATORS_IN
	JCLR	#ST_FW_READING_PROXIMITY,X:<STATUS,FW_ISR_LOCATORS_IN 
	BSET	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor on
; check proximity sensor pattern against target
	MOVE	Y:<FW_TARGET_PROXIMITY_PATTERN,X0	; X0 = Y:<FW_TARGET_PROXIMITY_PATTERN
	MOVE	Y:<DIG_IN,A				; A = Y:<DIG_IN
	MOVE	#DIG_IN_PROXIMITY_MASK,X1		; X1 = #DIG_IN_PROXIMITY_MASK
	AND	X1,A				; A = A & X1 (#DIG_IN_PROXIMITY_MASK), isolate DIG_IN proximity bits
	CMP	X0,A					; Does X0 (FW_TARGET_PROXIMITY_PATTERN) == 
							; A (DIG_IN & PROXIMITY_MASK)
; If we are at the right proximity location, start stopping movement
	JEQ	<FW_ISR_READING_PROXIMITY_STOP
; FW_MOVE_TIMEOUT = (FW_MOVE_TIMEOUT+1)
	MOVE	Y:<FW_MOVE_TIMEOUT,A			; A = FW_MOVE_TIMEOUT
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_MOVE_TIMEOUT+1
	MOVE	A,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = A (FW_MOVE_TIMEOUT+1)
; if FW_MOVE_TIMEOUT >= #100000 move timeout
	MOVE	#>100000,X0
	CMP	X0,A					; Is FW_MOVE_TIMEOUT < #100000
	JLT	<FW_ISR_END				; If (FW_MOVE_TIMEOUT < #100000) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor off
	BCLR    #ST_FW_READING_PROXIMITY,X:<STATUS	; We are no longer moving wheel/reading proximity sensors
	MOVE	#>7,X0					; X0 = 7
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 7
	JMP	<FW_ISR_END
FW_ISR_READING_PROXIMITY_STOP
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; Turn motor off NOW rather than later
	BCLR    #ST_FW_READING_PROXIMITY,X:<STATUS	; We have finished reading proximity sensors
	BSET    #ST_FW_LOCATORS_IN,X:<STATUS		; Start moving locators in
; reset move timeout FW_MOVE_TIMEOUT to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = 0
	JMP	<FW_ISR_END
FW_ISR_LOCATORS_IN
; If we are not moving the locators in goto FW_ISR_DISENGAGE_CLUTCH
	JCLR	#ST_FW_LOCATORS_IN,X:<STATUS,FW_ISR_DISENGAGE_CLUTCH
; move locators in
	BSET	#DIG_OUT_BIT_LOCATOR_1_IN,Y:<DIG_OUT	; move locator 1 in
	BSET	#DIG_OUT_BIT_LOCATOR_2_IN,Y:<DIG_OUT	; move locator 2 in
	BSET	#DIG_OUT_BIT_LOCATOR_3_IN,Y:<DIG_OUT	; move locator 3 in
; check whether the locators are in
; if locator 1 not in goto FW_ISR_LOCATOR_NOT_IN
	JCLR	#DIG_IN_BIT_LOCATOR_1_IN,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_IN
; if locator 2 not in goto FW_ISR_LOCATOR_NOT_IN
	JCLR	#DIG_IN_BIT_LOCATOR_2_IN,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_IN 
; if locator 3 not in goto FW_ISR_LOCATOR_NOT_IN
	JCLR	#DIG_IN_BIT_LOCATOR_3_IN,Y:<DIG_IN,FW_ISR_LOCATOR_NOT_IN 
; all three locators are in at this point, goto next stage
	BCLR    #ST_FW_LOCATORS_IN,X:<STATUS		; We have finished moving locators in
	BSET    #ST_FW_DISENGAGE_CLUTCH,X:<STATUS	; Start disengaging clutch
; reset move timeout FW_MOVE_TIMEOUT to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = 0
	JMP	<FW_ISR_END
FW_ISR_LOCATOR_NOT_IN
; FW_MOVE_TIMEOUT = (FW_MOVE_TIMEOUT+1)
	MOVE	Y:<FW_MOVE_TIMEOUT,A			; A = FW_MOVE_TIMEOUT
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_MOVE_TIMEOUT+1
	MOVE	A,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = A (FW_MOVE_TIMEOUT+1)
; if FW_MOVE_TIMEOUT >= #100000 move timeout
	MOVE	#>100000,X0
	CMP	X0,A					; Is FW_MOVE_TIMEOUT < #100000
	JLT	<FW_ISR_END				; If (FW_MOVE_TIMEOUT < #100000) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_LOCATORS_IN,X:<STATUS		; We are no longer moving the locators in
	MOVE	#>8,X0					; X0 = 8
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 8
	JMP	<FW_ISR_END
FW_ISR_DISENGAGE_CLUTCH
; If we are not disengaging the clutch goto FW_ISR_END
	JCLR	#ST_FW_DISENGAGE_CLUTCH,X:<STATUS,FW_ISR_END

	BCLR	#DIG_OUT_BIT_CLUTCH_ENGAGE,Y:<DIG_OUT ; clear engage clutch on Y:DIG_OUT
; Have we disengaged the clutch yet
; if clutch disengaged goto FW_ISR_DISENGAGE_CLUTCH_DONE 
	JSET	#DIG_IN_BIT_CLUTCH_DISENGAGED,Y:<DIG_IN,FW_ISR_DISENGAGE_CLUTCH_DONE
; FW_MOVE_TIMEOUT = (FW_MOVE_TIMEOUT+1)
	MOVE	Y:<FW_MOVE_TIMEOUT,A			; A = FW_MOVE_TIMEOUT
	MOVE	X:<ONE,X0				; X0 = 1
	ADD	X0,A					; A = FW_MOVE_TIMEOUT+1
	MOVE	A,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = A (FW_MOVE_TIMEOUT+1)
; if FW_MOVE_TIMEOUT >= #100000 move timeout
	MOVE	#>100000,X0
	CMP	X0,A					; Is FW_MOVE_TIMEOUT < #100000
	JLT	<FW_ISR_END				; If (FW_MOVE_TIMEOUT < #100000) goto FW_ISR_END
; timeout error
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
	BCLR    #ST_FW_DISENGAGE_CLUTCH,X:<STATUS	; We are no longer engaging the clutch
	MOVE	#>9,X0					; X0 = 9
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 9
	JMP	<FW_ISR_END
FW_ISR_DISENGAGE_CLUTCH_DONE
	BCLR    #ST_FW_DISENGAGE_CLUTCH,X:<STATUS	; We have finished disengaging the clutch
; reset move timeout FW_MOVE_TIMEOUT to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:<FW_MOVE_TIMEOUT			; FW_MOVE_TIMEOUT = 0
; We have finished moving the filter wheel, switch off the filter wheel move mode.
	BCLR    #ST_FW_MOVE,X:<STATUS			; Take out of filter wheel move mode.
; check proximity sensor pattern against target
	MOVE	Y:<FW_TARGET_PROXIMITY_PATTERN,X0	; X0 = Y:<FW_TARGET_PROXIMITY_PATTERN
	MOVE	Y:<DIG_IN,A				; A = Y:<DIG_IN
	MOVE	#DIG_IN_PROXIMITY_MASK,X1		; X1 = #DIG_IN_PROXIMITY_MASK
	AND	X1,A				; A = A & X1 (#DIG_IN_PROXIMITY_MASK), isolate DIG_IN proximity bits
	CMP	X0,A					; Does X0 (FW_TARGET_PROXIMITY_PATTERN) == 
							; A (DIG_IN & PROXIMITY_MASK)
; If we are at the right proximity location, we have finished!
	JEQ	<FW_ISR_END
; We have stopped moving in the wrong position, set an error
	MOVE	#>10,X0					; X0 = 10
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 10
; end of filter wheel part of the ISR
FW_ISR_END

       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.1  2011/06/02 14:06:08  cjm
	Initial revision

	*
