
#include "drivers/clock.h"
#include "drivers/gpio.h"
#include "drivers/pit.h"
#include "drivers/ethernet.h"
#include "drivers/event_queue.h"
#include "drivers/scheduler.h"
#include "switch.h"
#include "stp.h"
#include <CMSIS/MK66F18.h>
#include <debugio.h>
#include <assert.h>
#include <string.h>

extern "C" void __assert(const char *__expression, const char *__filename, int __line)
{
	debug_printf("Assertion failure.\n");
	__BKPT();
}

#undef SYSTEM_MCG_C5_VALUE
#undef SYSTEM_MCG_C6_VALUE
#undef SYSTEM_MCG_C9_VALUE
#undef SYSTEM_SIM_CLKDIV2_VALUE

/* MCG_C1: CLKS=0,FRDIV=4,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
#define SYSTEM_MCG_C1_VALUE 0x22U /* MCG_C1 */
/* MCG_C2: LOCRE0=0,FCFTRIM=0,RANGE=2,HGO=0,EREFS=1,LP=0,IRCS=0 */
#define SYSTEM_MCG_C2_VALUE 0x24U /* MCG_C2 */
/* MCG_C4: DMX32=0,DRST_DRS=0,FCTRIM=0,SCFTRIM=0 */
#define SYSTEM_MCG_C4_VALUE 0x00U /* MCG_C4 */
/* MCG_SC: ATME=0,ATMS=0,ATMF=0,FLTPRSRV=0,FCRDIV=0,LOCS0=0 */
#define SYSTEM_MCG_SC_VALUE 0x00U /* MCG_SC */
/* MCG_C5: PLLCLKEN=0,PLLSTEN=0,PRDIV=0 */
#define SYSTEM_MCG_C5_VALUE 0x00U /* MCG_C5 */
/* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV=0x0C */
#define SYSTEM_MCG_C6_VALUE 0x4CU /* MCG_C6 */
/* MCG_C7: OSCSEL=0 */
#define SYSTEM_MCG_C7_VALUE 0x00U /* MCG_C7 */
/* MCG_C9: PLL_CME=0,PLL_LOCRE=0,EXT_PLL_LOCS=0 */
#define SYSTEM_MCG_C9_VALUE 0x00U /* MCG_C9 */
/* MCG_C11: PLLCS=0 */
#define SYSTEM_MCG_C11_VALUE 0x00U /* MCG_C11 */
/* OSC_CR: ERCLKEN=1,EREFSTEN=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
#define SYSTEM_OSC_CR_VALUE 0x80U /* OSC_CR */
/* SIM_CLKDIV2: USBDIV=6,USBFRAC=1 */
#define SYSTEM_SIM_CLKDIV2_VALUE 0x0DU /* SIM_CLKDIV2 */
/* SIM_CLKDIV3: PLLFLLDIV=0,PLLFLLFRAC=0 */
#define SYSTEM_SIM_CLKDIV3_VALUE 0x00U /* SIM_CLKDIV3 */
/* SIM_SOPT1: USBREGEN=0,USBSSTBY=0,USBVSTBY=0,OSC32KSEL=3,RAMSIZE=0 */

