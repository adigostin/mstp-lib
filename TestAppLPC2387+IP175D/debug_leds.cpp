
#include "debug_leds.h"
#include "internal/stp_bridge.h"
#include "drivers/gpio.h"
#include <nxp/iolpc2387.h>
#include <intrinsics.h>
#include <assert.h>

static const pin_t shiftRegisterData  = { .port_index = 1, .bit_pos = 23 };
static const pin_t shiftRegisterLatch = { .port_index = 1, .bit_pos = 24 };
static const pin_t shiftRegisterClock = { .port_index = 1, .bit_pos = 25 };

static void write_shift_register (unsigned char data)
{
	for (int i = 7; i >= 0; i--)
	{
		gpio_set (shiftRegisterData, data & (1u << i));

		gpio_set (shiftRegisterClock);
		__no_operation ();
		__no_operation ();
		__no_operation ();
		gpio_clear (shiftRegisterClock);
	}

	// pulse on LED_LATCH
	gpio_set (shiftRegisterLatch);
	__no_operation ();
	__no_operation ();
	__no_operation ();
	gpio_clear (shiftRegisterLatch);
}

void init_debug_leds()
{
	FIO0SET  =  (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 27);
	FIO0DIR |= ((1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 27));

	FIO1SET  =  (1 << 23) | (1 << 24) | (1 << 25);
	FIO1DIR |= ((1 << 23) | (1 << 24) | (1 << 25));

	gpio_clear(shiftRegisterData);
	gpio_make_output(shiftRegisterData);

	gpio_clear(shiftRegisterLatch);
	gpio_make_output(shiftRegisterLatch);

	gpio_clear(shiftRegisterClock);
	gpio_make_output(shiftRegisterClock);
}

void update_debug_leds (const STP_BRIDGE* b)
{
	static const unsigned int cist_tree = 0;

	if (STP_GetPortEnabled(b, 0))
		FIO0CLR = (1 << 0);
	else
		FIO0SET = (1 << 0);

	if (STP_GetPortLearning(b, 0, cist_tree))
		FIO0CLR = (1 << 1);
	else
		FIO0SET = (1 << 1);

	if (STP_GetPortForwarding(b, 0, cist_tree))
		FIO0CLR = (1 << 4);
	else
		FIO0SET = (1 << 4);

	if (STP_GetPortOperEdge(b, 0))
		FIO0CLR = (1 << 5);
	else
		FIO0SET = (1 << 5);

	if (b->ports[0]->trees[cist_tree]->tcWhile)
		FIO0CLR = (1 << 27);
	else
		FIO0SET = (1 << 27);

	// -----------------------------------------------

	unsigned char data = 0xff;
	if (STP_GetPortEnabled (b, 1))
		data &= ~(1 << 0);
	if (STP_GetPortLearning (b, 1, cist_tree))
		data &= ~(1 << 1);
	if (STP_GetPortForwarding (b, 1, cist_tree))
		data &= ~(1 << 2);
	if (STP_GetPortOperEdge (b, 1))
		data &= ~(1 << 3);
	if (b->ports[1]->trees[cist_tree]->tcWhile)
		data &= ~(1 << 4);
	write_shift_register(data);
}
