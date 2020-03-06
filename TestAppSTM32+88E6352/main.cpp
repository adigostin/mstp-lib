

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

static const uint8_t bpdu_dest_address[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x00 };
static const uint8_t bpdu_llc[3] = { 0x42, 0x42, 0x03 };

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

uint16_t read_phy_register (uint8_t phy_addr, uint8_t reg_addr)
{
	enet_write_smi (switch_dev_addr_global2, global2_smi_phy_command, (1u << 15) | (1u << 12) | (0b10 << 10) | (phy_addr << 5) | reg_addr);
	while (enet_read_smi (switch_dev_addr_global2, global2_smi_phy_command) & (1u << 15))
		;
	uint16_t reg = enet_read_smi (switch_dev_addr_global2, global2_smi_phy_data);
	return reg;
}

void write_phy_register (uint8_t phy_addr, uint8_t reg_addr, uint16_t value)
{
	enet_write_smi (switch_dev_addr_global2, global2_smi_phy_data, value);
	enet_write_smi (switch_dev_addr_global2, global2_smi_phy_command, (1 << 15) | (1 << 12) | (0b01 << 10) | (phy_addr << 5) | reg_addr);
	while (enet_read_smi (switch_dev_addr_global2, global2_smi_phy_command) & (1u << 15))
		;
}

