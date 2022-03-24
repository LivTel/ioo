; filter_wheel_equ.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_equ.asm,v 1.5 2014-01-27 16:07:08 cjm Exp $
	This include file defines EQUates used by the filter wheel.
	Some of these define what we are using the digital input and output bits for,
	located at DIG_IN (Y:0) and DIG_OUT (Y:1).
	Some define bits for the status word STATUS (X:0). Note the first five (0..4) bits are already used.
	This source file should be included after the "Bit definitions of STATUS word" in utilappl.asm.
	Note the values of the bit definitions will need updating in libo_ccd:ccd_filter_wheel.c if they change.
	Version: $Revision: 1.5 $
	Author: $Author: cjm $
	*
;Note the values of the bit definitions will need updating in libo_ccd:ccd_filter_wheel.c if they change.

;  Extra Bit definitions of STATUS word
ST_FW_MOVE		EQU	5	; Set if a filter wheel move (FWM) operation is in progress.
ST_FW_RESET		EQU	6	; Set if a filter wheel reset (FWR) operation is in progress.
ST_FW_LOCATORS_OUT	EQU	7	; Set if the ISR is currently moving the locators out.
ST_FW_MOVE_IN_POSITION	EQU	8	; Set if the ISR is currently moving the wheel, and it is not yet in a position
ST_FW_MOVE_OUT_POSITION EQU	9	; Set if the ISR is currently moving the wheel, and it is in position and moving out
ST_FW_LOCATORS_IN	EQU	10	; Set if the ISR is currently moving the locators in.

; Bit definitions of the digital input word DIG_IN (Y:0)
; Note these values take the form of a bit number i.e. '4' means the fourth bit is set,
; rather than a bit mask number (1<<'4') == 16
; This is so they can be used with BSET,BCLR,JSET,JCLR
DIG_IN_BIT_LOCATOR_1_IN	      EQU	0	; (1<<0) Clear when locator 1 is in the in position
DIG_IN_BIT_LOCATOR_1_OUT      EQU	1	; (1<<1) Clear when locator 1 is in the out position
DIG_IN_BIT_LOCATOR_2_IN	      EQU	2	; (1<<2) Clear when locator 2 is in the in position
DIG_IN_BIT_LOCATOR_2_OUT      EQU	3	; (1<<3) Clear when locator 2 is in the out position
DIG_IN_BIT_LOCATOR_3_IN	      EQU	4	; (1<<4) Clear when locator 3 is in the in position
DIG_IN_BIT_LOCATOR_3_OUT      EQU	5	; (1<<5) Clear when locator 3 is in the out position
DIG_IN_BIT_CLUTCH_ENGAGED     EQU	6	; (1<<6) Set when motor clutch is engaged
DIG_IN_BIT_CLUTCH_DISENGAGED  EQU	7	; (1<<7) Set when motor clutch is disengaged

DIG_IN_BIT_PROXIMITY_1_ON     EQU	8	; (1<<8) Clear when proximity sensor 1 is triggered
DIG_IN_BIT_PROXIMITY_2_ON     EQU	9	; (1<<9) Clear when proximity sensor 2 is triggered
; DIN10 blown on 01/2014 site trip (short/7ohm) . Swapped Prox 3 to DIN14
;DIG_IN_BIT_PROXIMITY_3_ON     EQU	10      ; (1<<10) Clear when proximity sensor 3 is triggered 
DIG_IN_BIT_PROXIMITY_3_ON     EQU	14	; (1<<14) Clear when proximity sensor 3 is triggered
DIG_IN_BIT_PROXIMITY_4_ON     EQU	11	; (1<<11) Clear when proximity sensor 4 is triggered
DIG_IN_BIT_PROXIMITY_5_ON     EQU	12	; (1<<12) Clear when proximity sensor 5 is triggered
DIG_IN_BIT_PROXIMITY_6_ON     EQU	13	; (1<<13) Clear when proximity sensor 6 is triggered

; used for ANDing with DIG_IN to get proximity sensors
; DIG_IN_PROXIMITY_MASK	      EQU	$003f00	; Bits 8-13 set, no longer works as bit DIN10 blown
DIG_IN_PROXIMITY_MASK	      EQU	$007b00	; Bits 8-9,11-14 set

; Bit definitions of the digital output word DIG_OUT (Y:1)
; digital outputs float high when the SDSU controller is turned on,
; and are set low (0) when the utility board application program is loaded.
DIG_OUT_BIT_LOCATORS_IN		EQU	0	; (1<<0) Set to push the locators in, clear to move the locators out
DIG_OUT_BIT_MOTOR_ON		EQU	1	; (1<<3) Set to turn motor on, clear to stop motor
DIG_OUT_BIT_CLUTCH_DISENGAGE    EQU	2	; (1<<4) Clear to engage clutch, set to disengage clutch


       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.4  2011/08/18 16:45:36  cjm
	First working version of detent counting code.

	Revision 1.3  2011/07/26 13:57:37  cjm
	Re-add reset status bit.
	Removed clutch engage/disengage status bits.
	All locators controlled by 1 output bit.

	Revision 1.2  2011/07/21 10:47:34  cjm
	First O version, with clutch engage/disengage and too many locator outputs.

	Revision 1.1  2011/06/02 14:06:13  cjm
	Initial revision

	*
