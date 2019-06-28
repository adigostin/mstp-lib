
#include "assert.h"
#include <stm32f769xx.h>
#include <stdio.h>

extern "C" void __assert(const char *file, int line)
{
	__disable_irq();
	printf ("\r\nAssertion failed in %s, line %d.\r\n", file, line);
	printf ("Restarting the firmware...\r\n");
	asm ("BKPT 0");
	volatile bool loop = true;
	while(loop)
		;
}

extern "C" void HardFault_Handler()
{
	__disable_irq();
	printf ("HardFault_Handler() called.\r\n");
	printf ("Restarting the firmware...\r\n");
	volatile bool loop = true;
	while(loop)
		;
}

