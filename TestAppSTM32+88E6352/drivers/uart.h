
#pragma once
#include "gpio.h"
#include <stm32f769xx.h>
#include <stddef.h>

typedef void (*uart_rxne_callback_t)(uint8_t ch);
typedef void (*uart_tx_callback_t)(void* arg);

void uart_init (USART_TypeDef* uart, const pin_and_af_t& tx_pin, const pin_and_af_t& rx_pin, uint32_t baud_rate, uart_rxne_callback_t rx_callback);
void uart_set_baud_rate (USART_TypeDef *uart, uint32_t br);
void uart_send_blocking (USART_TypeDef *uart, uint8_t ch);
bool uart_is_sending (USART_TypeDef *uart);

// note that the packet buffer must remain valid until the callback is called
void uart_send (USART_TypeDef *uart, const uint8_t* packet, size_t len, uart_tx_callback_t callback, void* callback_arg);
