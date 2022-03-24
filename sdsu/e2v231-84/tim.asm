       COMMENT *

This file is used to generate boot DSP code for the 250 MHz fiber optic
	timing board using a DSP56303 as its main processor. It supports 
	frame transfer and binning.

This was originally the latest timing board code for the e2v230 chip.
Now porting the older fif486 code, hopefully to end up supporting bif486 (back illuminated Fairchild 486).
	*
	PAGE    132     ; Printronix page width - 132 columns

; Include the boot file so addressing is easy
	INCLUDE	"timboot.asm"
	
	ORG	P:,P:

CC	EQU	ARC22+ARC48+ARC50+SHUTTER_CC+SUBARRAY+SPLIT_PARALLEL+SPLIT_SERIAL+BINNING

; Put number of words of application in P: for loading application from EEPROM
	DC	TIMBOOT_X_MEMORY-@LCV(L)-1

; Define CLOCK as a macro to produce in-line code to reduce execution time
CLOCK	MACRO
	JCLR	#SSFHF,X:HDR,*		; Don't overfill the WRSS FIFO
	REP	Y:(R0)+			; Repeat
	MOVEP	Y:(R0)+,Y:WRSS		; Write the waveform to the FIFO
	ENDM

; Set software to IDLE mode
START_IDLE_CLOCKING
	MOVE	#IDLE,R0		; Exercise clocks when idling
	MOVE	R0,X:<IDL_ADR
	BSET	#IDLMODE,X:<STATUS	; Idle after readout
	JMP     <FINISH			; Need to send header and 'DON'

; Keep the CCD idling when not reading out
IDLE	DO      Y:<NSR,IDL1     	; Loop over number of pixels per line
	MOVE    #<SERIAL_IDLE,R0 	; Serial transfer on pixel
	CLOCK  				; Go to it
	MOVE	#COM_BUF,R3
	JSR	<GET_RCV		; Check for FO or SSI commands
	JCC	<NO_COM			; Continue IDLE if no commands received
	ENDDO
	JMP     <PRC_RCV		; Go process header and command
NO_COM	NOP
IDL1
	MOVE    #<PARALLEL_CLEAR_SPLIT,R0 ; Address of parallel clocking waveform
	CLOCK  				; Go clock out the CCD charge
	JMP     <IDLE

;  *****************  Exposure and readout routines  *****************

; Overall loop - transfer and read NPR lines
RDCCD

; Test to see if the binning or image size parameters have changed
	JSET	#ST_DIRTY,X:STATUS,CHANGED ; Test if binning parameters have changed
	MOVE	Y:<LAST_NSBIN,A		; Previous binning factor
	MOVE	Y:<NSBIN,X0		; Current binning factor
	CMP	X0,A
	JNE	<CHANGED		; If changed, re-calculate everything
	MOVE	Y:<LAST_NPBIN,A
	MOVE	Y:<NPBIN,X0
	CMP	X0,A
	JEQ	<CHANGED
	MOVE	Y:<LAST_NSR,A		; Test if dimensions have changed
	MOVE	Y:<NSR,X0
	CMP	X0,A
	JNE	<CHANGED
	MOVE	Y:<LAST_NPR,A
	MOVE	Y:<NPR,X0
	CMP	X0,A
	JEQ	<P_SHIFT		; If all are unchanged go read out

; Calculate readout parameters for whole image readout
CHANGED	MOVE	Y:<NBOXES,A		; NBOXES = 0 => full image readout
	TST	A
	JNE	<SUB_IMG
	MOVE	A1,Y:<NP_SKIP		; = 0 since NBOXES = 0	
	MOVE	#PRESKIP,X0
	MOVE	X0,Y:<NS_SKP1
	MOVE	A1,Y:<NS_SKP2		; = 0 since NBOXES = 0
	MOVE	A1,Y:<NR_BIAS		; = 0 since NBOXES = 0
	MOVE	Y:<NSR,X0		; NS_READ = NSR
	MOVE	X0,Y:<NS_READ
	MOVE	Y:<NPR,X0		; NP_READ = NPR
	MOVE	X0,Y:<NP_READ
	JMP	<GO_ON

; Set up for subarray readout
SUB_IMG	MOVE	#READ_TABLE,R7		; Parameter table for subimage readout
	NOP
	NOP
	MOVE	Y:(R7)+,A
	ADD	#PRESKIP,A
	NOP
	MOVE	A1,Y:<NP_SKIP
	MOVE	Y:(R7)+,X1		; NS_SKP1 = # to skip before the read
	MOVE	Y:<NSBIN,X0
	MPYuu	X0,X1,A
	ASR	A
	MOVE	A0,A
	NOP
	MOVE	A1,Y:<NS_SKP1
	MOVE	Y:(R7)+,X1		; NS_SKP2 = # to skip after the read
	MOVE	Y:<NSBIN,X0
	MPY	X0,X1,A
	ASR	A
	JGT	<SKP2_OK		; If NS_SKP2 .LE. = then set to zero
	CLR	A
	NOP
