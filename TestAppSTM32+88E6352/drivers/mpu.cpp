
#include "mpu.h"
#include <stm32f769xx.h>

void mpu_disable()
{
	// Make sure outstanding transfers are done.
	__DMB();

	// Disable fault exceptions
	SCB->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;

	// Disable the MPU and clear the control register
	MPU->CTRL = 0;
}

void mpu_enable (uint32_t control)
{
	// Enable the MPU
	MPU->CTRL = control | MPU_CTRL_ENABLE_Msk;

	// Enable fault exceptions
	SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;

	// Ensure MPU setting take effects
	__DSB();
	__ISB();
}

void mpu_config_region (MPU_Region_InitTypeDef *MPU_Init)
{
	// Check the parameters
	//assert_param(IS_MPU_REGION_NUMBER(MPU_Init->Number));
	// assert_param(IS_MPU_REGION_ENABLE(MPU_Init->Enable));

	// Set the Region number
	MPU->RNR = MPU_Init->Number;

	if (MPU_Init->Enable)
	{
		// Check the parameters
        //assert_param(IS_MPU_INSTRUCTION_ACCESS(MPU_Init->DisableExec));
        //assert_param(IS_MPU_REGION_PERMISSION_ATTRIBUTE(MPU_Init->AccessPermission));
        //assert_param(IS_MPU_TEX_LEVEL(MPU_Init->TypeExtField));
        //assert_param(IS_MPU_ACCESS_SHAREABLE(MPU_Init->IsShareable));
        //assert_param(IS_MPU_ACCESS_CACHEABLE(MPU_Init->IsCacheable));
        //assert_param(IS_MPU_ACCESS_BUFFERABLE(MPU_Init->IsBufferable));
        //assert_param(IS_MPU_SUB_REGION_DISABLE(MPU_Init->SubRegionDisable));
        //assert_param(IS_MPU_REGION_SIZE(MPU_Init->Size));

		MPU->RBAR = MPU_Init->BaseAddress;
		MPU->RASR = ((uint32_t)MPU_Init->DisableExec             << MPU_RASR_XN_Pos)   |
		            ((uint32_t)MPU_Init->AccessPermission        << MPU_RASR_AP_Pos)   |
		            ((uint32_t)MPU_Init->TypeExtField            << MPU_RASR_TEX_Pos)  |
		            ((uint32_t)MPU_Init->IsShareable             << MPU_RASR_S_Pos)    |
		            ((uint32_t)MPU_Init->IsCacheable             << MPU_RASR_C_Pos)    |
		            ((uint32_t)MPU_Init->IsBufferable            << MPU_RASR_B_Pos)    |
		            ((uint32_t)MPU_Init->SubRegionDisable        << MPU_RASR_SRD_Pos)  |
		            ((uint32_t)MPU_Init->Size                    << MPU_RASR_SIZE_Pos) |
		            ((uint32_t)MPU_Init->Enable                  << MPU_RASR_ENABLE_Pos);
	}
	else
	{
		MPU->RBAR = 0x00;
		MPU->RASR = 0x00;
	}
}