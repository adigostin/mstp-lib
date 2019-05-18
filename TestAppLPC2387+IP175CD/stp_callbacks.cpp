
#include "stp.h"
#include "drivers/ethernet.h"
#include "drivers/timer.h"
#include "drivers/scheduler.h"
#include "debug_leds.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

extern unsigned short chip_id;

static void StpCallback_EnableBpduTrapping (const struct STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	if (chip_id == 0x175C)
	{
		if (enable)
		{
			// Forward BPDUs only to the CPU (i.e., don't flood them across all ports).
			// Page 93 of IP175C datasheet.
			unsigned short reg = ENET_MIIReadRegister (30, 26);
			reg = (reg & 0xFF00) | 0xE0;
			ENET_MIIWriteRegister (30, 26, reg);
		}
		else
		{
			// Resume flooding of BPDUs across ports.
			unsigned short reg = ENET_MIIReadRegister (30, 26);
			reg = (reg & 0xFF00) | 0xA0;
			ENET_MIIWriteRegister (30, 26, reg);
		}
	}
	else if (chip_id == 0x175D)
	{
		if (enable)
		{
			// Forward BPDUs only to the CPU.
			// Page 79 of the IP175D datasheet.
			unsigned short reg = ENET_MIIReadRegister (20, 8);
			reg = (reg & ~3u) | 1u;
			ENET_MIIWriteRegister (20, 8, reg);
		}
		else
		{
			// Here goes the code that undoes the switch chip configuration from above.
			// This is not yet implemented in this demo app.
			assert(false);
		}
	}
	else
		assert(false);
}

static void StpCallback_EnableLearning (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	if (chip_id == 0x175C)
	{
		// IP175C doesn't support flushing the filtering database.
		// As a workaround, we disable learning at power-up and we keep it disabled.
		/*
		unsigned short bit = (1u << (8 + portIndex));

		unsigned short reg = ENET_MIIReadRegister (30, 16);
		if (enable)
			reg = reg | bit;
		else
			reg = reg & ~bit;
		ENET_MIIWriteRegister (30, 16, reg);
		*/
	}
	else if (chip_id == 0x175D)
	{
		unsigned short i = ENET_MIIReadRegister (20, 6);

		if (portIndex == 0)
		{
			i = (i & ~(1ul << 0)) | ((enable != 0) ? (1ul << 0) : 0);
		}
		else if (portIndex == 1)
		{
			i = (i & ~(1ul << 1)) | ((enable != 0) ? (1ul << 1) : 0);
		}
		else
			assert (0);

		ENET_MIIWriteRegister (20, 6, i);
	}
	else
		assert(false);

	printf ("STP: %s %s on port index %d.\r\n", enable ? "Enabled" : "Disabled", "  learning", portIndex);

	update_debug_leds(bridge);
}

static void StpCallback_EnableForwarding (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	if (chip_id == 0x175C)
	{
		unsigned short bit = 1u << portIndex;

		unsigned short reg = ENET_MIIReadRegister (30, 16);
		if (enable)
			reg = reg | bit;
		else
			reg = reg & ~bit;
		ENET_MIIWriteRegister (30, 16, reg);
	}
	else if (chip_id == 0x175D)
	{
		unsigned short i = ENET_MIIReadRegister (20, 6);

		if (portIndex == 0)
		{
			i = (i & ~(1ul << 8)) | ((enable != 0) ? (1ul << 8) : 0);
		}
		else if (portIndex == 1)
		{
			i = (i & ~(1ul << 9)) | ((enable != 0) ? (1ul << 9) : 0);
		}
		else
			assert (0);

		ENET_MIIWriteRegister (20, 6, i);
	}
	else
		assert(false);

	printf ("STP: %s %s on port index %d.\r\n", enable ? "Enabled" : "Disabled", "forwarding", portIndex);

	update_debug_leds(bridge);
}

static unsigned char BpduFrameBuffer [21 + 36];
static unsigned int BpduFrameSize;

static void* StpCallback_TransmitGetBuffer (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	assert (portIndex < 2);

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
	ethernet_send (BpduFrameBuffer, BpduFrameSize);
}

static void StpCallback_FlushFdb (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
{
	if (chip_id == 0x175C)
	{
		// IP175C doesn't support flushing the filtering database.
		// As a workaround, we disable learning at power-up and we keep it disabled.
	}
	else if (chip_id == 0x175D)
	{
		// quickly age out everything
		ENET_MIIWriteRegister (20, 14, 0x60);

		// wait 2 ms while the IC ages out the table
		scheduler_wait(3);

		// reenable slow aging (~5 min)
		ENET_MIIWriteRegister (20, 14, 5);
	}
	else
		assert(false);
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
	printf ("STP: TC\r\n");
}

static void StpCallback_OnPortRoleChanged (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_PORT_ROLE role, unsigned int timestamp)
{
	printf ("STP: port index %d role = %s.\r\n", portIndex, STP_GetPortRoleString(role));
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

extern STP_CALLBACKS const stp_callbacks =
{
	StpCallback_EnableBpduTrapping,
	StpCallback_EnableLearning,
	StpCallback_EnableForwarding,
	StpCallback_TransmitGetBuffer,
	StpCallback_TransmitReleaseBuffer,
	StpCallback_FlushFdb,
	StpCallback_DebugStrOut,
	StpCallback_OnTopologyChange,
	StpCallback_OnPortRoleChanged,
	StpCallback_AllocAndZeroMemory,
	StpCallback_FreeMemory
};

