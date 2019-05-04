
#include "stp.h"
#include "drivers/uart.h"
#include "drivers/serial_console.h"
#include "drivers/vic.h"
#include "drivers/scheduler.h"
#include "drivers/ethernet.h"
#include "drivers/gpio.h"
#include "drivers/event_queue.h"
#include "debug_leds.h"
#include <nxp/iolpc2387.h>
#include <stdio.h>
#include <stdlib.h>
#include <intrinsics.h>
#include <string.h>
#include <assert.h>

#define PORT_RJ45_1		0
#define PORT_RJ45_2		1
#define PORT_RMII		2

extern STP_CALLBACKS const stp_callbacks;
static STP_BRIDGE* bridge;
unsigned short chip_id;

// ============================================================================

static void InitPins()
{
	PINSEL0 =
		  (1 << 4)   // P0[2]  is serial TXD0
		| (1 << 6);  // P0[3]  is serial RXD0
	PINSEL1 = 0;
	PINSEL2 =
		  (1 << 0)   // P1[0]  is ENET
		| (1 << 2)   // P1[1]  is ENET
		| (1 << 8)   // P1[4]  is ENET
		| (1 << 16)  // P1[8]  is ENET
		| (1 << 18)  // P1[9]  is ENET
		| (1 << 20)  // P1[10] is ENET
		| (1 << 30); // P1[15] is ENET
	PINSEL3 =
		  (1 << 0)   // P1[16] is ENET_MDC
		| (1 << 2);  // P1[17] is ENET_MDIO
	PINSEL4 = 0;
	PINSEL5 = 0;
	PINSEL6 = 0;
	PINSEL7 = 0;
	PINSEL8 = 0;
	PINSEL9 = 0;
	PINSEL10 = 0; // ETM interface disabled

	// Disable pull-ups and pull-downs on the processor pins used by Ethernet.
	PINMODE2_bit.P1_0 = 2;
	PINMODE2_bit.P1_1 = 2;
	PINMODE2_bit.P1_4 = 2;
	PINMODE2_bit.P1_8 = 2;
	PINMODE2_bit.P1_9 = 2;
	PINMODE2_bit.P1_10 = 2;
	PINMODE2_bit.P1_15 = 2;
	PINMODE3_bit.P1_16 = 2;
	PINMODE3_bit.P1_17 = 2;

	// Wait about 10ms.
	for (volatile int i = 100; i; i--)
	{
		for (volatile int j = 500; j; j--)
			;
	}
}

static void InitClock ()
{
	// 1. Init OSC
	SCS_bit.OSCRANGE = 0;
	SCS_bit.OSCEN = 1;
	// 2.  Wait for OSC ready
	while(!SCS_bit.OSCSTAT);
	// 3. Disconnect PLL
	PLLCON_bit.PLLC = 0;
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
	// 4. Disable PLL
	PLLCON_bit.PLLE = 0;
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
	// 5. Select source clock for PLL
	CLKSRCSEL_bit.CLKSRC = 1; // Selects the main oscillator as a PLL clock source.
	// 6. Set PLL settings 288 MHz
	PLLCFG_bit.MSEL = 24-1;
	PLLCFG_bit.NSEL = 2-1;
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
	// 7. Enable PLL
	PLLCON_bit.PLLE = 1;
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
	// 8. Wait for the PLL to achieve lock
	while(!PLLSTAT_bit.PLOCK);
	// 9. Set clk divider settings
	CCLKCFG   = 4-1;            // 1/4 Fpll - 72 MHz
	USBCLKCFG = 6-1;            // 1/6 Fpll - 48 MHz
	PCLKSEL0 = PCLKSEL1 = 0;    // other peripherals
	// 10. Connect the PLL
	PLLCON_bit.PLLC = 1;
	PLLFEED = 0xAA;
	PLLFEED = 0x55;
}

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

static const serial_command commands[] = {
	{ "log", "log [on|off] - Enables or disables STP logging.", &process_log_command },
	{ "stp", "stp [on|off] - Enables or disables the protocol.", &process_stp_command },
	{ 0, 0, 0 }
};

