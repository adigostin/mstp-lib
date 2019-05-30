
#include "stp.h"
#include "drivers/serial_console.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern STP_BRIDGE* bridge;

static void process_log_command (const char* params)
{
	if (*params == 0)
	{
		printf ("STP logging is currently %s.\r\n", STP_IsLoggingEnabled(bridge) ? "enabled" : "disabled");
		return;
	}

	if (strcasecmp(params, "on") == 0)
	{
		STP_EnableLogging (bridge, true);
		printf ("STP logging is now %s.\r\n", "enabled");
	}
	else if (strcasecmp(params, "off") == 0)
	{
		STP_EnableLogging (bridge, false);
		printf ("STP logging is now %s.\r\n", "disabled");
	}
	else
		printf ("Wrong params.\r\n");
}

static void process_stp_command (const char* params)
{
	if (*params == 0)
	{
		printf ("STP is currently %s.\r\n", STP_IsBridgeStarted(bridge) ? "enabled" : "disabled");
		return;
	}

	if (strcasecmp(params, "on") == 0)
	{
		if (STP_IsBridgeStarted(bridge))
			printf ("STP is already %s.\r\n", "enabled");
		else
		{
			printf ("Enabling STP...\r\n");
			STP_StartBridge(bridge, true);
			printf ("STP is now %s.\r\n", "enabled");
		}
	}
	else if (strcasecmp(params, "off") == 0)
	{
		if (!STP_IsBridgeStarted(bridge))
			printf ("STP is already %s.\r\n", "disabled");
		else
		{
			printf ("Disabling STP...\r\n");
			STP_StopBridge(bridge, true);
			printf ("STP is now %s.\r\n", "disabled");
		}
	}
	else
		printf ("Wrong params.\r\n");

}

extern const serial_command commands[] = {
	{ "log", "log [on|off] - Enables or disables STP logging.", &process_log_command },
	{ "stp", "stp [on|off] - Enables or disables the protocol.", &process_stp_command },
	{ 0, 0, 0 }
};
