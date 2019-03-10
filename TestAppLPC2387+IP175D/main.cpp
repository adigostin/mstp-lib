
#include <nxp/iolpc2387.h>
#include <stdio.h>
#include <stdlib.h>
#include <intrinsics.h>
#include <string.h>
#include <assert.h>
#include "stp.h"
#include "uart.h"
#include "vic.h"
#include "timer.h"
#include "LPC23xx_enet.h"

#define PORT_RJ45_1		0
#define PORT_RJ45_2		1
#define PORT_RMII		2

extern STP_CALLBACKS const Callbacks;

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
		  (0 << 0)   // P0[0]  is GPIO
		| (0 << 2)   // P0[1]  is GPIO
		| (1 << 4)   // P0[2]  is serial TXD0
		| (1 << 6)   // P0[3]  is serial RXD0
		| (0 << 8)   // P0[4]  is GPIO
		| (0 << 10)  // P0[5]  is GPIO
		| (0 << 12)  // P0[6]  is GPIO
		| (0 << 14)  // P0[7]  is GPIO
		| (0 << 16)  // P0[8]  is GPIO
		| (0 << 18)  // P0[9]  is GPIO
		| (0 << 20)  // P0[10] is GPIO
		| (0 << 22)  // P0[11] is GPIO
		| (0 << 24)  // P0[12] is missing
		| (0 << 26)  // P0[13] is missing
		| (0 << 28)  // P0[14] is missing
		| (0 << 30); // P0[15] is GPIO
	PINSEL1 = 0;
	PINSEL2 =
		  (1 << 0)   // P1[0]  is ENET
		| (1 << 2)   // P1[1]  is ENET
		| (0 << 4)   // P1[2]  is missing
		| (0 << 6)   // P1[3]  is missing
		| (1 << 8)   // P1[4]  is ENET
		| (0 << 10)  // P1[5]  is missing
		| (0 << 12)  // P1[6]  is missing
		| (0 << 14)  // P1[7]  is missing
		| (1 << 16)  // P1[8]  is ENET
		| (1 << 18)  // P1[9]  is ENET
		| (1 << 20)  // P1[10] is ENET
		| (0 << 22)  // P1[11] is missing
		| (0 << 24)  // P1[12] is missing
		| (0 << 26)  // P1[13] is missing
		| (0 << 28)  // P1[14] is GPIO
		| (1 << 30); // P1[15] is ENET
	PINSEL3 =
		  (1 << 0)   // P1[16] is ENET_MDC
		| (1 << 2)   // P1[17] is ENET_MDIO
		| (0 << 4)   // P1[18] is not connected
		| (2 << 6)   // P1[19] is USB_PPWR1
		| (0 << 8)   // P1[20] is GPIO
		| (0 << 10)  // P1[21] is GPIO
		| (2 << 12)  // P1[22] is USB_PWRD1
		| (0 << 14)  // P1[23] is GPIO
		| (2 << 16)  // P1[24] is PWM
		| (0 << 18)  // P1[25] is GPIO
		| (0 << 20)  // P1[26] is GPIO
		| (2 << 22)  // P1[27] is USB_OVRCR1
		| (0 << 24)  // P1[28] is GPIO
		| (0 << 26)  // P1[29] is GPIO
		| (0 << 28)  // P1[30] is GPIO
		| (0 << 30); // P1[31] is GPIO
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

	// Enable Fast GPIO0,1
	SCS_bit.GPIOM = 1;

	VIC_Init();

	Timer_Init (72000000, 4);

	UART_Init (UART_INDEX_0, 72000000, 115200, NULL);

	__enable_interrupt();

	printf ("\x1B[2J"); // ANSI_CLEAR_SCREEN
	printf("\r\nThis is the IAR-LPC-P2378-SK demo app that comes with IAR Embedded Workbench for ARM, "
		   "with the minimum required changes to support the IP175D switch and RSTP.\r\n");

	// -----------------------------------
	// On my board the RESET pin of the switch IC is pulled down with a resistor to keep it from
	// starting at powerup, and wired to P1[14] so that the software can start it when RSTP is running.
	// Let's deassert here this RESET signal, to let the switch IC generate the clock signal for
	// the LPC Ethernet module. This clock signal is needed by the MII management interface of the LPC.

	FIO1SET = (1 << 14);
	FIO1DIR |= (1 << 14);

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

	// -----------------------------------

	// Initialize the driver of the built-in Ethernet controller.
	static const unsigned char MacAddress[] = { 0x01, 0x55, 0x55, 0xAA, 0xAA, 0x01 };

	ENET_Init (MacAddress);

	// -----------------------------------
	// Configure the RMII link between the switch chip and the built-in Ethernet controller.
	// See the IP175D datasheet for details.

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

	// -----------------------------------
	// Wait until we have a good RMII link.
	// See paragraph 5.4.2 in the IP175D datasheet.

	while ((ENET_MIIReadRegister (21, 1) & (1 << 5)) == 0)
		;

	printf ("21, 0 = 0x%04x\r\n", ENET_MIIReadRegister (21, 0));
	printf ("21, 3 = 0x%04x\r\n", ENET_MIIReadRegister (21, 3));

	// -----------------------------------
	// Reset the switching core.
	ENET_MIIWriteRegister (20, 2, 0x175c);

	// specifications say that we must wait at least 2 ms after this reset
	Timer_Wait (3);

	// -----------------------------------
	// Initialize and start the STP library.

	unsigned int timestamp = Timer_GetTimeMilliseconds();

	STP_BRIDGE* bridge = STP_CreateBridge (3, 0, 0, &Callbacks, MacAddress, 2);

	STP_EnableLogging (bridge, true);

	//STP_SetBridgePriority (bridge, 0, 0x9000, timestamp);

	// Port 2 of the switch chip, which on my board is the RMII connection to the microcontroller's Ethernet module, is an Edge port.
	STP_SetPortAdminEdge (bridge, PORT_RMII, true, timestamp);

	// Port 2 (RMII) is always enabled.
	STP_OnPortEnabled (bridge, PORT_RMII, 100, true, timestamp);

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
				}
				else if (!(reg & (1 << 2)) && STP_GetPortEnabled (bridge, portIndex))
				{
					// link is now down
					STP_OnPortDisabled (bridge, portIndex, timestamp);
				}
			}
		}
		else if ((Timer_GetTimeMilliseconds() - oneSecondTimerTickCount) >= 1000)
		{
			// Code that runs once every second and calls into the STP library.
			oneSecondTimerTickCount += 1000;

			STP_OnOneSecondTick (bridge, timestamp);
		}
	}
}

