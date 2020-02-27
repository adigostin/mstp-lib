
// This is a limited ("very" limited) Ethernet driver that contains
// only what's needed to demonstrate the STP library on this platform.

#include "ethernet.h"
#include "clock.h"
#include <string.h>
#include <assert.h>
#include <debugio.h>

static const enet_callbacks* callbacks;

struct alignas(8) descriptor
{
	uint16_t size;
	uint16_t status;
	volatile uint8_t* data;
};

static constexpr size_t max_frame_size = 1518; // as recommended in ENET_RCR field MAX_FL

static constexpr size_t rx_buffer_count = 5;
static constexpr size_t rx_buffer_size = (max_frame_size + 0xF) & ~0xFu;
static volatile descriptor rx_descriptors[rx_buffer_count] __attribute__((section (".non_init")));
using rx_buffer = uint8_t[rx_buffer_size];
static volatile rx_buffer rx_buffers[rx_buffer_count] __attribute__((section (".non_init")));
static size_t rx_consume_index;

static constexpr size_t tx_buffer_count = 5;
static constexpr size_t tx_buffer_size = (max_frame_size + 0xF) & ~0xFu;
static volatile descriptor tx_descriptors[tx_buffer_count] __attribute__((section (".non_init")));
using tx_buffer = uint8_t[tx_buffer_size];
static volatile tx_buffer tx_buffers[tx_buffer_count] __attribute__((section (".non_init")));
static size_t tx_produce_index;

void enet_init (const ethernet_pins& pins, const uint8_t mac_address[6], const enet_callbacks* irql_callbacks)
{
	::callbacks = irql_callbacks;

	clock_enable(ENET);

	gpio_make_alternate(pins.rmii_ref_clk);
	gpio_make_alternate(pins.rmii_crs_dv);
	gpio_make_alternate(pins.rmii_rxd0);
	gpio_make_alternate(pins.rmii_rxd1);
	gpio_make_alternate(pins.rmii_tx_en);
	gpio_make_alternate(pins.rmii_txd0);
	gpio_make_alternate(pins.rmii_txd1);
	if (pins.rmii_mdio.pin.port)
		gpio_make_alternate(pins.rmii_mdio);
	if (pins.rmii_mdc.pin.port)
		gpio_make_alternate(pins.rmii_mdc);

	//ENET->ECR |= ENET_ECR_RESET_MASK;
	ENET->ECR = ENET_ECR_DBSWP_MASK;

	for (size_t i = 0; i < rx_buffer_count; i++)
	{
		rx_descriptors[i].data = rx_buffers[i];
		rx_descriptors[i].status = (1u << 15); // E - empty, ready to receive
	}
	rx_descriptors[rx_buffer_count - 1].status |= (1u << 13); // W - wrap
    __DSB();
	rx_consume_index = 0;
	static_assert ((rx_buffer_size & 0xF) == 0);
	ENET->MRBR = rx_buffer_size;
	ENET->RDSR = (uint32_t)&rx_descriptors[0];
	ENET->RCR = (rx_buffer_size << ENET_RCR_MAX_FL_SHIFT) | ENET_RCR_CRCFWD_MASK | ENET_RCR_FCE_MASK | ENET_RCR_MII_MODE_MASK | ENET_RCR_RMII_MODE_MASK;

	for (size_t i = 0; i < tx_buffer_count; i++)
	{
		tx_descriptors[i].data = tx_buffers[i];
		tx_descriptors[i].status = 0;
	}
	tx_descriptors[tx_buffer_count - 1].status |= (1u << 13); // W - wrap
    __DSB();
	tx_produce_index = 0;
	static_assert ((tx_buffer_size & 0xF) == 0);
	ENET->TDSR = (uint32_t)&tx_descriptors[0];
	ENET->TCR = ENET_TCR_FDEN_MASK;

	ENET->FTRL = max_frame_size;

	ENET->TFWR = ENET_TFWR_STRFWD_MASK;
	ENET->RSFL = 0;

    ENET->PALR = (mac_address[0] << 24) | (mac_address[1] << 16) | (mac_address[2] << 8) | mac_address[3];
    ENET->PAUR = (mac_address[4] << 24) | (mac_address[5] << 16);

	if (callbacks && callbacks->rx)
	{
		ENET->EIMR |= ENET_EIMR_RXF_MASK;
		NVIC_EnableIRQ(ENET_Receive_IRQn);
	}

	if (callbacks && callbacks->tx)
	{
		ENET->EIMR |= ENET_EIMR_TXF_MASK;
		NVIC_EnableIRQ(ENET_Transmit_IRQn);
	}

	if (callbacks && callbacks->error)
	{
		ENET->EIMR |= ENET_EIMR_EBERR_MASK;
		NVIC_EnableIRQ(ENET_Error_IRQn);
	}

	ENET->ECR |= ENET_ECR_ETHEREN_MASK;

	// It seems setting RDAR has effect only after we enable the peripheral by setting ETHEREN (maybe same with TDAR)
	ENET->RDAR = ENET_RDAR_RDAR_MASK;
}

