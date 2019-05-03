
#pragma once
//#include "event_queue.h"
//#include <stddef.h>
#include <stdint.h>

void     scheduler_init (uint32_t timer, uint32_t clock_frequency);
bool     scheduler_is_init();
uint32_t scheduler_get_time_ms32();
uint64_t scheduler_get_time_ms64();
void     scheduler_wait (uint32_t ms);

struct timer_t;

timer_t* scheduler_schedule_irql_timer  (void (*callback)(void*), void* callback_arg, const char* debug_name, uint32_t period_ms, bool repeatable);
timer_t* scheduler_schedule_irql_timer  (void (*callback)(),                          const char* debug_name, uint32_t period_ms, bool repeatable);
timer_t* scheduler_schedule_event_timer (void (*callback)(void*), void* callback_arg, const char* debug_name, uint32_t period_ms, bool repeatable);
timer_t* scheduler_schedule_event_timer (void (*callback)(),                          const char* debug_name, uint32_t period_ms, bool repeatable);
void     scheduler_cancel_timer (timer_t* timer);
