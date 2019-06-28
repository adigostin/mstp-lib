

#include "drivers/assert.h"
#include "drivers/timer.h"
#include "drivers/clock.h"
#include "drivers/scheduler.h"
#include "drivers/serial_console.h"
#include "drivers/ethernet.h"
#include "8836352.h"
#include "../mstp-lib/stp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stm32f769xx.h>

static constexpr uint8_t fallback_address[6] = { 0x10, 0x20, 0x30, 0x40, 0x55, 0x60 };

static constexpr struct ethernet_pins ethernet_pins
{
	.rmii_ref_clk = { {GPIOA,  1 }, 11 },
	.rmii_mdio    = { {GPIOA,  2 }, 11 },
	.rmii_mdc     = { {GPIOC,  1 }, 11 },
	.rmii_crs_dv  = { {GPIOA,  7 }, 11 },
	.rmii_rxd0    = { {GPIOC,  4 }, 11 },
	.rmii_rxd1    = { {GPIOC,  5 }, 11 },
	.rmii_tx_en   = { {GPIOB, 11 }, 11 },
	.rmii_txd0    = { {GPIOB, 12 }, 11 },
	.rmii_txd1    = { {GPIOB, 13 }, 11 },
};

STP_BRIDGE* bridge;

// ============================================================================

static void on_enet_frame_received (uint8_t* frame, size_t frame_len)
{
	enet_dump_frame (frame, frame_len);
}

// ============================================================================

static void* StpCallback_AllocAndZeroMemory (unsigned int size)
{
	//printf ("STP: allocating %u bytes.\r\n", size);
	auto res = malloc(size);
	assert(res);
	memset (res, 0, size);
	return res;
}

static void StpCallback_FreeMemory (void* p)
{
	free(p);
}

static const STP_CALLBACKS stp_callbacks =
{
	.enableBpduTrapping    = [](auto...) { assert(false); },
	.enableLearning        = [](auto...) { assert(false); },
	.enableForwarding      = [](auto...) { assert(false); },
	.transmitGetBuffer     = [](auto...) { assert(false); return (void*)nullptr; },
	.transmitReleaseBuffer = [](auto...) { assert(false); },
	.flushFdb              = [](auto...) { assert(false); },
	.debugStrOut           = [](auto...) { assert(false); },
	.onTopologyChange      = [](auto...) { assert(false); },
	.onPortRoleChanged     = [](auto...) { assert(false); },
	.allocAndZeroMemory    = &StpCallback_AllocAndZeroMemory,
	.freeMemory            = &StpCallback_FreeMemory,
};

int main()
{
	SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2)); // Configure FPU: set CP10 and CP11 Full Access
	SCB->VTOR = FLASH_BASE; // Vector Table Relocation in Internal FLASH

	clock_init(8);

	SCB_EnableICache();
	SCB_EnableDCache();

	static uint8_t event_queue_buffer[1024] __attribute__((section (".non_init")));
	event_queue_init (event_queue_buffer, sizeof(event_queue_buffer));

	serial_console_init (USART3, { GPIOC, 10, 7 }, { GPIOC, 11, 7 });
	printf (ANSI_CLEAR_SCREEN ANSI_WHITEONBLACK "\r\n\r\nTest App STM32+8836352.\r\n");

	printf ("Type '?' and Enter to see a list of available serial commands.\r\n");

	scheduler_init (TIM2);

	// ========================================================================
	// Initialize ethernet.

	static constexpr uint8_t mac_address[6] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };

	gpio_make_input({ GPIOA, 5 }, pin_pull::none);
	auto start_time = scheduler_get_time_ms32();
	while (gpio_get({ GPIOA, 5 }))
	{
		if (scheduler_get_time_ms32() - start_time >= 1500)
		{
			printf (ANSI_REDONBLACK "Switch INTn stays high." ANSI_WHITEONBLACK "\r\n");
			assert(false);
			while(1)
				;
		}
	}

	bool ok = enet_init (ethernet_pins, mac_address, &on_enet_frame_received);
	assert(ok);

	for(size_t pi=0; pi<2; pi++)
	{
		auto value = read_phy_register (pi, 9);
		write_phy_register (pi, 9, (value & 0xFC00));
	}
	/*
	// primary netif for DHCP
	ip_addr_t ip_addr = { (ram_config.ip_addr != 0xFFFF'FFFF) ? ram_config.ip_addr : PP_HTONL(LWIP_MAKEU32(192, 168, 0, 10)) };
	ip_addr_t netmask = { (ram_config.netmask != 0xFFFF'FFFF) ? ram_config.netmask : PP_HTONL(LWIP_MAKEU32(255, 255, 255, 0)) };
	ip_addr_t gateway = { (ram_config.gateway != 0xFFFF'FFFF) ? ram_config.gateway : PP_HTONL(LWIP_MAKEU32(192, 168, 0, 1)) };
	if(ram_config.hostname_len != 0xFFFF)
		gnetif.hostname = (const char *)ram_config.hostname;
	else
		gnetif.hostname = SNMP_LWIP_MIB2_SYSNAME;
	netif_add (&gnetif, &ip_addr, &netmask, &gateway, nullptr, &netif_init, &netif_input);
	netif_set_up(&gnetif);
	netif_set_default (&gnetif);

	// On the TA board we have an RMII link which, if we manage to initialize our Ethernet driver, is always good.
	netif_set_link_up (&gnetif);

	print_ip_config (nullptr);
	netif_set_status_callback (&gnetif, on_netif_status_changed);

	dhcp_start (&gnetif);

	scheduler_schedule_event_timer ([] { dhcp_coarse_tmr(); }, "dhcp_coarse_tmr", DHCP_COARSE_TIMER_MSECS, true);
	scheduler_schedule_event_timer ([] { dhcp_fine_tmr(); }, "dhcp_fine_tmr", DHCP_FINE_TIMER_MSECS, true);
	*/
	// ========================================================================

	extern const serial_command ta_commands[];
	serial_console_register_command_set(ta_commands);

	// Enable debugging while in sleep mode (we'll be putting the processor in this mode with the WFI instruction below).
	DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;

	static constexpr pin_t fault_led = { GPIOJ, 5 };
	gpio_make_output (fault_led, pin_output_speed_t::low, false);

	bridge = STP_CreateBridge (5, 4, 16, &stp_callbacks, mac_address, 100);

	while(true)
	{
		__WFI();
		event_queue_pop_all();
	}
}