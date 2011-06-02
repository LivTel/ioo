; filter_wheel_variables.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_variables.asm,v 1.1 2011-06-02 14:06:31 cjm Exp $
	*

;Note the default values of the variables and the generated memory locations of these variables
;will need updating in libo_ccd:ccd_filter_wheel.c if they change.

; The number of times through the 0.8ms ISR before we turn on detent checking.
FW_DETENT_CHECK_COUNT	DC	1000	; How many times round the ISR out of the detent before we start
				        ; checking for the next detent.
				        ; Wheel moves at 8rpm :	 1 rev = 7.5 seconds:
				        ; 5 position wheel. 1 position = 1.5 seconds, 1500 ms,
				        ; wheel takes 1875 ISRs between detents.
				        ; 7 position wheel. 1 position = 1.07 seconds, 1071 ms,
				        ; wheel takes 1339 ISR's between detents.
FW_DETENT_CHECK_NUM	DC	0	; Incremented in the 0.8ms ISR, reset when we reach FW_DETENT_CHECK_COUNT 
					; (and we start checking for detents).
FW_POS_MOVE 	DC	0	; The number of positions (detent positions) we need to move. This is decremented
				; each time we enter a detent, and the move stops when it is zero.

       COMMENT * 
	$Log: not supported by cvs2svn $
	*
