
#ifndef __LPC23XX_ENET_H
#define __LPC23XX_ENET_H

// Max allowed size on the wire is 1536, but we will tell the controller to add 4 CRC bytes
// and the switch IC to add 4 more, so the max allowed size in this point is 1536 - 4 - 4.
// There's a lot to be changed in the driver if we want frames 1536 bytes long in software.
#define MAX_FRAME_SIZE_IN_SOFTWARE	(1536 - 4 - 4)

void ENET_Init (const unsigned char macAddress[6]);

unsigned int tapdev_read (void* pPacket);
void tapdev_send (void* pPacket, unsigned int size);

void           ENET_MIIWriteRegister (unsigned char DevId, unsigned char RegAddr, unsigned short Value);
unsigned short ENET_MIIReadRegister  (unsigned char DevId, unsigned char RegAddr);

#endif
