; filter_wheel_variables.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_variables.asm,v 1.3 2011-07-26 13:58:48 cjm Exp $
	*

;Note the default values of the variables and the generated memory locations of these variables
;will need updating in libo_ccd:ccd_filter_wheel.c if they change.

FW_POS_COUNT	DC	       12	; The number of filters in the wheel
FW_LAST_POS	DC	       -1	; The last absolute position the filter wheel was in
FW_TARGET_POS	DC	       0	; The target absolute position we are moving the filter wheel to
FW_OFFSET_POS	DC	       0	; How many positions to move from FW_LAST_POS to FW_TARGET_POS
FW_TARGET_PROXIMITY_PATTERN DC 0	; The proximity pattern for the position we are heading towards
FW_ERROR_CODE	DC	       0	; Error code set when a filter wheel command fails
FW_TIMEOUT_INDEX DC	       0	; Number of times round the ISR for this move
FW_TIMEOUT_COUNT DC	       100000	; Max Number of times round the ISR before a timout

; Set of bit masks. When the proximity sensors in Y:DIG_IN match the mask, the wheel is in the position specified by
; the index in this list.
; Proximity sensors are bits DIG_IN_BIT_PROXIMITY_1_ON (8) to DIG_IN_BIT_PROXIMITY_6_ON (13)
; 00000000
; 1  100110
; 2  011000
; 3  000101
; 4  001011
; 5  100100
; 6  110010
; 7  010001
; 8  001010
; 9  101100
; 10 010100
; 11 100001
; 12 011010
FW_PROXIMITY_PATTERN	DC  %10011000000000,%01100000000000,%00010100000000,%00101100000000,%10010000000000
			DC  %11001000000000,%01000100000000,%00101000000000,%10110000000000,%01010000000000
			DC  %10000100000000,%01101000000000

; tmp variable
FW_TMP1			DC 0

       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.2  2011/07/21 10:48:02  cjm
	First O version, with clutch engage/disengage and too many locator outputs.

	Revision 1.1  2011/06/02 14:06:31  cjm
	Initial revision

	*
