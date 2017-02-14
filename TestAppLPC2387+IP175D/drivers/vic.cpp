
#include <assert.h>
#include "vic.h"

#include <nxp/iolpc2387.h>

static __interwork void IRQ_Handler_Thumb ()
{
	typedef void (*InterruptFunction) ();

	InterruptFunction function = (InterruptFunction) (unsigned int) VICADDRESS;

	assert (function != 0);

	function();  // Call vectored interrupt function.

	// Clear interrupt in VIC.
	VICADDRESS = 0;
}

extern "C" __irq __arm void IRQ_Handler ()
{
	IRQ_Handler_Thumb();
}

void VIC_Init(void)
{
volatile unsigned long * pVecAdd, *pVecCntl;
int i;
  // Assign all interrupt channels to IRQ
  VICINTSELECT  =  0;
  // Disable all interrupts
  VICINTENCLEAR = 0xFFFFFFFF;
  // Clear all software interrupts
  VICSOFTINTCLEAR = 0xFFFFFFFF;
  // VIC registers can be accessed in User or privileged mode
  VICPROTECTION = 0;
  // Clear interrupt
  VICADDRESS = 0;

  // Clear address of the Interrupt Service routine (ISR) for vectored IRQs
  // and disable all vectored IRQ slots
  for(i = 0,  pVecCntl = &VICVECTPRIORITY0, pVecAdd = &VICVECTADDR0; i < 32; ++i)
  {
    *pVecCntl++ = *pVecAdd++ = 0;
  }
}

/*************************************************************************
 * Function Name: VIC_SetVectoredIRQ
 * Parameters:  void(*pIRQSub)()
 *              unsigned int VicIrqSlot
 *              unsigned int VicIntSouce
 *
 * Return: void
 *
 * Description:  Init vectored interrupts
 *
 *************************************************************************/

void VIC_SetVectoredIRQ(void(*pIRQSub)(), unsigned int Priority, unsigned int VicIntSource)
{
	// load base address of vectored address registers
	volatile unsigned long* pReg = &VICVECTADDR0;

	// Set Address of callback function to corresponding Slot
	*(pReg+VicIntSource) = (unsigned int) pIRQSub;

	// load base address of ctrl registers
	pReg = &VICVECTPRIORITY0;

	// Set source channel and enable the slot
	*(pReg+VicIntSource) = Priority;

	// Clear FIQ select bit
	VICINTSELECT &= ~(1u << VicIntSource);

    VICINTENABLE = (1u << VicIntSource);
}
