
#include "switch.h"
#include "drivers/spi.h"

static constexpr uint32_t switch_spi_chip_select = 1;

void switch_init()
{
	// TODO: Raise the SPI speed. Switch chip can go up to 50 MHz.
	static const struct spi_pins spi_pins = { .clk  = { { PTB, 11 }, 2 }, .mosi = { { PTB, 16 }, 2 }, .miso = { { PTB, 17 }, 2 } };
	static const pin_and_af spi_cs_pins[] = { { { PTB, 10 }, 2 } };
	spi_init (SPI1, spi_pins, spi_cs_pins, sizeof(spi_cs_pins) / sizeof(spi_cs_pins[0]), 1000000);
}

void switch_write_reg (uint16_t reg, uint8_t value)
{
	const uint8_t tx[3] = { (uint8_t)(0x40 | (reg >> 7)), (uint8_t)(reg << 1), value };
	uint8_t rx[3];
	spi_transfer_blocking(SPI1, switch_spi_chip_select, tx, rx, 3);
}

uint8_t switch_read_reg (uint16_t reg)
{
	const uint8_t tx[3] = { (uint8_t)(0x60 | (reg >> 7)), (uint8_t)(reg << 1), 0 };
	uint8_t rx[3];
	spi_transfer_blocking (SPI1, switch_spi_chip_select, tx, rx, 3);
	return rx[2];
}

