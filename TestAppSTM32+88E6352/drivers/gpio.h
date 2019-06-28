
#pragma once
#include <stm32f769xx.h>
#include <stddef.h>

struct pin_t
{
	GPIO_TypeDef* port;
	uint32_t      bit;
};

struct pin_and_af_t
{
	pin_t    pin;
	uint32_t af;
};

enum class pin_output_speed_t { low = 0, medium = 1, high = 2, very_high = 3 };
enum class pin_pull { none = 0, up = 1, down = 2 };

void gpio_make_input     (const pin_t& pin, pin_pull pull);
void gpio_make_output    (const pin_t& pin, pin_output_speed_t output_speed, bool initial_level);
void gpio_make_alternate (const pin_and_af_t& pinaf, pin_output_speed_t output_speed, bool open_drain = false);
bool gpio_is_output      (const pin_t& pin);
bool gpio_get            (const pin_t& pin);
void gpio_set            (const pin_t& pin, bool level);
void gpio_make_output (GPIO_TypeDef* port, const uint8_t* pins, size_t count, pin_output_speed_t output_speed, uint32_t initial_value);
void gpio_set (GPIO_TypeDef* port, const uint8_t* pins, size_t count, uint32_t value);

template<size_t size>
struct pin_array
{
	GPIO_TypeDef* port;
	uint8_t       pin_numbers[size];
};

template<size_t count>
void gpio_make_output (const pin_array<count>& pins, pin_output_speed_t output_speed, uint32_t initial_value)
{
	gpio_make_output (pins.port, pins.pin_numbers, count, output_speed, initial_value);
}

template<size_t count>
void gpio_set (const pin_array<count>& pins, uint32_t value)
{
	gpio_set (pins.port, pins.pin_numbers, count, value);
}

