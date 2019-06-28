
#pragma once
#include <stm32f769xx.h>

typedef void (*timer_callback_t)();

void timer_init (TIM_TypeDef* timer, uint32_t prescaler_value, uint32_t reload_value, timer_callback_t callback);
