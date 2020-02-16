
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

using ethernet_frame_received_t = void(*)(uint8_t* packet, size_t len);

void enet_init (const ethernet_pins& pins, const uint8_t mac_address[6], ethernet_frame_received_t frame_received);
bool enet_is_init();
void enet_get_mac_address (uint8_t mac_address[6]);
void enet_send_blocking (const uint8_t* buffer, size_t len); // TODO: zero-copy