static void poll_port_status_and_call_library (size_t pi, unsigned int now)
{
	// See Table 59 on page 213 in 88E6352_Functional_Specification-Rev0-08.pdf.
	uint16_t reg = read_phy_register (pi, 17);
	if (reg & (1u << 10))
	{
		// link up
		if (reg & (1u << 11))
		{
			// speed and duplex resolved (or auto-negotiation disabled)
			if (!STP_GetPortEnabled(bridge, pi))
			{
				static constexpr uint32_t speeds[] = { 10, 100, 1000, (uint32_t)-1 };
				uint32_t speed = speeds[(reg >> 14) & 3];
				bool duplex = reg & (1u << 13);
				printf ("Port %d Link Up   (", pi);
				print_binary(reg);
				printf (") Speed %d, %s-duplex.\r\n", speed, duplex ? "Full" : "Half");
				STP_OnPortEnabled (bridge, pi, speed, true, now);
			}
		}
	}
	else
	{
		// link down
		if (STP_GetPortEnabled(bridge, pi))
		{
			printf ("Port %d Link Down (", pi);
			print_binary(reg);
			printf (").\r\n", pi);
			STP_OnPortDisabled (bridge, pi, now);
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

	uint32_t now = scheduler_get_time_ms32();
/*
	printf ("%u.%03u: RX: %02x:%02x:%02x:%02x:%02x:%02x  %02x:%02x:%02x:%02x:%02x:%02x  %02x%02x\r\n",
		now / 1000, now % 1000,
		data[0], data[1], data[2], data[3], data[4], data[5],
		data[6], data[7], data[8], data[9], data[10], data[11],
		data[12], data[13]);
*/
	unsigned int etherTypeOrSize = (data[20] << 8) | data[21];

	// 3 is the size of the LLC field
	if ((etherTypeOrSize < 3) || (etherTypeOrSize > 1536))
	{
		printf ("BPDU etherTypeOrSize\r\n");
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
	unsigned int port_index = tag[1] >> 3;

	if (!STP_GetPortEnabled(bridge, port_index))
		poll_port_status_and_call_library(port_index, now);

	STP_OnBpduReceived (bridge, port_index, bpdu, bpdu_size, now);
}

static void on_enet_frame_received (uint8_t* frame, size_t frame_len)
{
	if ((frame_len >= 6) && (memcmp(frame, bpdu_dest_address, 6) == 0))
	{
		if (STP_IsBridgeStarted(bridge))
			return validate_and_process_bpdu(frame, frame_len);
	}
}

// ============================================================================
// STP callbacks

static void StpCallback_EnableBpduTrapping (const struct STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	if (enable)
	{
		// Tell the switch IC to forward to the CPU port the reserved multicast frames (DA of 01:80:C2:00:00:0x).
		// See bit 3 in Table 133 on page 297 in 88E6352_Functional_Specification-Rev0-08.pdf.
		// After setting this, the switch no longer floods these frames across ports.
		uint16_t value = enet_read_smi (switch_dev_addr_global2, 0x05);
		value |= (1 << 3);
		enet_write_smi (switch_dev_addr_global2, 0x05, value);

		// Tell the switch IC to tag frames that are going out of the port wired to the CPU (P6). Table 65 on page 224.
		value = enet_read_smi (switch_dev_addr_port6, 0x04);
		value = (value & 0xFCFF) | (0b11 << 8); // Frame Mode is EtherType DSA, so Control(MGMT?) frames egress always with an EtherType DSA tag
		value = (value & 0xCFFF) | (0b00 << 12); // Egress Mode 00, see datasheet
		enet_write_smi (switch_dev_addr_port6, 0x04, value);
	}
	else
	{
		// Put back default (power-up) values in the fields we set above.
		uint16_t value = enet_read_smi (switch_dev_addr_port6, 0x04);
		value = (value & 0xFCFF) | (0b11 << 8);
		value = (value & 0xCFFF) | (0b00 << 12);
		enet_write_smi (switch_dev_addr_port6, 0x04, value);

		value = enet_read_smi (switch_dev_addr_global2, 0x05);
		value &= ~(1 << 3);
		enet_write_smi (switch_dev_addr_global2, 0x05, value);
	}
}

// 88E6352 does not have distinct bits for learning and forwarding, but instead a bitfield that controls
// both at once. This function writes the bitfield value out of the distinct bits that STP works with.
// See page 227 in 88E6352_Functional_Specification-Rev0-08.pdf.
static void write_port_state_register (size_t port_index, bool learning, bool forwarding)
{
	auto value = enet_read_smi (switch_dev_addr_port0 + port_index, 4);
	value &= 0xFFFC;

	if (forwarding)
	{
		value |= 3;
		printf ("Port P%u state: FORWARDING\r\n", port_index);
	}
	else if (learning)
	{
		value |= 2;
		printf ("Port P%u state: LEARNING\r\n", port_index);
	}
	else
	{
		value |= 1;
		printf ("Port P%u state: BLOCKING\r\n", port_index);
	}

	enet_write_smi (switch_dev_addr_port0 + port_index, 4, value);
}

static void StpCallback_EnableLearning (const struct STP_BRIDGE* bridge, unsigned int port_index, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	bool forwarding = STP_GetPortForwarding (bridge, port_index, treeIndex);
	write_port_state_register (port_index, enable, forwarding);
}

static void StpCallback_EnableForwarding (const struct STP_BRIDGE* bridge, unsigned int port_index, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	bool learning = STP_GetPortLearning (bridge, port_index, treeIndex);
	write_port_state_register (port_index, learning, enable);
}

static uint8_t tx_bpdu_buffer[128];
static size_t tx_bpdu_size;

static void* StpCallback_TransmitGetBuffer (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	auto buffer = tx_bpdu_buffer;
	::tx_bpdu_size = bpduSize;

	assert (25 + bpduSize <= sizeof(tx_bpdu_buffer));

	// 6 bytes for the destination MAC address.
	memcpy (buffer, bpdu_dest_address, 6);

	// 6 bytes for the source MAC address, which for BPDU packets is the sending port's address.
	// We generate the port address from the bridge address, by adding (1+portIndex) to the last byte.
	memcpy (&buffer[6], STP_GetBridgeAddress(bridge)->bytes, 6);
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
	buffer[16] = 0x40;
	buffer[17] = portIndex << 3;
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

static void StpCallback_TransmitReleaseBuffer (const struct STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	enet_send_blocking (tx_bpdu_buffer, tx_bpdu_size + 25);
}

static constexpr uint16_t atu_operation_reg_addr = 0xB;
static constexpr uint16_t atu_data_reg_addr = 0xC;
static constexpr uint16_t atu_mac_bytes01 = 0x0D;
static constexpr uint16_t atu_mac_bytes23 = 0x0E;
static constexpr uint16_t atu_mac_bytes45 = 0x0F;

static void flush_atu (unsigned int switchPortIndex)
{
	enet_write_smi (switch_dev_addr_global1, atu_data_reg_addr, (switchPortIndex << 4) | (0xF << 8));

	// Now initiate the Flush operation by setting ATUBusy=1 and ATUOp=2 (flush all non-static entries for a port).
	enet_write_smi (switch_dev_addr_global1, atu_operation_reg_addr, (1 << 15) | (1 << 12));

	// Wait for the operation to complete in hardware before returning. This is a requirement of RSTP.
	while (enet_read_smi(switch_dev_addr_global1, atu_operation_reg_addr) & (1u << 15))
		;
}

static void dump_atu()
{
	printf ("ATU:\r\n");

	enet_write_smi (switch_dev_addr_global1, atu_mac_bytes01, 0xFFFF);
	enet_write_smi (switch_dev_addr_global1, atu_mac_bytes23, 0xFFFF);
	enet_write_smi (switch_dev_addr_global1, atu_mac_bytes45, 0xFFFF);

	for (int i = 0; i < 10; i++)
	{
		while (enet_read_smi (switch_dev_addr_global1, atu_operation_reg_addr) & (1u << 15))
			;

		enet_write_smi (switch_dev_addr_global1, atu_operation_reg_addr, (1 << 15) | (0b100 << 12));

		uint16_t mac01 = enet_read_smi (switch_dev_addr_global1, atu_mac_bytes01);
		uint16_t mac23 = enet_read_smi (switch_dev_addr_global1, atu_mac_bytes23);
		uint16_t mac45 = enet_read_smi (switch_dev_addr_global1, atu_mac_bytes45);

		if ((mac01 == 0xFFFF) && (mac23 == 0xFFFF) && (mac45 == 0xFFFF))
			break;

		uint16_t data = enet_read_smi (switch_dev_addr_global1, atu_data_reg_addr);

		printf ("  %04x%04x%04x EntryState=0x%x ports=", mac01, mac23, mac45, data & 0x000F);
		unsigned int mask = (data >> 4) & 0x3F;
		for (size_t pi = 0; (mask != 0) && (pi < 6); pi++)
		{
			if (mask & (1 << pi))
			{
				mask &= ~(1 << pi);
				printf ("P%u%s", pi, (mask != 0) ? "," : "\r\n");
			}
		}
	}
}

static void StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
{
	assert (STP_GetStpVersion(bridge) == STP_VERSION_RSTP);
	assert (treeIndex == 0);
	assert (flushType == STP_FLUSH_FDB_TYPE_IMMEDIATE);

	//dump_atu();
	//printf ("Flushing entries for P%u... ", portIndex);
	flush_atu(portIndex);
	//printf ("... flushed.\r\n");
	//dump_atu();
}

static void StpCallback_DebugStrOut (const struct STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
    printf ("%s", nullTerminatedString);
    if (flush)
        fflush (stdout);
}

static void StpCallback_OnPortRoleChanged (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_PORT_ROLE role, unsigned int timestamp)
{
}

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
	.enableBpduTrapping    = &StpCallback_EnableBpduTrapping,
	.enableLearning        = &StpCallback_EnableLearning,
	.enableForwarding      = &StpCallback_EnableForwarding,
	.transmitGetBuffer     = &StpCallback_TransmitGetBuffer,
	.transmitReleaseBuffer = &StpCallback_TransmitReleaseBuffer,
	.flushFdb              = &StpCallback_FlushFdb,
	.debugStrOut           = &StpCallback_DebugStrOut,
	.onTopologyChange      = nullptr,
	.onPortRoleChanged     = &StpCallback_OnPortRoleChanged,
	.allocAndZeroMemory    = &StpCallback_AllocAndZeroMemory,
	.freeMemory            = &StpCallback_FreeMemory,
};

static void poll_links()
{
	auto now = scheduler_get_time_ms32();
	for (size_t pi = 0; pi < 2; pi++)
		poll_port_status_and_call_library(pi, now);
}

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
	printf (ANSI_CLEAR_SCREEN ANSI_WHITEONBLACK "\r\n\r\nTest App STM32+88E6352.\r\n");

	extern const serial_command ta_commands[];
	serial_console_register_command_set(ta_commands);

	printf ("Type '?' and Enter to see a list of available serial commands.\r\n");

	scheduler_init (TIM2);

	// ========================================================================
	// Initialize the Marvell switch.

	// Do a hardware reset of the switch IC, because we (the firmware) might have been restarted and the switch IC might be running with old configuration.
	gpio_make_output ({ GPIOJ, 2 }, pin_output_speed_t::low, false, true);
	// Marvell spec says min 10ms.
	scheduler_wait(10);
	gpio_make_input ({ GPIOJ, 2 }, pin_pull::none);

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

	// Initialize our Ethernet.
	static constexpr uint8_t mac_address[6] = { 0x10, 0x20, 0x30, 0x40, 0x54, 0x65 };
	bool ok = enet_init (ethernet_pins, mac_address, &on_enet_frame_received);
	assert(ok);

	// Tell the Marvell switch that our CPU is wired to port 6 (Table 122).
	// We do this regardless of whether STP is enabled or not.
	uint16_t value = enet_read_smi (switch_dev_addr_global1, 0x1A);
	value = (value & 0xFF0F) | (6 << 4);
	enet_write_smi (switch_dev_addr_global1, 0x1A, value);

	// Tell the Marvell switch to treat as reserved multicast frames those frames with a DA of 01:80:C2:00:00:00
	// See Table 131 on page 294 in 88E6352_Functional_Specification-Rev0-08.pdf.
	value = enet_read_smi (switch_dev_addr_global2, 0x03);
	value |= 1;
	enet_write_smi (switch_dev_addr_global2, 0x03, value);

	// Enable forwarding on the management port.
	write_port_state_register(6, true, true);

	for (size_t pi = 0; pi < 2; pi++)
	{
		// Disable advertising 1Gbit to make auto-negotiation faster (our hardware doesn't support 1Gbit).
		value = read_phy_register (pi, 9);
		write_phy_register (pi, 9, value & 0xFCFF);

		// Power-up the PHY (PHYs start powered-down because we have a pull-down on the NO_CPU pin).
		value = read_phy_register (pi, 0);
		write_phy_register (pi, 0, value & ~(1u << 11));
	}

	scheduler_schedule_event_timer (poll_links, "poll_links", 100, true);

	// ========================================================================

	bridge = STP_CreateBridge (5, 0, 16, &stp_callbacks, mac_address, 100);

//	STP_EnableLogging (bridge, true);

	// comment this line to disable STP
	STP_StartBridge (bridge, scheduler_get_time_ms32());

	if (!STP_IsBridgeStarted(bridge))
	{
		// Enable forwarding on ports. Forwarding starts disabled because we have a pull-down on the NO_CPU pin.
		for (size_t pi = 0; pi < 2; pi++)
			write_port_state_register(pi, true, true);
	}

	scheduler_schedule_event_timer([] { STP_OnOneSecondTick(bridge, scheduler_get_time_ms32()); }, "STP Tick", 1000, true);

	// -----------------------------------------------------------
	// Enable debugging while in sleep mode (we'll be putting the processor in this mode with the WFI instruction below).
	DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;

	static constexpr pin_t fault_led = { GPIOJ, 5 };
	gpio_make_output (fault_led, pin_output_speed_t::low, false);

	while(true)
	{
		__WFI();
		event_queue_pop_all();
	}
}