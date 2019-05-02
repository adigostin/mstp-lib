
#include "stp.h"
#include "drivers/uart.h"
#include "drivers/vic.h"
#include "drivers/timer.h"
#include "drivers/LPC23xx_enet.h"
#include "drivers/gpio.h"
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

extern STP_CALLBACKS const callbacks_ip175c;
extern STP_CALLBACKS const callbacks_ip175d;

// ============================================================================

int putchar (int c)
{
	UART_Send (UART_INDEX_0, (unsigned char) c);
	return c;
}

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

	gpio_init();

	VIC_Init();

	Timer_Init (72000000, 4);

	UART_Init (UART_INDEX_0, 72000000, 115200, NULL);

	__enable_interrupt();

	printf ("\x1B[2J"); // ANSI_CLEAR_SCREEN
	printf("\r\nThis is the IAR-LPC-P2378-SK demo app that comes with IAR Embedded Workbench for ARM, "
		   "with the minimum required changes to support the IP175D switch and RSTP.\r\n");

	init_debug_leds();

	// -----------------------------------
	// On my board the RESET pin of the switch IC is pulled down with a resistor to keep it from
	// starting at powerup, and wired to P1[18] so that the software can start it when RSTP is running.
	// Let's deassert here this RESET signal, to let the switch IC generate the clock signal for
	// the LPC Ethernet module. This clock signal is needed by the MII management interface of the LPC.

	FIO1SET = (1 << 18);
	FIO1DIR |= (1 << 18);

	// Give it about 10ms to start its clock.
	Timer_Wait (10);

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

	unsigned short chipId = ENET_MIIReadRegister (20, 0);
	if (chipId == 0xffff)
	{
		chipId = 0x175c;
		printf ("IP175C detected.\r\n");
	}
	else if (chipId == 0x175D)
	{
		printf ("IP175D detected.\r\n");
	}
	else
		assert(false);

	// -----------------------------------

	// Initialize the driver of the built-in Ethernet controller.
	static const unsigned char MacAddress[] = { 0x01, 0x55, 0x55, 0xAA, 0xAA, 0x01 };

	ENET_Init (MacAddress);

	// -----------------------------------
	// Configure the RMII link between the switch chip and the built-in Ethernet controller.
	// See the IP175D datasheet for details.

	if (chipId == 0x175c)
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
	else if (chipId == 0x175d)
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
		if (chipId == 0x175c)
		{
			// See MII MAC mode register, page 96 of IP175C datasheet.
			if (ENET_MIIReadRegister (31, 3) & (1 << 5))
				break;
		}
		else if (chipId == 0x175d)
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

	if (chipId == 0x175c)
	{
		// Reset switching core - this doesn't reset registers 29.0-31, 30.0-31, 31.0-31
		// See page 80 of IP175C datasheet.
		ENET_MIIWriteRegister (30, 0, 0x175c);
	}
	else if (chipId == 0x175d)
	{
		ENET_MIIWriteRegister (20, 2, 0x175c);
	}
	else
		assert (false);

	// specifications say that we must wait at least 2 ms after this reset
	Timer_Wait (3);

	// -----------------------------------
	// Initialize and start the STP library.

	unsigned int timestamp = Timer_GetTimeMilliseconds();

	const STP_CALLBACKS* callbacks = (chipId == 0x175c) ? &callbacks_ip175c : &callbacks_ip175d;
	STP_BRIDGE* bridge = STP_CreateBridge (2, 0, 0, callbacks, MacAddress, 2);

	STP_EnableLogging (bridge, true);

	//STP_SetBridgePriority (bridge, 0, 0x9000, timestamp);

	STP_StartBridge (bridge, timestamp);

	// -----------------------------------
	// Now that STP is running, we can safely power-up the RJ45 ports.

	// enable with auto-negotiation
	ENET_MIIWriteRegister (PORT_RJ45_1, 0, (1 << 12));
	ENET_MIIWriteRegister (PORT_RJ45_2, 0, (1 << 12));

	unsigned int oneSecondTimerTickCount = Timer_GetTimeMilliseconds();
	unsigned int portCheckTimerTickCount = Timer_GetTimeMilliseconds();

	// ========================================================================

	while(1)
	{
		timestamp = Timer_GetTimeMilliseconds();

		static unsigned char frame [MAX_FRAME_SIZE_IN_SOFTWARE];

		unsigned int frameSize = tapdev_read (frame);
		if (frameSize > 0)
		{
			unsigned int receivePortIndex = ((frame [13] == 1) ? 0 : 1);

			//printf ("frame[12]=%02x, frame[13]=%02x\r\n", frame[12], frame[13]);

			if (!STP_GetPortEnabled (bridge, receivePortIndex))
			{
				STP_OnPortEnabled (bridge, receivePortIndex, 100, true, timestamp);
				update_debug_leds(bridge);
			}

			if (memcmp (frame, "\x01\x80\xC2\x00\x00\x00", 6) == 0)
			{
				STP_OnBpduReceived (bridge, receivePortIndex, &frame[21], frameSize - 21, timestamp);
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
		else if ((Timer_GetTimeMilliseconds() - portCheckTimerTickCount) >= 100)
		{
			// Code that runs every 100 ms and checks the link state of the RJ45 ports.

			portCheckTimerTickCount += 100;

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
		else if ((Timer_GetTimeMilliseconds() - oneSecondTimerTickCount) >= 1000)
		{
			// Code that runs once every second and calls into the STP library.
			oneSecondTimerTickCount += 1000;

			STP_OnOneSecondTick (bridge, timestamp);
			update_debug_leds(bridge);
		}
	}
}

// ============================================================================
