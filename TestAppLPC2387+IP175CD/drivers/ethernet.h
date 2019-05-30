
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef void (*ethernet_receive_handler) (uint8_t* data, size_t size);

void ethernet_init (const uint8_t macAddress[6], ethernet_receive_handler rx_handler);

void ethernet_send (void* data, size_t size);

void     ENET_MIIWriteRegister (unsigned char DevId, unsigned char RegAddr, unsigned short Value);
uint16_t ENET_MIIReadRegister  (unsigned char DevId, unsigned char RegAddr);
