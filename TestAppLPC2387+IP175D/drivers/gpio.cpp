
#include "gpio.h"
#include <nxp/iolpc2387.h>
#include <intrinsics.h>
#include <assert.h>

void gpio_init()
{
	SCS_bit.GPIOM = 1; // Enable Fast GPIO0,1
}

void gpio_make_output (pin_t pin)
{
	__istate_t save = __get_interrupt_state();
	__disable_irq ();

	switch (pin.port_index)
	{
		case 0: FIO0DIR |= (1UL << pin.bit_pos); break;
		case 1: FIO1DIR |= (1UL << pin.bit_pos); break;
		case 2: FIO2DIR |= (1UL << pin.bit_pos); break;
		case 3: FIO3DIR |= (1UL << pin.bit_pos); break;
		case 4: FIO4DIR |= (1UL << pin.bit_pos); break;
	}

	__set_interrupt_state (save);
}

void gpio_set (pin_t pin, bool level)
{
	if (level)
	{
		switch (pin.port_index)
		{
			case 0: FIO0SET = (1u << pin.bit_pos); break;
			case 1: FIO1SET = (1u << pin.bit_pos); break;
			case 2: FIO2SET = (1u << pin.bit_pos); break;
			case 3: FIO3SET = (1u << pin.bit_pos); break;
			case 4: FIO4SET = (1u << pin.bit_pos); break;
		}
	}
	else
	{
		switch (pin.port_index)
		{
			case 0:	FIO0CLR = (1u << pin.bit_pos); break;
			case 1:	FIO1CLR = (1u << pin.bit_pos); break;
			case 2:	FIO2CLR = (1u << pin.bit_pos); break;
			case 3:	FIO3CLR = (1u << pin.bit_pos); break;
			case 4:	FIO4CLR = (1u << pin.bit_pos); break;
		}
	}
}
