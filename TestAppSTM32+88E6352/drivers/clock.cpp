
#include "clock.h"
#include "assert.h"
#include <stm32f769xx.h>

static uint32_t external_clock_mhz  __attribute__((section (".non_init")));

static const uint8_t pllp_division_factors[] = { 2, 4, 6, 8 };

static const uint8_t pllsaidivr_factors[] = { 2, 4, 8, 16 };

// The system Clock is configured as follows:
//  - System Clock source            = PLL (HSE)
//  - SYSCLK(Hz)                     = 216'000'000
//  - HCLK(Hz)                       = 216'000'000
//  - AHB Prescaler                  = 1
//  - APB1 Prescaler                 = 4
//  - APB2 Prescaler                 = 2
//  - HSI Frequency(Hz)              = 16'000'000 (used if 0 is passed to clock_init)
//  - PLL_M                          = as parameter to clock_init
//  - PLL_N                          = 432
//  - PLL_P                          = 2
//  - PLL_Q                          = 9
//  - PLL_R                          = 7
//  - VDD(V)                         = 3.3
//  - Main regulator output voltage  = Scale1 mode
//  - Flash Latency(WS)              = 7

void clock_init (uint32_t external_clock_mhz)
{
	::external_clock_mhz = external_clock_mhz;

	// Set FLASH latency.
	FLASH->ACR = (FLASH->ACR & ~0xF) | 7;

	// Enable PWR clock.
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	// Delay after an RCC peripheral clock enabling.
	asm ("nop;nop;nop;nop;");

	// Activation OverDrive Mode.
	PWR->CR1 |= PWR_CR1_ODEN;
	while ((PWR->CR1 & PWR_CR1_ODEN) == 0)
		;

	// Activation OverDrive Switching.
	PWR->CR1 |= PWR_CR1_ODSWEN;
	while ((PWR->CR1 & PWR_CR1_ODSWEN) == 0)
		;

	if (external_clock_mhz != 0)
	{
		RCC->CR = RCC->CR & ~RCC_CR_HSION | RCC_CR_HSEON;
		while((RCC->CR & RCC_CR_HSERDY) == 0 )
			;
	}

	assert ((external_clock_mhz == 0) || (external_clock_mhz <= 0x3F)); // 3F is the maximum divider value RCC_PLLCFGR_PLLM

	// Main PLL configuration and activation.
	RCC->PLLCFGR = RCC->PLLCFGR & ~(RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP)
	             | (external_clock_mhz ? RCC_PLLCFGR_PLLSRC_HSE : RCC_PLLCFGR_PLLSRC_HSI)
	             | ((external_clock_mhz == 0) ? 16 : external_clock_mhz) // set PLLM to get 1MHz (16 MHz is the HSI frequency)
	             | (432 << RCC_PLLCFGR_PLLN_Pos) // set PLLN to get 432 MHz
	             | 0;                            // set PLLP to 0 for a division factor of 2, to get 216MHz for the main system clock


	RCC->CR |= RCC_CR_PLLON;
	while ((RCC->CR & RCC_CR_PLLRDY) != RCC_CR_PLLRDY)
		;

	// Set AHB prescaler.
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV1;

	// Select PLL as system clock.
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
		;

	// Set APB1 & APB2 prescaler.
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE1) | RCC_CFGR_PPRE1_DIV4;
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV2;
}

struct clock_info_t
{
	void* peripheral_base_address;
	volatile uint32_t* reg;
	uint32_t bitmask;
};

