
#include "uart.h"
#include "clock.h"
#include "assert.h"
#include <string.h>

static constexpr uint32_t uart_count = 8;
static constexpr IRQn_Type uart_irqs[8] = { USART1_IRQn, USART2_IRQn, USART3_IRQn, UART4_IRQn, UART5_IRQn, USART6_IRQn, UART7_IRQn, UART8_IRQn };

static size_t get_uart_index (USART_TypeDef* uart)
{
	if (uart == USART1) return 0;
	if (uart == USART2) return 1;
	if (uart == USART3) return 2;
	if (uart ==  UART4) return 3;
	if (uart ==  UART5) return 4;
	if (uart == USART6) return 5;
	if (uart ==  UART7) return 6;
	if (uart ==  UART8) return 7;
	assert(false); return 0;
}

struct uart_params_t
{
	uart_rxne_callback_t rx;
	uint32_t overrun_error_count;

	struct
	{
		const uint8_t*     packet;
		size_t             pos;
		size_t             len;
		uart_tx_callback_t callback;
		void*              callback_arg;
	} tx;
};

static uart_params_t uart_params[uart_count];

static void ISR (USART_TypeDef* uart, uint32_t uart_index)
{
	auto params = &uart_params[uart_index];

	if (uart->ISR & USART_ISR_RXNE)
	{
		uint8_t ch = uart->RDR;
		params->rx(ch);
	}

	if (uart->ISR & USART_ISR_ORE)
	{
		// Overrun Error
		params->overrun_error_count++;
		uart->ICR = USART_ICR_ORECF;
	}

	if ((uart->CR1 & USART_CR1_TXEIE) && (uart->ISR & USART_ISR_TXE))
	{
		uart->TDR = params->tx.packet[params->tx.pos];
		params->tx.pos++;
		if (params->tx.pos == params->tx.len)
			uart->CR1 = uart->CR1 & ~USART_CR1_TXEIE | USART_CR1_TCIE;
	}

	if ((uart->CR1 & USART_CR1_TCIE) && (uart->ISR & USART_ISR_TC))
	{
		assert (params->tx.pos == params->tx.len);
		uart->CR1 = uart->CR1 & ~USART_CR1_TCIE;
		uart->ICR = USART_ISR_TC;
		auto callback = params->tx.callback;
		auto arg      = params->tx.callback_arg;
		memset (&params->tx, 0, sizeof(params->tx));
		callback(arg);
	}
}

extern "C" void USART1_IRQHandler() { ISR (USART1, 0); }
extern "C" void USART2_IRQHandler() { ISR (USART2, 1); }
extern "C" void USART3_IRQHandler() { ISR (USART3, 2); }
extern "C" void USART6_IRQHandler() { ISR (USART6, 5); }
extern "C" void UART4_IRQHandler() { ISR (UART4, 3); }
extern "C" void UART5_IRQHandler() { ISR (UART5, 4); }
extern "C" void UART7_IRQHandler() { ISR (UART7, 6); }
extern "C" void UART8_IRQHandler() { ISR (UART8, 7); }

void uart_init (USART_TypeDef* uart, const pin_and_af_t& tx_pin, const pin_and_af_t& rx_pin, uint32_t baud_rate, uart_rxne_callback_t rx_callback)
{
	clock_enable(uart);

	gpio_make_alternate (tx_pin, pin_output_speed_t::very_high);
	gpio_make_alternate (rx_pin, pin_output_speed_t::very_high);

	// USART needs to be in disabled state, in order to be able to configure some bits in CRx registers.
	assert ((uart->CR1 & USART_CR1_UE) == 0);

	uart->CR1 = USART_CR1_RXNEIE | USART_CR1_TE | USART_CR1_RE;

	auto clock_freq = clock_get_freq(uart);

	// Note: the calculation will have to be changed if we ever use an oversampling value of 8.
	uint32_t brr = (clock_freq + baud_rate / 2) / baud_rate;
	assert (brr >= 16);
	assert (brr <= 0xFFFFu);
	uart->BRR = (uint16_t)brr;

	auto i = get_uart_index(uart);

	memset (&uart_params[i], 0, sizeof(uart_params[i]));
	uart_params[i].rx = rx_callback;

	NVIC_EnableIRQ(uart_irqs[i]);

	uart->CR1 |= USART_CR1_UE;
}

void uart_send_blocking (USART_TypeDef *uart, uint8_t ch)
{
	// This must be called with interrupts enabled. An assertion is not possible at the moment,
	// beucase the assertion itself disables the interrupts and calls this function, causing a fault (stack overflow?)
	assert (clock_enabled(uart));
	assert (uart->CR1 & USART_CR1_UE);

	while (uart_is_sending(uart))
		;

	uart->TDR = ch;
}

bool uart_is_sending (USART_TypeDef *uart)
{
	return (uart->ISR & USART_ISR_TXE) == 0;
}

void uart_send (USART_TypeDef *uart, const uint8_t* packet, size_t len, uart_tx_callback_t callback, void* callback_arg)
{
	assert (len > 0);
	assert (clock_enabled(uart));
	assert (uart->CR1 & USART_CR1_UE);
	assert (!uart_is_sending(uart));

	auto params = &uart_params[get_uart_index(uart)];
	assert (params->tx.packet == nullptr);
	params->tx.packet = packet;
	params->tx.len    = len;
	params->tx.pos    = 0;
	params->tx.callback     = callback;
	params->tx.callback_arg = callback_arg;

	uart->CR1 = uart->CR1 | USART_CR1_TXEIE;
}
