
#include "serial_console.h"
#include "assert.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static void(*output)(char ch);
static bool echo_input;
static bool use_crlf;
static constexpr uint8_t indent_size = 2;
static uint8_t indent_level = 0;
static bool lineIsEmpty = true;
static const serial_command* command_sets[10];
static char serialBuffer[128];
static size_t serialSize;

static void print_str (const char* s)
{
	while(*s)
	{
		output(*s);
		s++;
	}
}

static void print_eol()
{
	if (use_crlf)
		output('\x0D');
	output('\x0A');
}

static void show_help (const char* params)
{
	for (auto p : command_sets)
	{
		if (p != nullptr)
		{
			while (p->command != 0)
			{
				unsigned short i;

				print_str (p->command);

				for (i = strlen (p->command); i < 8; i++)
					output(' ');
				print_str(" - ");

				print_str (p->description);
				print_eol();

				p++;
			}
		}
	}
}

static const serial_command default_commands[] =
{
	{ "help",    "display this help", show_help },
	{ "?",       "display this help", show_help },
	{ "cls",     "clear screen", [](const char*) { print_str(ANSI_CLEAR_SCREEN); } },
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

	print_str ("unknown command.");
	print_eol();
}

void serial_console_process_input (char ch)
{
	if (ch == (use_crlf ? 0x0d : 0x0a))
	{
		if (echo_input)
			output(ch);

		if (serialSize > 0)
		{
			serialBuffer [serialSize] = 0;
			process_text_command (serialBuffer);
			serialSize = 0;
		}
	}
	else if ((ch == 0x08) || (ch == 127))
	{
		if (serialSize > 0)
		{
			print_str ("\x08 \x08");
			serialSize--;
		}
	}
	else if ((ch >= 32) && (serialSize < sizeof(serialBuffer) - 1))
	{
		if (echo_input)
			output(ch);

		serialBuffer [serialSize++] = ch;
	}
}

void serial_console_init (void(*output)(char ch), bool echo_input, bool use_crlf)
{
	::output     = output;
	::echo_input = echo_input;
	::use_crlf   = use_crlf;
	serial_console_register_command_set(default_commands);
}

int __putchar (int ch, __printf_tag_ptr)
{
	static bool previous_ch;

	//if (use_crlf && (ch == '\n') && (previous_ch != '\r'))
	//	output('\r');

	output((char)ch);

	if (ch == '\n')
	{
		for (uint8_t i = 0; i < indent_level; i++)
			output(' ');

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
			output(' ');
	}
}

void unindent()
{
	assert (indent_level >= indent_size); // You're trying to Unindent() more than you've Indent()-ed.

	indent_level -= indent_size;

	if (lineIsEmpty)
	{
		for (uint8_t i = 0; i < indent_size; i++)
			output('\x08');
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
