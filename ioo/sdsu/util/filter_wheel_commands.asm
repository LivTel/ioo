; filter_wheel_commands.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_commands.asm,v 1.4 2011-08-18 16:44:43 cjm Exp $
	This include file contains all the commands we can issue to the utility board,
	to start filter wheel operations.
	This source file should be included in utilappl.asm, after all the other commands are defined.
	Version: $Revision: 1.4 $
	Author: $Author: cjm $
	*

; Filter wheel move: FWM <position>
FILTER_WHEEL_MOVE
; command parameters
	MOVE	X:(R4)+,X0			; Get first command parameter (position) into X0
	MOVE	X0,Y:<FW_TARGET_POS		; Move X0 to Y:FW_TARGET_POS - parameter is position
; find target proximity sensor pattern
        MOVE	#FW_PROXIMITY_PATTERN,R5	; R5 = Address of FW_PROXIMITY_PATTERN
	MOVE	Y:<FW_TARGET_POS,X0		; X0 = Y:FW_TARGET_POS
	MOVE    Y:(R5),X1			; X1 = proximity pattern at Y:(FW_PROXIMITY_PATTERN + 0)
	REP	X0				; Repeat next line R0 (Y:FW_TARGET_POS) times
	MOVE    Y:(R5)+,X1			; X1 = proximity pattern at Y:(FW_PROXIMITY_PATTERN++)
	MOVE    Y:(R5),X1			; X1 = proximity pattern at Y:(FW_PROXIMITY_PATTERN + Y:FW_TARGET_POS)
	MOVE	X1,Y:<FW_TARGET_PROXIMITY_PATTERN ; Y:FW_TARGET_PROXIMITY_PATTERN = X1 = proximity pattern
; set FW_OFFSET_POS
	MOVE	Y:<FW_LAST_POS,A			; A = Y:FW_LAST_POS
; check LAST_POS OK
	TST	A
	JGE	FW_MOVE_LAST_POS_OK		; if A (Y:<FW_LAST_POS) >= 0, Y:FW_LAST_POS is OK, 
						; goto FW_MOVE_LAST_POS_OK
; Y:FW_ERROR_CODE = 1 : Y:FW_LAST_POS < 0
	MOVE	#>1,X0				; X0 = 1
	MOVE	X0,Y:FW_ERROR_CODE		; Y:FW_ERROR_CODE = X0 = 1
	JMP     ERROR				; Go transmit error reply
FW_MOVE_LAST_POS_OK
	MOVE	Y:<FW_TARGET_POS,A		; A = Y:FW_TARGET_POS
	TST	A
	JGE	FW_MOVE_TARGET_POS_OK		; If Y:FW_TARGET_POS >= 0 goto FW_MOVE_TARGET_POS_OK
; Y:FW_ERROR_CODE = 2 : Y:FW_TARGET_POS < 0
	MOVE	#>2,X0				; X0 = 2
	MOVE	X0,Y:FW_ERROR_CODE		; Y:FW_ERROR_CODE = X0 = 2
	JMP     ERROR				; Go transmit error reply