extern "C" void SystemInit()
{
	// Disable the watchdog
	WDOG->UNLOCK = 0xC520;
	WDOG->UNLOCK = 0xD928;
	WDOG->STCTRLH = 0x1D2;

	MPU_CESR = 0; // Disable MPU. All accesses from all bus masters are allowed. The Ethernet peripheral needs this.

	SMC->PMPROT = 0xAA; // allow entering all power modes

    // Enable HSRUN mode
    SMC->PMCTRL = 0x60;
    while (SMC->PMSTAT != 0x80)
        ;

    SIM->CLKDIV1 = (0 << 28)  // OUTDIV1=0, divide MCGOUTCLK by 1 to get the core/system clock
	             | (7 << 24)  // OUTDIV2=7, divide MCGOUTCLK by 8 to get the bus clock
	             | (2 << 20)  // OUTDIV3=
	             | (5 << 16); // OUTDIV4=
    SIM->SOPT2 = SIM->SOPT2 & ~SIM_SOPT2_PLLFLLSEL_MASK | (1 << SIM_SOPT2_PLLFLLSEL_SHIFT); // PLL/FLL clock select: MCGPLLCLK clock

    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;
    /* PORTA_PCR18: ISF=0,MUX=0 */
    PORTA_PCR18 &= (uint32_t) ~(uint32_t)((PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x07)));
    if (((SYSTEM_MCG_C2_VALUE)&MCG_C2_EREFS_MASK) != 0x00U)
    {
        /* PORTA_PCR19: ISF=0,MUX=0 */
        PORTA_PCR19 &= (uint32_t) ~(uint32_t)((PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x07)));
    }
    MCG->SC = SYSTEM_MCG_SC_VALUE;                                                                                  /* Set SC (fast clock internal reference divider) */
    MCG->C2 = (MCG->C2 & (uint8_t)(~(MCG_C2_FCFTRIM_MASK))) | (SYSTEM_MCG_C2_VALUE & (uint8_t)(~(MCG_C2_LP_MASK))); /* Set C2 (freq. range, ext. and int. reference selection etc. excluding trim bits; low power bit is set later) */
    OSC->CR = SYSTEM_OSC_CR_VALUE;                                                                                  /* Set OSC_CR (OSCERCLK enable, oscillator capacitor load) */
    MCG->C7 = SYSTEM_MCG_C7_VALUE;                                                                                  /* Set C7 (OSC Clock Select) */

    MCG->C1 = (SYSTEM_MCG_C1_VALUE) | MCG_C1_CLKS(0x02); /* Set C1 (clock source selection, FLL ext. reference divider, int. reference enable etc.) - PBE mode*/

    if ((((SYSTEM_MCG_C2_VALUE)&MCG_C2_EREFS_MASK) != 0x00U) && (((SYSTEM_MCG_C7_VALUE)&MCG_C7_OSCSEL_MASK) == 0x00U))
    {
        while ((MCG->S & MCG_S_OSCINIT0_MASK) == 0x00U)
        { /* Check that the oscillator is running */
        }
    }
    // Check that the source of the FLL reference clock is the requested one.
    if (SYSTEM_MCG_C1_VALUE & MCG_C1_IREFS_MASK)
    {
        while ((MCG->S & MCG_S_IREFST_MASK) == 0x00U)
			;
    }
    else
    {
        while ((MCG->S & MCG_S_IREFST_MASK) != 0x00U)
			;
    }
    MCG->C4 = ((SYSTEM_MCG_C4_VALUE) & (uint8_t)(~(MCG_C4_FCTRIM_MASK | MCG_C4_SCFTRIM_MASK))) | (MCG->C4 & (MCG_C4_FCTRIM_MASK | MCG_C4_SCFTRIM_MASK)); /* Set C4 (FLL output; trim values not changed) */

    /* PLL clock can be used to generate clock for some devices regardless of clock generator (MCGOUTCLK) mode. */
    MCG->C5 = (SYSTEM_MCG_C5_VALUE) & (uint8_t)(~(MCG_C5_PLLCLKEN_MASK)); /* Set C5 (PLL settings, PLL reference divider etc.) */
    MCG->C6 = (SYSTEM_MCG_C6_VALUE) & (uint8_t) ~(MCG_C6_PLLS_MASK);      /* Set C6 (PLL select, VCO divider etc.) */
    if ((SYSTEM_MCG_C5_VALUE)&MCG_C5_PLLCLKEN_MASK)
    {
        MCG->C5 |= MCG_C5_PLLCLKEN_MASK; /* PLL clock enable in mode other than PEE or PBE */
    }

    MCG_C11 = SYSTEM_MCG_C11_VALUE; /* Set C11 (Select PLL used to derive MCGOUT */
    MCG->C6 |= (MCG_C6_PLLS_MASK);  /* Set C6 (PLL select, VCO divider etc.) */

	// Wait until PLL is locked
    while ((MCG->S & MCG_S_LOCK0_MASK) == 0x00U)
		;

    MCG->C1 &= (uint8_t) ~(MCG_C1_CLKS_MASK);

	// Wait until output of the PLL is selected.
    while ((MCG->S & MCG_S_CLKST_MASK) != 0x0CU)
		;

	// Wait until output of the correct PLL is selected
	while (MCG->S2 != SYSTEM_MCG_C11_VALUE)
		;

    SIM->CLKDIV2 = ((SIM->CLKDIV2) & (uint32_t)(~(SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK))) | ((SYSTEM_SIM_CLKDIV2_VALUE) & (SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK)); /* Selects the USB clock divider. */
    SIM->CLKDIV3 = ((SIM->CLKDIV3) & (uint32_t)(~(SIM_CLKDIV3_PLLFLLFRAC_MASK | SIM_CLKDIV3_PLLFLLDIV_MASK))) | ((SYSTEM_SIM_CLKDIV3_VALUE) & (SIM_CLKDIV3_PLLFLLFRAC_MASK | SIM_CLKDIV3_PLLFLLDIV_MASK)); /* Selects the PLLFLL clock divider. */
}

