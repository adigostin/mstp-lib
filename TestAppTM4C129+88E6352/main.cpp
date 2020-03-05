
#include "switch.h"
#include "clock.h"
#include "ethernet.h"
#include "smi.h"
#include "../mstp-lib/stp.h"
#include <__cross_studio_io.h>
#include <TM4C1294KCPDT.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

static const uint8_t bpdu_dest_address[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x00 };
static const uint8_t bpdu_llc[3] = { 0x42, 0x42, 0x03 };

static const uint8_t mac_address[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00 };

extern "C" void __assert (const char* expression, const char *file, int line)
{
	__disable_irq();
	printf ("\r\nAssertion failed in file %s, line %d. Restarting the firmware...\r\n", file, line);
	asm ("BKPT 0");
	volatile bool loop = true;
	while(loop)
		;
}

extern "C" void HardFault_Handler()
{
	__disable_irq();
	printf ("HardFault_Handler() called. Restarting the firmware...\r\n");
	volatile bool loop = true;
	while(loop)
		;
}

extern "C" void __error__(char* file, uint32_t line)
{
	__assert (nullptr, file, line);
}

static void init_stdout_serial()
{
    SYSCTL->RCGCUART |= (1 << 0); // enable UART0 clock
	SYSCTL->RCGCGPIO |= (1 << 0); // enable GPIOA clock

	// Configure the pin multiplexing for this UART
	GPIOA_AHB->PCTL = (GPIOA_AHB->PCTL & ~0xFFu) | 0x11; // GPIOA pins 0, 1: alternate function 1

	GPIOA_AHB->AFSEL |= (1 << 0) | (1 << 1); // GPIOA pins 0, 1 - alternate function
	GPIOA_AHB->DEN   |= (1 << 0) | (1 << 1); // digital enable

	UART0->IBRD = 65;
	UART0->FBRD = 7;
	UART0->LCRH = (3 << 5) | (1 << 4); // WLEN = 3 (word length = 8 bits), FEN = 1 (enable FIFO)
	UART0->CTL |= (1 << 0) | (1 << 8) | (1 << 9); // set UARTEN, TXE, RXE
}

extern "C" int __putchar (int ch, __printf_tag_ptr)
{
	while (UART0->FR & (1 << 5))
		;
	UART0->DR = ch;
	return 1;
}

#define ANSI_WHITEONBLACK  "\x1B[0;37;40m"
#define ANSI_GREENONBLACK  "\x1B[0;32;40m"
#define ANSI_YELLOWONBLACK "\x1B[0;33;40m"
#define ANSI_BLACKONYELLOW "\x1B[0;30;43m"
#define ANSI_REDONBLACK    "\x1B[0;31;40m"
#define ANSI_CLEAR_SCREEN  "\x1B[2J"
#define ANSI_CURSOR_0_0    "\x1B[0;0H"

// ============================================================================

// These addresses are hardcoded with resistors on the board.
static constexpr uint8_t stp_switch_mdio_address = 2;
static constexpr uint8_t other_switch_mdio_address = 4;

//  STP Port Index  |   STP Port Number    | switch port index | description
//                  | (in lib debug output |                   |
// -----------------|----------------------|-------------------|-------------
//   Non-STP port   |                      |        0          | to MCU
// STP port index 0 |          1           |        1          | J1 FWD connection
// STP port index 1 |          2           |        2          | J3 RJ-45
// STP port index 2 |          3           |        3          | GBE0 (not going anywhere)
// STP port index 3 |          4           |        4          | J2 AFT connection
// STP port index 4 |          5           |        5          | SERDES to other (non-STP) switch IC

static constexpr uint32_t stp_port_count = 5;

static constexpr uint32_t atu_operation_reg_addr = 0xB;
static constexpr uint32_t atu_data_reg_addr = 0xC;
static constexpr uint32_t atu_mac_bytes01 = 0x0D;
static constexpr uint32_t atu_mac_bytes23 = 0x0E;
static constexpr uint32_t atu_mac_bytes45 = 0x0F;

static STP_BRIDGE* stp_bridge;

static const char* get_port_name_ (uint32_t switch_port_index)
{
	switch (switch_port_index)
	{
		case 0: return "MCU";
		case 1: return "J1";
		case 2: return "J3";
		case 3: return "GBE0";
		case 4: return "J2";
		case 5: return "SERDES";
		default: assert(false); return NULL;
	}
}