// TODO: keep them sorted and do binary search only
static const clock_info_t clock_infos[] =
{
	{ TIM1,  &RCC->APB2ENR, RCC_APB2ENR_TIM1EN },
	{ TIM2,  &RCC->APB1ENR, RCC_APB1ENR_TIM2EN },
	{ TIM3,  &RCC->APB1ENR, RCC_APB1ENR_TIM3EN },
	{ GPIOA, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN },
	{ GPIOB, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOBEN },
	{ GPIOC, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOCEN },
	{ GPIOD, &RCC->AHB1ENR, RCC_AHB1ENR_GPIODEN },
	{ GPIOE, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOEEN },
	{ GPIOF, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOFEN },
	{ GPIOG, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOGEN },
	{ GPIOH, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOHEN },
	{ GPIOI, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOIEN },
	{ GPIOJ, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOJEN },
	{ GPIOK, &RCC->AHB1ENR, RCC_AHB1ENR_GPIOKEN },
	{ USART1, &RCC->APB2ENR, RCC_APB2ENR_USART1EN },
	{ USART2, &RCC->APB1ENR, RCC_APB1ENR_USART2EN },
	{ USART3, &RCC->APB1ENR, RCC_APB1ENR_USART3EN },
	{ UART4,  &RCC->APB1ENR, RCC_APB1ENR_UART4EN },
	{ UART5,  &RCC->APB1ENR, RCC_APB1ENR_UART5EN },
	{ USART6, &RCC->APB2ENR, RCC_APB2ENR_USART6EN },
	{ UART7,  &RCC->APB1ENR, RCC_APB1ENR_UART7EN },
	{ UART8,  &RCC->APB1ENR, RCC_APB1ENR_UART8EN },
	{ ETH,    &RCC->AHB1ENR, RCC_AHB1ENR_ETHMACEN | RCC_AHB1ENR_ETHMACRXEN | RCC_AHB1ENR_ETHMACTXEN },
	{ SYSCFG, &RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN },
	{ CAN1,   &RCC->APB1ENR, RCC_APB1ENR_CAN1EN },
	{ CAN2,   &RCC->APB1ENR, RCC_APB1ENR_CAN2EN },
	{ CAN3,   &RCC->APB1ENR, RCC_APB1ENR_CAN3EN },
	{ SAI1,   &RCC->APB2ENR, RCC_APB2ENR_SAI1EN },
	{ SAI2,   &RCC->APB2ENR, RCC_APB2ENR_SAI2EN },
	{ DMA1,   &RCC->AHB1ENR, RCC_AHB1ENR_DMA1EN },
	{ DMA2,   &RCC->AHB1ENR, RCC_AHB1ENR_DMA2EN },
	{ SPI2,   &RCC->APB1ENR, RCC_APB1ENR_SPI2EN },
	{ I2C4,   &RCC->APB1ENR, RCC_APB1ENR_I2C4EN },
	{ LTDC,   &RCC->APB2ENR, RCC_APB2ENR_LTDCEN },
    { DSI,    &RCC->APB2ENR, RCC_APB2ENR_DSIEN },
	{ nullptr, nullptr, 0 },
};

void clock_enable (void* peripheral_base_address)
{
	auto p = clock_infos;
	while (p->peripheral_base_address != nullptr)
	{
		if (p->peripheral_base_address == peripheral_base_address)
		{
			*p->reg |= p->bitmask;
			break;
		}

		p++;
	}

	assert (p->peripheral_base_address != nullptr); // not found in array - needs to be added

	// Delay after an RCC peripheral clock enabling.
	// TODO: wait as much as necessary
	asm ("nop;nop;nop;nop;");
	asm ("nop;nop;nop;nop;");
	asm ("nop;nop;nop;nop;");
	asm ("nop;nop;nop;nop;");

	// Or maybe we should force a "clock reset" like this?
	//RCC->APB1RSTR |= (RCC_APB1RSTR_I2C4RST);  // Force the I2C peripheral clock reset
	//RCC->APB1RSTR &= ~(RCC_APB1RSTR_I2C4RST); // Release the I2C peripheral clock reset
}

bool clock_enabled (void* peripheral_base_address)
{
	auto p = clock_infos;
	while (p->peripheral_base_address != nullptr)
	{
		if (p->peripheral_base_address == peripheral_base_address)
			return (*p->reg & p->bitmask) == p->bitmask;

		p++;
	}

	assert (p->peripheral_base_address != nullptr); // not found in array - needs to be added
	return false;
}

uint32_t clock_get_hs_freq()
{
	if ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC_HSE_Msk) == RCC_PLLCFGR_PLLSRC_HSE)
		return ::external_clock_mhz * 1'000'000;
	else
		return 16'000'000;
}

uint32_t clock_get_pll_input_freq()
{
	uint32_t osc = clock_get_hs_freq();
	uint32_t pllm = (RCC->PLLCFGR & RCC_PLLCFGR_PLLM_Msk) >> RCC_PLLCFGR_PLLM_Pos;
	return osc / pllm;
}

