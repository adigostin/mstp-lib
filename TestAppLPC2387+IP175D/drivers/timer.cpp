
#include <nxp/iolpc2387.h>
#include "timer.h"
#include "vic.h"

#define TICKS_PER_SECOND        1000

static volatile unsigned int tickCount;

static void Timer0IntrHandler ()
{
	T0IR = 1; // clear interrupt flag

	tickCount++;
}

void Timer_Init (unsigned int clockFrequency, unsigned int IntrPriority)
{
	tickCount = 0;

	VIC_SetVectoredIRQ (Timer0IntrHandler, IntrPriority, VIC_TIMER0);

	PCONP_bit.PCTIM0 = 1;
	PCLKSEL0_bit.PCLK_TIMER0 = 1; // from main clock
	T0MR0 = clockFrequency / TICKS_PER_SECOND;
	T0MCR = 3; // Interrupt and Reset on MR0
	T0TCR = 1; // enable it
}

unsigned int Timer_GetTimeMilliseconds ()
{
	return tickCount;
}

void Timer_Wait (unsigned int milliseconds)
{
	unsigned int start = ::tickCount;

	while (::tickCount < start + milliseconds)
		;
}
