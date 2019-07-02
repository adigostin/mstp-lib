
#pragma once
#include "gpio.h"
#include <stddef.h>

struct ethernet_pins
{
	pin_and_af_t  rmii_ref_clk;
	pin_and_af_t  rmii_mdio;
	pin_and_af_t  rmii_mdc;
	pin_and_af_t  rmii_crs_dv;
	pin_and_af_t  rmii_rxd0;
	pin_and_af_t  rmii_rxd1;
	pin_and_af_t  rmii_tx_en;
	pin_and_af_t  rmii_txd0;
	pin_and_af_t  rmii_txd1;
};

using ethernet_frame_received_t = void(*)(uint8_t* packet, size_t len);

struct ethernet_diags
{
	uint32_t frames_discarded_event_queue_full;
};

bool enet_init (const struct ethernet_pins& pins, const uint8_t mac_address[6], ethernet_frame_received_t frame_received);
bool enet_is_init();
void enet_get_mac_address (uint8_t mac_address[6]);
void enet_send_blocking (const uint8_t* buffer, size_t len); // TODO: zero-copy
void enet_dump_frame (const uint8_t* frame, size_t frame_len);
uint16_t enet_read_smi  (uint16_t dev_addr, uint16_t reg_number);
void     enet_write_smi (uint16_t dev_addr, uint16_t reg_number, uint16_t value);
