
#include "serial_console.h"
#include "event_queue.h"
#include "assert.h"
#include <string.h>
#include <stdio.h>

static USART_TypeDef *uart;
static constexpr uint8_t indent_size = 2;
static uint8_t indent_level = 0;
static bool lineIsEmpty = true;
static const serial_command* command_sets[10];
static char serialBuffer[128];
static size_t serialSize;
static bool insert_cr_before_lf;

static void show_help (const char* params)
{
	for (auto p : command_sets)
	{
		if (p != nullptr)
		{
			while (p->command != 0)
			{
				unsigned short i;

				printf (p->command);

				for (i = strlen (p->command); i < 8; i++)
					putchar (' ');
				printf (" - ");

				printf ("%s\r\n", p->description);

				p++;
			}
		}
	}
}

static const serial_command local_commands[] =
{
	{ "help",    "display this help", show_help },
	{ "?",       "display this help", show_help },
	{ "cls",     "clear screen", [](const char*) { printf(ANSI_CLEAR_SCREEN); } },
	{ nullptr, nullptr, nullptr }
};

static bool process_text_command1 (const char* command, const serial_command* p)
{
	while (p->command != 0)
	{
		const char* p0 = p->command;
		const char* p1 = command;

		while (1)
		{
			if ((*p0 == 0) && (*p1 == 0x20))
			{
				p->handler (p1 + 1);
				return true;
			}
			else if ((*p0 == 0) && (*p1 == 0))
			{
				p->handler (p1);
				return true;
			}
			else if (*p0 != *p1)
				break;

			p0++;
			p1++;
		}

		p++;
	}

	return false;
}

static void process_text_command (const char* command)
{
	for (auto set : command_sets)
	{
		if (set && process_text_command1 (command, set))
			return;
	}

	printf ("unknown command.\r\n");
}

static void uart_rx_callback_event (void* arg)
{
	char ch = (char)(intptr_t)arg;

	if (ch == 0x0d)
	{
		if (serialSize > 0)
		{
			serialBuffer [serialSize] = 0;
			printf ("\r\n");
			process_text_command (serialBuffer);
			serialSize = 0;
		}
	}
	else if ((ch == 0x08) || (ch == 127))
	{
		if (serialSize > 0)
		{
			printf ("\x08 \x08");
			serialSize--;
		}
	}
	else if ((ch >= 32) && (serialSize < sizeof(serialBuffer) - 1))
	{
		putchar (ch);
		serialBuffer [serialSize++] = ch;
	}
}

static void uart_rx_callback_irql (uint8_t ch)
{
	event_queue_try_push (uart_rx_callback_event, (void*)(intptr_t)ch, "uart_rx_callback_event");
}

void serial_console_init (USART_TypeDef* uart, const pin_and_af_t& tx_pin, const pin_and_af_t& rx_pin)
{
	::uart = uart;
	::serialSize = 0;
	uart_init (uart, tx_pin, rx_pin, 115200, uart_rx_callback_irql);

	serial_console_register_command_set(local_commands);
}

int __putchar (int ch, __printf_tag_ptr)
{
//	if (b == '\n')
//		UART_SendByteToDebugPort ('\r');

	static bool previous_ch;

	if (insert_cr_before_lf && (ch == '\n') && (previous_ch != '\r'))
		uart_send_blocking(::uart, '\r');

	uart_send_blocking (::uart, (uint8_t)ch);

	if (ch == '\n')
	{
		for (uint8_t i = 0; i < indent_level; i++)
			uart_send_blocking (::uart, ' ');

		lineIsEmpty = true;
	}
	else
		lineIsEmpty = false;

	return 1;
}

void indent()
{
	indent_level += indent_size;

	if (lineIsEmpty)
	{
		for (uint8_t i = 0; i < indent_size; i++)
			uart_send_blocking (::uart, ' ');
	}
}

void unindent()
{
	assert (indent_level >= indent_size); // You're trying to Unindent() more than you've Indent()-ed.

	indent_level -= indent_size;

	if (lineIsEmpty)
	{
		for (uint8_t i = 0; i < indent_size; i++)
			uart_send_blocking (::uart, '\x08');
	}
}

void serial_console_register_command_set (const serial_command* commands)
{
	for (auto& set : command_sets)
	{
		if (set == nullptr)
		{
			set = commands;
			return;
		}
	}

	assert(false); // too many command sets
}

void serial_console_unregister_command_set (const serial_command* commands)
{
	assert(false); // not implemented
}

void print_binary (uint16_t val)
{
	for (size_t i = 0; i < 16; i++)
	{
		printf ((val & 0x8000) ? "1" : "0");
		if ((i < 15) && ((i & 3) == 3))
			printf (" ");

		val <<= 1;
	}
}

void serial_console_enable_insert_cr_before_lf (bool enable)
{
	insert_cr_before_lf = enable;
}
