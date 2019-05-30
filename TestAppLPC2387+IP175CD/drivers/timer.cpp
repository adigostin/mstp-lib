
#include "timer.h"
#include "vic.h"
#include <nxp/iolpc2387.h>
#include <assert.h>
#include <stddef.h>

static const size_t timer_count = 4;

static timer_callback callbacks[timer_count];

static void timer0_isr()
{
	T0IR = 1; // clear interrupt flag
	callbacks[0]();
}

void timer_init (uint32_t timer, uint32_t clock_frequency, uint32_t ticks_per_second, timer_callback callback)
{
	if (timer == 0)
	{
		PCONP_bit.PCTIM0 = 1;
		PCLKSEL0_bit.PCLK_TIMER0 = 1; // from main clock
		T0MR0 = clock_frequency / ticks_per_second;
		T0MCR = 3; // Interrupt and Reset on MR0
		T0TCR = 1; // enable it
		callbacks[0] = callback;
		VIC_SetVectoredIRQ (timer0_isr, 4, VIC_TIMER0);
	}
	else
		assert(false); // not implemented
}
