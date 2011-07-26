; filter_wheel_command_table.asm
       COMMENT * 
	$Header: /space/home/eng/cjm/cvs/ioo/sdsu/util/filter_wheel_command_table.asm,v 1.2 2011-07-26 13:52:45 cjm Exp $
	This include file is included in the declaration of the utility board's command table.
	It should contain the command set used by the filter wheel.
	Note the number of commands contained in this file change the next line in utilappl.asm.
	This line must be updated when new commands are added here.
	This source file should be included in utilappl.asm, after the application commands defined in it.
	Version: $Revision: 1.2 $
	Author: $Author: cjm $
	*

;  Command table entries
	DC	'FWA',FILTER_WHEEL_ABORT	; Abort current filter wheel operation
	DC	'FWM',FILTER_WHEEL_MOVE		; Move filter wheel
	DC	'FWR',FILTER_WHEEL_RESET	; Reset filter wheel

       COMMENT * 
	$Log: not supported by cvs2svn $
	Revision 1.1  2011/07/21 10:47:41  cjm
	Initial revision

	*