FW_MOVE_TARGET_POS_OK
	MOVE	Y:<FW_TARGET_POS,A		; A = Y:FW_TARGET_POS
	MOVE	Y:<FW_LAST_POS,X0		; X0 = Y:FW_LAST_POS
	SUB	X0,A				; A = A - X0 = Y:FW_TARGET_POS-Y:FW_LAST_POS
	JLT	FW_MOVE_OFFSET1			; if ((Y:FW_TARGET_POS-Y:FW_LAST_POS) < 0, goto FW_MOVE_OFFSET1
	MOVE	A,Y:<FW_OFFSET_POS		; Y:FW_OFFSET_POS = A = Y:FW_TARGET_POS-Y:FW_LAST_POS	
	JMP	FW_MOVE_OFFSET2			; Finished calculating FW_OFFSET_POS, goto FW_MOVE_OFFSET2
FW_MOVE_OFFSET1
; else Y:FW_OFFSET_POS = Y:FW_POS_COUNT + (Y:FW_TARGET_POS-Y:FW_LAST_POS)
; Therefore add FW_POS_COUNT to A(Y:FW_TARGET_POS-Y:FW_LAST_POS)
	MOVE	Y:<FW_POS_COUNT,X0		; X0 = Y:FW_POS_COUNT
	ADD	X0,A				; A = A + X0 = Y:FW_POS_COUNT + (Y:FW_TARGET_POS-Y:FW_LAST_POS)
	MOVE	A,Y:<FW_OFFSET_POS		; Y:FW_OFFSET_POS = A = 
						; Y:FW_POS_COUNT + (Y:FW_TARGET_POS-Y:FW_LAST_POS)	
FW_MOVE_OFFSET2
; check Y:FW_OFFSET_POS != 0, and if so returned DON (no move to make)
	TST	A				; A = Y:FW_OFFSET_POS
	JNE	FW_MOVE_OFFSET3			; IF A (Y:FW_OFFSET_POS) != 0, goto FW_MOVE_OFFSET3
	JMP	FINISH				; Y:FW_OFFSET_POS == 0  here, therefore nothing to do, return DON
FW_MOVE_OFFSET3
; reset move timeout FW_TIMEOUT_INDEX to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:FW_TIMEOUT_INDEX		        ; FW_TIMEOUT_INDEX = 0
; reset error code to 0
	MOVE	X0,Y:FW_ERROR_CODE		        ; FW_ERROR_CODE = X0 = 0
; Set relevant ST_FW_MOVE and ST_FW_LOCATORS_OUT bits in X:STATUS, so the ISR knows what we are doing
	BSET    #ST_FW_MOVE,X:<STATUS		        ; We are starting to move the filter wheel
	BSET	#ST_FW_LOCATORS_OUT,X:<STATUS		; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_MOVE_IN_POSITION,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_MOVE_OUT_POSITION,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_LOCATORS_IN,X:<STATUS	        ; Ensure rest of filter wheel status bits are not set
; we always engage the clutch atm
	BCLR	#DIG_OUT_BIT_CLUTCH_DISENGAGE,Y:<DIG_OUT   ; engage clutch on Y:DIG_OUT
	JMP	FINISH					; Issue 'DON' and get next command

FILTER_WHEEL_RESET
; set FW_OFFSET_POS to 1 - move filter wheel to next position
	MOVE	X:<ONE,X0			; X0 = 1
	MOVE	X0,Y:<FW_OFFSET_POS		; Y:FW_OFFSET_POS = X0 = 1
; reset move timeout FW_TIMEOUT_INDEX to zero
	MOVE	X:<ZERO,X0				; X0 = 0
	MOVE	X0,Y:FW_TIMEOUT_INDEX		        ; FW_TIMEOUT_INDEX = 0
; reset error code to 0
	MOVE	X0,Y:FW_ERROR_CODE		        ; FW_ERROR_CODE = X0 = 0
; Set relevant ST_FW_MOVE and ST_FW_LOCATORS_OUT bits in X:STATUS, so the ISR knows what we are doing
	BSET    #ST_FW_RESET,X:<STATUS		        ; We are starting to reset the filter wheel
	BSET	#ST_FW_LOCATORS_OUT,X:<STATUS		; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_MOVE_IN_POSITION,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_MOVE_OUT_POSITION,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_LOCATORS_IN,X:<STATUS	        ; Ensure rest of filter wheel status bits are not set
; we always engage the clutch atm
	BCLR	#DIG_OUT_BIT_CLUTCH_DISENGAGE,Y:<DIG_OUT   ; engage clutch on Y:DIG_OUT
	JMP	FINISH					; Issue 'DON' and get next command

; Filter wheel abort: FWA
; Resets ST_FW_RESET and ST_FW_MOVE STATUS bits (X:<STATUS)
; Clears filter wheel output bits DIG_OUT_BIT_MOTOR_ON
FILTER_WHEEL_ABORT
	BCLR    #ST_FW_MOVE,X:<STATUS 		; Take out of filter wheel move mode.
	BCLR    #ST_FW_RESET,X:<STATUS 		; Take out of filter wheel reset mode.
; Leave the locators driven out
; Leave the clutch cylinder engaged
; turn off the motor
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT ; turn motor off
; Set error code to 9: Abort
	MOVE	#>9,X0				; X0 = 9
	MOVE	X0,Y:FW_ERROR_CODE		; Y:FW_ERROR_CODE = X0 = 9
	JMP	FINISH				; Issue 'DON' and get next command

       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.3  2011/07/26 13:51:49  cjm
	Latest version - counts in and out of proximity sensors.

	Revision 1.2  2011/07/21 10:51:19  cjm
	First O version, with clutch engage/disengage and too many locator outputs.

	Revision 1.1  2011/06/02 14:06:16  cjm
	Initial revision

	*
