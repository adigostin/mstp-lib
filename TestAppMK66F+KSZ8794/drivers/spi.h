
#pragma once
#include "gpio.h"
#include <stddef.h>

struct spi_pins
{
	pin_and_af clk;
	pin_and_af mosi;
	pin_and_af miso;
};

void spi_init (SPI_Type* spi, const spi_pins& pins, const pin_and_af* cs_pins, size_t cs_pin_count, uint32_t baud_rate);

template<size_t cs_pin_count>
void spi_init (SPI_Type* spi, const spi_pins& pins, const pin_and_af (&cs_pins)[cs_pin_count], uint32_t baud_rate)
{
	spi_init (spi, pins, cs_pins, cs_pin_count, baud_rate);
}

void spi_send_blocking (SPI_Type* spi, const uint8_t* data, size_t size);
//void spi_receive_blocking (SPI_Type* spi, uint8_t* data, size_t size);