static switch_dev_addr get_dev_addr_from_stp_port (unsigned int stp_port_index)
{
	switch (stp_port_index)
	{
		case 0: return switch_dev_addr_port1; // EA1
		case 1: return switch_dev_addr_port2; // EA2
		case 2: return switch_dev_addr_port3; // EA3
		case 3: return switch_dev_addr_port3; // EA4
		case 4: return switch_dev_addr_port4; // SERDES-A
		default: assert(false); return switch_dev_addr_port0;
	}
}


static uint32_t get_switch_port_from_stp_port (uint32_t stp_port_index)
{
	assert (stp_port_index <= 4);
	return stp_port_index + 1;
}

static uint32_t get_stp_port_from_switch_port (uint32_t switch_port_index)
{
	assert (switch_port_index >= 1 && switch_port_index <= 5);
	return switch_port_index - 1;
}

static void flush_atu (unsigned int switchPortIndex)
{
	switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_data_reg_addr, (switchPortIndex << 4) | (0xF << 8));

	// Now initiate the Flush operation by setting ATUBusy=1 and ATUOp=2 (flush all non-static entries for a port).
	switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_operation_reg_addr, (1 << 15) | (1 << 12));

	// Wait for the operation to complete in hardware before returning. This is a requirement of RSTP.
	while (switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_operation_reg_addr) & (1u << 15))
		;
}

static void dump_atu()
{
	printf ("ATU:\r\n");

	switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_mac_bytes01, 0xFFFF);
	switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_mac_bytes23, 0xFFFF);
	switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_mac_bytes45, 0xFFFF);

	for (int i = 0; i < 10; i++)
	{
		while (switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_operation_reg_addr) & (1u << 15))
			;

		switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_operation_reg_addr, (1 << 15) | (0b100 << 12));

		uint16_t mac01 = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_mac_bytes01);
		uint16_t mac23 = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_mac_bytes23);
		uint16_t mac45 = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_mac_bytes45);

		if ((mac01 == 0xFFFF) && (mac23 == 0xFFFF) && (mac45 == 0xFFFF))
			break;

		uint16_t data = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, atu_data_reg_addr);

		printf ("  %04x%04x%04x EntryState=0x%x ports=", mac01, mac23, mac45, data & 0x000F);
		unsigned int mask = (data >> 4) & 0x3F;
		for (unsigned int switchPortIndex = 0; (mask != 0) && (switchPortIndex < 6); switchPortIndex++)
		{
			if (mask & (1 << switchPortIndex))
			{
				mask &= ~(1 << switchPortIndex);
				printf ("%s%s", get_port_name_(switchPortIndex), (mask != 0) ? "," : "\r\n");
			}
		}
	}
}

// 88E6352 does not have distinct bits for learning and forwarding, but instead a bitfield that controls
// both at once. This function writes the bitfield value out of the distinct bits that STP works with.
// See page 227 in 88E6352_Functional_Specification-Rev0-08.pdf.
static void write_port_state_register (bool learning, bool forwarding, unsigned int stp_port_index)
{
	uint32_t reg_addr = 4; // Port Control register, page 227 of 88E6352_Functional_Specification-Rev0-08.pdf.

	auto dev_addr = get_dev_addr_from_stp_port(stp_port_index);
	auto value = switch_read_register (stp_switch_mdio_address, dev_addr, reg_addr);
	value &= 0xFFFC;

	auto switch_port_index = get_switch_port_from_stp_port(stp_port_index);
	if (forwarding)
	{
		value |= 3;
		printf ("Port No. : %d (%s)\tPort State: FORWARDING\r\n", stp_port_index, get_port_name_(switch_port_index));
	}
	else if (learning)
	{
		value |= 2;
		printf ("Port No. : %d (%s)\tPort State: LEARNING\r\n", stp_port_index, get_port_name_(switch_port_index));
	}
	else
	{
		value |= 1;
		printf ("Port No. : %d (%s)\tPort State: BLOCKING\r\n", stp_port_index, get_port_name_(switch_port_index));
	}

	switch_write_register (stp_switch_mdio_address, dev_addr, reg_addr, value);
}

