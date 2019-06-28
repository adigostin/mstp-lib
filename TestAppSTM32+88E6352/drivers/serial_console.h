
#pragma once
#include "uart.h"

struct serial_command
{
	const char* command;
    const char* description;
	void (*handler)(const char* params);
};

void serial_console_init (USART_TypeDef* uart, const pin_and_af_t& tx_pin, const pin_and_af_t& rx_pin);
void serial_console_register_command_set (const serial_command* commands);
void serial_console_unregister_command_set (const serial_command* commands);
void serial_console_enable_insert_cr_before_lf (bool enable);
void indent();
void unindent();
void print_binary (uint16_t val);

#define ANSI_WHITEONBLACK  "\x1B[0;37;40m"
#define ANSI_GREENONBLACK  "\x1B[0;32;40m"
#define ANSI_YELLOWONBLACK "\x1B[0;33;40m"
#define ANSI_BLACKONYELLOW "\x1B[0;30;43m"
#define ANSI_REDONBLACK    "\x1B[0;31;40m"
#define ANSI_CLEAR_SCREEN  "\x1B[2J"