// ============================================================================

static const uint8_t bpdu_dest_address[6] = { 0x01, 0x80, 0xC2, 0, 0, 0 };
static const uint8_t bpdu_llc_field[3] = { 0x42, 0x42, 0x03 };

static void print_port_info (size_t port_index, STP_PORT_ROLE role, bool learning, bool forwarding)
{
	const char* role_string = STP_GetPortRoleString(role);
	const char* state = (!learning && !forwarding) ? "Blocking" : (forwarding ? "Forwarding" : "Learning");
	debug_printf("Port %d: %s %s\n", port_index + 1, role_string, state);
}

static void StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	uint8_t val;

	if (enable)
	{
		// Write 8 bytes containing an entry in the static MAC address table.
		switch_write_reg (113, 0);          // bits 63..56 = 0 (filter by MAC, not by FID)
		switch_write_reg (114, 0b01110000); // bits 55..48 (override, valid, forward to port 4)
		switch_write_reg (115, bpdu_dest_address[0]); // bits 47..40
		switch_write_reg (116, bpdu_dest_address[1]); // bits 39..32
		switch_write_reg (117, bpdu_dest_address[2]); // bits 31..24
		switch_write_reg (118, bpdu_dest_address[3]); // bits 23..16
		switch_write_reg (119, bpdu_dest_address[4]); // bits 15..8
		switch_write_reg (120, bpdu_dest_address[5]); // bits 7..0
		// Write reg 110 to say that next operation is a write to the static MAC address table.
		switch_write_reg (110, 0);
		// Write the index into the table; writing initiates the operation.
		switch_write_reg (111, 0);

		// Enable tail tagging.
		val = switch_read_reg (12);
		switch_write_reg (12, val | (1u << 1));
	}
	else
	{
		val = switch_read_reg (12);
		switch_write_reg (12, val & ~(1u << 1));

		switch_write_reg (114, 0); // bits 55..48 (not valid)
		switch_write_reg (110, 0);
		switch_write_reg (111, 0);
	}

	// We've just changed tail tagging mode. The switch IC offers no way to tell between
	// tagged and non-tagged frames, so if there's traffic going on rigth now, some frames
	// will be received or transmitted malformed. Let's ignore this problem in this demo app.
}

static void StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	uint16_t reg = 0x12 + (portIndex << 4);
	auto val = switch_read_reg (reg);
	if (enable)
	{
		if (val & 1)
			switch_write_reg (reg, val & 0xFE);
	}
	else
	{
		if ((val & 1) == 0)
			switch_write_reg (reg, val | 1);
	}

	if (debug_enabled())
	{
		auto role = STP_GetPortRole(bridge, portIndex, treeIndex);
		bool learning = enable;
		bool forwarding = STP_GetPortForwarding(bridge, portIndex, treeIndex);
		print_port_info (portIndex, role, learning, forwarding);
	}
}

