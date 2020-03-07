
#pragma once
#include <stdint.h>

void    switch_init();
void    switch_write_reg (uint16_t reg, uint8_t value);
uint8_t switch_read_reg (uint16_t reg);
