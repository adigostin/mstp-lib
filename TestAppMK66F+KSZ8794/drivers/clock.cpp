
#include "clock.h"
#include <CMSIS/MK66F18.h>
#include <assert.h>

static uint32_t external_clock_mhz;

void clock_init (uint32_t external_clock_mhz)
{
	::external_clock_mhz = external_clock_mhz;
	assert(false); // not implemented
}

void clock_enable (void* peripheral_base_address)
{
	if (peripheral_base_address == PORTA)
	{
		SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
		return;
	}

	if (peripheral_base_address == PORTB)
	{
		SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
		return;
	}

	if (peripheral_base_address == PORTC)
	{
		SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
		return;
	}

	if (peripheral_base_address == PIT)
	{
		SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
		return;
	}

	assert(false); // not implemented
}

bool clock_enabled (void* peripheral_base_address)
{
	if (peripheral_base_address == PORTA)
		return SIM->SCGC5 & SIM_SCGC5_PORTA_MASK;

	if (peripheral_base_address == PORTB)
		return SIM->SCGC5 & SIM_SCGC5_PORTB_MASK;

	if (peripheral_base_address == PORTC)
		return SIM->SCGC5 & SIM_SCGC5_PORTC_MASK;

	if (peripheral_base_address == PIT)
		return SIM->SCGC6 & SIM_SCGC6_PIT_MASK;

	assert(false); // not implemented
	return false;
}