SKP2_OK	MOVE	A0,Y:<NS_SKP2
	MOVE	Y:<NSREAD,X0		; NS_READ = # of pixels to read
	MOVE	X0,Y:<NS_READ
	MOVE	Y:<NPREAD,X0		; NP_READ = # of rows to read
	MOVE	X0,Y:<NP_READ
	MOVE	Y:<NRBIAS,X0		; NR_BIAS = # of bias pixels to read
	MOVE	X0,Y:<NR_BIAS

; Generate new waveform and image dimensions
GO_ON	JSR	<GENERATE_SERIAL_WAVEFORM
	BCLR	#ST_DIRTY,X:<STATUS
	JCLR	#SPLIT_S,X:STATUS,SPL_PAR
	MOVE	Y:<NS_READ,A		; Split serials require / 2
	NOP
	LSR	A
	NOP
	MOVE	A1,Y:<NS_READ
	MOVE	Y:<NR_BIAS,A		; Number of bias pixels to read
	NOP
	LSR	A
	NOP
	MOVE	A,Y:<NR_BIAS

SPL_PAR	JCLR	#SPLIT_P,X:STATUS,DUMMY_NSR_MOD
	MOVE	Y:<NP_READ,A		; Split parallels require / 2
	NOP
	LSR	A
	NOP
	MOVE	A1,Y:<NP_READ
DUMMY_NSR_MOD
	JCLR	#ST_DUMMY,X:STATUS,WT_CLK
	; Dummy outputs: divide number of pixels by two NS_READ /= 2 ; NR_BIAS /= 2 ;
	; As each serial pixel read will now produce twice as many pixels as real amplifiers used.
	MOVE	Y:<NS_READ,A		
	NOP
	LSR	A
	NOP
	MOVE	A1,Y:<NS_READ
	MOVE	Y:<NR_BIAS,A		; Number of bias pixels to read
	NOP
	LSR	A
	NOP
	MOVE	A,Y:<NR_BIAS

; Update the image dimensions and binning factors being used
WT_CLK	MOVE	Y:<NSBIN,X0
	MOVE	X0,Y:<LAST_NSBIN
	MOVE	Y:<NPBIN,X0
	MOVE	X0,Y:<LAST_NPBIN		; Previous binning factors = current
	MOVE	Y:<NSR,X0
	MOVE	X0,Y:<LAST_NSR
	MOVE	Y:<NPR,X0
	MOVE	X0,Y:<LAST_NPR		; Previous readout dimensions = current

; Skip over the required number of rows for subimage readout
P_SHIFT	DO      Y:<NP_SKIP,L_PSKIP	
	DO	Y:<NPBIN,L_PS
	MOVE    Y:<PARALLEL_CLEAR,R0
	CLOCK
L_PS	NOP
L_PSKIP

; *******  Begin readout over the entire array  ******
; This is the main loop over each line to be read out
	DO	Y:<NP_READ,LPR
	MOVE    Y:<PARALLEL_START,R0
	CLOCK
; subtract 1 from bin parallel bining NPBIN, and call the BIN waveform that many times
        MOVE    Y:<NPBIN,A
	SUB	#1,A
	JLE	L_SP			; Don't loop zero or a negative number
	NOP
	DO      A1,L_SP
	MOVE    Y:<PARALLEL_BIN,R0
	CLOCK
	NOP
L_SP
	MOVE    Y:<PARALLEL_END,R0
	CLOCK

; Check for a command once per line. Only the ABORT command should be issued.
	MOVE	#COM_BUF,R3
	JSR	<GET_RCV		; Was a command received?
	JCC	<CONTINUE_READ		; If no, continue reading out
	JMP	<PRC_RCV		; If yes, go process it

; Abort the readout currently underway
ABR_RDC	JCLR	#ST_RDC,X:<STATUS,ABORT_EXPOSURE
	ENDDO				; Properly terminate readout loop
	JMP	<ABORT_EXPOSURE
CONTINUE_READ	

; Do a fast skip over NS_SKP1 pixels
	DO	Y:<NS_SKP1,L_S
	MOVE	Y:<SERIAL_SKIP,R0
	CLOCK 
L_S

