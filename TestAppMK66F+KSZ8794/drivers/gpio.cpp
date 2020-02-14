
#include "gpio.h"
#include "clock.h"
#include <assert.h>

static PORT_Type* port_from_gpio (GPIO_Type* gpio_port)
{
	if (gpio_port == PTA)
		return PORTA;
	if (gpio_port == PTB)
		return PORTB;
	if (gpio_port == PTC)
		return PORTC;
	if (gpio_port == PTD)
		return PORTD;
	if (gpio_port == PTE)
		return PORTE;

	assert(false);
	return nullptr;
}

void gpio_make_output (GPIO_Type* gpio_port, uint32_t bit, bool initial_level)
{
	auto port = port_from_gpio(gpio_port);

	if (!clock_enabled(port))
		clock_enable(port);

	if (initial_level)
		gpio_port->PSOR = (1u << bit);
	else
		gpio_port->PCOR = (1u << bit);
	gpio_port->PDDR |= (1u << bit); // make pin output
	port->PCR[bit] = 0x0100; // make pin GPIO
}

void gpio_make_output (const struct pin& pin, bool initial_level)
{
	gpio_make_output(pin.port, pin.bit, initial_level);
}

void gpio_make_alternate (const pin_and_af& pinaf)
{
	auto port = port_from_gpio(pinaf.pin.port);
	if (!clock_enabled(port))
		clock_enable(port);

	port->PCR[pinaf.pin.bit] = (pinaf.af << 8);
}

void gpio_set (GPIO_Type* gpio_port, uint32_t bit, bool level)
{
	if (level)
		gpio_port->PSOR = (1u << bit);
	else
		gpio_port->PCOR = (1u << bit);
}

void gpio_set (const struct pin& pin, bool level)
{
	if (level)
		pin.port->PSOR = (1u << pin.bit);
	else
		pin.port->PCOR = (1u << pin.bit);
}

void gpio_toggle (GPIO_Type* gpio_port, uint32_t bit)
{
	gpio_port->PTOR = (1u << bit);
}
