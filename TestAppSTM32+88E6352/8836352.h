
#pragma once
#include <stdint.h>

static constexpr uint8_t global2_dev_addr = 0x1C;
static constexpr uint8_t global2_smi_phy_command = 0x18;
static constexpr uint8_t global2_smi_phy_data = 0x19;

void switch_init();
uint16_t read_phy_register (uint8_t phy_addr, uint8_t reg_addr);
void write_phy_register (uint8_t phy_addr, uint8_t reg_addr, uint16_t value);

