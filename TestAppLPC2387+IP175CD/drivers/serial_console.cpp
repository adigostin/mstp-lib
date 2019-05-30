
#include "serial_console.h"
#include "uart.h"
#include "event_queue.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static unsigned uart;
static const uint8_t indent_size = 2;
static uint8_t indent_level = 0;
static bool lineIsEmpty = true;
static const serial_command* command_sets[10];
static char serialBuffer[128];
static size_t serialSize;
static bool insert_cr_before_lf;

static void show_help (const char* params)
{
	for (size_t i = 0; i < sizeof(command_sets) / sizeof(command_sets[0]); i++)
	{
		const serial_command* p = command_sets[i];
		if (p != NULL)
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

static void clear_screen (const char* params)
{
	printf(ANSI_CLEAR_SCREEN);
}

static const serial_command local_commands[] =
{
	{ "help",    "display this help", show_help },
	{ "?",       "display this help", show_help },
	{ "cls",     "clear screen", clear_screen },
	{ NULL, NULL, NULL }
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
	for (size_t i = 0; i < sizeof(command_sets) / sizeof(command_sets[0]); i++)
	{
		const serial_command* set = command_sets[i];
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

static void uart_rx_callback_irql (unsigned uart_index, unsigned char data)
{
	event_queue_try_push (uart_rx_callback_event, (void*)data, "uart_rx_callback_event");
}

void serial_console_init (unsigned uart, unsigned clock_frequency)
{
	::uart = uart;
	uart_init (uart, clock_frequency, 115200, uart_rx_callback_irql);
	serial_console_register_command_set(local_commands);
}

extern "C" int putchar (int ch)
{
	if (insert_cr_before_lf && (ch == '\n'))
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

	return ch;
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
	for (size_t i = 0; i < sizeof(command_sets) / sizeof(command_sets[0]); i++)
	{
		const serial_command* set = command_sets[i];
		if (set == NULL)
		{
			command_sets[i] = commands;
			return;
		}
	}

	assert(false); // too many command sets
}

void serial_console_unregister_command_set (const serial_command* commands)
{
	assert(false); // not implemented
}

void serial_console_enable_insert_cr_before_lf (bool enable)
{
	insert_cr_before_lf = enable;
}
