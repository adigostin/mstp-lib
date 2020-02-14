
#include "spi.h"
#include "clock.h"

void spi_init (SPI_Type* spi, const spi_pins& pins, const pin_and_af* cs_pins, size_t cs_pin_count, uint32_t baud_rate)
{
	// TODO: assert not init

	clock_enable (spi);

	gpio_make_alternate (pins.clk);
	if (pins.mosi.pin.port != nullptr)
		gpio_make_alternate (pins.mosi);
	if (pins.miso.pin.port != nullptr)
		gpio_make_alternate (pins.miso);

	for (size_t i = 0; i < cs_pin_count; i++)
	{
		if (cs_pins[i].pin.port)
			gpio_make_alternate (cs_pins[i]);
	}

	// TODO: maybe disable the FIFOs by setting MCR[DIS_TXF] and MCR[DIS_RXF]
	spi->CTAR[0] = (7u << SPI_CTAR_FMSZ_SHIFT);
	spi->MCR = spi->MCR & ~(SPI_MCR_MDIS_MASK | SPI_MCR_HALT_MASK) | SPI_MCR_MSTR_MASK;
	//spi->CR2 = spi->CR2 & ~SPI_CR2_DS_Msk | (15 << SPI_CR2_DS_Pos) | SPI_CR2_SSOE | SPI_CR2_NSSP;
	//spi->CR1 = SPI_CR1_SPE | (6 << SPI_CR1_BR_Pos) | SPI_CR1_MSTR;
}

void spi_send_blocking (SPI_Type* spi, const uint8_t* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		while ((spi->SR & SPI_SR_TFFF_MASK) == 0)
			;

		uint32_t eoq = (i == size - 1) ? SPI_PUSHR_EOQ_MASK : 0;
		spi->PUSHR = spi->PUSHR & ~(SPI_PUSHR_EOQ_MASK | SPI_PUSHR_PCS_MASK) | eoq | (1 << SPI_PUSHR_PCS_SHIFT) | data[i];
	}

	while (spi->SR & SPI_SR_TCF_MASK == 0)
		;
	spi->SR = SPI_SR_TCF_MASK;
}

/*
void spi_receive_blocking (SPI_Type* spi, uint8_t* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		while ((spi->SR & SPI_SR_RFDF_MASK) == 0)
			;

		uint32_t eoq = (i == size - 1) ? SPI_PUSHR_EOQ_MASK : 0;
		spi->PUSHR = spi->PUSHR & ~(SPI_PUSHR_EOQ_MASK | SPI_PUSHR_PCS_MASK) | eoq | (1 << SPI_PUSHR_PCS_SHIFT) | data[i];
	}
}
*/
