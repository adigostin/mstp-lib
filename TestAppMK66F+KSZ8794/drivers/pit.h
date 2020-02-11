
#pragma once
#include <stdint.h>

typedef void (*pit_callback_t)();

void pit_init (uint32_t pit_channel, uint32_t reload_value, pit_callback_t callback);
