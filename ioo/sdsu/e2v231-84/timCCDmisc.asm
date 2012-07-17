; $Header: /space/home/eng/cjm/cvs/ioo/sdsu/e2v231-84/timCCDmisc.asm,v 1.2 2012-07-17 17:34:04 cjm Exp $
; Copied from e2v230 version.
; Various changed imported from fif486 version
; Miscellaneous CCD control routines
POWER_OFF
	JSR	<CLEAR_SWITCHES		; Clear all analog switches
	BSET	#LVEN,X:HDR 
	BSET	#HVEN,X:HDR 
	JMP	<FINISH

; Execute the power-on cycle, as a command
POWER_ON
	JSR	<CLEAR_SWITCHES		; Clear all analog switches
	JSR	<PON			; Turn on the power control board
	JCLR	#PWROK,X:HDR,PWR_ERR	; Test if the power turned on properly
	JSR	<SET_BIASES		; Turn on the DC bias supplies
	MOVE	#IDLE,R0		; Put controller in IDLE state
	MOVE	R0,X:<IDL_ADR
	MOVE	#$1064,X0
	MOVE	X0,X:<STATUS
	JSR	<SEL_OS
	JMP	<FINISH

; The power failed to turn on because of an error on the power control board
PWR_ERR	BSET	#LVEN,X:HDR		; Turn off the low voltage emable line
	BSET	#HVEN,X:HDR		; Turn off the high voltage emable line
	JMP	<ERROR

; As a subroutine, turn on the low voltages (+/- 6.5V, +/- 16.5V) and delay
PON	BCLR	#LVEN,X:HDR		; Set these signals to DSP outputs 
	MOVE	#2000000,X0
	DO      X0,*+3			; Wait 20 millisec for settling
	NOP 	

; Turn on the high +36 volt power line and then delay
	BCLR	#HVEN,X:HDR		; HVEN = Low => Turn on +36V
	MOVE	#2000000,X0
	DO      X0,*+3			; Wait 20 millisec for settling
	NOP
	RTS

; Set all the DC bias voltages and video processor offset values, reading
;   them from the 'DACS' table
SET_BIASES
	BSET	#3,X:PCRD		; Turn on the serial clock
	BCLR	#1,X:<LATCH		; Separate updates of clock driver
	BSET	#CDAC,X:<LATCH		; Disable clearing of DACs
	BSET	#ENCK,X:<LATCH		; Enable clock and DAC output switches
	MOVEP	X:LATCH,Y:WRLATCH	; Write it to the hardware
	JSR	<PAL_DLY		; Delay for all this to happen

; Read DAC values from a table, and write them to the DACs
	MOVE	#DACS,R0		; Get starting address of DAC values
	NOP
	NOP
	DO      Y:(R0)+,L_DAC		; Repeat Y:(R0)+ times
	MOVE	Y:(R0)+,A		; Read the table entry
	JSR	<XMIT_A_WORD		; Transmit it to TIM-A-STD
	NOP
L_DAC

; Let the DAC voltages all ramp up before exiting
	MOVE	#400000,X0
	DO	X0,*+3			; 4 millisec delay
	NOP
	BCLR	#3,X:PCRD		; Turn the serial clock off
	RTS

SET_BIAS_VOLTAGES
	JSR	<SET_BIASES
	JMP	<FINISH

CLR_SWS	JSR	<CLEAR_SWITCHES
	JMP	<FINISH

; Clear all video processor analog switches to lower their power dissipation
CLEAR_SWITCHES
	BSET	#3,X:PCRD	; Turn the serial clock on
	MOVE	#$0C3000,A	; Value of integrate speed and gain switches
	CLR	B
	MOVE	#$100000,X0	; Increment over board numbers for DAC writes
	MOVE	#$001000,X1	; Increment over board numbers for WRSS writes
