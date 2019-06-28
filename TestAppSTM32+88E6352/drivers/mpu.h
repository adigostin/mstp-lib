
#pragma once
#include <stdint.h>

// Note AG:
// This MPU driver contains the minimum functionality required for configuring the Ethernet DMA memory.
// You probably can't use it for anything else without major rewriting.

struct MPU_Region_InitTypeDef
{
	uint8_t                Enable;                /*!< Specifies the status of the region. 
													 This parameter can be a value of @ref CORTEX_MPU_Region_Enable                 */
	uint8_t                Number;                /*!< Specifies the number of the region to protect. 
													 This parameter can be a value of @ref CORTEX_MPU_Region_Number                 */
	uint32_t               BaseAddress;           /*!< Specifies the base address of the region to protect.                           */
	uint8_t                Size;                  /*!< Specifies the size of the region to protect. 
													 This parameter can be a value of @ref CORTEX_MPU_Region_Size                   */
	uint8_t                SubRegionDisable;      /*!< Specifies the number of the subregion protection to disable. 
													 This parameter must be a number between Min_Data = 0x00 and Max_Data = 0xFF    */         
	uint8_t                TypeExtField;          /*!< Specifies the TEX field level.
													 This parameter can be a value of @ref CORTEX_MPU_TEX_Levels                    */                 
	uint8_t                AccessPermission;      /*!< Specifies the region access permission type. 
													 This parameter can be a value of @ref CORTEX_MPU_Region_Permission_Attributes  */
	uint8_t                DisableExec;           /*!< Specifies the instruction access status. 
													 This parameter can be a value of @ref CORTEX_MPU_Instruction_Access            */
	uint8_t                IsShareable;           /*!< Specifies the shareability status of the protected region. 
													 This parameter can be a value of @ref CORTEX_MPU_Access_Shareable              */
	uint8_t                IsCacheable;           /*!< Specifies the cacheable status of the region protected. 
													 This parameter can be a value of @ref CORTEX_MPU_Access_Cacheable              */
	uint8_t                IsBufferable;          /*!< Specifies the bufferable status of the protected region. 
													 This parameter can be a value of @ref CORTEX_MPU_Access_Bufferable             */
};

			
#define  MPU_REGION_ENABLE     ((uint8_t)0x01U)
#define  MPU_REGION_DISABLE    ((uint8_t)0x00U)
#define   MPU_REGION_SIZE_256B     ((uint8_t)0x07U) 
#define   MPU_REGION_SIZE_16KB     ((uint8_t)0x0DU) 
#define  MPU_REGION_FULL_ACCESS    ((uint8_t)0x03U)  
#define  MPU_ACCESS_BUFFERABLE         ((uint8_t)0x01U)
#define  MPU_ACCESS_NOT_BUFFERABLE     ((uint8_t)0x00U)
#define  MPU_ACCESS_CACHEABLE         ((uint8_t)0x01U)
#define  MPU_ACCESS_NOT_CACHEABLE     ((uint8_t)0x00U)
#define  MPU_ACCESS_SHAREABLE        ((uint8_t)0x01U)
#define  MPU_ACCESS_NOT_SHAREABLE    ((uint8_t)0x00U)
#define  MPU_REGION_NUMBER0    ((uint8_t)0x00U)  
#define  MPU_REGION_NUMBER1    ((uint8_t)0x01U) 
#define  MPU_REGION_NUMBER2    ((uint8_t)0x02U)  
#define  MPU_TEX_LEVEL0    ((uint8_t)0x00U)
#define  MPU_TEX_LEVEL1    ((uint8_t)0x01U)
#define  MPU_TEX_LEVEL2    ((uint8_t)0x02U)
#define  MPU_INSTRUCTION_ACCESS_ENABLE      ((uint8_t)0x00U)
#define  MPU_INSTRUCTION_ACCESS_DISABLE     ((uint8_t)0x01U)
#define  MPU_PRIVILEGED_DEFAULT      ((uint32_t)0x00000004U)

void mpu_disable ();
void mpu_enable (uint32_t control);
void mpu_config_region (MPU_Region_InitTypeDef *MPU_Init);
