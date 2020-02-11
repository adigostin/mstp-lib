
#include "pit.h"
#include "clock.h"
#include <CMSIS/MK66F18.h>

static pit_callback_t callbacks[4];

void pit_init (uint32_t pit_channel, uint32_t reload_value, pit_callback_t callback)
{
	if (!clock_enabled(PIT))
		clock_enable(PIT);

	::callbacks[pit_channel] = callback;
	PIT->CHANNEL[pit_channel].LDVAL = reload_value;
	PIT->CHANNEL[pit_channel].TCTRL = 3; // TIE + TEN
	PIT->MCR = 1; // MDIS=0, FRZ=1

	NVIC_EnableIRQ ((IRQn_Type)(PIT0_IRQn + pit_channel));
}

extern "C" void PIT0_IRQHandler()
{
	callbacks[0]();
	PIT->CHANNEL[0].TFLG = 1;
}

extern "C" void PIT1_IRQHandler()
{
	callbacks[1]();
	PIT->CHANNEL[1].TFLG = 1;
}

extern "C" void PIT2_IRQHandler()
{
	callbacks[2]();
	PIT->CHANNEL[2].TFLG = 1;
}

extern "C" void PIT3_IRQHandler()
{
	callbacks[3]();
	PIT->CHANNEL[3].TFLG = 1;
}

