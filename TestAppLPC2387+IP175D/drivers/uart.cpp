
#include <nxp/iolpc2387.h>
#include <stdio.h>
#include <intrinsics.h>
#include <assert.h>
#include "uart.h"
#include "vic.h"

// ============================================================================

static UART_RX_CALLBACK callbacks[4] = { NULL, NULL, NULL, NULL };

//static volatile unsigned long* const iirRegs[] = {&U0IIR, &U1IIR, &U2IIR, &U3IIR};
static volatile unsigned char* const rbrRegs[] = {&U0RBR, &U1RBR, &U2RBR, &U3RBR};
static volatile unsigned char* const thrRegs[] = {&U0THR, &U1THR, &U2THR, &U3THR};
static volatile unsigned long* const fcrRegs[] = {&U0FCR, &U1FCR, &U2FCR, &U3FCR};

static volatile __uartlsr_bits* const lsrRegs[] = {&U0LSR_bit, &U1LSR_bit, &U2LSR_bit, &U3LSR_bit};
//static volatile __uartlcr_bits* const lcrRegs[] = {&U0LCR_bit, &U1LCR_bit, &U2LCR_bit, &U3LCR_bit};
//static volatile __uartier0_bits* const ierRegs[] = {&U0IER_bit, (volatile __uartier0_bits*) &U1IER_bit, &U2IER_bit, &U3IER_bit};

// bits of PCONP
//static const dword pcuartBits[] = {(1 << 3), (1 << 4), (1 << 24), (1 << 25)};

// ============================================================================

static void UART0_ISR ()
{
	unsigned char b = *rbrRegs [UART_INDEX_0];
	if (callbacks [UART_INDEX_0] != NULL)
		callbacks [UART_INDEX_0] (UART_INDEX_0, b);
}

static void UART1_ISR ()
{
	unsigned char b = *rbrRegs [UART_INDEX_1];
	if (callbacks [UART_INDEX_1] != NULL)
		callbacks [UART_INDEX_1] (UART_INDEX_1, b);
}

static void UART2_ISR ()
{
	unsigned char b = *rbrRegs [UART_INDEX_2];
	if (callbacks [UART_INDEX_2] != NULL)
		callbacks [UART_INDEX_2] (UART_INDEX_2, b);
}

static void UART3_ISR ()
{
	unsigned char b = *rbrRegs [UART_INDEX_3];
	if (callbacks [UART_INDEX_3] != NULL)
		callbacks [UART_INDEX_3] (UART_INDEX_3, b);
}

// ============================================================================

void UART_Init (UART_INDEX uartIndex, unsigned int clockFrequency, unsigned int baudrate, UART_RX_CALLBACK callback)
{
	assert (callbacks [uartIndex] == NULL);

	callbacks [uartIndex] = callback;

	unsigned int Fdiv = ( clockFrequency / 16 ) / baudrate; // baud rate;

	switch (uartIndex)
	{
		case UART_INDEX_0:
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
				VIC_SetVectoredIRQ (UART0_ISR, 4, VIC_UART0);
			}
			break;

		case UART_INDEX_1:
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
				VIC_SetVectoredIRQ (UART1_ISR, 4, VIC_UART1);
			}
			break;

		case UART_INDEX_2:
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
				VIC_SetVectoredIRQ (UART2_ISR, 4, VIC_UART2);
			}
			break;

		case UART_INDEX_3:
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
				VIC_SetVectoredIRQ (UART3_ISR, 4, VIC_UART3);
			}
			break;
		default:
			assert (false);
	}

}

// ============================================================================

void UART_Uninit (UART_INDEX uartIndex)
{
	*fcrRegs [uartIndex] = 0;
}

// ============================================================================

void UART_Send (UART_INDEX uartIndex, unsigned char data)
{
	bool done = false;
	do
	{
		if (lsrRegs [uartIndex]->THRE)
		{
			*thrRegs [uartIndex] = data;
			done = true;
		}

	} while (done == false);

	while (lsrRegs [uartIndex]->TEMT == 0)
		;
}