; Clock and video process prescan "pixels" without SXMIT to fill the pipeline
; There are 50 PRESCAN pixels according to the SPEC sheet, but if we set PRESCAN to 0 we will get the
; PRESCAN data on the final image.
	MOVE	Y:SXMIT_ADR,R0
	MOVE	#>$74,X0
	MOVE	X0,Y:(R0)
	DO	#PRESCAN,L_3
	MOVE	#PXL_TBL,R0		; Serial processing without SXMIT
	CLOCK 
L_3
	MOVE	Y:SXMIT_ADR,R0
	MOVE	Y:SXMIT,X0
	MOVE	X0,Y:(R0)

; Clock, video process and SXMIT remaining pixels minus 3 video processor pixels
	MOVE	Y:<NS_READ,A
	SUB	#3,A
	JLE	L_RD			; Don't loop zero or a negative number
	NOP
	DO	A1,L_RD
	MOVE	#PXL_TBL,R0
	CLOCK
L_RD

; Skip over NS_SKP2 pixels if needed for subimage readout
	MOVE	Y:<NS_SKP2,A		; Protect against negative values
	TST	A
	JLE	<RDBIAS
	DO	Y:<NS_SKP2,L_SB
	MOVE	Y:<SERIAL_SKIP,R0
	CLOCK
L_SB

; Read the bias pixels if needed for subimage readout
RDBIAS	MOVE	Y:<NR_BIAS,A		; Protect against negative values
	TST	A
	JLE	<PIPE

; Clock and video process some pixels without SXMIT to fill the pipeline  
	MOVE	Y:SXMIT_ADR,R0
	MOVE	#>$74,X0
	MOVE	X0,Y:(R0)
	DO	#PRESCAN,L_4
	MOVE	#PXL_TBL,R0		; Serial processing without SXMIT
	CLOCK 
L_4
	MOVE	Y:SXMIT_ADR,R0
	MOVE	Y:SXMIT,X0
	MOVE	X0,Y:(R0)

	DO      Y:<NR_BIAS,PIPE
	MOVE	#PXL_TBL,R0
	CLOCK
L_RB	NOP

; SXMIT the pixels left over in the pipeline: three pixels  
PIPE	DO	#3,L_L3
	MOVE	#PXL_TBL,R0
	CLOCK
L_L3	NOP				; End of parallel loop
LPR

; Restore the controller to non-image data transfer and idling if necessary
RDC_END	JCLR	#IDLMODE,X:<STATUS,NO_IDL
	MOVE	#IDLE,R0
	MOVE	R0,X:<IDL_ADR
	JMP	<RDC_E
NO_IDL	MOVE	#TST_RCV,R0	 	; Don't idle after readout
	MOVE	R0,X:<IDL_ADR
RDC_E	JSR	<WAIT_TO_FINISH_CLOCKING
	BCLR	#ST_RDC,X:<STATUS	; Set status to not reading out
        JMP     <START

; ******  Include many routines not directly needed for readout  *******
	INCLUDE "timCCDmisc.asm"


TIMBOOT_X_MEMORY	EQU	@LCV(L)

;  ****************  Setup memory tables in X: space ********************

; Define the address in P: space where the table of constants begins

	IF	@SCP("DOWNLOAD","HOST")
	ORG     X:END_COMMAND_TABLE,X:END_COMMAND_TABLE
	ENDIF

	IF	@SCP("DOWNLOAD","ROM")
	ORG     X:END_COMMAND_TABLE,P:
	ENDIF

; Application commands
	DC	'PON',POWER_ON
	DC	'POF',POWER_OFF
	DC	'SBV',SET_BIAS_VOLTAGES
	DC	'IDL',START_IDLE_CLOCKING
	DC	'RDC',STR_RDC    
	DC	'CLR',CLEAR   

; Exposure and readout control routines
	DC	'SET',SET_EXPOSURE_TIME
	DC	'RET',READ_EXPOSURE_TIME
	DC	'SEX',START_EXPOSURE
	DC	'PEX',PAUSE_EXPOSURE
	DC	'REX',RESUME_EXPOSURE
	DC	'AEX',ABORT_EXPOSURE
	DC	'ABR',ABR_RDC
	DC	'CRD',CONTINUE_READ
	DC	'OSH',OPEN_SHUTTER
	DC	'CSH',CLOSE_SHUTTER
	
; Support routines
	DC	'SGN',ST_GAIN      
	DC	'SBN',SET_BIAS_NUMBER
	DC	'SMX',SET_MUX
	DC	'CSW',CLR_SWS
	DC	'SOS',SELECT_OUTPUT_SOURCE
	DC	'SSS',SET_SUBARRAY_SIZES
	DC	'SSP',SET_SUBARRAY_POSITIONS
	DC	'RCC',READ_CONTROLLER_CONFIGURATION

; More commands
	DC	'SPT',SET_PIXEL_TIME
	DC	'GWF',GENERATE_WAVEFORM

