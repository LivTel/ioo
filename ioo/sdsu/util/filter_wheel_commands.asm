; filter_wheel_commands.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_commands.asm,v 1.2 2011-07-21 10:51:19 cjm Exp $
	This include file contains all the commands we can issue to the utility board,
	to start filter wheel operations.
	This source file should be included in utilappl.asm, after all the other commands are defined.
	Version: $Revision: 1.2 $
	Author: $Author: cjm $
	*

; Filter wheel move: FWM <position>
FILTER_WHEEL_MOVE
; command parameters
	MOVE	X:(R4)+,X0			; Get first command parameter (position) into X0
	MOVE	X0,Y:<FW_POS			; Move X0 to Y:<FW_POS - parameter is position
; find target proximity sensor pattern
        MOVE	#FW_PROXIMITY_PATTERN,R5	; R5 = Address of FW_PROXIMITY_PATTERN
	MOVE	Y:<FW_POS,X0			; X0 = Y:<FW_POS
	MOVE    Y:(R5),X1			; X1 = proximity pattern at Y:(FW_PROXIMITY_PATTERN + 0)
	REP	X0				; Repeat next line R0 (Y:<FW_POS) times
	MOVE    Y:(R5)+,X1			; X1 = proximity pattern at Y:(FW_PROXIMITY_PATTERN++)
	MOVE    Y:(R5),X1			; X1 = proximity pattern at Y:(FW_PROXIMITY_PATTERN + Y:<FW_POS)
	MOVE	X1,Y:<FW_TARGET_PROXIMITY_PATTERN ; Y:<FW_TARGET_PROXIMITY_PATTERN = X1 = proximity pattern
; reset move timeout FW_MOVE_TIMEOUT to zero
	MOVE	X:<ZERO,X0			; X0 = 0
	MOVE	X0,Y:<FW_MOVE_TIMEOUT		; FW_MOVE_TIMEOUT = 0
; reset error code to 0
	MOVE	X0,Y:<FW_ERROR_CODE		; FW_ERROR_CODE = X0 = 0
; Set relevant ST_FW_MOVE and ST_FW_ENGAGE_CLUTCH bits in X:STATUS, so the ISR knows what we are doing
	BSET    #ST_FW_MOVE,X:<STATUS		; We are starting to move the filter wheel
	BSET    #ST_FW_ENGAGE_CLUTCH,X:<STATUS	; Run the engage clutch part of the filter wheel move ISR
	BCLR	#ST_FW_LOCATORS_OUT,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_READING_PROXIMITY,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_LOCATORS_IN,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_DISENGAGE_CLUTCH,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	BCLR	#ST_FW_DISENGAGE_CLUTCH,X:<STATUS	; Ensure rest of filter wheel status bits are not set
	JMP	<FINISH				; Issue 'DON' and get next command

; Filter wheel abort: FWA
; Resets ST_FW_RESET and ST_FW_MOVE STATUS bits (X:<STATUS)
; Clears filter wheel output bits DIG_OUT_BIT_MOTOR_ON
FILTER_WHEEL_ABORT
	BCLR    #ST_FW_MOVE,X:<STATUS 		; Take out of filter wheel move mode.
; Leave the locators driven out
; Leave the clutch cyclinder engaged
; turn off the motor
	BCLR	#DIG_OUT_BIT_MOTOR_ON,Y:<DIG_OUT	; turn motor off
; Set error code to 1: Abort
	MOVE	#>9,X0					; X0 = 1
	MOVE	X0,Y:<FW_ERROR_CODE			; Y:<FW_ERROR_CODE = X0 = 1
	JMP	<FINISH				; Issue 'DON' and get next command


       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.1  2011/06/02 14:06:16  cjm
	Initial revision

	*
