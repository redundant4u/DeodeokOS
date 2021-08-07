#include "Utility.h"
#include "AssemblyUtility.h"
#include <stdarg.h>

volatile QWORD g_qwTickCount = 0;

void kMemSet(void* pvDestination, BYTE bData, int iSize)
{
    int i;

    for(i = 0; i < iSize; i++)
    {
        ((char*) pvDestination)[i] = bData;
    }
}

int kMemCpy(void* pvDestination, const void* pvSource, int iSize)
{
    int i;

    for(i = 0; i < iSize; i++)
    {
        ((char*) pvDestination)[i] = ((char*) pvSource)[i];
    }

    return iSize;
}

int kMemCmp(const void* pvDestination, const void* pvSource, int iSize)
{
    int i;
    char cTemp;

    for(i = 0; i < iSize; i++)
    {
        cTemp = ((char*) pvDestination)[i] - ((char*) pvSource)[i];
        if(cTemp != 0)
        {
            return (int) cTemp;
        }
    }
    return 0;
}

BOOL kSetInterruptFlag(BOOL bEnableInterrupt)
{
    QWORD qwRFLAGS;

    qwRFLAGS = kReadRFLAGS();
    if(bEnableInterrupt == TRUE)
    {
        kEnableInterrupt();
    }
    else
    {
        kDisableInterrupt();
    }

    if(qwRFLAGS & 0x0200)
    {
        return TRUE;
    }
    return FALSE;
}

int kStrLen(const char* pcBuffer)
{
    int i;

    for(i = 0; ; i++)
    {
        if(pcBuffer[i] == '\0')
        {
            break;
        }
    }
    return i;
}

static gs_qwTotalRAMMBSize = 0;

void kCheckTotalRAMSize(void)
{
    DWORD* pdwCurrentAddress;
    DWORD dwPreviousValue;

    pdwCurrentAddress = (DWORD*) 0x4000000;
    while(1)
    {
        dwPreviousValue = *pdwCurrentAddress;
        *pdwCurrentAddress = 0x12345678;
        if(*pdwCurrentAddress != 0x12345678)
        {
            break;
        }

        *pdwCurrentAddress = dwPreviousValue;
        pdwCurrentAddress += (0x400000 / 4);
    }

    gs_qwTotalRAMMBSize = (QWORD) pdwCurrentAddress / 0x100000;
}

QWORD kGetTotalRAMSize(void)
{
    return gs_qwTotalRAMMBSize;
}

long kAToI(const char* pcBuffer, int iRadix)
{
    long lReturn;

    switch(iRadix)
    {
        case 16:
            lReturn = kHexStringToQword(pcBuffer);
            break;

        case 10:
        default:
            lReturn = kDecimalStringToLong(pcBuffer);
            break;
    }
    return lReturn;
}

QWORD kHexStringToQword(const char* pcBuffer)
{
    QWORD qwValue = 0;
    int i;

    for(i = 0; pcBuffer[i] != '\0'; i++)
    {
        qwValue *= 16;

        if(('A' <= pcBuffer[i]) && (pcBuffer[i] <= 'Z'))
        {
            qwValue += (pcBuffer[i] - 'A') + 10;
        }
        else if(('a' <= pcBuffer[i]) && (pcBuffer[i] <= 'z'))
        {
            qwValue += (pcBuffer[i] - 'a') + 10;   
        }
        else
        {
            qwValue += pcBuffer[i] - '0';
        }
    }
    return qwValue;
}

long kDecimalStringToLong(const char* pcBuffer)
{
    long lValue = 0;
    int i;

    if(pcBuffer[0] == '-')
    {
        i = 1;
    }
    else
    {
        i = 0;
    }

    for(; pcBuffer[i] != '\0'; i++)
    {
        lValue *= 10;
        lValue += pcBuffer[i] - '0';
    }

    if(pcBuffer[0] == '-')
    {
        lValue = -lValue;
    }
    return lValue;
}

int kIToA(long lValue, char* pcBuffer, int iRadix)
{
    int iReturn;

    switch(iRadix)
    {
        case 16:
            iReturn = kHexToString(lValue, pcBuffer);
            break;

        case 10:
        default:
            iReturn = kDecimalToString(lValue, pcBuffer);
            break;
    }
    return iReturn;
}