uint32_t clock_get_pll_output_freq()
{
	uint32_t pll_input = clock_get_pll_input_freq();
	uint32_t plln = (RCC->PLLCFGR & RCC_PLLCFGR_PLLN_Msk) >> RCC_PLLCFGR_PLLN_Pos;
	uint32_t pllp = (RCC->PLLCFGR & RCC_PLLCFGR_PLLP_Msk) >> RCC_PLLCFGR_PLLP_Pos;
	pllp = pllp_division_factors[pllp];

	return pll_input * plln / pllp;
}

uint32_t clock_get_sysclk_freq()
{
	if (RCC->CR & RCC_CR_PLLON)
		return clock_get_pll_output_freq();

	// not implemented
	assert(false); return 0;
}

static constexpr uint16_t ahb_prescale_values[] = { 1, 1, 1, 1,    1, 1, 1, 1,     2, 4, 8, 16,    64, 128, 256, 512 };

uint32_t clock_get_ahb_freq()
{
	uint32_t HPRE = (RCC->CFGR >> 4) & 0xF;
	return clock_get_sysclk_freq() / ahb_prescale_values[HPRE];
}

static constexpr uint8_t apb_prescale_values[] = { 1, 1, 1, 1,    2, 4, 8, 16 };

uint32_t clock_get_apb1_prescale()
{
	uint32_t PPRE1 = (RCC->CFGR >> 10) & 7;
	return apb_prescale_values[PPRE1];
}

uint32_t clock_get_apb2_prescale()
{
	uint32_t PPRE2 = (RCC->CFGR >> 13) & 7;
	return apb_prescale_values[PPRE2];
}

// APB1 = "low-speed"
uint32_t clock_get_apb1_freq()
{
	return clock_get_ahb_freq() / clock_get_apb1_prescale();
}

// APB2 = "high-speed"
uint32_t clock_get_apb2_freq()
{
	return clock_get_ahb_freq() / clock_get_apb2_prescale();
}

enum class clock_source_t { apb1, apb2, system, hsi, lse };

uint32_t clock_get_source_freq (clock_source_t source)
{
	if (source == clock_source_t::apb1)
		return clock_get_apb1_freq();

	if (source == clock_source_t::apb2)
		return clock_get_apb2_freq();

	if (source == clock_source_t::system)
		return clock_get_sysclk_freq();

	assert(false); return 0;
}

static clock_source_t get_uart1_clock_source()
{
	switch ((RCC->DCKCFGR2 & RCC_DCKCFGR2_USART1SEL_Msk) >> RCC_DCKCFGR2_USART1SEL_Pos)
	{
		case 0:  return clock_source_t::apb2;
		case 1:  return clock_source_t::system;
		case 2:  return clock_source_t::hsi;
		default: return clock_source_t::lse;
	}
}

static clock_source_t get_uart2_clock_source()
{
	switch ((RCC->DCKCFGR2 & RCC_DCKCFGR2_USART2SEL_Msk) >> RCC_DCKCFGR2_USART2SEL_Pos)
	{
		case 0:  return clock_source_t::apb1;
		case 1:  return clock_source_t::system;
		case 2:  return clock_source_t::hsi;
		default: return clock_source_t::lse;
	}
}

static clock_source_t get_uart3_clock_source()
{
	switch ((RCC->DCKCFGR2 & RCC_DCKCFGR2_USART3SEL_Msk) >> RCC_DCKCFGR2_USART3SEL_Pos)
	{
		case 0:  return clock_source_t::apb1;
		case 1:  return clock_source_t::system;
		case 2:  return clock_source_t::hsi;
		default: return clock_source_t::lse;
	}
}

static clock_source_t get_uart4_clock_source()
{
	switch ((RCC->DCKCFGR2 & RCC_DCKCFGR2_UART4SEL_Msk) >> RCC_DCKCFGR2_UART4SEL_Pos)
	{
		case 0:  return clock_source_t::apb1;
		case 1:  return clock_source_t::system;
		case 2:  return clock_source_t::hsi;
		default: return clock_source_t::lse;
	}
}