static void StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	uint16_t reg = 0x12 + (portIndex << 4);
	auto val = switch_read_reg (reg);
	if (enable)
	{
		if ((val & 0b110) != 0b110)
			switch_write_reg (reg, val | 0b110);
	}
	else
	{
		if (val & 0b110)
			switch_write_reg (reg, val & ~0b110);
	}

	if (debug_enabled())
	{
		auto role = STP_GetPortRole(bridge, portIndex, treeIndex);
		bool learning = STP_GetPortLearning(bridge, portIndex, treeIndex);
		bool forwarding = enable;
		print_port_info (portIndex, role, learning, forwarding);
	}
}

static void* StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	// 6 bytes for DA
	// 6 bytes for SA
	// 2 bytes for EtherType Or Size
	// 3 bytes for LLC
	// then the BPDU content
	// 1 byte for the Microchip tail tag
	size_t frame_size = 6 + 6 + 2 + 3 + bpduSize + 1;

	// The Ethernet peripheral makes sure to transmit frames at least 64 bytes long, as required by the standard.
	// Since we ask it to append 4 bytes of CRC, it will extend to 60 bytes all our small frames.
	// We must take this into account when we append the tail tag, because the tag must be in the byte
	// just before the CRC in the frame received by the switch IC from our Ethernet peripheral.
	if (frame_size < 60)
		frame_size = 60;

	uint8_t* frame = enet_write_get_buffer(frame_size);
	if (!frame)
		return nullptr;

	memcpy (frame, bpdu_dest_address, 6);
	memcpy (&frame[6], STP_GetBridgeAddress(bridge)->bytes, 6);
	bool wrap = (uint32_t) frame[11] + 1 + portIndex >= 256;
	frame[11] += (1 + portIndex);
	if (wrap)
		frame[10]++;

	uint16_t etherTypeOrSize = 3 + bpduSize;
	frame[12] = uint8_t (etherTypeOrSize >> 8);
	frame[13] = uint8_t (etherTypeOrSize & 0xFF);

	memcpy (&frame[14], bpdu_llc_field, 3);

	// Tail tag field that tells the switch IC which port to send this bpdu out of.
	frame[frame_size - 1] = 1 << portIndex;

	return &frame[17];
}

static void StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	uint8_t* data = (uint8_t*)bufferReturnedByGetBuffer - 17;
	enet_write_release_buffer(data);
}

static void StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
{
	// The KSZ8794 switch doesn't offer a straightforward way of flushing the table
	// for a given port; see description of Bit 5 in Register 2 in its datasheet.
	//
	// Let's implement this callback by forcing learning to 'disabled' for our port,
	// then flushing the table, then restoring the learning state to whatever it was.
	// This might flush more ports than necessary, and this in turn might lead
	// to slightly increased traffic for a short time; we're ok with that in this demo app.

	// TODO: see what the spec has to say about flushing in TABLE 3-13 section "Disabled State".

	uint16_t reg = 0x12 + (portIndex << 4);
	uint8_t val = switch_read_reg(reg);

	if ((val & 1) == 0)
		switch_write_reg (reg, val | 1);

	uint8_t reg2val = switch_read_reg(2);
	switch_write_reg (2, reg2val | (1 << 5));

	if ((val & 1) == 0)
		switch_write_reg (reg, val);
}

static void StpCallback_DebugStrOut (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
	if (debug_enabled())
		debug_printf("%s", nullTerminatedString);
}

static void StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	if (debug_enabled())
		debug_printf("TC\n");
}

static void StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	if (debug_enabled())
	{
		bool learning = STP_GetPortLearning(bridge, portIndex, treeIndex);
		bool forwarding = STP_GetPortForwarding(bridge, portIndex, treeIndex);
		print_port_info (portIndex, role, learning, forwarding);
	}
}

