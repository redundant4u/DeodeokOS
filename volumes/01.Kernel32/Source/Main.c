#include "Types.h"

void kPrintString(int iX, int iY, const char* pcString);
BOOL kInitiakizeKernel64Area(void);

void Main(void)
{
    DWORD i;

    kPrintString(0, 3, "C Language Kernel Start");

    kInitiakizeKernel64Area();
    kPrintString(0, 4, "IA-32e Kernel Area Initialization Complete");
    while(1);
}

void kPrintString(int iX, int iY, const char* pcString)
{
    CHARACTER* pstScreen = (CHARACTER*) 0xB8000;
    int i;

    pstScreen += (iY * 80) + iX;
    for( i = 0; pcString[i] != 0; i++) {
        pstScreen[i].bCharactor = pcString[i];
    }
}

BOOL kInitiakizeKernel64Area(void)
{
    DWORD* pdwCurrentAddress;

    pdwCurrentAddress = (DWORD*) 0x100000;

    while((DWORD) pdwCurrentAddress < 0x60000)
    {
        *pdwCurrentAddress = 0x00;

        if(*pdwCurrentAddress != 0)
        {
            return FALSE;
        }

        pdwCurrentAddress++;
    }

    return TRUE;
}