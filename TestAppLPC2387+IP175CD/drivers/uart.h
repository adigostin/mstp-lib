
#pragma once

typedef void (*uart_rx_callback) (unsigned uart_index, unsigned char data);

void uart_init (unsigned uart_index, unsigned clock_frequency, unsigned baudrate, uart_rx_callback rx_callback);
void uart_send_blocking (unsigned uart_index, unsigned char data);
