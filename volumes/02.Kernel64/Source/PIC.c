#include "PIC.h"


void kInitializePIC(void)
{
    kOutPortByte(PIC_MASTER_PORT1, 0x11);

    kOutPortByte(PIC_MASTER_PORT2, PIC_IRQSTARTVECTOR);
    kOutPortByte(PIC_MASTER_PORT2, 0x04);
    kOutPortByte(PIC_MASTER_PORT2, 0x01);

    kOutPortByte(PIC_SLAVE_PORT1, 0x11);

    kOutPortByte(PIC_SLAVE_PORT2, PIC_IRQSTARTVECTOR + 8);
    kOutPortByte(PIC_SLAVE_PORT2, 0x02);
    kOutPortByte(PIC_SLAVE_PORT2, 0x01);
}

void kMaskPICInterrupt(WORD wIRQBitmask)
{
    kOutPortByte(PIC_MASTER_PORT2, (BYTE) wIRQBitmask);
    kOutPortByte(PIC_SLAVE_PORT2, (BYTE) (wIRQBitmask >> 8));
}

void kSendEOIToPIC(int iIRQNumber)
{
    kOutPortByte(PIC_MASTER_PORT1, 0x20);

    if(iIRQNumber >= 8)
    {
        kOutPortByte(PIC_SLAVE_PORT1, 0x20);
    }
}