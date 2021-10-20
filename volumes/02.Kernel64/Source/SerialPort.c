#include "SerialPort.h"
#include "Utility.h"

static SERIALMANAGER gs_stSerialManager;

void kInitializeSerialPort(void)
{
    WORD wPortBaseAddress;

    kInitializeMutex(&(gs_stSerialManager.stLock));

    wPortBaseAddress = SERIAL_PORT_COM1;

    // 인터럽트 활성화 레지스터에 0을 전송하여 모든 인터럽트 비활성화
    kOutPortByte(wPortBaseAddress + SERIAL_PORT_INDEX_INTERRUPTENABLE, 0);

    // 통신 속도를 115200으로 설정
    // 라인 제어 레지스터의 DLAB 비트를 1로 설정하여 제수 래치 레지스터에 접근
    kOutPortByte(wPortBaseAddress + SERIAL_PORT_INDEX_LINECONTROL, SERIAL_LINECONTROL_DLAB);

    // LSB 제수 래치 레지스터에 제수의 하위 8비트를 전송
    kOutPortByte(wPortBaseAddress + SERIAL_PORT_INDEX_DIVISORLATCHLSB, SERIAL_DIVISORLATCH_115200);

    // MSB 제수 래치 레지스터에 제수의 상위 8비트를 전송
    kOutPortByte(wPortBaseAddress + SERIAL_PORT_INDEX_DIVISORLATCHMSB, SERIAL_DIVISORLATCH_115200 >> 8);

    // 송수신 방법을 설정
    // 라인 제어 레지스터에 통신 방법을 8비트, No Parity
    kOutPortByte(wPortBaseAddress + SERIAL_PORT_INDEX_LINECONTROL,
        SERIAL_LINECONTROL_8BIT | SERIAL_LINECONTROL_NOPARITY | SERIAL_LINECONTROL_1BITSTOP);

    // FIFO의 인터럽트 발생 시점을 14바이트로 설정
    kOutPortByte(wPortBaseAddress + SERIAL_PORT_INDEX_FIFOCONTROL,
        SERIAL_FIFOCONTROL_FIFOENABLE | SERIAL_FIFOCONTROL_14BYTEFIFO);
}

static BOOL kIsSerialTransmitterBufferEmpty(void)
{
    BYTE bData;

    bData = kInPortByte(SERIAL_PORT_COM1 + SERIAL_PORT_INDEX_LINESTATUS);
    if((bData & SERIAL_LINESTATUS_TRANSMITBUFFEREMPTY) == SERIAL_LINESTATUS_TRANSMITBUFFEREMPTY)
    {
        return TRUE;
    }
    return FALSE;
}

void kSendSerialData(BYTE *pbBuffer, int iSize)
{
    int iSentByte;
    int iTempSize;
    int j;

    kLock(&(gs_stSerialManager.stLock));

    iSentByte = 0;
    while(iSentByte < iSize)
    {
        while(kIsSerialTransmitterBufferEmpty() == FALSE)
        {
            kSleep(0);
        }

        iTempSize = MIN(iSize - iSentByte, SERIAL_FIFOMAXSIZE);
        for(j = 0; j < iTempSize; j++)
        {
            kOutPortByte(SERIAL_PORT_COM1 + SERIAL_PORT_INDEX_TRANSMITBUFFER, pbBuffer[iSentByte + j]);
        }
        iSentByte += iTempSize;
    }

    kUnlock(&(gs_stSerialManager.stLock));
}

static BOOL kIsSerialReceiveBufferFull(void)
{
    BYTE bData;

    bData = kInPortByte(SERIAL_PORT_COM1 + SERIAL_PORT_INDEX_LINESTATUS);
    if((bData & SERIAL_LINESTATUS_RECEIVEDDATAREADY) == SERIAL_LINESTATUS_RECEIVEDDATAREADY)
    {
        return TRUE;
    }
    return FALSE;
}

int kReceiveSerialData(BYTE* pbBuffer, int iSize)
{
    int i;

    kLock(&(gs_stSerialManager.stLock));

    for(i = 0; i < iSize; i++)
    {
        if(kIsSerialReceiveBufferFull() == FALSE)
        {
            break;
        }
        pbBuffer[i] = kInPortByte(SERIAL_PORT_COM1 + SERIAL_PORT_INDEX_RECEIVEBUFFER);
    }

    kUnlock(&(gs_stSerialManager.stLock));

    return i;
}

void kClearSerialFIFO(void)
{
    kLock(&(gs_stSerialManager.stLock));

    kOutPortByte(SERIAL_PORT_COM1 + SERIAL_PORT_INDEX_FIFOCONTROL,
        SERIAL_FIFOCONTROL_FIFOENABLE | SERIAL_FIFOCONTROL_14BYTEFIFO |
        SERIAL_FIFOCONTROL_CLEARRECEIVEFIFO | SERIAL_FIFOCONTROL_CLEARTRANSMITFIFO);

    kUnlock(&(gs_stSerialManager.stLock));
}