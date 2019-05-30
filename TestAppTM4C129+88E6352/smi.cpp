
#include "smi.h"
#include <TM4C1294KCPDT.h>
#include <assert.h>

void smi_init()
{
	SYSCTL->RCGCSSI |= (1 << 0);   // enable SSI0 clock
	SYSCTL->RCGCGPIO |= (1 << 0);  // enable GPIOA clock
	GPIOA_AHB->PCTL |= 0x00FFFF00; // GPIOA pins 2, 3, 4, 5 - special function 15
	GPIOA_AHB->AFSEL |= (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5); // GPIOA pins 2, 3, 4, 5 - alternate function
	GPIOA_AHB->DEN   |= (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5); // digital enable
	GPIOA_AHB->ODR |= (1 << 4) | (1 << 5); // pins 4 and 5 - open drain
	// SSInClk = SysClk / (CPSR * (1 + SCR))
	// we want SSInClk = 1 MHz
	SSI0->CPSR = 2;
	SSI0->CR0 = (59 << 8) // SCR = 59
		| (1 << 7) // SPH = 1
		| (1 << 6) // SPO = 1
		| (0 << 4) // FRF = 0
		| (15 << 0); // DSS = 15
	SSI0->CR1 = (1 << 1); // set SSE (enable module)
}


static void write_16b (uint16_t value)
{
	assert ((SSI0->SR & (1 << 2)) == 0); // RX FIFO should be empty (RNE bit)

	assert (SSI0->SR & (1 << 1)); // TX FIFO should be not-full (TNF bit)

	SSI0->DR = (uint16_t)~value;

	// Wait till the SSI peripheral puts in the RX FIFO what we just sent, and discard it.
	while ((SSI0->SR & (1 << 2)) == 0);
	uint16_t read_back_value = SSI0->DR;
	assert (read_back_value == value);
}

static uint16_t read_16b()
{
	assert ((SSI0->SR & (1 << 2)) == 0); // RX FIFO should be empty

	assert (SSI0->SR & (1 << 1)); // TX FIFO should be not-full (TNF bit)

	// Send zeroes (which will keep our active-low output undriven) and read back what the switch sends us.
	SSI0->DR = 0;

	while ((SSI0->SR & (1 << 2)) == 0);
	uint16_t value = SSI0->DR;
	return value;
}

void smi_write (uint8_t phy_addr, uint8_t reg_addr, uint16_t data)
{
	// https://en.wikipedia.org/wiki/Management_Data_Input/Output

	uint16_t command =
		  (0b01 << 14)    // ST
		| (0b01 << 12)    // OP
		| (phy_addr << 7) // PA5
		| (reg_addr << 2) // RA5
		| 0b10;           // TA

	write_16b (0xFFFF); // 16 bits of preamble are enough for 88E6352.
	write_16b (command);
	write_16b (data);
}

uint16_t smi_read (uint8_t phy_addr, uint8_t reg_addr)
{
	// https://en.wikipedia.org/wiki/Management_Data_Input/Output

	uint16_t command =
		  (0b01 << 14)    // ST
		| (0b10 << 12)    // OP
		| (phy_addr << 7) // PA5
		| (reg_addr << 2) // RA5
		| 0b10;           // TA

	write_16b (0xFFFF); // 16 bits of preamble are enough for 88E6352.
	write_16b (command);
	return read_16b();
}
