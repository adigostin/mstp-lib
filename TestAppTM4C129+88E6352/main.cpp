
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

static const uint8_t mac_address[6] = { 0x00, 0x10, 0x20, 0x33, 0x44, 0x55 };

static STP_BRIDGE* stp_bridge;

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

// Port mapping:
//  STP Port Index  | switch port index | description
// -----------------|-------------------|-------------
//   Non-STP port   |        0          | to microcontroller
// STP port index 0 |        1          | 
// STP port index 1 |        2          | 
// STP port index 2 |        3          | 
// STP port index 3 |        4          | 
// STP port index 4 |        5          | SERDES

static constexpr uint32_t stp_port_count = 5;

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

static constexpr uint8_t stp_switch_mdio_address = 2;
static constexpr uint8_t other_switch_mdio_address = 4;

// ============================================================================

static void StpCallback_EnableLearning (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp)
{
	bool forwarding = STP_GetPortForwarding (bridge, portIndex, treeIndex);
//	WritePortStateRegister (enable, forwarding, portIndex);
}

static void StpCallback_EnableForwarding (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp)
{
	bool learning = STP_GetPortLearning (bridge, portIndex, treeIndex);
//	WritePortStateRegister (learning, enable, portIndex);
}

static void* StpCallback_TransmitGetBuffer (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	assert(false); // not implemented
}

static void StpCallback_TransmitReleaseBuffer (const struct STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	assert(false); // not implemented
}

static void StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE  flushType)
{
	assert(false); // not implemented
}

static void StpCallback_DebugStrOut (const struct STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
    printf ("%s", nullTerminatedString);
    if (flush)
        fflush (stdout);
}

static void StpCallback_OnTopologyChange (const STP_BRIDGE* bridge)
{
	assert(false); // not implemented
}

static void StpCallback_OnNotifiedTopologyChange (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp)
{
	assert(false); // not implemented
}
	
static void StpCallback_OnPortRoleChanged (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_PORT_ROLE role, unsigned int timestamp)
{
}

static void StpCallback_OnConfigChanged (const struct STP_BRIDGE* bridge, unsigned int timestamp)
{
	assert(false); // not implemented
}

static void* StpCallback_AllocAndZeroMemory (unsigned int size)
{
	auto p = malloc(size);
	memset (p, 0, size);
	return p;
}

static void StpCallback_FreeMemory(void* p)
{
	free(p);
}

static const STP_CALLBACKS stp_callbacks = 
{
	.enableLearning           = StpCallback_EnableLearning,
	.enableForwarding         = StpCallback_EnableForwarding,
	.transmitGetBuffer        = StpCallback_TransmitGetBuffer,
	.transmitReleaseBuffer    = StpCallback_TransmitReleaseBuffer,
	.flushFdb                 = StpCallback_FlushFdb,
	.debugStrOut              = StpCallback_DebugStrOut,
	.onTopologyChange         = StpCallback_OnTopologyChange,
	.onNotifiedTopologyChange = StpCallback_OnNotifiedTopologyChange,
	.onPortRoleChanged        = StpCallback_OnPortRoleChanged,
	.onConfigChanged          = StpCallback_OnConfigChanged,
	.allocAndZeroMemory       = StpCallback_AllocAndZeroMemory,
	.freeMemory               = StpCallback_FreeMemory,
};

static void process_received_frame (const uint8_t* data, size_t size)
{
	uint32_t now = clock_get_time_ms();
	printf ("%u.%03u: RX: %02x:%02x:%02x:%02x:%02x:%02x  %02x:%02x:%02x:%02x:%02x:%02x  %02x%02x\r\n",
		now / 1000, now % 1000,
		data[0], data[1], data[2], data[3], data[4], data[5], 
		data[6], data[7], data[8], data[9], data[10], data[11], 
		data[12], data[13]);

	if ((size >= 6) && (memcmp (data, bpdu_dest_address, 6) == 0))
	{
    	if (stp_bridge != nullptr)
		{
			//assert(false); // not implemented
		}
	}
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

	ethernet_init(mac_address);

	auto now = clock_get_time_ms();

	stp_bridge = STP_CreateBridge (stp_port_count, 1, 0, &stp_callbacks, mac_address, 100);

	STP_EnableLogging (stp_bridge, 1);

	//STP_StartBridge (stp_bridge, now);

	// Tell the switch IC that our CPU is wired to port 0 (Monitor Control register)
	//value = switch_read_register (stp_switch_mdio_address, dev_addr_global1, 0x1A);
	//value &= 0xFF0F;
	//switch_write_register (stp_switch_mdio_address, dev_addr_global1, 0x1A, value);

	uint32_t next_stp_1s_tick = clock_get_time_ms() + 1000;

	while (true)
	{
		now = clock_get_time_ms();

		ethernet_try_get_received_frames(process_received_frame);

		if (clock_get_time_ms() >= next_stp_1s_tick)
		{
			//STP_OnOneSecondTick (stp_bridge, now);
			next_stp_1s_tick += 1000;
		}
	}
}