int kHexToString(QWORD qwValue, char* pcBuffer)
{
    QWORD i;
    QWORD qwCurrentValue;

    if(qwValue == 0)
    {
        pcBuffer[0] = '0';
        pcBuffer[1] = '\0';
        return 1;
    }

    for(i = 0; qwValue > 0; i++)
    {
        qwCurrentValue = qwValue % 16;
        if(qwCurrentValue >= 10)
        {
            pcBuffer[i] = 'A' + (qwCurrentValue - 10);   
        }
        else
        {
            pcBuffer[i] = '0' + qwCurrentValue;
        }

        qwValue = qwValue / 16;
    }
    pcBuffer[i] = '\0';

    kReverseString(pcBuffer);
    return i;
}

int kDecimalToString(long lValue, char* pcBuffer)
{
    long i;

    if(lValue == 0)
    {
        pcBuffer[0] = '0';
        pcBuffer[1] = '\0';
        return 1;
    }
    
    if(lValue < 0)
    {
        i = 1;
        pcBuffer[0] = '-';
        lValue = -lValue;
    }
    else
    {
        i = 0;
    }

    for(; lValue > 0; i++)
    {
        pcBuffer[i] = '0' + lValue % 10;
        lValue = lValue / 10;
    }
    pcBuffer[i] = '\0';

    if(pcBuffer[0] == '-')
    {
        kReverseString(&(pcBuffer[1]));
    }
    else
    {
        kReverseString(pcBuffer);
    }

    return i;
}

void kReverseString(char* pcBuffer)
{
    int iLength;
    int i;
    char cTemp;

    iLength = kStrLen(pcBuffer);
    for(i = 0; i < iLength / 2; i++)
    {
        cTemp = pcBuffer[i];
        pcBuffer[i] = pcBuffer[iLength - 1 - i];
        pcBuffer[iLength - 1 - i] = cTemp;
    }
}

int kSPrintf(char* pcBuffer, const char* pcFormatString, ...)
{
    va_list ap;
    int iReturn;

    va_start(ap, pcFormatString);
    iReturn = kVSPrintf(pcBuffer, pcFormatString, ap);
    va_end(ap);

    return iReturn;
}

int kVSPrintf(char* pcBuffer, const char* pcFormatString, va_list ap)
{
    QWORD i, j;
    int iBufferIndex = 0;
    int iFormatLength, iCopyLength;
    char* pcCopyString;
    QWORD qwValue;
    int iValue;

    iFormatLength = kStrLen(pcFormatString);
    for(i = 0; i < iFormatLength; i++)
    {
        if(pcFormatString[i] == '%')
        {
            i++;
            switch(pcFormatString[i])
            {
                case 's':
                    pcCopyString = (char*) (va_arg(ap, char*));
                    iCopyLength = kStrLen(pcCopyString);
                    kMemCpy(pcBuffer + iBufferIndex, pcCopyString, iCopyLength);
                    iBufferIndex += iCopyLength;
                    break;

                case 'c':
                    pcBuffer[iBufferIndex] = (char) (va_arg(ap, int));
                    iBufferIndex++;
                    break;

                case 'd':
                case 'i':
                    iValue = (int) (va_arg(ap, int));
                    iBufferIndex += kIToA(iValue, pcBuffer + iBufferIndex, 10);
                    break;

                case 'x':
                case 'X':
                    qwValue = (DWORD) (va_arg(ap, DWORD)) & 0xFFFFFFFF;
                    iBufferIndex += kIToA(qwValue, pcBuffer + iBufferIndex, 16);
                    break;

                case 'q':
                case 'Q':
                case 'p':
                    qwValue = (QWORD) (va_arg(ap, QWORD));
                    iBufferIndex += kIToA(qwValue, pcBuffer + iBufferIndex, 16);
                    break;

                default:
                    pcBuffer[iBufferIndex] = pcFormatString[i];
                    iBufferIndex++;
                    break;
            }
        }
        else
        {
            pcBuffer[iBufferIndex] = pcFormatString[i];
            iBufferIndex++;
        }
    }

    pcBuffer[iBufferIndex] = '\0';
    return iBufferIndex;
}

QWORD kGetTickCount(void)
{
    return g_qwTickCount;
}