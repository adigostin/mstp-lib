
#include "drivers/ethernet.h"
#include "drivers/serial_console.h"
#include "drivers/scheduler.h"
#include "../mstp-lib/stp.h"
#include "8836352.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

extern STP_BRIDGE* bridge;

extern uint16_t read_phy_register (uint8_t phy_addr, uint8_t reg_addr);
extern void write_phy_register (uint8_t phy_addr, uint8_t reg_addr, uint16_t value);

static void process_mac_command (const char *params)
{
	if (*params == 0)
	{
		uint8_t cma[6];
		enet_get_mac_address(cma);
		printf("%02X:%02X:%02X:%02X:%02X:%02X \r\n", cma[0], cma[1], cma[2], cma[3], cma[4], cma[5]);
		return;
	}

	printf ("Setting not yet implemented.\r\n");
}

static void process_phy_command (const char* params)
{
	uint32_t addr, reg, value;
	auto param_count = sscanf (params, "%d, %d, %x", &addr, &reg, &value);
	if (param_count == 2)
	{
		value = read_phy_register ((uint8_t)addr, (uint8_t)reg);
		printf ("[Phy %u] [Reg %u] = 0x%04x (", addr, reg, value);
		print_binary(value);
		printf (")\r\n");
	}
	else if (param_count == 3)
	{
		write_phy_register ((uint8_t)addr, (uint8_t)reg, (uint16_t)value);
		value = read_phy_register ((uint8_t)addr, (uint8_t)reg);
		printf ("Written. Value read back = 0x%04x (", value);
		print_binary(value);
		printf (")\r\n");
	}
	else
		printf ("bad params!\r\n");
}

static void smi_test_callback()
{
	constexpr size_t count = 512;

	static uint32_t qq[count / 32];
	static size_t qi;

	uint16_t value = (uint16_t)rand();
	enet_write_smi (switch_dev_addr_global2, global2_smi_phy_data, value);
	uint16_t rb = enet_read_smi (switch_dev_addr_global2, global2_smi_phy_data);

	if (value == rb)
		qq[(qi / 32)] |= (1u << (qi % 32));
	else
		qq[(qi / 32)] &= ~(1u << (qi % 32));

	qi++;
	if (qi == count)
	{
		qi = 0;
		uint32_t goods = 0;
		for (size_t i = 0; i < sizeof(qq) / sizeof(qq[0]); i++)
			goods += __builtin_popcount(qq[i]);

		printf ("%d\r\n", 100 * goods / count);
	}
}

static void process_smi_test_command (const char*)
{
	static timer_t* t;

	if (!t)
		t = scheduler_schedule_event_timer(smi_test_callback, "test", 1, true);
	else
	{
		scheduler_cancel_timer(t);
		t = nullptr;
	}
}

static void phy_test_callback()
{
	constexpr size_t count = 512;

	static uint32_t qq[count / 32];
	static size_t qi;

	uint16_t value = (uint16_t)rand();
	write_phy_register (0, 14, value);
	uint16_t rb = read_phy_register (0, 14);

	if (value == rb)
		qq[(qi / 32)] |= (1u << (qi % 32));
	else
		qq[(qi / 32)] &= ~(1u << (qi % 32));

	qi++;
	if (qi == count)
	{
		qi = 0;
		uint32_t goods = 0;
		for (size_t i = 0; i < sizeof(qq) / sizeof(qq[0]); i++)
			goods += __builtin_popcount(qq[i]);

		printf ("%d\r\n", 100 * goods / count);
	}
}

static void process_phy_test_command (const char*)
{
	static timer_t* t;

	if (!t)
		t = scheduler_schedule_event_timer(phy_test_callback, "phy_test", 1, true);
	else
	{
		scheduler_cancel_timer(t);
		t = nullptr;
	}
}

static void process_stp_command (const char* params)
{
	if (*params == 0)
	{
		printf ("STP is %s.\r\n", STP_IsBridgeStarted(bridge) ? "on" : "off");
		return;
	}

	if (strcmp(params, "on") == 0)
	{
		if (STP_IsBridgeStarted(bridge))
			printf ("STP already on.\r\n");
		else
			STP_StartBridge(bridge, scheduler_get_time_ms32());
		return;
	}

	if (strcmp(params, "off") == 0)
	{
		if (!STP_IsBridgeStarted(bridge))
			printf ("STP already off.\r\n");
		else
			STP_StopBridge(bridge, scheduler_get_time_ms32());
		return;
	}
}

extern const serial_command ta_commands[] =
{
	{ "mac",       "mac [aa:bb:cc:dd:ee:ff] - show or set the MAC adress", process_mac_command },
	{ "phy",       "\"phy devaddr, regaddr[, hex_value]\" - reads/writes a PHY register", process_phy_command },
	{ "smitest",   "", process_smi_test_command },
	{ "phytest",   "", process_phy_test_command },
	{ "stp",       "stp [on|off] - shows the stp state, or turns it on or off", process_stp_command },
	{ nullptr, nullptr, nullptr }
};