// ============================================================================

static void on_one_second_timer()
{
	uint32_t timestamp = scheduler_get_time_ms32();
	STP_OnOneSecondTick (bridge, timestamp);
	update_debug_leds(bridge);
}

static void on_port_state_polling_timer()
{
	// Code that runs every 100 ms and checks the link state of the RJ45 ports.
	uint32_t timestamp = scheduler_get_time_ms32();

	for (unsigned int portIndex = 0; portIndex < 2; portIndex++)
	{
		unsigned short reg = ENET_MIIReadRegister (portIndex, 1);
		if ((reg & (1 << 2)) && !STP_GetPortEnabled (bridge, portIndex))
		{
			// link is now good
			STP_OnPortEnabled (bridge, portIndex, 100, true, timestamp);
			update_debug_leds(bridge);
		}
		else if (!(reg & (1 << 2)) && STP_GetPortEnabled (bridge, portIndex))
		{
			// link is now down
			STP_OnPortDisabled (bridge, portIndex, timestamp);
			update_debug_leds(bridge);
		}
	}
}

static void enable_source_port_special_tag()
{
	if (chip_id == 0x175c)
	{
		// Enable source-port special tag.
		// See the spanning tree register, page 89 of the IP175C datasheet.
		unsigned short reg = ENET_MIIReadRegister (30, 16);
		reg |= (1u << 7);
		ENET_MIIWriteRegister (30, 16, reg);

		// Add source-port tag for packets going to port 5 (RMII port).
		// Remove source-port tag from packets going to ports 0-4.
		// See the Tag register 10, page 76 of IP175C.
		reg = ENET_MIIReadRegister (29, 23);
		reg = reg | (1 << 1) | (0x1f << 6);
		ENET_MIIWriteRegister (29, 23, reg);
	}
	else if (chip_id == 0x175d)
	{
		// Enable special tagging for RX and TX.
		// See the Miscellaneous Control Register, page 102 of the IP175D datasheet.
		unsigned short reg = ENET_MIIReadRegister (21, 22);
		reg = reg | 3;
		ENET_MIIWriteRegister (21, 22, reg);

		// Add source-port tag for packets going to port 5 (RMII port).
		// See the Add Tag Control Register, page 111 of the IP175D datasheet.
		ENET_MIIWriteRegister (23, 8, (1 << 5));

		// Remove source-port tag from packets going to ports 0-4.
		// See the Remove Tag Control Register, page 111 of the IP175D datasheet.
		ENET_MIIWriteRegister (23, 16, 0x1f);
	}
	else
		assert (false);
}

// ============================================================================

static void on_frame_received (uint8_t* frame, size_t size)
{
	uint32_t timestamp = scheduler_get_time_ms32();

	unsigned int receivePortIndex = ((frame [13] == 1) ? 0 : 1);

	//printf ("frame[12]=%02x, frame[13]=%02x\r\n", frame[12], frame[13]);

	if (!STP_GetPortEnabled (bridge, receivePortIndex))
	{
		STP_OnPortEnabled (bridge, receivePortIndex, 100, true, timestamp);
		update_debug_leds(bridge);
	}

	if (memcmp (frame, "\x01\x80\xC2\x00\x00\x00", 6) == 0)
	{
		STP_OnBpduReceived (bridge, receivePortIndex, &frame[21], size - 21, timestamp);
		update_debug_leds(bridge);
	}
	else if ((frame[12] == 0x08) && (frame[13] == 0))
	{
		// IP frame
	}
	else if ((frame[12] == 0x08) && (frame[13] == 0x06))
	{
		// ARP frame
	}
}

// ============================================================================

