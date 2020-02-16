
#include "ethernet.h"
#include "clock.h"
#include <string.h>
#include <assert.h>

namespace std
{
	template <class T, size_t N> constexpr size_t size(const T (&)[N]) noexcept { return N; }
}

struct alignas(8) rx_descriptor
{
	uint16_t size;
	uint16_t flags;
	volatile uint8_t* data;
};

static constexpr uint32_t rx_buffer_count = 20;
static constexpr uint32_t rx_buffer_size = 256;
static volatile rx_descriptor rx_descriptors[rx_buffer_count];
static volatile uint8_t rx_buffers[rx_buffer_count * rx_buffer_size];

void enet_init (const ethernet_pins& pins, const uint8_t mac_address[6], ethernet_frame_received_t frame_received)
{
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
	for (size_t i = 0; i < std::size(rx_descriptors); i++)
	{
		rx_descriptors[i].data = &rx_buffers[i * rx_buffer_size];
		rx_descriptors[i].flags = (1u << 15); // E - empty, ready to receive
	}
	rx_descriptors[std::size(rx_descriptors) - 1].flags |= (1u << 13); // W - wrap
	static_assert ((rx_buffer_size & 0xF) == 0);
	ENET->MRBR = rx_buffer_size;
	ENET->RDSR = (uint32_t)&rx_descriptors[0];
    __DSB();

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

	ENET->EIMR |= ENET_EIMR_RXF_MASK | ENET_EIMR_TXF_MASK | ENET_EIMR_EBERR_MASK;
	//NVIC_EnableIRQ(ENET_1588_Timer_IRQn);
	//NVIC_EnableIRQ(ENET_Transmit_IRQn);
	NVIC_EnableIRQ(ENET_Receive_IRQn);
	NVIC_EnableIRQ(ENET_Error_IRQn);

	ENET->ECR |= ENET_ECR_ETHEREN_MASK;

	// It seems setting RDAR has effect only after we enable the peripheral by setting ETHEREN
	ENET->RDAR = ENET_RDAR_RDAR_MASK;
}

extern "C" void ENET_1588_Timer_IRQHandler()
{
	assert(false); // not implemented
}

extern "C" void ENET_Transmit_IRQHandler()
{
	assert(false); // not implemented
}

extern "C" void ENET_Receive_IRQHandler()
{
	assert(false); // not implemented
}

extern "C" void ENET_Error_IRQHandler()
{
	assert(false); // not implemented
}
