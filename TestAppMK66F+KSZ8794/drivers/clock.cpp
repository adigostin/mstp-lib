
#include "clock.h"
#include <CMSIS/MK66F18.h>
#include <assert.h>

static uint32_t external_clock_mhz;

void clock_init (uint32_t external_clock_mhz)
{
	::external_clock_mhz = external_clock_mhz;
	assert(false); // not implemented
}

struct clock_info
{
	void* peripheral_base_address;
	volatile uint32_t* reg;
	uint32_t bitmask;
};

static const clock_info clock_infos[] =
{
	{ PORTA, &SIM->SCGC5, SIM_SCGC5_PORTA_MASK },
	{ PORTB, &SIM->SCGC5, SIM_SCGC5_PORTB_MASK },
	{ PORTC, &SIM->SCGC5, SIM_SCGC5_PORTC_MASK },
	{ PIT,   &SIM->SCGC6, SIM_SCGC6_PIT_MASK   },
	{ SPI1,  &SIM->SCGC6, SIM_SCGC6_SPI1_MASK  },
	{ nullptr, nullptr, 0 },
};

void clock_enable (void* peripheral_base_address)
{
	auto p = clock_infos;
	while (p->peripheral_base_address != nullptr)
	{
		if (p->peripheral_base_address == peripheral_base_address)
		{
			*p->reg |= p->bitmask;
			break;
		}

		p++;
	}

	assert (p->peripheral_base_address != nullptr); // not found in array - needs to be added

	// Delay after an RCC peripheral clock enabling.
	// TODO: wait as much as necessary
	asm ("nop;nop;nop;nop;");
	asm ("nop;nop;nop;nop;");
	asm ("nop;nop;nop;nop;");
	asm ("nop;nop;nop;nop;");

	// Or maybe we should force a "clock reset"?
}

bool clock_enabled (void* peripheral_base_address)
{
	auto p = clock_infos;
	while (p->peripheral_base_address != nullptr)
	{
		if (p->peripheral_base_address == peripheral_base_address)
			return *p->reg & p->bitmask;

		p++;
	}

	assert (p->peripheral_base_address != nullptr); // not found in array - needs to be added
	return false;
}

