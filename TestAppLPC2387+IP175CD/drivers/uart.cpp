
#include "uart.h"
#include "vic.h"
#include <nxp/iolpc2387.h>
#include <stdio.h>
#include <intrinsics.h>
#include <assert.h>

// ============================================================================

static uart_rx_callback callbacks[4] = { NULL, NULL, NULL, NULL };

//static volatile unsigned long* const iirRegs[] = {&U0IIR, &U1IIR, &U2IIR, &U3IIR};
static volatile unsigned char* const thrRegs[] = {&U0THR, &U1THR, &U2THR, &U3THR};
//static volatile unsigned long* const fcrRegs[] = {&U0FCR, &U1FCR, &U2FCR, &U3FCR};

static volatile __uartlsr_bits* const lsrRegs[] = {&U0LSR_bit, &U1LSR_bit, &U2LSR_bit, &U3LSR_bit};
//static volatile __uartlcr_bits* const lcrRegs[] = {&U0LCR_bit, &U1LCR_bit, &U2LCR_bit, &U3LCR_bit};
//static volatile __uartier0_bits* const ierRegs[] = {&U0IER_bit, (volatile __uartier0_bits*) &U1IER_bit, &U2IER_bit, &U3IER_bit};

// bits of PCONP
//static const dword pcuartBits[] = {(1 << 3), (1 << 4), (1 << 24), (1 << 25)};

// ============================================================================

static void uart0_isr()
{
	unsigned char b = U0RBR;
	if (callbacks [0] != NULL)
		callbacks [0] (0, b);
}

static void uart1_isr()
{
	unsigned char b = U1RBR;
	if (callbacks [1] != NULL)
		callbacks [1] (1, b);
}

static void uart2_isr()
{
	unsigned char b = U2RBR;
	if (callbacks [2] != NULL)
		callbacks [2] (2, b);
}

static void uart3_isr()
{
	unsigned char b = U3RBR;
	if (callbacks [3] != NULL)
		callbacks [3] (3, b);
}

// ============================================================================

void uart_init (unsigned uart_index, unsigned clock_frequency, unsigned baudrate, uart_rx_callback callback)
{
	assert (callbacks [uart_index] == NULL);

	callbacks [uart_index] = callback;

	unsigned Fdiv = ( clock_frequency / 16 ) / baudrate;

	switch (uart_index)
	{
		case 0:
			PCONP_bit.PCUART0 = 1;
			PCLKSEL0_bit.PCLK_UART0 = 01; // source clock is processor clock
			U0LCR = 0x83; // 8 bits, no Parity, 1 Stop bit
			U0DLM = Fdiv / 256;
			U0DLL = Fdiv % 256;
			U0LCR = 0x03; // DLAB = 0
			U0FCR = 0x07; // Enable and reset TX and RX FIFO.
			if (callback != NULL)
			{
				U0IER_bit.RDAIE = 1; // enable Receive Data Available interrupt
				VIC_SetVectoredIRQ (uart0_isr, 4, VIC_UART0);
			}
			break;

		case 1:
			PCONP_bit.PCUART1 = 1;
			PCLKSEL0_bit.PCLK_UART1 = 01; // source clock is processor clock
			U1LCR = 0x83; // 8 bits, no Parity, 1 Stop bit
			U1DLM = Fdiv / 256;
			U1DLL = Fdiv % 256;
			U1LCR = 0x03; // DLAB = 0
			U1FCR = 0x07; // Enable and reset TX and RX FIFO.
			if (callback != NULL)
			{
				U1IER_bit.RDAIE = 1; // enable Receive Data Available interrupt
				VIC_SetVectoredIRQ (uart1_isr, 4, VIC_UART1);
			}
			break;

		case 2:
			PCONP_bit.PCUART2 = 1;
			PCLKSEL1_bit.PCLK_UART2 = 01; // source clock is processor clock
			U2LCR = 0x83; // 8 bits, no Parity, 1 Stop bit
			U2DLM = Fdiv / 256;
			U2DLL = Fdiv % 256;
			U2LCR = 0x03; // DLAB = 0
			U2FCR = 0x07; // Enable and reset TX and RX FIFO.
			if (callback != NULL)
			{
				U2IER_bit.RDAIE = 1; // enable Receive Data Available interrupt
				VIC_SetVectoredIRQ (uart2_isr, 4, VIC_UART2);
			}
			break;

		case 3:
			PCONP_bit.PCUART3 = 1;
			PCLKSEL1_bit.PCLK_UART3 = 01; // source clock is processor clock
			U3LCR = 0x83; // 8 bits, no Parity, 1 Stop bit
			U3DLM = Fdiv / 256;
			U3DLL = Fdiv % 256;
			U3LCR = 0x03; // DLAB = 0
			U3FCR = 0x07; // Enable and reset TX and RX FIFO.
			if (callback != NULL)
			{
				U3IER_bit.RDAIE = 1; // enable Receive Data Available interrupt
				VIC_SetVectoredIRQ (uart3_isr, 4, VIC_UART3);
			}
			break;
		default:
			assert (false);
	}

}

// ============================================================================

void uart_send_blocking (unsigned uart_index, unsigned char data)
{
	bool done = false;
	do
	{
		if (lsrRegs [uart_index]->THRE)
		{
			*thrRegs [uart_index] = data;
			done = true;
		}

	} while (done == false);

	while (lsrRegs [uart_index]->TEMT == 0)
		;
}

