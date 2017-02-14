
#ifndef UARTSIMPLE_H
#define UARTSIMPLE_H

enum UART_INDEX
{
	UART_INDEX_0 = 0,
	UART_INDEX_1 = 1,
	UART_INDEX_2 = 2,
	UART_INDEX_3 = 3
};

typedef void (*UART_RX_CALLBACK) (UART_INDEX uartIndex, unsigned char receivedByte);

void UART_Init   (UART_INDEX uartIndex, unsigned int clockFrequency, unsigned int baudrate, UART_RX_CALLBACK callback);
void UART_Uninit (UART_INDEX uartIndex);
void UART_Send   (UART_INDEX uartIndex, unsigned char data);

#endif