END_APPLICATON_COMMAND_TABLE	EQU	@LCV(L)

	IF	@SCP("DOWNLOAD","HOST")
NUM_COM			EQU	(@LCV(R)-COM_TBL_R)/2	; Number of boot + 
							;  application commands
EXPOSING		EQU	CHK_TIM			; Address if exposing
CONTINUE_READING	EQU	RDCCD	 		; Address if reading out
	ENDIF

	IF	@SCP("DOWNLOAD","ROM")
	ORG     Y:0,P:
	ENDIF

; Now let's go for the timing waveform tables
	IF	@SCP("DOWNLOAD","HOST")
        ORG     Y:0,Y:0
	ENDIF

GAIN	DC	END_APPLICATON_Y_MEMORY-@LCV(L)-1
; NSR's location should match ccd_setup.c:SETUP_ADDRESS_DIMENSION_COLS.
NSR     DC      4400   	 	; Number Serial Read, set by host computer
; NPR's location should match ccd_setup.c:SETUP_ADDRESS_DIMENSION_ROWS.
NPR     DC      4096	     	; Number Parallel Read, set by host computer
NS_CLR	DC      NSCLR          ; To clear the serial register
NP_CLR	DC	NPCLR		; To clear the parallel register 
; NSBIN's location should match ccd_setup.c:SETUP_ADDRESS_BIN_X
NSBIN   DC      1       	; Serial binning parameter
; NPBIN's location should match ccd_setup.c:SETUP_ADDRESS_BIN_Y
NPBIN   DC      1       	; Parallel binning parameter
TST_DATA DC	0		; For synthetic image
CONFIG	DC	CC		; Controller configuration
NS_READ	DC	0		; Number of serials to read
NP_READ	DC	0		; Number of parallels to read
NR_BIAS	DC	0		; Number of bias pixels to read
NROWS	DC	40		; Number of rows in the storage region
SBIN	DC	1		; Current serial binning number
OS	DC	'__D'		; Name of the output source(s)
LAST_CLK 	DC	0	; Last clock before SXMIT
SXMIT_ADR 	DC	0	; Address of SXMIT value
LAST_SXMIT 	DC	0	; Last value of SXMIT
SYN_DAT		DC	0	; Synthetic image mode pixel count
LAST_NSR 	DC	0	; Last used number of columns
LAST_NPR 	DC	0	; Last used number of rows
LAST_NSBIN 	DC	0	; Last used serial binning number
LAST_NPBIN 	DC	0	; Last used parallel binning number
SHDEL		DC	SH_DEL	; Delay from shutter close to start of readout

; Waveform table addresses
PARALLEL_START 		DC	PARALLEL_SPLIT_START
PARALLEL_BIN 		DC	PARALLEL_SPLIT_BIN
PARALLEL_END 		DC	PARALLEL_SPLIT_END
PARALLEL_CLEAR 		DC	PARALLEL_CLEAR_SPLIT
SERIAL_SKIP 		DC	SERIAL_SKIP_SPLIT
FIRST_CLOCKS 		DC 	FIRST_CLOCKS_SPLIT
CLOCK_LINE 		DC 	CLOCK_LINE_SPLIT
SXMIT			DC 	$00F0C0
CHARGE_DUMP 		DC 	CHARGE_DUMP_SPLIT
INTEGRATE_SIGNAL 	DC 	INTEGRATE_SIGNAL_SPLIT

; These three parameters are read from the READ_TABLE when needed by the
;   RDCCD routine as it loops through the required number of boxes
NP_SKIP	DC	0		; Number of rows to skip
NS_SKP1	DC	0		; Number of serials to clear before read
NS_SKP2	DC	0		; Number of serials to clear after read

; Subimage readout parameters. One subimage box only
NBOXES	DC	0		; Number of boxes to read
NRBIAS	DC	0		; Number of bias pixels to read
NSREAD	DC	0		; Number of columns in subimage read
NPREAD	DC	0		; Number of rows in subimage read
READ_TABLE DC	0,0,0		; #1 = Number of rows to clear 
				; #2 = Number of columns to skip before 
				;   subimage read 
				; #3 = Number of rows to clear after 
				;   subimage clear
; Include the waveform table for the designated type of CCD
	INCLUDE "WAVEFORM_FILE" ; Readout and clocking waveform file
; cjm
; PXL_TBL must be the last entry before END_APPLICATON_Y_MEMORY
; At readout this memory location is the head of a list of real-time generated serial readout clocks
; generated by GENERATE_SERIAL_WAVEFORM
PXL_TBL DC      0               ;  cjm

END_APPLICATON_Y_MEMORY	EQU	@LCV(L)

; End of program
	END

