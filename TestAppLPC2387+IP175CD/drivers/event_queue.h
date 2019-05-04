
#pragma once
#include <stddef.h>
#include <stdint.h>

void event_queue_init (uint8_t* buffer, size_t buffer_size);
bool event_queue_is_init();
bool event_queue_try_push (void(*handler)(void*, size_t), const void* payload, size_t payload_size, const char* debug_name);
bool event_queue_try_push (void(*handler)(void*), void* arg, const char* debug_name);
bool event_queue_try_push (void(*handler)(), const char* debug_name);
void event_queue_pop_all();
