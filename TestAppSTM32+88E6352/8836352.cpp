
#include "8836352.h"
#include "drivers/ethernet.h"
#include "drivers/serial_console.h"
#include "drivers/scheduler.h"
#include <stdio.h>

struct link_speed_and_duplex
{
	uint32_t speed_megabits_per_second;
	bool     full_duplex;
};

static link_speed_and_duplex link_states[2];

uint16_t read_phy_register (uint8_t phy_addr, uint8_t reg_addr)
{
	enet_write_smi (global2_dev_addr, global2_smi_phy_command, (1u << 15) | (1u << 12) | (0b10 << 10) | (phy_addr << 5) | reg_addr);
	while (enet_read_smi (global2_dev_addr, global2_smi_phy_command) & (1u << 15))
		;
	uint16_t reg = enet_read_smi (global2_dev_addr, global2_smi_phy_data);
	return reg;
}

void write_phy_register (uint8_t phy_addr, uint8_t reg_addr, uint16_t value)
{
	enet_write_smi (global2_dev_addr, global2_smi_phy_data, value);
	enet_write_smi (global2_dev_addr, global2_smi_phy_command, (1 << 15) | (1 << 12) | (0b01 << 10) | (phy_addr << 5) | reg_addr);
	while (enet_read_smi (global2_dev_addr, global2_smi_phy_command) & (1u << 15))
		;
}

static void poll_links()
{
	// Table 328 on page 448: Copper Specific Status Register 1
	for (uint8_t pi = 0; pi < 2; pi++)
	{
		uint16_t reg = read_phy_register (pi, 17);
		if (reg & (1u << 10))
		{
			// link up
			if (reg & (1u << 11))
			{
				// speed and duplex resolved (or auto-negotiation disabled)
				static constexpr uint32_t speeds[] = { 10, 100, 1000, (uint32_t)-1 };
				uint32_t speed = speeds[(reg >> 14) & 3];
				bool duplex = reg & (1u << 13);
				if ((link_states[pi].speed_megabits_per_second != speed) || (link_states[pi].full_duplex != duplex))
				{
					link_states[pi].speed_megabits_per_second = speed;
					link_states[pi].full_duplex = duplex;
					printf ("Port %d Link Up   (", pi);
					print_binary(reg);
					printf (") Speed %d, %s-duplex.\r\n", speed, duplex ? "Full" : "Half");
				}
			}
		}
		else
		{
			// link down
			if (link_states[pi].speed_megabits_per_second)
			{
				link_states[pi].speed_megabits_per_second = 0;
				printf ("Port %d Link Down (", pi);
				print_binary(reg);
				printf (").\r\n", pi);
			}
		}

		if (reg == 0xFFFF || reg == 0x7fff || reg == 0x3fff)
		{
			volatile int a = 0;
			a = reg;
		}
	}
}

void switch_init()
{
	scheduler_schedule_event_timer (poll_links, "poll_links", 100, true);
}

