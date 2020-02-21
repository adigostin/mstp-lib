
#include "ethernet.h"
#include "clock.h"
#include <string.h>
#include <assert.h>
#include <debugio.h>

static const enet_callbacks* callbacks;

struct alignas(8) rx_descriptor
{
	uint16_t size;
	uint16_t status;
	volatile uint8_t* data;
};

static constexpr size_t rx_buffer_count = 5;
static constexpr size_t rx_buffer_size = 1536;
static volatile rx_descriptor rx_descriptors[rx_buffer_count];
using rx_buffer = uint8_t[rx_buffer_size];
static volatile rx_buffer rx_buffers[rx_buffer_count];
static size_t rx_consume_index;

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

	memset ((void*)&rx_descriptors[0], 0, sizeof(rx_descriptors));
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

	// TODO: init TX descriptors

	ENET->RCR = (1536 << ENET_RCR_MAX_FL_SHIFT)
		| ENET_RCR_CRCFWD_MASK
| ENET_RCR_PROM_MASK
| ENET_RCR_FCE_MASK
		| ENET_RCR_MII_MODE_MASK | ENET_RCR_RMII_MODE_MASK;

	// Configures MAC transmit controller: duplex mode, mac address insertion.
//	tcr = base->TCR & ~(ENET_TCR_FDEN_MASK | ENET_TCR_ADDINS_MASK);
//	tcr |= ((kENET_MiiHalfDuplex != config->miiDuplex) ? (uint32_t)ENET_TCR_FDEN_MASK : 0U) |
//		   ((0U != (macSpecialConfig & (uint32_t)kENET_ControlMacAddrInsert)) ? (uint32_t)ENET_TCR_ADDINS_MASK : 0U);
//	base->TCR = tcr;
//	base->TDSR      = (uint32_t)bufferConfig->txBdStartAddrAlign;

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
	//ENET->TDAR = ENET_TDAR_TDAR_MASK;
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
		__DSB();
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
	__DSB();
	rx_consume_index = last ? 0 : (rx_consume_index + 1);
	ENET->RDAR = ENET_RDAR_RDAR_MASK;

	read_get_buffer_called = false;
}
