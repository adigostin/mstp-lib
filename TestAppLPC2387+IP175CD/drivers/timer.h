
#pragma once
#include <stdint.h>

typedef void (*timer_callback)();

void timer_init (uint32_t timer, uint32_t clock_frequency, uint32_t ticks_per_second, timer_callback callback_irql);
