
#include "gpio.h"
#include "clock.h"
#include "assert.h"

void gpio_make_input (const pin_t& pin, pin_pull pull)
{
	assert (pin.bit < 16);

	if (!clock_enabled(pin.port))
		clock_enable(pin.port);

	pin.port->MODER = pin.port->MODER & ~(3u << (pin.bit * 2));
	pin.port->PUPDR = pin.port->PUPDR & ~(3u << (pin.bit * 2)) | ((uint32_t) pull << (pin.bit * 2));
}

void gpio_make_output (const pin_t& pin, pin_output_speed_t output_speed, bool initial_level)
{
	assert (pin.bit < 16);

	if (!clock_enabled(pin.port))
		clock_enable(pin.port);

	if (initial_level)
		pin.port->BSRR = (1u << pin.bit);
	else
		pin.port->BSRR = (1u << (pin.bit + 16));

	pin.port->OSPEEDR = pin.port->OSPEEDR & ~(3u << (pin.bit * 2)) | ((uint32_t) output_speed << (pin.bit * 2));
	// TODO: port->PUPDR
	// TODO: port->OTYPER
	pin.port->MODER   = pin.port->MODER   & ~(3u << (pin.bit * 2)) | (1 << (pin.bit * 2));
}

void gpio_make_alternate (const pin_and_af_t& pinaf, pin_output_speed_t output_speed, bool open_drain)
{
	auto port = pinaf.pin.port;
	auto os = (uint32_t) output_speed;
	uint32_t bit = pinaf.pin.bit;
	assert (bit < 16);
	assert (pinaf.af < 16);

	if (!clock_enabled(port))
		clock_enable(port);

	// TODO: port->PUPDR
	port->AFR[bit / 8] = port->AFR[bit / 8] & ~(15u << ((bit % 8) * 4)) | (pinaf.af << ((bit % 8) * 4));
	port->OTYPER  = port->OTYPER  & ~(1u << bit) | (open_drain ? (1u << bit) : 0);
	port->OSPEEDR = port->OSPEEDR & ~(3u << (bit * 2)) | (os << (bit * 2));
	port->MODER   = port->MODER   & ~(3u << (bit * 2)) | (2u << (bit * 2));
}

bool gpio_is_output (const pin_t& pin)
{
	uint32_t mode = (pin.port->MODER >> (pin.bit * 2)) & 3;
	return mode == 1;
}

bool gpio_get (const pin_t& pin)
{
	return (pin.port->IDR & (1u << pin.bit)) != 0;
}

void gpio_set (const pin_t& pin, bool level)
{
	if (level)
		pin.port->BSRR = (1u << pin.bit);
	else
		pin.port->BSRR = (0x10000u << pin.bit);
}

void gpio_make_output (GPIO_TypeDef* port, const uint8_t* pins, size_t count, pin_output_speed_t output_speed, uint32_t initial_value)
{
	if (!clock_enabled(port))
		clock_enable(port);

	uint32_t bsrr = 0;
	uint32_t ospeedr = port->OSPEEDR;
	uint32_t moder = port->MODER;

	for (size_t i = 0; i < count; i++)
	{
		uint32_t pin = pins[i];
		assert (pin < 16);

		if (initial_value & (1u << i))
			bsrr |= (1u << pin);
		else
			bsrr |= (1u << (pin + 16));

		ospeedr = ospeedr & ~(3u << (pin * 2)) | ((uint32_t) output_speed << (pin * 2));
		// TODO: port->PUPDR
		// TODO: port->OTYPER
		moder = moder & ~(3u << (pin * 2)) | (1u << (pin * 2));
	}

	port->BSRR = bsrr;
	port->OSPEEDR = ospeedr;
	port->MODER = moder;
}

void gpio_set (GPIO_TypeDef* port, const uint8_t* pins, size_t count, uint32_t value)
{
	if (count == 0)
		return;

	uint32_t bsrr = 0;
	for (size_t i = 0; i < count; i++)
	{
		uint32_t pin = pins[i];
		if (value & (1u << i))
			bsrr |= (1u << pins[i]);
		else
			bsrr |= (1u << (pins[i] + 16));
	}

	port->BSRR = bsrr;
}
