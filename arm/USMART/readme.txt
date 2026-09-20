USMART V3.1
   USMART is a neat serial debugging component from ALIENTEK. Through it you can call any
function in your program from a serial terminal and run it, so you can change a function's inputs
freely (numbers in decimal or hex, strings and function entry addresses are all accepted). Up to 10 
arguments are supported and the return value is shown. V2.1 adds the hex and dec commands, which set the argument display format and convert between bases.
For example:
typing "hex 100" shows HEX 0X64 in the serial terminal.
typing "dec 0X64" shows DEC 100 in the serial terminal.

Change log
V1.4
Added support for functions taking string parameters, which widens the range of use considerably.
Reduced memory use: 79 bytes of static memory at 10 parameters, adapting dynamically to number and string lengths
V2.0 
1, Changed the list command to print the full function expression.
2, Added the id command, which prints each function's entry address.
3, Changed parameter matching to support calling a function as a parameter (by entry address).
4, Added a macro for the function name length.	
V2.1 20110707		 
1, Added the dec and hex commands, which set the parameter display base and perform base conversion.
Note: with no argument, dec/hex set the display base; with an argument they perform a base conversion.
e.g. "dec 0XFF" converts 0XFF to 255 and returns it over the serial port.
e.g. "hex 100" 	converts 100 to 0X64 and returns it over the serial port
2, Added the usmart_get_cmdname function, which fetches a command name.
V2.2 20110726	
1, Fixed the parameter count being wrong for void parameters.
2, Changed the default data display format to hexadecimal.
V2.3 20110815
1, Removed the rule that a function name must be followed by "(".
2, Fixed the bug where a string parameter could not contain "(".
3, Changed how a function's default parameter display format is set. 
V2.4 20110905
1, Changed usmart_get_cmdname to cap the maximum parameter length, which stops the hang seen when a bad parameter is entered.
2, Added the USMART_ENTIM2_SCAN macro, which selects whether TIM2 is used to run the scan function periodically.
V2.5 20110930
1, Changed usmart_init to void usmart_init(u8 sysclk) so the scan interval is set automatically from the system clock (fixed at 100ms).
2, Removed the uart_init call from usmart_init; the serial port must now be initialised externally so the user can manage it.
V2.6 20111009
1, Added the read_addr and write_addr functions, which read and write any internal address (it must be a valid one). Handy for debugging.
2, read_addr and write_addr can be enabled or disabled through USMART_USE_WRFUNS.
3, Tidied up usmart_strcmp.			  
V2.7 20111024
1, Fixed the missing newline when a return value is shown in hexadecimal.
2, Added a check for whether a function has a return value; the value is only shown when there is one.
V2.8 20111116
1, Fixed the hang that could follow an argument-less command such as list.
V2.9 20120917
1, Fixed the bug where functions of the form void*xxx(void) were not recognised.
V3.0 20130425
1, Added escape-character support in string parameters.
V3.1 20131120
1, Added the runtime system command, which measures function execution time.
Usage:
Send "runtime 1" to turn function timing on
Send "runtime 0" to turn function timing off
The runtime timing feature needs USMART_ENTIMX_SCAN set to 1!!



							ALIENTEK
							Support forum: www.openedv.com
							Modified: 2013/11/20
							Copyright(C) ALIENTEK 2011-2021