; we used to loop over fifteen (#15) video boards here, but we now loop over #2 as 
; addressing issues caused DAC entries to be overwritten in the clock driver board
	DO	#2,L_VIDEO	; Fifteen video processor boards maximum
	JSR	<XMIT_A_WORD	; Transmit A to TIM-A-STD
	ADD	X0,A
	MOVE	B,Y:WRSS
	JSR	<PAL_DLY	; Delay for the serial data transmission
	ADD	X1,B
L_VIDEO	
	BCLR	#CDAC,X:<LATCH		; Enable clearing of DACs
	BCLR	#ENCK,X:<LATCH		; Disable clock and DAC output switches
	MOVEP	X:LATCH,Y:WRLATCH 	; Execute these two operations
	BCLR	#3,X:PCRD		; Turn the serial clock off
	RTS

SET_SHUTTER_STATE
	MOVE	X:LATCH,A
	AND	#$FFEF,A
	OR	X0,A
	NOP
	MOVE	A1,X:LATCH
	MOVEP	A1,Y:WRLATCH	
	RTS	
	
; Open the shutter from the timing board, executed as a command
OPEN_SHUTTER
	BSET    #ST_SHUT,X:<STATUS 	; Set status bit to mean shutter open
	MOVE	#0,X0
	JSR	<SET_SHUTTER_STATE
	JMP	<FINISH

; Close the shutter from the timing board, executed as a command
CLOSE_SHUTTER
	BCLR    #ST_SHUT,X:<STATUS 	; Clear status to mean shutter closed
	MOVE	#>$10,X0
	JSR	<SET_SHUTTER_STATE
	JMP	<FINISH

; Shutter subroutines
OSHUT	BSET    #ST_SHUT,X:<STATUS 	; Set status bit to mean shutter open
	MOVE	#0,X0
	JSR	<SET_SHUTTER_STATE
	RTS

CSHUT	BCLR    #ST_SHUT,X:<STATUS 	; Clear status to mean shutter closed
	MOVE	#>$10,X0
	JSR	<SET_SHUTTER_STATE
	RTS

; Clear the CCD, executed as a command
CLEAR	
	;JSR	<CLR_CCD
	DO      Y:<NP_CLR,LPCLR2	; Loop over number of lines in image
	MOVE    #PARALLEL_CLEAR_SPLIT,R0 ; Address of parallel transfer waveform
	CLOCK
	NOP
LPCLR2
; clear serial register
	DO	Y:<NS_CLR,LSCLR2	; Clear out the serial shift register
	MOVE	#SERIAL_CLEAR,R0
	CLOCK
	NOP
LSCLR2
	JMP     <FINISH

; Start the exposure timer and monitor its progress
EXPOSE	MOVEP	#0,X:TLR0		; Load 0 into counter timer
	MOVE	#0,X0
	MOVE	X0,X:<ELAPSED_TIME	; Set elapsed exposure time to zero
	MOVE	X:<EXPOSURE_TIME,B
	TST	B			; Special test for zero exposure time
	JEQ	<END_EXP		; Don't even start an exposure
	SUB	#1,B			; Timer counts from X:TCPR0+1 to zero
	BSET	#TIM_BIT,X:TCSR0	; Enable the timer #0
	MOVE	B,X:TCPR0
CHK_RCV	JCLR    #EF,X:HDR,CHK_TIM	; Simple test for fast execution
	MOVE	#COM_BUF,R3		; The beginning of the command buffer
	JSR	<GET_RCV		; Check for an incoming command
	JCS	<PRC_RCV		; If command is received, go check it
CHK_TIM	JCLR	#TCF,X:TCSR0,CHK_RCV	; Wait for timer to equal compare value
END_EXP	BCLR	#TIM_BIT,X:TCSR0	; Disable the timer
	JMP	(R7)			; This contains the return address

; Start the exposure, expose, and initiate the CCD readout
START_EXPOSURE
	MOVE	#$020102,B
	JSR	<XMT_WRD
	MOVE	#'IIA',B		; Initialize the PCI image address
	JSR	<XMT_WRD
; Clearing the CCD is now done separatly from the C layer
;	JSR	<CLR_CCD
	MOVE	#TST_RCV,R0		; Process commands during the exposure
	MOVE	R0,X:<IDL_ADR
; Put serial clocks to exposure state
	MOVE	#SERIALS_EXPOSE,R0	; Put serial clocks in the correct state
        CLOCK
	JSR	<WAIT_TO_FINISH_CLOCKING
; Operate the shutter if needed and begin exposure
	JCLR	#SHUT,X:STATUS,L_SEX0
	JSR	<OSHUT
L_SEX0	MOVE	#L_SEX1,R7		; Return address at end of exposure
	JMP	<EXPOSE			; Delay for specified exposure time
L_SEX1

STR_RDC	JSR	<PCI_READ_IMAGE		; Get the PCI board reading the image
	BSET	#ST_RDC,X:<STATUS 	; Set status to reading out
	JCLR	#SHUT,X:STATUS,TST_SYN
	JSR	<CSHUT			; Close the shutter if necessary
TST_SYN	JSET	#TST_IMG,X:STATUS,SYNTHETIC_IMAGE

; Delay readout until the shutter has fully closed
	MOVE	Y:<SHDEL,A
	TST	A
	JLE	<S_DEL0
	MOVE	#100000,X0
	DO	A,S_DEL0		; Delay by Y:SHDEL milliseconds
	DO	X0,S_DEL1
	NOP
S_DEL1	NOP
S_DEL0	NOP

	JMP	<RDCCD			; Finally, go read out the CCD


; Set the desired exposure time
SET_EXPOSURE_TIME
	MOVE	X:(R3)+,Y0
	MOVE	Y0,X:EXPOSURE_TIME
	JCLR	#TIM_BIT,X:TCSR0,FINISH	; Return if exposure not occurring
	MOVE	Y0,X:TCPR0		; Update timer if exposure in progress
	JMP	<FINISH

; Read the time remaining until the exposure ends
READ_EXPOSURE_TIME
	JSET	#TIM_BIT,X:TCSR0,RD_TIM	; Read DSP timer if its running
	MOVE	X:<ELAPSED_TIME,Y1
	JMP	<FINISH1
RD_TIM	MOVE	X:TCR0,Y1		; Read elapsed exposure time
	JMP	<FINISH1

; Pause the exposure - close the shutter and stop the timer
PAUSE_EXPOSURE
	MOVEP	X:TCR0,X:ELAPSED_TIME	; Save the elapsed exposure time
	BCLR    #TIM_BIT,X:TCSR0	; Disable the DSP exposure timer
	JSR	<CSHUT			; Close the shutter
	JMP	<FINISH

; Resume the exposure - open the shutter if needed and restart the timer
RESUME_EXPOSURE
	MOVEP	X:ELAPSED_TIME,X:TLR0	; Restore elapsed exposure time
	BSET	#TIM_BIT,X:TCSR0	; Re-enable the DSP exposure timer
	JCLR	#SHUT,X:STATUS,L_RES
	JSR	<OSHUT			; Open the shutter if necessary
L_RES	JMP	<FINISH

; Abort exposure - close the shutter, stop the timer and resume idle mode
ABORT_EXPOSURE
	JSR	<CSHUT			; Close the shutter
	BCLR    #TIM_BIT,X:TCSR0	; Disable the DSP exposure timer
	JCLR	#IDLMODE,X:<STATUS,NO_IDL2 ; Don't idle after readout
	MOVE	#IDLE,R0
	MOVE	R0,X:<IDL_ADR
	JMP	<RDC_E2
NO_IDL2	MOVE	#TST_RCV,R0
	MOVE	R0,X:<IDL_ADR
RDC_E2	JSR	<WAIT_TO_FINISH_CLOCKING
	BCLR	#ST_RDC,X:<STATUS	; Set status to not reading out
	DO      #4000,*+3		; Wait 40 microsec for the fiber
	NOP				;  optic to clear out 
	JMP	<FINISH

; Generate a synthetic image by simply incrementing the pixel counts
SYNTHETIC_IMAGE
	CLR	A
	DO      Y:<NPR,LPR_TST      	; Loop over each line readout
	DO      Y:<NSR,LSR_TST		; Loop over number of pixels per line
	REP	#20			; #20 => 1.0 microsec per pixel
	NOP
	ADD	#1,A			; Pixel data = Pixel data + 1
	NOP
	MOVE	A,B
	JSR	<XMT_PIX		;  transmit them
	NOP
LSR_TST	
	NOP
LPR_TST	
        JMP     <RDC_END		; Normal exit

; Transmit the 16-bit pixel datum in B1 to the host computer
XMT_PIX	ASL	#16,B,B
	NOP
	MOVE	B2,X1
	ASL	#8,B,B
	NOP
	MOVE	B2,X0
	NOP
	MOVEP	X1,Y:WRFO
	MOVEP	X0,Y:WRFO
	RTS

; Test the hardware to read A/D values directly into the DSP instead
;   of using the SXMIT option, A/Ds #2 and 3.
READ_AD	MOVE	X:(RDAD+2),B
	ASL	#16,B,B
	NOP
	MOVE	B2,X1
	ASL	#8,B,B
	NOP
	MOVE	B2,X0
	NOP
	MOVEP	X1,Y:WRFO
	MOVEP	X0,Y:WRFO
	REP	#10
	NOP
	MOVE	X:(RDAD+3),B
	ASL	#16,B,B
	NOP
	MOVE	B2,X1
	ASL	#8,B,B
	NOP
	MOVE	B2,X0
	NOP
	MOVEP	X1,Y:WRFO
	MOVEP	X0,Y:WRFO
	REP	#10
	NOP
	RTS

; Alert the PCI interface board that images are coming soon
PCI_READ_IMAGE
	MOVE	#$020104,B		; Send header word to the FO xmtr
	JSR	<XMT_WRD
	MOVE	#'RDA',B
	JSR	<XMT_WRD
	MOVE	Y:NSR,B			; Number of columns to read
	JSR	<XMT_WRD
	MOVE	Y:NPR,B			; Number of rows to read
	JSR	<XMT_WRD
	RTS

; Wait for the clocking to be complete before proceeding
WAIT_TO_FINISH_CLOCKING
	JSET	#SSFEF,X:PDRD,*		; Wait for the SS FIFO to be empty
	RTS

; Delay for serial writes to the PALs and DACs by 8 microsec
PAL_DLY	DO	#800,*+3		; Wait 8 usec for serial data xmit
	NOP
	RTS

; Let the host computer read the controller configuration
READ_CONTROLLER_CONFIGURATION
	MOVE	Y:<CONFIG,Y1		; Just transmit the configuration
	JMP	<FINISH1

; Set the video processor gain and integrator speed for all video boards
;  Command syntax is  SGN  #GAIN  #SPEED, #GAIN = 1, 2, 5 or 10	
;					  #SPEED = 0 for slow, 1 for fast
ST_GAIN	BSET	#3,X:PCRD	; Turn on the serial clock
	MOVE	X:(R3)+,A	; Gain value (1,2,5 or 10)
	MOVE	#>1,X0
	CMP	X0,A		; Check for gain = x1
	JNE	<STG2
	MOVE	#>$77,B
	JMP	<STG_A
STG2	MOVE	#>2,X0		; Check for gain = x2
	CMP	X0,A
	JNE	<STG5
	MOVE	#>$BB,B
	JMP	<STG_A
STG5	MOVE	#>5,X0		; Check for gain = x5
	CMP	X0,A
	JNE	<STG10
	MOVE	#>$DD,B
	JMP	<STG_A
STG10	MOVE	#>10,X0		; Check for gain = x10
	CMP	X0,A
	JNE	<ERROR
	MOVE	#>$EE,B

STG_A	MOVE	X:(R3)+,A	; Integrator Speed (0 for slow, 1 for fast)
	NOP
	JCLR	#0,A1,STG_B
	BSET	#8,B1
	NOP
	BSET	#9,B1
STG_B	MOVE	#$0C3C00,X0
	OR	X0,B
	NOP
	MOVE	B,Y:<GAIN	; Store the GAIN value for later use

; Send this same value to 15 video processor boards whether they exist or not
	MOVE	#$100000,X0	; Increment value
; This used to loop round over 15 Video boards (#15).
; Unfortunately this changed the P1V DAC voltage on the clock driver board due to an addressing issue.
	DO	#2,STG_LOOP
	MOVE	B1,A1
	JSR	<XMIT_A_WORD	; Transmit A to TIM-A-STD
	JSR	<PAL_DLY	; Wait for SSI and PAL to be empty
	ADD	X0,B		; Increment the video processor board number
	NOP
STG_LOOP
	BCLR	#3,X:PCRD		; Turn the serial clock off
	JMP	<FINISH

ERR_SGN	MOVE	X:(R3)+,A
	BCLR	#3,X:PCRD		; Turn the serial clock off
	JMP	<ERROR

; Set a particular DAC numbers, for setting DC bias voltages, clock driver  
;   voltages and video processor offset
; This is code for the ARC32 clock driver and ARC45 CCD video processor
;
; SBN  #BOARD  #DAC  ['CLK' or 'VID'] voltage
;
;				#BOARD is from 0 to 15
;				#DAC number
;				#voltage is from 0 to 4095

SET_BIAS_NUMBER			; Set bias number
	BSET	#3,X:PCRD	; Turn on the serial clock
	MOVE	X:(R3)+,A	; First argument is board number, 0 to 15
	REP	#20
	LSL	A
	NOP
	MOVE	A,X1		; Save the board number
	MOVE	X:(R3)+,A	; Second argument is DAC number
	MOVE	X:(R3),B	; Third argument is 'VID' or 'CLK' string
	CMP	#'VID',B
	JNE	<CLK_DRV
	REP	#14
	LSL	A
	NOP
	BSET	#19,A1		; Set bits to mean video processor DAC
	NOP
	BSET	#18,A1
	JMP	<BD_SET
CLK_DRV	CMP	#'CLK',B
	JNE	<ERR_SBN

; For ARC32 do some trickiness to set the chip select and address bits
	MOVE	A1,B
	REP	#14
	LSL	A
	MOVE	#$0E0000,X0
	AND	X0,A
	MOVE	#>7,X0
	AND	X0,B		; Get 3 least significant bits of clock #
	CMP	#0,B
	JNE	<CLK_1
	BSET	#8,A
	JMP	<BD_SET
CLK_1	CMP	#1,B
	JNE	<CLK_2
	BSET	#9,A
	JMP	<BD_SET
CLK_2	CMP	#2,B
	JNE	<CLK_3
	BSET	#10,A
	JMP	<BD_SET
CLK_3	CMP	#3,B
	JNE	<CLK_4
	BSET	#11,A
	JMP	<BD_SET
CLK_4	CMP	#4,B
	JNE	<CLK_5
	BSET	#13,A
	JMP	<BD_SET
CLK_5	CMP	#5,B
	JNE	<CLK_6
	BSET	#14,A
	JMP	<BD_SET
CLK_6	CMP	#6,B
	JNE	<CLK_7
	BSET	#15,A
	JMP	<BD_SET
CLK_7	CMP	#7,B
	JNE	<BD_SET
	BSET	#16,A

BD_SET	OR	X1,A		; Add on the board number
	NOP
	MOVE	A,X0
	MOVE	X:(R3)+,B	; Third argument (again) is 'VID' or 'CLK' string
	CMP	#'VID',B
	JEQ	<VID
	MOVE	X:(R3)+,A	; Fourth argument is voltage value, 0 to $fff
	REP	#4
	LSR	A		; Convert 12 bits to 8 bits for ARC32
	MOVE	#>$FF,Y0	; Mask off just 8 bits
	AND	Y0,A
	OR	X0,A
	JMP	<XMT_SBN
VID	MOVE	X:(R3)+,A	; Fourth argument is voltage value for ARC45, 12 bits
	OR	X0,A

XMT_SBN	JSR	<XMIT_A_WORD	; Transmit A to TIM-A-STD
	JSR	<PAL_DLY	; Wait for the number to be sent
	BCLR	#3,X:PCRD	; Turn the serial clock off
	JMP	<FINISH
ERR_SBN	MOVE	X:(R3)+,A	; Read and discard the fourth argument
	BCLR	#3,X:PCRD	; Turn the serial clock off
	JMP	<ERROR

; Specify the MUX value to be output on the clock driver board
; Command syntax is  SMX  #clock_driver_board #MUX1 #MUX2
;				#clock_driver_board from 0 to 15
;				#MUX1, #MUX2 from 0 to 23

SET_MUX	BSET	#3,X:PCRD	; Turn on the serial clock
	MOVE	X:(R3)+,A	; Clock driver board number
	REP	#20
	LSL	A
	MOVE	#$003000,X0	; Bits to select MUX on ARC30 board
	OR	X0,A
	NOP
	MOVE	A1,X1		; Move here for later use
	
; Get the first MUX number
	MOVE	X:(R3)+,A	; Get the first MUX number
	TST	A
	JLT	<ERR_SM1
	MOVE	#>24,X0		; Check for argument less than 32
	CMP	X0,A
	JGE	<ERR_SM1
	MOVE	A,B
	MOVE	#>7,X0
	AND	X0,B
	MOVE	#>$18,X0
	AND	X0,A
	JNE	<SMX_1		; Test for 0 <= MUX number <= 7
	BSET	#3,B1
	JMP	<SMX_A
SMX_1	MOVE	#>$08,X0
	CMP	X0,A		; Test for 8 <= MUX number <= 15
	JNE	<SMX_2
	BSET	#4,B1
	JMP	<SMX_A
SMX_2	MOVE	#>$10,X0
	CMP	X0,A		; Test for 16 <= MUX number <= 23
	JNE	<ERR_SM1
	BSET	#5,B1
SMX_A	OR	X1,B1		; Add prefix to MUX numbers
	NOP
	MOVE	B1,Y1

; Add on the second MUX number
	MOVE	X:(R3)+,A	; Get the next MUX number
	TST	A
	JLT	<ERR_SM2
	MOVE	#>24,X0		; Check for argument less than 32
	CMP	X0,A
	JGE	<ERR_SM2
	REP	#6
	LSL	A
	NOP
	MOVE	A,B
	MOVE	#$1C0,X0
	AND	X0,B
	MOVE	#>$600,X0
	AND	X0,A
	JNE	<SMX_3		; Test for 0 <= MUX number <= 7
	BSET	#9,B1
	JMP	<SMX_B
SMX_3	MOVE	#>$200,X0
	CMP	X0,A		; Test for 8 <= MUX number <= 15
	JNE	<SMX_4
	BSET	#10,B1
	JMP	<SMX_B
SMX_4	MOVE	#>$400,X0
	CMP	X0,A		; Test for 16 <= MUX number <= 23
	JNE	<ERR_SM2
	BSET	#11,B1
SMX_B	ADD	Y1,B		; Add prefix to MUX numbers
	NOP
	MOVE	B1,A
	JSR	<XMIT_A_WORD	; Transmit A to TIM-A-STD
	JSR	<PAL_DLY	; Delay for all this to happen
	BCLR	#3,X:PCRD	; Turn the serial clock off
	JMP	<FINISH
ERR_SM1	MOVE	X:(R3)+,A	; Throw off the last argument
ERR_SM2	BCLR	#3,X:PCRD	; Turn the serial clock off
	JMP	<ERROR

; Specify subarray readout coordinates, one rectangle only
SET_SUBARRAY_SIZES
	CLR	A
	NOP	
	MOVE	A,Y:<NBOXES		; Number of subimage boxes = 0 to start
	MOVE    X:(R3)+,X0
	MOVE	X0,Y:<NRBIAS		; Number of bias pixels to read
	MOVE    X:(R3)+,X0
	MOVE	X0,Y:<NSREAD		; Number of columns in subimage read
	MOVE    X:(R3)+,X0
	MOVE	X0,Y:<NPREAD		; Number of rows in subimage read	
	BSET	#ST_DIRTY,X:<STATUS	; Readout parameters have been changed
	JMP	<FINISH

SET_SUBARRAY_POSITIONS
	MOVE	#READ_TABLE,R7
	MOVE	X:(R3)+,X0
	NOP
	MOVE	X0,Y:(R7)+	; Number of rows (parallels) to clear
	MOVE	X:(R3)+,X0
	MOVE	X0,Y:(R7)+	; Number of columns (serials) clears before
	MOVE	X:(R3)+,X0	;  the box readout
	MOVE	X0,Y:(R7)+	; Number of columns (serials) clears after	
	MOVE	#>1,X0
	MOVE	X0,Y:<NBOXES
	BSET	#ST_DIRTY,X:<STATUS ; Readout parameters have been changed
	JMP	<FINISH

; Generate the waveform table as a command, for diagnostic purposes
GENERATE_WAVEFORM
	JSR	<GENERATE_SERIAL_WAVEFORM
	JMP	<FINISH
	
; Generate the serial readout waveform table for the chosen
;   value of OS, binning and integration time.

GENERATE_SERIAL_WAVEFORM

; Insert the serias fiber optic transmit command
	MOVE	#(PXL_TBL+1),R1         ; cjm generated serial waveforms start at PXL_TBL+1 = R1, PXL_TBL will hold count?  
	NOP
	NOP
	MOVE	Y:SXMIT,X0
	MOVE	R1,Y:<SXMIT_ADR		; Save these two for later
	MOVE	X0,Y:(R1)+              ; cjm Add  Y:SXMIT to PXL_TBL
        ; Add FIRST_CLOCKS waveforms to PXL_TBL
	MOVE	Y:FIRST_CLOCKS,R0
	NOP
	NOP
	DO	Y:(R0)+,L_FIRST_CLOCKS
	MOVE	Y:(R0)+,X0
	MOVE	X0,Y:(R1)+
L_FIRST_CLOCKS

; Generate the binning waveforms if needed
	MOVE	Y:<NSBIN,A
	SUB	#1,A
	JLE	<GEN_RST        ; if Y:<NSBIN == 1 go straight to GEN_RST
	DO	A1,L_BIN        ; loop over Y:<NSBIN-1
        ;  Add CLOCK_LINE to PXL_TBL
	MOVE	Y:CLOCK_LINE,R0 
	NOP
	NOP
	MOVE	Y:(R0)+,A
	NOP
	DO	A1,L_CLOCK_LINE
	MOVE	Y:(R0)+,X0
	MOVE	X0,Y:(R1)+
L_CLOCK_LINE
	NOP
L_BIN

; Generate the video waveforms for integrating the reset level
; add RESET_VIDEO to PXL_TBL
GEN_RST	MOVE	#RESET_VIDEO,R0 
	NOP
	NOP
	DO	Y:(R0)+,L_RESET_VIDEO
	MOVE	Y:(R0)+,X0
	MOVE	X0,Y:(R1)+
L_RESET_VIDEO

; Generate the waveforms for dumping charge and integrating the signal level
; add CHARGE_DUMP to PXL_TBL
	MOVE	Y:CHARGE_DUMP,R0
	NOP
	NOP
	MOVE	Y:(R0)+,A
	NOP
	DO	A1,L_CHARGE_DUMP
	MOVE	Y:(R0)+,X0
	MOVE	X0,Y:(R1)+
L_CHARGE_DUMP

; Finally, calculate the number of entries in the waveform table just generated
; SET PXL_TBL index 0 to be number of clock waveforms to clock e.g. Y:PXL_TBL = (R1-1)- PXL_TBL
	MOVE	#PXL_TBL,X0
	MOVE	X0,R0
	MOVE	R1,A
	SUB	X0,A
	SUB	#1,A
	NOP
	MOVE	A1,Y:(R0)
	RTS

; Select the amplifier and readout mode
;   'SOS'  Amplifier_name = '__C', '__D', '__B', '__A' or 'ALL'
;   			 or '__E', '__F', '__G', '__H'
; Now also '_AC': Upper and Lower left hand amplifiers.
; Now also '_BD': Upper and Lower right hand amplifiers.
SELECT_OUTPUT_SOURCE
	MOVE    X:(R3)+,Y0
	MOVE	Y0,Y:<OS
	JSR	<SEL_OS
	JMP	<FINISH
	
SEL_OS	MOVE	Y:<OS,X0		; Get amplifier(s) name
	MOVE	#'ALL',A		; All Amplifiers = readout #0 to #3
	CMP	X0,A
	JNE	<CMP_LL
	MOVE	#PARALLEL_SPLIT_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_SPLIT_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_SPLIT_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_SPLIT,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_SPLIT,X0
	MOVE	X0,Y:SERIAL_SKIP
	MOVE	#FIRST_CLOCKS_SPLIT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_SPLIT,X0
	MOVE	X0,Y:CLOCK_LINE
	MOVE	#$00F0C0,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_SPLIT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BSET	#SPLIT_S,X:STATUS
	BSET	#SPLIT_P,X:STATUS
	RTS

CMP_LL	MOVE	#'__C',A		; Lower Left Amplifier = readout #0
	CMP	X0,A
	JEQ	<EQ_LL
	MOVE	#'__E',A
	CMP	X0,A
	JNE	<CMP_LR
EQ_LL	MOVE	#PARALLEL_DOWN_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_DOWN_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_DOWN_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_DOWN,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_LEFT,X0
	MOVE	X0,Y:SERIAL_SKIP

	MOVE	#FIRST_CLOCKS_LEFT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_LEFT,X0
	MOVE	X0,Y:CLOCK_LINE
	MOVE	#$00F000,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_LEFT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BCLR	#SPLIT_S,X:STATUS
	BCLR	#SPLIT_P,X:STATUS
	RTS

CMP_LR	MOVE	#'__D',A		; Lower Right Amplifier = readout #1
	CMP	X0,A
	JEQ	<EQ_LR
	MOVE	#'__F',A
	CMP	X0,A
	JNE	<CMP_UR
EQ_LR	MOVE	#PARALLEL_DOWN_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_DOWN_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_DOWN_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_DOWN,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_RIGHT,X0
	MOVE	X0,Y:SERIAL_SKIP

	MOVE	#FIRST_CLOCKS_RIGHT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_RIGHT,X0
	MOVE	X0,Y:CLOCK_LINE
	MOVE	#$00F041,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_RIGHT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BCLR	#SPLIT_S,X:STATUS
	BCLR	#SPLIT_P,X:STATUS
	RTS

CMP_UR	MOVE	#'__B',A		; Upper Right Amplifier = readout #2
	CMP	X0,A
	JEQ	<EQ_UR
	MOVE	#'__G',A
	CMP	X0,A
	JNE	<CMP_UL
EQ_UR	MOVE	#PARALLEL_UP_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_UP_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_UP_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_UP,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_RIGHT,X0
	MOVE	X0,Y:SERIAL_SKIP

	MOVE	#FIRST_CLOCKS_RIGHT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_RIGHT,X0
	MOVE	X0,Y:CLOCK_LINE
	MOVE	#$00F082,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_RIGHT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BCLR	#SPLIT_S,X:STATUS
	BCLR	#SPLIT_P,X:STATUS
	RTS

CMP_UL	MOVE	#'__A',A		; Upper Left Amplifier = readout #3
	CMP	X0,A
	JEQ	<EQ_UL
	MOVE	#'__H',A
	CMP	X0,A
	JNE	<CMP_BL
EQ_UL	MOVE	#PARALLEL_UP_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_UP_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_UP_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_UP,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_LEFT,X0
	MOVE	X0,Y:SERIAL_SKIP

	MOVE	#FIRST_CLOCKS_LEFT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_LEFT,X0
	MOVE	X0,Y:CLOCK_LINE
	MOVE	#$00F0C3,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_LEFT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BCLR	#SPLIT_S,X:STATUS
	BCLR	#SPLIT_P,X:STATUS
	RTS
CMP_BL	MOVE	#'_AC',A		; Upper and Lower Left Amplifiers
	CMP	X0,A
	JNE	<CMP_BR
	MOVE	#PARALLEL_SPLIT_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_SPLIT_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_SPLIT_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_SPLIT,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_LEFT,X0
	MOVE	X0,Y:SERIAL_SKIP

	MOVE	#FIRST_CLOCKS_LEFT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_LEFT,X0
	MOVE	X0,Y:CLOCK_LINE
	;; This can't actually be done. We need to transmit A/D converters 0 and 3, without
	;; transmitting 1 and 2, and you encodify the SXMIT command in terms of start and end converters.
	;; So the following value is WRONG
	MOVE	#$00F081,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_LEFT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BCLR	#SPLIT_S,X:STATUS
	BSET	#SPLIT_P,X:STATUS
	RTS
CMP_BR	MOVE	#'_BD',A		; Upper and Lower Right Amplifiers = readout #1 and #2
	CMP	X0,A
	JNE	<ERROR
EQ_BR	MOVE	#PARALLEL_SPLIT_START,X0
	MOVE	X0,Y:PARALLEL_START
	MOVE	#PARALLEL_SPLIT_BIN,X0
	MOVE	X0,Y:PARALLEL_BIN
	MOVE	#PARALLEL_SPLIT_END,X0
	MOVE	X0,Y:PARALLEL_END
	MOVE	#PARALLEL_CLEAR_SPLIT,X0
	MOVE	X0,Y:PARALLEL_CLEAR
	MOVE	#SERIAL_SKIP_RIGHT,X0
	MOVE	X0,Y:SERIAL_SKIP
	MOVE	#FIRST_CLOCKS_RIGHT,X0
	MOVE	X0,Y:FIRST_CLOCKS
	MOVE	#CLOCK_LINE_RIGHT,X0
	MOVE	X0,Y:CLOCK_LINE
	MOVE	#$00F081,X0
	MOVE	X0,Y:SXMIT
	MOVE	#CHARGE_DUMP_RIGHT,X0
	MOVE	X0,Y:CHARGE_DUMP
	BCLR	#SPLIT_S,X:STATUS
	BSET	#SPLIT_P,X:STATUS
	RTS

; Set the pixel time by adjusting the reset and signal integration times
SET_PIXEL_TIME
	MOVE	X:(R3)+,A		; Get the number of 40 nanosec delays
	LSL	#16,A
	NOP
	MOVE	A1,X0
	
	MOVE	Y:INTEGRATE_RESET,A
	AND	#$00FFFF,A
	ADD	X0,A
	NOP
	MOVE	A1,Y:INTEGRATE_RESET

	MOVE	Y:INTEGRATE_SIGNAL_LEFT,A
	AND	#$00FFFF,A
	ADD	X0,A
	NOP
	MOVE	A1,Y:INTEGRATE_SIGNAL_LEFT

	MOVE	Y:INTEGRATE_SIGNAL_RIGHT,A
	AND	#$00FFFF,A
	ADD	X0,A
	NOP
	MOVE	A1,Y:INTEGRATE_SIGNAL_RIGHT

	MOVE	Y:INTEGRATE_SIGNAL_SPLIT,A
	AND	#$00FFFF,A
	ADD	X0,A
	NOP
	MOVE	A1,Y:INTEGRATE_SIGNAL_SPLIT

	BSET	#ST_DIRTY,X:<STATUS	; Readout parameters have been changed
	JMP	<FINISH


;
; $Log: not supported by cvs2svn $
; Revision 1.1  2012/03/28 16:54:54  cjm
; Initial revision
;
;
