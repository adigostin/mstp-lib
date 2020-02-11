
#pragma once
#include <CMSIS/MK66F18.h>
#include <stddef.h>

struct pin
{
	GPIO_Type* port; // one of PTA, PTB...
	uint32_t   bit;
};

void gpio_make_output (GPIO_Type* gpio_port, uint32_t bit, bool initial_level);
void gpio_make_output (const struct pin& pin, bool initial_level);
void gpio_set         (GPIO_Type* gpio_port, uint32_t bit, bool level);
void gpio_set         (const struct pin& pin, bool level);
void gpio_toggle      (GPIO_Type* gpio_port, uint32_t bit);
