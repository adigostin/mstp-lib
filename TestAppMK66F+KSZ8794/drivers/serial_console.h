
#pragma once

struct serial_command
{
	const char* command;
    const char* description;
	void (*handler)(const char* params);
};

void serial_console_init (void(*output)(char ch), bool echo_input, bool use_crlf);
void serial_console_process_input (char ch);
void serial_console_register_command_set (const serial_command* commands);
void serial_console_unregister_command_set (const serial_command* commands);
void indent();
void unindent();

#define ANSI_WHITEONBLACK  "\x1B[0;37;40m"
#define ANSI_GREENONBLACK  "\x1B[0;32;40m"
#define ANSI_YELLOWONBLACK "\x1B[0;33;40m"
#define ANSI_BLACKONYELLOW "\x1B[0;30;43m"
#define ANSI_REDONBLACK    "\x1B[0;31;40m"
#define ANSI_CLEAR_SCREEN  "\x1B[2J"
