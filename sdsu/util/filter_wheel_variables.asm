; filter_wheel_variables.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_variables.asm,v 1.5 2014-01-27 16:11:25 cjm Exp $
	*

; Note the default values of the variables and the generated memory locations of these variables
; will need updating in libo_ccd:ccd_filter_wheel.c if they change.

FW_POS_COUNT	DC	       12	; The number of filters in the wheel
FW_LAST_POS	DC	       -1	; The last absolute position the filter wheel was in (0..N-1)
FW_TARGET_POS	DC	       0	; The target absolute position we are moving the filter wheel to (0..N-1)
FW_OFFSET_POS	DC	       0	; How many positions to move from FW_LAST_POS to FW_TARGET_POS
FW_TARGET_PROXIMITY_PATTERN DC 0	; The proximity pattern for the position we are heading towards
FW_ERROR_CODE	DC	       0	; Error code set when a filter wheel command fails
FW_TIMEOUT_INDEX DC	       0	; Number of times round the ISR for this move
FW_TIMEOUT_COUNT DC	       10000	; Max Number of times round the 0.8ms ISR before a timout

; Set of bit masks. When the proximity sensors in Y:DIG_IN match the mask, the wheel is in the position specified by
; the index in this list.
; The proximity sensors go low (0) when they are triggered.
; Old values when proximity sensors were in bits 8-13, before DIN10 blew
; Proximity sensors are bits DIG_IN_BIT_PROXIMITY_1_ON (8) to DIG_IN_BIT_PROXIMITY_6_ON (13)
; 00000000
; bitedcba98
; 1   100110
; 2   111001
; 3   010111
; 4   001011
; 5   110110
; 6   101100
; 7   011101
; 8   101011
; 9   010010
; 10  110101
; 11  011110
; 12  101001
;FW_PROXIMITY_PATTERN	DC  %10011000000000,%11100100000000,%01011100000000,%00101100000000
;			DC  %11011000000000,%10110000000000,%01110100000000,%10101100000000
;			DC  %01001000000000,%11010100000000,%01111000000000,%10100100000000
; Proximity sensors are bits DIG_IN_BIT_PROXIMITY_1_ON (8) to DIG_IN_BIT_PROXIMITY_6_ON (13)
; 00000000
; bitedcba98
; 1  1100010
; 2  0111001
; 3  1010011
; 4  0001011
; 5  1110010
; 6  1101000
; 7  1011001
; 8  0101011
; 9  0010010
; 10 1110001
; 11 1011010
; 12 0101001
FW_PROXIMITY_PATTERN	DC  %110001000000000,%011100100000000,%101001100000000,%000101100000000
			DC  %111001000000000,%110100000000000,%101100100000000,%010101100000000
			DC  %001001000000000,%111000100000000,%101101000000000,%010100100000000

; tmp variable
;FW_TMP1			DC 0

       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.4  2011/08/18 16:48:14  cjm
	First working version of detent counting code.
	Proximity sensors now go low (0) when they are triggered.
	FW_TIMEOUT_COUNT shortened.
	More comments added.

	Revision 1.3  2011/07/26 13:58:48  cjm
	Added more variables for proximity in/out counting. Renamed timout variables.

	Revision 1.2  2011/07/21 10:48:02  cjm
	First O version, with clutch engage/disengage and too many locator outputs.

	Revision 1.1  2011/06/02 14:06:31  cjm
	Initial revision

	*
