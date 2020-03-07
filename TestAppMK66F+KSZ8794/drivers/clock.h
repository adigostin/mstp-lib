
#include <stdint.h>

void clock_init (uint32_t external_clock_mhz);
void clock_enable (void* peripheral_base_address);
bool clock_enabled (void* peripheral_base_address);
