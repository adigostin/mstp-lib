
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

static void get_mac_address()
{
/*
	uint8_t fma[6];
    app.get_mac_address(fma);

	uint8_t cma[6];
	enet_get_mac_address(cma);

    if (memcmp(fma, cma, 6) == 0)
	{
		printf("%02X:%02X:%02X:%02X:%02X:%02X \r\n", cma[0], cma[1], cma[2], cma[3], cma[4], cma[5]);
	}
	else if (fma[0] == 0xff && fma[1] == 0xff && fma[2] == 0xff && fma[3] == 0xff && fma[4] == 0xff && fma[5] == 0xff)
	{
		printf("No MAC address has been set on this device.\r\n");
		printf("MAC address currently in use = %02X:%02X:%02X:%02X:%02X:%02X \r\n", cma[0], cma[1], cma[2], cma[3], cma[4], cma[5]);
	}
    else
	{
		printf("MAC address currently in use = %02X:%02X:%02X:%02X:%02X:%02X \r\n", cma[0], cma[1], cma[2], cma[3], cma[4], cma[5]);
		printf("MAC address after restart    = %02X:%02X:%02X:%02X:%02X:%02X \r\n", fma[0], fma[1], fma[2], fma[3], fma[4], fma[5]);
    }
	*/
}

static void process_mac_command(const char *params)
{
	if (*params == 0)
		return get_mac_address();

	if (strlen(params) != 17)
	{
		printf("The address must have the format 11:22:33:44:55:66\r\n");
		return;
	}

	int ints[6];
	int parsed = sscanf(params, "%02X:%02X:%02X:%02X:%02X:%02X", &ints[0], &ints[1], &ints[2], &ints[3], &ints[4], &ints[5]);
	if (parsed != 6)
	{
		printf("Address not valid.\r\n");
		return;
	}

	uint8_t mac_addr[6] = { (uint8_t)ints[0], (uint8_t)ints[1], (uint8_t)ints[2], (uint8_t)ints[3], (uint8_t)ints[4], (uint8_t)ints[5] };
	//app.set_mac_address (mac_addr);
	//printf ("The MAC address has been set. You need to restart the device for this change to take effect.\r\n");
}
/*
void print_ip_config (const char* text_before)
{
	if (text_before != nullptr)
		printf ("%s\r\n", text_before);

	indent();

	uint32_t ip = netif_ip_addr4(&gnetif)->addr;
	printf ("ip addr: %s\r\n", ipaddr_ntoa(&gnetif.ip_addr));

	uint32_t mask = netif_ip_netmask4(&gnetif)->addr;
	printf ("netmask: %s\r\n", ipaddr_ntoa(&gnetif.netmask));

	uint32_t gw = netif_ip_gw4(&gnetif)->addr;
	printf ("gateway: %s\r\n",ipaddr_ntoa(&gnetif.gw));

	printf ("DHCP Client is %s.\r\n", ram_config.use_dhcp_client ? "enabled" : "disabled");

	printf ("Secondary IP for FCS communication hardcoded to ");
	printf ("%s", ipaddr_ntoa(&elbit_netif.ip_addr));
	printf (" // %s\r\n", ipaddr_ntoa(&elbit_netif.netmask));

	unindent();
}


static void process_ip_command (const char* params)
{
	while (*params == 0x20 || *params == 0x09)
		params++;

	if (*params == 0)
		return print_ip_config(nullptr);

	if (strcmp (params, "auto") == 0)
	{
		if (ram_config.use_dhcp_client)
			printf ("DHCP Client was already enabled. Restarting negotiation.\r\n");
		else
		{
			ram_config.use_dhcp_client = 1;
			write_config_to_flash();
			printf ("DHCP Client is now enabled and will remain enabled at power-up.\r\n");
		}

		dhcp_start(&gnetif);
		return;
	}

	ip4_addr_t address;
	if (!ip4addr_aton(params, &address))
		goto err;
	while (isdigit(*params) || (*params == '.'))
		params++;
	while (*params == 0x20)
		params++;

	ip4_addr_t netmask;
	if (!ip4addr_aton(params, &netmask) || !ip4_addr_netmask_valid(ip4_addr_get_u32(&netmask)))
		goto err;
	while (isdigit(*params) || (*params == '.'))
		params++;
	while (*params == 0x20)
		params++;

	ip4_addr_t gateway;
	gateway = IPADDR4_INIT(IPADDR_ANY);
	if (*params != 0)
	{
		if (!ip4addr_aton(params, &gateway))
			goto err;
	}

	set_manual_ip_config (&gnetif, &address, &netmask, &gateway);

	printf ("DHCP client was disabled and will remain disabled at power-up. ");
	printf ("To use DHCP again, type \"ip auto\". ");
	print_ip_config ("Active IP configuration:");

	return;

err:
	printf ("Wrong arguments.\r\n");
}

static void process_lwipstats_command (const char*)
{
	serial_console_enable_insert_cr_before_lf(true);
	stats_display();
	serial_console_enable_insert_cr_before_lf(false);
}
*/
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
//	{ "ip",        "Shows or sets the IP configuration: ip [auto | [addr mask [gateway]]]", process_ip_command },
//	{ "lwipstats", "", process_lwipstats_command },
	{ "phy",       "\"phy devaddr, regaddr[, hex_value]\" - reads/writes a PHY register", process_phy_command },
	{ "smitest",   "", process_smi_test_command },
	{ "phytest",   "", process_phy_test_command },
	{ "stp",       "stp [on|off] - shows the stp state, or turns it on or off", process_stp_command },
	{ nullptr, nullptr, nullptr }
};
