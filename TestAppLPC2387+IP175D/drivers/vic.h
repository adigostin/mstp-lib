
#ifndef __SYS_H
#define __SYS_H

void VIC_Init ();

void VIC_SetVectoredIRQ (void(*pIRQSub)(), unsigned int Priority, unsigned int VicIntSource);


#endif
