
#pragma once
#include <stdint.h>
#include <stdlib.h>

typedef void (*ethernet_rx_callback)(const uint8_t* data, size_t size);

void ethernet_init (const uint8_t* mac_address);
void ethernet_get_device_address (uint8_t* addr);
void ethernet_get_received (ethernet_rx_callback rx_callback);
void* ethernet_transmit_get_buffer (size_t size);
void ethernet_transmit_release_buffer (void* buffer);
