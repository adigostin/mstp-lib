
#pragma once

struct pin_t
{
	unsigned char port_index : 3;
	unsigned char bit_pos : 5;
};

void gpio_init();
void gpio_make_output (pin_t pin);
void gpio_set (pin_t pin, bool level);
inline void gpio_clear (pin_t pin) { gpio_set (pin, false); }
inline void gpio_set (pin_t pin) { gpio_set (pin, true); }
