
#pragma once
#include <stdint.h>

enum class clock_pllsaip { div2 = 0, div4 = 1, div6 = 2, div8 = 3 };
enum class clock_pllsaidivr { div2 = 0, div4 = 1, div8 = 2, div16 = 3 };
enum class clock_pllsaidivq {
	 div1 =  0,  div2 =  1,  div3 =  2,  div4 =  3,
     div5 =  4,  div6 =  5,  div7 =  6,  div8 =  7,
     div9 =  8, div10 =  9, div11 = 10, div12 = 11,
    div13 = 12, div14 = 13, div15 = 14, div16 = 15,
	div17 = 16, div18 = 17, div19 = 18, div20 = 19,
    div21 = 20, div22 = 21, div23 = 22, div24 = 23,
    div25 = 24, div26 = 25, div27 = 26, div28 = 27,
    div29 = 28, div30 = 29, div31 = 30, div32 = 31
};

struct clock_pll_sai_init
{
	uint32_t         pllsain;    // multiplier 50...432
    clock_pllsaip    pllsaip;    // divider for the 48 MHz clock for USB, RNG, SDMMC
	uint32_t         pllsaiq;    // divider for the SAI clock 2..15
	clock_pllsaidivq pllsaidivq; // further divider for SAI clock
	uint32_t         pllsair;    // divider for LCD clock
	clock_pllsaidivr pllsaidivr; // further divider for LCD clock
};

void clock_init (uint32_t external_clock_mhz); // or pass 0 to use the internal 16 MHz clock
void clock_enable (void* peripheral_base_address);
bool clock_enabled (void* peripheral_base_address);
uint32_t clock_get_apb1_prescale();
uint32_t clock_get_apb2_prescale();
uint32_t clock_get_apb1_freq();
uint32_t clock_get_apb2_freq();
uint32_t clock_get_ahb_freq();
uint32_t clock_get_sysclk_freq();
uint32_t clock_get_pll_input_freq();
uint32_t clock_get_pll_output_freq();
uint32_t clock_get_hs_freq();
uint32_t clock_get_freq (void* peripheral_base_address);
void clock_init_pllsai (const clock_pll_sai_init& s);
