
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

void spi_transfer_blocking (SPI_Type* spi, uint32_t chip_select, const uint8_t* out_data, uint8_t* in_data, size_t size);
