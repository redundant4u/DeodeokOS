#include "Types.h"
#include "Page.h"

void kPrintString(int iX, int iY, const char* pcString);
BOOL kInitiakizeKernel64Area(void);
BOOL kIsMemoryEnough(void);

void Main(void)
{
    DWORD i;

    kPrintString(0, 3, "C Language Kernel Start");

    kPrintString(0, 4, "Minimum Memory Size Check...................[    ]");
    if(kIsMemoryEnough() == FALSE)
    {
        kPrintString(45, 4, "FAIL");
        kPrintString(0, 5, "Not Enough Memory");
        while(1);
    }
    else
    {
        kPrintString(45, 4, "Pass");
    }

    kInitiakizeKernel64Area();
    kPrintString(0, 5, "IA-32e Kernel Area Initialize...............[    ]");
    if(kInitiakizeKernel64Area() == FALSE)
    {
        kPrintString(45, 5, "FAIL");
        kPrintString(0, 6, "Not Enough Memory");
        while(1);
    }
    kPrintString(45, 5, "Pass");

    kPrintString(0, 6, "IA-32e Page Tables Initialize...............[    ]");
    kInitializePageTables();
    kPrintString(45, 6, "Pass");

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

    while((DWORD) pdwCurrentAddress < 0x600000)
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

BOOL kIsMemoryEnough(void)
{
    DWORD* pdwCurrentAddress;

    pdwCurrentAddress = (DWORD*) 0x100000;

    while((DWORD) pdwCurrentAddress < 0x4000000)
    {
        *pdwCurrentAddress = 0x12345678;

        if(*pdwCurrentAddress != 0x12345678)
        {
            return FALSE;
        }


        pdwCurrentAddress += (0x100000 / 4);
    }

    return TRUE;
}