static uint8_t stp_heap[1500];
static size_t stp_heap_used_size = 0;

static void* StpCallback_AllocAndZeroMemory (unsigned int size)
{
	assert (stp_heap_used_size + size <= sizeof(stp_heap));
	void* res = &stp_heap[stp_heap_used_size];
	stp_heap_used_size += size;
	return res;
}

static void StpCallback_FreeMemory (void* p)
{
	assert(false); // not implemented
}

static const STP_CALLBACKS stp_callbacks =
{
	&StpCallback_EnableBpduTrapping,
	&StpCallback_EnableLearning,
	&StpCallback_EnableForwarding,
	&StpCallback_TransmitGetBuffer,
	&StpCallback_TransmitReleaseBuffer,
	&StpCallback_FlushFdb,
	&StpCallback_DebugStrOut,
	&StpCallback_OnTopologyChange,
	&StpCallback_OnPortRoleChanged,
	&StpCallback_AllocAndZeroMemory,
	&StpCallback_FreeMemory
};

static STP_BRIDGE* bridge;

// ============================================================================

static void make_mac_address (uint8_t address[6])
{
	const uint8_t* uid = (uint8_t*)&SIM->UIDH;

	uint64_t v0 = SIM->UIDH;
	uint64_t v1 = ((uint64_t)SIM->UIDMH << 16) | ((uint64_t)SIM->UIDML >> 16);
	uint64_t v2 = ((uint64_t)(SIM->UIDML & 0xFFFF) << 32) | SIM->UIDL;
	uint64_t sum = v0 + v1 + v2;
	address[0] = (sum >> 40) & 0xFE;
	address[1] = (sum >> 32);
	address[2] = (sum >> 24);
	address[3] = (sum >> 16);
	address[4] = (sum >> 8);
	address[5] = sum;
}

static const ethernet_pins eth_pins =
{
	.rmii_ref_clk = { { PTE, 26 }, 2 },
	.rmii_crs_dv  = { { PTA, 14 }, 4 },
	.rmii_rxd0    = { { PTA, 13 }, 4 },
	.rmii_rxd1    = { { PTA, 12 }, 4 },
	.rmii_tx_en   = { { PTA, 15 }, 4 },
	.rmii_txd0    = { { PTA, 16 }, 4 },
	.rmii_txd1    = { { PTA, 17 }, 4 },
	.rmii_mdio    = { },
	.rmii_mdc     = { },
};

static bool dump_received_packets = true;

static void read_port_speed_and_duplex (size_t port_index, uint32_t& speed, bool& duplex)
{
	uint8_t val = switch_read_reg (0x19 + (port_index << 4));
	speed = (val & (1u << 2)) ? 100 : 10;
	duplex = (val & (1u << 1));
}

static void process_received_frames()
{
	size_t frame_size;
	uint8_t* frame;
	while ((frame = enet_read_get_buffer (&frame_size)) != nullptr)
	{
		uint32_t timestamp = scheduler_get_time_ms32();

		if (dump_received_packets)
		{
			if (debug_enabled())
			{
				gpio_set(PTA, 10, false);
				debug_printf ("%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X %02X%02X ...(%d bytes)\r\n",
					frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
					frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
					frame[12], frame[13], frame_size);
				gpio_set(PTA, 10, true);
			}
		}

		size_t port_index;
		if (STP_IsBridgeStarted(bridge))
		{
			// We have enabled tail tagging in StpCallback_EnableBpduTrapping.
			port_index = frame[frame_size - 1];
			frame_size--;
		}

		if (memcmp (frame, bpdu_dest_address, 6) == 0)
		{
			if ((frame_size > 14 + 3)
				&& (memcmp(&frame[14], bpdu_llc_field, 3) == 0)
				&& STP_IsBridgeStarted(bridge))
			{
				if (!STP_GetPortEnabled(bridge, port_index))
				{
					uint32_t speed;
					bool duplex;
					read_port_speed_and_duplex (port_index, speed, duplex);
					STP_OnPortEnabled(bridge, port_index, speed, duplex, timestamp);
				}

				STP_OnBpduReceived(bridge, port_index, &frame[17], frame_size - 17, timestamp);
			}
		}
		else
		{
			// TODO: process non-STP frame
		}

		enet_read_release_buffer(frame);
	}
}