// ============================================================================
// STP callbacks

static void StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	uint16_t value;

	if (enable)
	{
		// Tell the switch IC to forward to the CPU port the reserved multicast frames (DA of 01:80:C2:00:00:0x).
		// See bit 3 in Table 133 on page 297 in 88E6352_Functional_Specification-Rev0-08.pdf.
		// After setting this, the switch no longer floods these frames across ports.
		value = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global2, 0x05);
		value |= (1 << 3);
		switch_write_register (stp_switch_mdio_address, switch_dev_addr_global2, 0x05, value);

		// Tell the switch IC to treat as MGMT frames those frames with a DA of 01:80:C2:00:00:00
		// See Table 131 on page 294 in 88E6352_Functional_Specification-Rev0-08.pdf.
		value = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global2, 0x03);
		value |= 1;
		switch_write_register (stp_switch_mdio_address, switch_dev_addr_global2, 0x03, value);

		// Tell the switch IC to tag frames that are going out of the port wired to the CPU (EA0). Table 65 on page 224.
		value = switch_read_register (stp_switch_mdio_address, switch_dev_addr_port0, 0x04);
		value = (value & 0xFCFF) | (0b11 << 8); // Frame Mode is EtherType DSA, so Control(MGMT?) frames egress always with an EtherType DSA tag
		value = (value & 0xCFFF) | (0b00 << 12); // Egress Mode 00, see datasheet
		switch_write_register (stp_switch_mdio_address, switch_dev_addr_port0, 0x04, value);
	}
	else
	{
		// Put back default (power-up) values in the fields we set above.
		assert(false); // TODO

		value = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global2, 0x05);
		value &= ~(1u << 3);
		switch_write_register (stp_switch_mdio_address, switch_dev_addr_global2, 0x05, value);
	}
}

static void StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int stp_port_index, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	bool forwarding = STP_GetPortForwarding (bridge, stp_port_index, treeIndex);
	write_port_state_register (enable, forwarding, stp_port_index);
}

static void StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int stp_port_index, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	bool learning = STP_GetPortLearning (bridge, stp_port_index, treeIndex);
	write_port_state_register (learning, enable, stp_port_index);
}

// See StpCallback_TransmitGetBuffer.html in the _help directory of the STP library.
static void* StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	uint8_t* buffer = (uint8_t*) ethernet_transmit_get_buffer(25 + bpduSize);

	// 6 bytes for the destination MAC address.
	memcpy (buffer, bpdu_dest_address, 6);

	// 6 bytes for the source MAC address, which for BPDU packets is the sending port's address.
	// We generate the port address from the bridge address, by adding (1+portIndex) to the last byte.
	memcpy (&buffer[6], STP_GetBridgeAddress(stp_bridge)->bytes, 6);
	bool wrap = (unsigned int) buffer[11] + 1 + portIndex >= 256;
	buffer[11] += (1 + portIndex);
	if (wrap)
		buffer[10]++;

	// 4 bytes for the Marvell "Ether Type" tag.
	buffer[12] = 0x91;
	buffer[13] = 0;
	buffer[14] = 0;
	buffer[15] = 0;

	// 4 bytes for the Marvell "From_CPU" DSA tag that tells the switch IC which port to send this BPDU out of.
	unsigned int switchPortIndex = get_switch_port_from_stp_port(portIndex);
	buffer[16] = 0x40 | stp_switch_mdio_address;
	buffer[17] = switchPortIndex << 3;
	buffer[18] = 0;
	buffer[19] = 0;

	// 2 bytes for EtherType/Size, which specifies the size of the payload starting at the LLC field, so 3 + bpduSize.
	uint16_t etherTypeOrSize = 3 + bpduSize;
	buffer[20] = uint8_t (etherTypeOrSize >> 8);
	buffer[21] = uint8_t (etherTypeOrSize & 0xFF);

	// 3 bytes for the LLC field, which normally are 0x42, 0x42, 0x03.
	memcpy (&buffer[22], bpdu_llc, 3);

	return &buffer[25];
}

static void StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	void* buffer = (uint8_t*) bufferReturnedByGetBuffer - 25;
	ethernet_transmit_release_buffer (buffer);
}

