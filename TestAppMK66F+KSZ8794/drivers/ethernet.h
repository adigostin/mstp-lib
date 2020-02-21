
#pragma once
#include "gpio.h"

struct ethernet_pins
{
	pin_and_af  rmii_ref_clk;
	pin_and_af  rmii_crs_dv;
	pin_and_af  rmii_rxd0;
	pin_and_af  rmii_rxd1;
	pin_and_af  rmii_tx_en;
	pin_and_af  rmii_txd0;
	pin_and_af  rmii_txd1;
	pin_and_af  rmii_mdio; // optional
	pin_and_af  rmii_mdc; // optional
};

struct enet_callbacks
{
	void(*rx)();
	void(*tx)();
	void(*error)();
};

void enet_init (const ethernet_pins& pins, const uint8_t mac_address[6], const enet_callbacks* irql_callbacks);
bool enet_is_init();
void enet_get_mac_address (uint8_t mac_address[6]);

uint8_t* enet_read_get_buffer (size_t* size);
void     enet_read_release_buffer (uint8_t* data);

uint8_t* enet_write_get_buffer (size_t len);
void     enet_write_release_buffer (uint8_t* data);
