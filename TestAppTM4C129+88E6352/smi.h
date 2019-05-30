
#pragma once
#include <stdint.h>

void     smi_init();
void     smi_write (uint8_t phy_addr, uint8_t reg_addr, uint16_t data);
uint16_t smi_read  (uint8_t phy_addr, uint8_t reg_addr);
