
#pragma once
#include "gpio.h"

typedef void (*uart_rxne_callback_t)(uint8_t ch);

void uart_init (LPUART_Type* uart);
