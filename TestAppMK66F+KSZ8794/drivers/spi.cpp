
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
	spi->MCR = spi->MCR & ~(SPI_MCR_MDIS_MASK | SPI_MCR_HALT_MASK) | SPI_MCR_MSTR_MASK | SPI_MCR_PCSIS_MASK;
}

void spi_transfer_blocking (SPI_Type* spi, uint32_t cs, const uint8_t* out_data, uint8_t* in_data, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		while ((spi->SR & SPI_SR_TFFF_MASK) == 0)
			;

		bool last = (i == size - 1);
		uint32_t pushr = spi->PUSHR & 0x03C00000u; // retain the reserved bits
		if (!last)
			pushr |= SPI_PUSHR_CONT_MASK;

		if (last)
			pushr |= SPI_PUSHR_EOQ_MASK;

		pushr |= (cs << SPI_PUSHR_PCS_SHIFT);
		pushr |= out_data[i];
		spi->PUSHR = pushr;

		while ((spi->SR & SPI_SR_RFDF_MASK) == 0)
			;
		uint32_t popr = spi->POPR;
		spi->SR = SPI_SR_RFDF_MASK;
		in_data[i] = (uint8_t)popr;
	}

	while ((spi->SR & SPI_SR_TCF_MASK) == 0)
		;
	spi->SR = (SPI_SR_TCF_MASK | SPI_SR_EOQF_MASK);
}