// ============================================================================

static void StpCallback_EnableBpduTrapping (const struct STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	if (enable)
	{
		// Enable special tagging for RX and TX.
		// See the Miscellaneous Control Register, page 102 of the IP175D datasheet.
		unsigned short reg = ENET_MIIReadRegister (21, 22);
		reg = (reg & ~3u) | 3u;
		ENET_MIIWriteRegister (21, 22, reg);

		// Add source-port tag for packets going to port 5 (RMII port).
		// See the Add Tag Control Register, page 111 of the IP175D datasheet.
		ENET_MIIWriteRegister (23, 8, (1 << 5));

		// Remove source-port tag from packets going to ports 0-4.
		// See the Remove Tag Control Register, page 111 of the IP175D datasheet.
		ENET_MIIWriteRegister (23, 16, 0x1f);

		// Forward BPDUs only to the CPU.
		// Page 79 of the IP175D datasheet.
		reg = ENET_MIIReadRegister (20, 8);
		reg = (reg & ~3u) | 1u;
		ENET_MIIWriteRegister (20, 8, reg);
	}
	else
	{
		// Here goes the code that undoes the switch chip configuration from above.
		// This is not yet implemented in this demo app.
	}
}

static void StpCallback_EnableLearning (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp)
{
	unsigned int i = ENET_MIIReadRegister (20, 6);

	if (portIndex == PORT_RJ45_1)
	{
		i = (i & ~(1ul << 0)) | ((enable != 0) ? (1ul << 0) : 0);
	}
	else if (portIndex == PORT_RJ45_2)
	{
		i = (i & ~(1ul << 1)) | ((enable != 0) ? (1ul << 1) : 0);
	}
	else if (portIndex == PORT_RMII)
	{
		i = (i & ~(1ul << 5)) | ((enable != 0) ? (1ul << 5) : 0);
	}
	else
		assert (0);

	ENET_MIIWriteRegister (20, 6, i);
}

