
#pragma once

void VIC_Init ();

void VIC_SetVectoredIRQ (void(*pIRQSub)(), unsigned int Priority, unsigned int VicIntSource);

__arm __interwork bool irq_enabled();