static clock_source_t get_uart5_clock_source()
{
	switch ((RCC->DCKCFGR2 & RCC_DCKCFGR2_UART5SEL_Msk) >> RCC_DCKCFGR2_UART5SEL_Pos)
	{
		case 0:  return clock_source_t::apb1;
		case 1:  return clock_source_t::system;
		case 2:  return clock_source_t::hsi;
		default: return clock_source_t::lse;
	}
}

// See Table 1. STM32F76xxx and STM32F77xxx register boundary addresses
uint32_t clock_get_freq (void* peripheral_base_address)
{
	if ((peripheral_base_address == TIM1) || (peripheral_base_address == TIM8))
		return clock_get_apb2_freq() * ((clock_get_apb2_prescale() == 1) ? 1 : 2);

	if ((peripheral_base_address == TIM2) || (peripheral_base_address == TIM3))
		return clock_get_apb1_freq() * ((clock_get_apb1_prescale() == 1) ? 1 : 2);

	if (peripheral_base_address == USART1)
		return clock_get_source_freq (get_uart1_clock_source());

	if (peripheral_base_address == USART2)
		return clock_get_source_freq (get_uart2_clock_source());

	if (peripheral_base_address == USART3)
		return clock_get_source_freq (get_uart3_clock_source());

	if (peripheral_base_address == UART4)
		return clock_get_source_freq (get_uart4_clock_source());

	if (peripheral_base_address == UART5)
		return clock_get_source_freq (get_uart5_clock_source());

	if ((peripheral_base_address == CAN1) || (peripheral_base_address == CAN2) || (peripheral_base_address == CAN3))
		return clock_get_apb1_freq();

	if (peripheral_base_address == LTDC)
	{
		uint32_t pll_input = clock_get_pll_input_freq();
		uint32_t pllsain = (RCC->PLLSAICFGR & RCC_PLLSAICFGR_PLLSAIN_Msk) >> RCC_PLLSAICFGR_PLLSAIN_Pos;
		uint32_t pllsair = (RCC->PLLSAICFGR & RCC_PLLSAICFGR_PLLSAIR_Msk) >> RCC_PLLSAICFGR_PLLSAIR_Pos;
		uint32_t divr = (RCC->DCKCFGR1 & RCC_DCKCFGR1_PLLSAIDIVR_Msk) >> RCC_DCKCFGR1_PLLSAIDIVR_Pos;
		divr = pllsaidivr_factors[divr];
		return pll_input * pllsain / pllsair / divr;
	}

	assert(false); // not implemented
	return 0;
}

void clock_init_pllsai (const clock_pll_sai_init& s)
{
	assert (s.pllsain >= 50 && s.pllsain <= 432);
	assert (s.pllsaiq >= 2 && s.pllsaiq <= 15);
	assert (s.pllsair >= 2 && s.pllsair <= 7);

	// Tell the hardware to disable PLLSAI and wait till it's disabled.
	RCC->CR &= ~RCC_CR_PLLSAION;
	while (RCC->CR & RCC_CR_PLLSAIRDY)
		;

	RCC->PLLSAICFGR = (s.pllsair << RCC_PLLSAICFGR_PLLSAIR_Pos)
	                | (s.pllsaiq << RCC_PLLSAICFGR_PLLSAIQ_Pos)
	                | ((int)s.pllsaip << RCC_PLLSAICFGR_PLLSAIP_Pos)
	                | (s.pllsain << RCC_PLLSAICFGR_PLLSAIN_Pos);

	RCC->DCKCFGR1 = RCC->DCKCFGR1 & ~RCC_DCKCFGR1_PLLSAIDIVR_Msk | ((int)s.pllsaidivr << RCC_DCKCFGR1_PLLSAIDIVR_Pos);
	RCC->DCKCFGR1 = RCC->DCKCFGR1 & ~RCC_DCKCFGR1_PLLSAIDIVQ_Msk | ((int)s.pllsaidivq << RCC_DCKCFGR1_PLLSAIDIVQ_Pos);

	// Enable it again.
	RCC->CR |= RCC_CR_PLLSAION;
	while (RCC->CR & RCC_CR_PLLSAIRDY == 0)
		;
}