static void StpCallback_EnableForwarding (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp)
{
	unsigned int i = ENET_MIIReadRegister (20, 6);

	if (portIndex == PORT_RJ45_1)
	{
		i = (i & ~(1ul << 8)) | ((enable != 0) ? (1ul << 8) : 0);
	}
	else if (portIndex == PORT_RJ45_2)
	{
		i = (i & ~(1ul << 9)) | ((enable != 0) ? (1ul << 9) : 0);
	}
	else if (portIndex == PORT_RMII)
	{
		i = (i & ~(1ul << 13)) | ((enable != 0) ? (1ul << 13) : 0);
	}
	else
		assert (0);

	ENET_MIIWriteRegister (20, 6, i);
}

static unsigned char BpduFrameBuffer [21 + 36];
static unsigned int BpduFrameSize;

static void* StpCallback_TransmitGetBuffer (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	if (portIndex == PORT_RMII)
	{
		// The library is trying to send a BPDU to the RMII port, which we know is us.
		// Let's not allow it cause it would be a waste of bandwidth.
		return NULL;
	}

	assert (21 + bpduSize <= sizeof (BpduFrameBuffer));
	BpduFrameSize = 21 + bpduSize;

	// Dest MAC address
	BpduFrameBuffer[0] = 0x01;
	BpduFrameBuffer[1] = 0x80;
	BpduFrameBuffer[2] = 0xC2;
	BpduFrameBuffer[3] = 0x00;
	BpduFrameBuffer[4] = 0x00;
	BpduFrameBuffer[5] = 0x00;

	// Source Mac Address
	memcpy (&BpduFrameBuffer[6], STP_GetBridgeAddress(bridge)->bytes, 6);
	assert ((unsigned int) BpduFrameBuffer[11] + 1 + portIndex <= 255);
	BpduFrameBuffer[11] += (1 + portIndex);

	// switch chip header
	BpduFrameBuffer[12] = 0x81;
	BpduFrameBuffer[13] = (1 << portIndex);
	BpduFrameBuffer[14] = 0;
	BpduFrameBuffer[15] = 0;

	// EtherType/Size
	BpduFrameBuffer[16] = 0;
	BpduFrameBuffer[17] = 3 + bpduSize;

	// LLC field
	BpduFrameBuffer[18] = 0x42;
	BpduFrameBuffer[19] = 0x42;
	BpduFrameBuffer[20] = 0x03;

	return &BpduFrameBuffer[21];
}

static void StpCallback_TransmitReleaseBuffer (const struct STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	tapdev_send (BpduFrameBuffer, BpduFrameSize);
}

static void StpCallback_FlushFdb (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType)
{
	// quickly age out everything
	ENET_MIIWriteRegister (20, 14, 0x60);

	// wait 2 ms while the IC ages out the table
	Timer_Wait (3);

	// reenable slow aging (~5 min)
	ENET_MIIWriteRegister (20, 14, 5);
}

static void StpCallback_DebugStrOut (const struct STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
	printf ("%s", nullTerminatedString);
	if (flush)
		fflush (stdout);
}

// See long comment at the end of 802_1Q_2011_procedures.cpp.
static void StpCallback_OnTopologyChange (const struct STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	// do nothing in this demo app
	//printf ("TC\r\n");
}

// See long comment at the end of 802_1Q_2011_procedures.cpp.
static void StpCallback_OnNotifiedTC (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp)
{
	// quickly age out everything
	ENET_MIIWriteRegister (20, 14, 0x60);

	// wait 2 ms while the IC ages out the table
	Timer_Wait (3);

	// reenable slow aging (~5 min)
	ENET_MIIWriteRegister (20, 14, 5);
}

void StpCallback_OnPortRoleChanged (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_PORT_ROLE role, unsigned int timestamp)
{
}

static void* StpCallback_AllocAndZeroMemory (unsigned int size)
{
	void* result = malloc (size);
	assert (result != NULL);
	memset (result, 0, size);
	return result;
}

static void StpCallback_FreeMemory (void* p)
{
	free (p);
}

static STP_CALLBACKS const Callbacks =
{
	StpCallback_EnableBpduTrapping,
	StpCallback_EnableLearning,
	StpCallback_EnableForwarding,
	StpCallback_TransmitGetBuffer,
	StpCallback_TransmitReleaseBuffer,
	StpCallback_FlushFdb,
	StpCallback_DebugStrOut,
	StpCallback_OnTopologyChange,
	StpCallback_OnNotifiedTC,
	StpCallback_OnPortRoleChanged,
	StpCallback_AllocAndZeroMemory,
	StpCallback_FreeMemory
};