static void poll_port_state()
{
	uint32_t timestamp = scheduler_get_time_ms32();

	for (size_t port_index = 0; port_index < 3; port_index++)
	{
		uint16_t reg = 0x1E + (port_index << 4);
		uint8_t val = switch_read_reg(reg);
		bool link_up = val & (1u << 5);

		if (link_up)
		{
			if (!STP_GetPortEnabled(bridge, port_index))
			{
				uint32_t speed;
				bool duplex;
				read_port_speed_and_duplex (port_index, speed, duplex);
				STP_OnPortEnabled (bridge, port_index, speed, duplex, timestamp);
			}
		}
		else
		{
			if (STP_GetPortEnabled(bridge, port_index))
				STP_OnPortDisabled(bridge, port_index, timestamp);
		}
	}
}

static volatile bool rx_pending = false;

static void rx_isr()
{
	rx_pending = true;
}

static constexpr struct enet_callbacks enet_callbacks = { .rx = rx_isr, .tx = nullptr, .error = nullptr };

static void on_one_second_tick()
{
	uint32_t timestamp = scheduler_get_time_ms32();
	STP_OnOneSecondTick (bridge, timestamp);
}

// ============================================================================

int main()
{
    //	clock_init (12);

	debug_printf ("\r\n\r\nTest App MK66F+KSZ8794.\r\n");

	static uint8_t event_queue_buffer[1024] __attribute__((section (".non_init")));
	event_queue_init (event_queue_buffer, sizeof(event_queue_buffer));

	scheduler_init();

	pit_init(0, 21000, scheduler_process_tick_irql);

	// initialize the led pins (leds off)
	gpio_make_output (PTA, 8, true);
	gpio_make_output (PTA, 9, true);
	gpio_make_output (PTA, 10, true);
	gpio_make_output (PTA, 11, true);

	scheduler_schedule_event_timer([]{ gpio_toggle(PTA, 9); }, nullptr, 1000, true);

	switch_init();

	SIM->SOPT2 |= SIM_SOPT2_RMIISRC_MASK; // Tell the Ethernet peripheral to take its clock from ENET_1588_CLKIN (not EXTAL)
	gpio_make_alternate(PTE, 26, 2);
	PORTE_PCR26 = PORT_PCR_MUX(0x02) |  PORT_PCR_ODE_MASK; // Set PTE26 as ENET_1588_CLKIN
	uint8_t mac_address[6];
	make_mac_address(mac_address);
	enet_init (eth_pins, mac_address, &enet_callbacks);

	// Enable promiscuous mode in the Ethernet peripheral, to let incoming BPDUs through.
	// That's good enough for this STP demo app.
	ENET->RCR |= ENET_RCR_PROM_MASK;

	bridge = STP_CreateBridge (3, 0, 0, &stp_callbacks, mac_address, 128);
	STP_StartBridge(bridge, scheduler_get_time_ms32());

	// Let's read the link states right now, and then set up polling every 100 ms.
	// A real application would use interrupts; for this demo app polling is good enough.
	poll_port_state();
	scheduler_schedule_event_timer (poll_port_state, "poll_port_state", 100, true);

	scheduler_schedule_event_timer(on_one_second_tick, "on_one_second_tick", 1000, true);

	while(1)
	{
		__WFI();

		event_queue_pop_all();

		if (rx_pending)
		{
			bool pushed = event_queue_try_push (process_received_frames, "process_received_frames");
			if (pushed)
				rx_pending = false;
		}
	}
}
