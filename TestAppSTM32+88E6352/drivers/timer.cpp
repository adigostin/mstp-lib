
#include "timer.h"
#include "clock.h"
#include "assert.h"

static constexpr volatile TIM_TypeDef* timers[14] = { TIM1, TIM2, TIM3, TIM4, TIM5, TIM6, TIM7, TIM8, TIM9, TIM10, TIM11, TIM12, TIM13, TIM14 };
static constexpr IRQn_Type timer_irqs[14] =
{
	(IRQn_Type) -1,
	TIM2_IRQn,
	TIM3_IRQn,
	TIM4_IRQn,
	TIM5_IRQn,
	TIM6_DAC_IRQn,
	TIM7_IRQn,
	(IRQn_Type) -1,
	TIM1_BRK_TIM9_IRQn,
	TIM1_UP_TIM10_IRQn,
	TIM1_TRG_COM_TIM11_IRQn,
	TIM8_BRK_TIM12_IRQn,
	TIM8_UP_TIM13_IRQn,
	TIM8_TRG_COM_TIM14_IRQn
};
static constexpr uint32_t timer_count = 14;

static timer_callback_t callbacks[timer_count];

static uint32_t get_timer_index (volatile TIM_TypeDef* timer)
{
	for (uint32_t i = 0; i < timer_count; i++)
	{
		if (timers[i] == timer)
			return i;
	}

	assert(false);
	return -1;
}

// extern "C" void TIM1_IRQHandler() - not implemented
extern "C" void TIM2_IRQHandler()               {  TIM2->SR &= ~TIM_SR_UIF; callbacks[ 1](); }
extern "C" void TIM3_IRQHandler()               {  TIM3->SR &= ~TIM_SR_UIF; callbacks[ 2](); }
extern "C" void TIM4_IRQHandler()               {  TIM4->SR &= ~TIM_SR_UIF; callbacks[ 3](); }
extern "C" void TIM5_IRQHandler()               {  TIM5->SR &= ~TIM_SR_UIF; callbacks[ 4](); }
extern "C" void TIM6_DAC_IRQHandler()           {  TIM6->SR &= ~TIM_SR_UIF; callbacks[ 5](); }
extern "C" void TIM7_IRQHandler()               {  TIM7->SR &= ~TIM_SR_UIF; callbacks[ 6](); }
// extern "C" void TIM8_IRQHandler() - not implemented
extern "C" void TIM1_BRK_TIM9_IRQHandler()      {  TIM9->SR &= ~TIM_SR_UIF; callbacks[ 8](); }
extern "C" void TIM1_UP_TIM10_IRQHandler()      { TIM10->SR &= ~TIM_SR_UIF; callbacks[ 9](); }
extern "C" void TIM1_TRG_COM_TIM11_IRQHandler() { TIM11->SR &= ~TIM_SR_UIF; callbacks[10](); }
extern "C" void TIM8_BRK_TIM12_IRQHandler()     { TIM12->SR &= ~TIM_SR_UIF; callbacks[11](); }
extern "C" void TIM8_UP_TIM13_IRQHandler()      { TIM13->SR &= ~TIM_SR_UIF; callbacks[12](); }
extern "C" void TIM8_TRG_COM_TIM14_IRQHandler() { TIM14->SR &= ~TIM_SR_UIF; callbacks[13](); }


void timer_init (TIM_TypeDef* timer, uint32_t prescaler_value, uint32_t reload_value, timer_callback_t callback)
{
	assert (prescaler_value <= 0xFFFF);
	clock_enable (timer);
	assert ((timer->CR1 & TIM_CR1_CEN) == 0);
	assert (callback != nullptr);

	assert (timer != TIM1); // not yet implemented - it has some complex interrrupts
	assert (timer != TIM8); // not yet implemented - it has some complex interrrupts
	assert (timer != TIM6); // not yet implemented - something about DAC

	if (reload_value >= 0x10000)
		assert ((timer == TIM2) || (timer == TIM5));

	uint32_t timer_index = get_timer_index(timer);

	auto irq = timer_irqs[timer_index];
	if (irq == -1)
		assert(false); // the timer is not yet implemented by this driver

	callbacks[timer_index] = callback;

	timer->PSC = prescaler_value;
	timer->ARR = reload_value;
	timer->DIER |= TIM_DIER_UIE;
	timer->CR1 |= TIM_CR1_CEN;

	NVIC_EnableIRQ (irq);
}