static void StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
{
	assert (STP_GetStpVersion(bridge) == STP_VERSION_RSTP);
	assert (treeIndex == 0);
	assert (flushType == STP_FLUSH_FDB_TYPE_IMMEDIATE);

	auto switchPortIndex = get_switch_port_from_stp_port(portIndex);
	const char* portName = get_port_name_ (switchPortIndex);

	//dump_atu();
	//printf ("Flushing entries for port %s... ", portName);
	flush_atu(switchPortIndex);
	//printf ("... flushed.\r\n");
	//dump_atu();
}

static void StpCallback_DebugStrOut (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
    printf ("%s", nullTerminatedString);
    if (flush)
        fflush (stdout);
}

static void StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	// nothing to do
}

static void StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_PORT_ROLE role, unsigned int timestamp)
{
}

static uint8_t stpHeap[1800]; // size determined empirically
static uint32_t stpHeapUsed = 0;

static void* StpCallback_AllocAndZeroMemory (unsigned int size)
{
	size = (size + 3) / 4 * 4;
	assert (stpHeapUsed + size <= sizeof(stpHeap));
	void* result = &stpHeap[stpHeapUsed];
	stpHeapUsed += size;
	printf ("stpHeapUsed=%d\r\n", stpHeapUsed);
	return result;
}

static void StpCallback_FreeMemory(void* p)
{
	assert(false); // not implemented
}

static const STP_CALLBACKS stp_callbacks =
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
	StpCallback_FreeMemory,
};

static bool read_port_status (uint32_t stp_port_index, uint32_t& speed, bool& duplex)
{
	auto devAddr = get_dev_addr_from_stp_port(stp_port_index);
	uint32_t reg_addr = 0; // Port Status register, page 227 of 88E6352_Functional_Specification-Rev0-08.pdf.
	auto status = switch_read_register(stp_switch_mdio_address, devAddr, reg_addr);
    if ((status & (1 << 11)) == 0)
		return false;

	switch ((status & 0x0300) >> 8)
	{
		case 0: speed = 10; break;
		case 1: speed = 100; break;
		case 2: speed = 1000; break;
		default: speed = 1000; break;
	}

	duplex = (status & (1 << 10));
	return true;
}

static void poll_port_status_and_call_library (unsigned int timestamp)
{
	for (uint32_t stp_port_index = 0; stp_port_index < stp_port_count; stp_port_index++)
	{
		uint32_t speed;
		bool duplex;
		bool link_good = read_port_status(stp_port_index, speed, duplex);

		if (link_good)
		{
			if (!STP_GetPortEnabled(stp_bridge, stp_port_index))
				STP_OnPortEnabled(stp_bridge, stp_port_index, speed, duplex, timestamp);
		}
		else
		{
			if (STP_GetPortEnabled(stp_bridge, stp_port_index))
				STP_OnPortDisabled(stp_bridge, stp_port_index, timestamp);
		}
	}
}

static void validate_and_process_bpdu (const uint8_t* data, size_t size)
{
	// Let's make some sanity checks on the received BPDU and ignore it if it's malformed.

	// 25 is the size of headers before the BPDU: DA(6), SA(6), Marvell EtherType DSA tag(4), Marvell To_CPU DSA tag(4), EtherType/Size(2), LLC(3)
	if (size < 25)
	{
		printf ("BPDU len < 25\r\n");
		return;
	}

	uint32_t now = clock_get_time_ms();

	printf ("%u.%03u: RX: %02x:%02x:%02x:%02x:%02x:%02x  %02x:%02x:%02x:%02x:%02x:%02x  %02x%02x\r\n",
		now / 1000, now % 1000,
		data[0], data[1], data[2], data[3], data[4], data[5],
		data[6], data[7], data[8], data[9], data[10], data[11],
		data[12], data[13]);

	unsigned int etherTypeOrSize = (data[20] << 8) | data[21];

	// 3 is the size of the LLC field
	if ((etherTypeOrSize < 3) || (etherTypeOrSize > 1536))
	{
		printf ("BPDU etherTypeOrSize\r\n");
		return;
	}

	if (memcmp (data, bpdu_dest_address, 6) != 0)
	{
		printf ("BPDU bad DA\r\n");
		return;
	}

	// We ignore the four-byte EtherType tag that starts at index 12.

	// See Figure 29 on page 90 in 88E6352_Functional_Specification-Rev0-08.pdf.
	const uint8_t* tag = &data[16];

	uint8_t tagCode = (tag[1] & 6) | ((tag[2] >> 4) & 1);
	if (tagCode != 0)
	{
		printf("BPDU tag Code\r\n");
		return;
	}

	if (memcmp (&data[22], bpdu_llc, 3) != 0)
	{
		printf ("BPDU LLC\r\n");
		return;
	}

	const uint8_t* bpdu = &data[25];
	unsigned int bpdu_size = etherTypeOrSize - 3;
	unsigned int switch_port_index = tag[1] >> 3;
	unsigned int stp_port_index = get_stp_port_from_switch_port(switch_port_index);

	if (!STP_GetPortEnabled(stp_bridge, stp_port_index))
	{
		uint32_t speed;
		bool duplex;
		bool link_good = read_port_status (stp_port_index, speed, duplex);
		assert(link_good);
		STP_OnPortEnabled(stp_bridge, stp_port_index, speed, duplex, now);
	}

	STP_OnBpduReceived (stp_bridge, stp_port_index, bpdu, bpdu_size, now);
}