int main ()
{
	// Power down the EMAC controller. Required during debugging.
	PCONP_bit.PCENET = 0;

	InitClock();
	InitPins();

	// MAM init
	MAMCR_bit.MODECTRL = 0;
	MAMTIM_bit.CYCLES  = 3;   // FCLK > 40 MHz
	MAMCR_bit.MODECTRL = 2;   // MAM functions fully enabled

	static __no_init unsigned char event_queue_buffer[1024];
	event_queue_init (event_queue_buffer, sizeof(event_queue_buffer));

	gpio_init();

	VIC_Init();

	scheduler_init (0, 72000000);

	serial_console_init (0, 72000000);

	serial_console_register_command_set (commands);

	__enable_interrupt();

	printf ("\x1B[2J"); // ANSI_CLEAR_SCREEN
	printf("\r\nSample embedded app for mstp-lib that runs on LPC2xxx and IP175C/D.\r\n");

	init_debug_leds();

	// -----------------------------------
	// On my board the RESET pin of the switch IC is pulled down with a resistor to keep it from
	// starting at powerup, and wired to P1[18] so that the software can start it when RSTP is running.
	// Let's deassert here this RESET signal, to let the switch IC generate the clock signal for
	// the LPC Ethernet module. This clock signal is needed by the MII management interface of the LPC.

	FIO1SET = (1 << 18);
	FIO1DIR |= (1 << 18);

	// Give it about 10ms to start its clock.
	scheduler_wait(10);

	// -----------------------------------
	// Initialize RMII.

	// Power Up the EMAC controller.
	PCONP_bit.PCENET = 1;

	// For some reason the MII management interface requires this bit to be set.
	MAC1 = (1 << 1); // MAC1_PASS_ALL;

	MCFG = 0x8018;  // clk/20
	MCMD = 0;
	MCFG_bit.RSTMIIMGMT = 0;

	COMMAND_bit.RMII = 1;

	// -----------------------------------
	// Now that we have a working MII management interface, let's power-down the RJ45 ports
	// before they have a chance to start forwarding and to create any loop.
	// We do this by writing to Register 0 of port's PHY (see Table 22-7 on page 65 in 802.3-2012)

	ENET_MIIWriteRegister (PORT_RJ45_1, 0, (1 << 11));
	ENET_MIIWriteRegister (PORT_RJ45_2, 0, (1 << 11));

	chip_id = ENET_MIIReadRegister (20, 0);
	if (chip_id == 0xffff)
	{
		chip_id = 0x175c;
		printf ("IP175C detected.\r\n");
	}
	else if (chip_id == 0x175D)
	{
		printf ("IP175D detected.\r\n");
	}
	else
		assert(false);

	// Let's instruct the switch chip to insert source port information in frames forwarded to us.
	// Since the switch doesn't appear to be able to insert this tag only in BPDUs (DA of 0180C2000000),
	// we enable this setting at startup and discard the tag for non-BPDU frames.
	enable_source_port_special_tag();

	// -----------------------------------

	// Initialize the driver of the built-in Ethernet controller.
	static const unsigned char MacAddress[] = { 0x80, 0x55, 0x55, 0xAA, 0xAA, 0x01 };

	ethernet_init (MacAddress, &on_frame_received);

	// -----------------------------------
	// Configure the RMII link between the switch chip and the built-in Ethernet controller.
	// See the IP175D datasheet for details.

	if (chip_id == 0x175c)
	{
		// See MII control register, page 98 of IP175C datasheet.
		ENET_MIIWriteRegister (31, 5,
							  (1 << 15) // P4EXT = 1
							| (0 << 11) // MII0_mac_mode_en = 0 (MII0 works as a PHY and should be connected to an external MAC device)
							| (1 << 10) // MII0_RMII_EN = 1 (MII0 RMII interface enabled)
							| (0 << 9)  // MII2_RMII_EN = 0 (MII2 RMII interface disabled)
							| (0 << 8)  // MII1_RMII_EN = 0 (MII1 RMII interface disabled)
							| (1 << 6)  // MII0_MAC_REPEATER = 1 (external PHY's TXEN does not loop back to CRS)
							| (1 << 4)  // MII0_PHY_COL_DELAY = 1 (collision delay 24 clocks)
							| (0 << 2));// MII2_EN = 0

		// P4_FORCE100 - page 74 of IP175C datasheet.
		ENET_MIIWriteRegister (29, 22,
							  (1 << 15) // P4_FORCE (enable force mode)
							| (1 << 10) // P4_FORCE100 (force MII0 (PHY mode) to be 100M)
							| (1 << 5)); // P4_FORCE_FULL (force MII0 (PHY mode) to be full duplex)
	}
	else if (chip_id == 0x175d)
	{
		ENET_MIIWriteRegister (21, 3,
							  (1 << 15) // P4EXT = 1
							| (0 << 11) // MII0_MAC_MODE_EN (MII0 works as a PHY and should be connected to an external MAC device)
							| (1 << 10) // MII0 RMII interface enabled
							| (0 << 9)	// MII2 RMII interface disabled
							| (0 << 8)	// MII1 RMII interface disabled
							| (1 << 6)	// MII0_MAC_REPEATER = 1 (external PHY's TXEN does not loop back to CRS)
							| (1 << 4)	// MII0_PHY_COL_DELAY = 1 (collision delay 24 clocks)
							| (0 << 2));// MII2_EN = 0

		ENET_MIIWriteRegister (20, 4,
							  (1 << 15)   // MAC5_FORCE_100
							| (1 << 13)); // MAC5_FORCE_FULL
	}
	else
		assert (false);

	// -----------------------------------
	// Wait until we have a good RMII link.
	// See paragraph 5.4.2 in the IP175D datasheet.

	while(true)
	{
		if (chip_id == 0x175c)
		{
			// See MII MAC mode register, page 96 of IP175C datasheet.
			if (ENET_MIIReadRegister (31, 3) & (1 << 5))
				break;
		}
		else if (chip_id == 0x175d)
		{
			// See paragraph 5.4.2 in the IP175D datasheet.
			if (ENET_MIIReadRegister (21, 1) & (1 << 5))
				break;
		}
		else
			assert (false);
	}

	// -----------------------------------
	// Reset the switching core.

	if (chip_id == 0x175c)
	{
		// Reset switching core - this doesn't reset registers 29.0-31, 30.0-31, 31.0-31
		// See page 80 of IP175C datasheet.
		ENET_MIIWriteRegister (30, 0, 0x175C);
	}
	else if (chip_id == 0x175d)
	{
		ENET_MIIWriteRegister (20, 2, 0x175D);
	}
	else
		assert (false);

	// specifications say that we must wait at least 2 ms after this reset
	scheduler_wait(3);

	// IP175C doesn't support flushing the filtering database.
	// As a workaround, we disable learning at power-up and we keep it disabled.
	if (chip_id == 0x175C)
	{
		unsigned short reg = ENET_MIIReadRegister (30, 16);
		reg = reg & 0xCF;
		ENET_MIIWriteRegister (30, 16, reg);
	}

	// -----------------------------------
	// Initialize and start the STP library.

	uint32_t timestamp = scheduler_get_time_ms32();

	bridge = STP_CreateBridge (2, 0, 0, &stp_callbacks, MacAddress, 2);

	//STP_EnableLogging (bridge, true);

	//STP_SetBridgePriority (bridge, 0, 0x9000, timestamp);

	STP_StartBridge (bridge, timestamp);

	// -----------------------------------
	// Now that STP is running, we can safely power-up the RJ45 ports.

	// enable with auto-negotiation
	ENET_MIIWriteRegister (PORT_RJ45_1, 0, (1 << 12));
	ENET_MIIWriteRegister (PORT_RJ45_2, 0, (1 << 12));

	scheduler_schedule_event_timer (on_one_second_timer, "on_one_second_timer", 1000, true);
	scheduler_schedule_event_timer (on_port_state_polling_timer, "on_port_state_polling_timer", 100, true);

	// ========================================================================

	while(true)
	{
		event_queue_pop_all();
	}
}

// ============================================================================