void enet_get_mac_address (uint8_t mac_address[6])
{
	mac_address[0] = ENET->PALR >> 24;
	mac_address[1] = ENET->PALR >> 16;
	mac_address[2] = ENET->PALR >> 8;
	mac_address[3] = ENET->PALR;
	mac_address[4] = ENET->PAUR >> 24;
	mac_address[5] = ENET->PAUR >> 16;
}

extern "C" void ENET_1588_Timer_IRQHandler()
{
	assert(false); // not implemented
}

extern "C" void ENET_Transmit_IRQHandler()
{
	ENET->EIR = ENET_EIR_TXF_MASK;
	callbacks->tx();
}

extern "C" void ENET_Receive_IRQHandler()
{
	ENET->EIR = ENET_EIR_RXF_MASK;
	callbacks->rx();
}

extern "C" void ENET_Error_IRQHandler()
{
	assert(false); // not implemented
}

static bool read_get_buffer_called;

uint8_t* enet_read_get_buffer (size_t* size)
{
	assert (!read_get_buffer_called);

	__DSB(); // bring latest descriptor data from memory to processor's cache

scan:
	auto desc = &rx_descriptors[rx_consume_index];
	if (desc->status & (1u << 15)) // E
		return nullptr;

	bool last = (rx_consume_index == rx_buffer_count - 1);

	// The L flag should be set, as for now we use descriptors with large buffers (1500+ bytes) that should fully contain any received frame.
	assert (desc->status & (1u << 11));

	static constexpr uint16_t error_flags = (1 << 4) | (1 << 2) || (1 << 1) | (1 << 0);
	if (desc->status & error_flags)
	{
		// Ignore it and mark it as empty (E flag) and if needed also as last (W flag).
		desc->status = (1u << 15) | (last ? (1u << 13) : 0);
		__DSB(); // push new descriptor data from processor's cache to memory
		rx_consume_index = last ? 0 : (rx_consume_index + 1);
		ENET->RDAR = ENET_RDAR_RDAR_MASK;
		goto scan;
	}

	*size = desc->size;
	::read_get_buffer_called = true;
	return (uint8_t*) desc->data;
}

void enet_read_release_buffer(uint8_t* data)
{
	assert (read_get_buffer_called);

	auto desc = &rx_descriptors[rx_consume_index];
	bool last = (rx_consume_index == rx_buffer_count - 1);

	// Mark it as empty (E flag) and if needed also as last (W flag).
	desc->status = (1u << 15) | (last ? (1u << 13) : 0);
	__DSB(); // push new descriptor data from processor's cache to memory
	rx_consume_index = last ? 0 : (rx_consume_index + 1);
	ENET->RDAR = ENET_RDAR_RDAR_MASK;

	read_get_buffer_called = false;
}

uint8_t* enet_write_get_buffer (size_t len)
{
	__DSB(); // bring latest descriptor data from memory to processor's cache

	auto desc = &tx_descriptors[tx_produce_index];
	if (desc->status & (1u << 14)) // TO1
		return nullptr;

	desc->status |= (1u << 14); // set TO1 to mark the descriptor as "currently written to"
	desc->size = len;
	return (uint8_t*)desc->data;
}

void enet_write_release_buffer (uint8_t* data)
{
	auto desc = &tx_descriptors[tx_produce_index];
	assert (desc->status & (1u << 14)); // TO1
	assert (desc->data == data);

	bool last = (tx_produce_index == tx_buffer_count - 1);

	desc->status = (1u << 15) | (last ? (1u << 13) : 0) | (1u << 11) | (1u << 10); // R, optionally W, L, TC
	__DSB(); // push new descriptor data from processor's cache to memory
	tx_produce_index = last ? 0 : (tx_produce_index + 1);
	ENET->TDAR = ENET_TDAR_TDAR_MASK;
}