static void process_received_frame (const uint8_t* data, size_t size)
{
	if ((size >= 6) && (memcmp(data, bpdu_dest_address, 6) == 0))
		return validate_and_process_bpdu(data, size);
}

int main()
{
	clock_init();

	init_stdout_serial();

	//debug_printf
	printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_0_0 ANSI_WHITEONBLACK "Starting STP Test App\r\n");

	SYSCTL->RCGCGPIO |= (1 << 0) | (1 << 1) | (1 << 9); // enable clocks to ports A, B, K
	for (volatile int i = 0; i < 100; i++)
		;

	smi_init();
	switch_init(stp_switch_mdio_address);
	switch_init(other_switch_mdio_address);

	// Tell the switch IC that our CPU is wired to port 0.
	// Table 122 on page 283.
	auto value = switch_read_register (stp_switch_mdio_address, switch_dev_addr_global1, 0x1A);
	value &= 0xFF0F;
	switch_write_register (stp_switch_mdio_address, switch_dev_addr_global1, 0x1A, value);

	ethernet_init(mac_address);

	auto now = clock_get_time_ms();

	stp_bridge = STP_CreateBridge (stp_port_count, 0, 0, &stp_callbacks, mac_address, 100);
//	STP_EnableLogging (stp_bridge, 1);

	static bool stp_enabled_in_config = true;
	if (stp_enabled_in_config)
	{
		STP_StartBridge (stp_bridge, now);
	}
	else
	{
		// STP disabled in config; enable forwarding on all ports.
		for (uint32_t dev_addr = switch_dev_addr_port0; dev_addr <= switch_dev_addr_port4; dev_addr++)
		{
			uint32_t reg_addr = 4; // Port Control register, page 227 of 88E6352_Functional_Specification-Rev0-08.pdf.
        	auto value = switch_read_register (stp_switch_mdio_address, (switch_dev_addr)dev_addr, reg_addr);
			value |= 3;
			switch_write_register (stp_switch_mdio_address, (switch_dev_addr)dev_addr, reg_addr, value);
		}
	}

	uint32_t next_stp_1s_tick = now + 1000;
	uint32_t next_port_poll = now + 100;

	while (true)
	{
		now = clock_get_time_ms();

		ethernet_get_received(process_received_frame);

		if (clock_get_time_ms() >= next_port_poll)
		{
			if (stp_enabled_in_config)
				poll_port_status_and_call_library (now);
			next_port_poll += 100;
		}

		if (clock_get_time_ms() >= next_stp_1s_tick)
		{
			if (stp_enabled_in_config)
				STP_OnOneSecondTick (stp_bridge, now);
			else
			{
				size_t frame_size = 100;
	            uint8_t* b = (uint8_t*)ethernet_transmit_get_buffer (frame_size);
				b[0] = 0x11; b[1] = 0x20; b[2] = 0x10; b[3] = 0x20; b[4] = 0x10; b[5] = 0x20;
				ethernet_get_device_address(&b[6]);
				b[12] = 0;
				b[13] = frame_size;
				ethernet_transmit_release_buffer(b);
			}

			next_stp_1s_tick += 1000;
		}
	}
}
