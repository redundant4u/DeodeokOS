#include "MultiProcessor.h"
#include "MPConfigurationTable.h"
#include "AssemblyUtility.h"
#include "LocalAPIC.h"
#include "PIT.h"

volatile int g_iWakeUpApplicationProcessorCount = 0;
volatile QWORD g_qwAPICIDAddress = 0;

BOOL kStartUpApplicationProcessor(void)
{
    if(kAnalysisMPConfigurationTable() == FALSE)
    {
        return FALSE;
    }

    kEnableGlobalLocalAPIC();

    kEnableSoftwareLocalAPIC();

    if(kWakeUpApplicationProcessor() == FALSE)
    {
        return FALSE;
    }


    return TRUE;
}

static BOOL kWakeUpApplicationProcessor(void)
{
    MPCONFIGURATIONMANAGER* pstMPManager;
    MPCONFIGURATIONTABLEHEADER* pstMPHeader;
    QWORD qwLocalAPICBaseAddress;
    BOOL bInterruptFlag;
    int i;

    bInterruptFlag = kSetInterruptFlag(FALSE);

    pstMPManager = kGetMPConfigurationManager();
    pstMPHeader = pstMPManager->pstMPConfigurationTableHeader;
    qwLocalAPICBaseAddress = pstMPHeader->dwMemoryMapIOAddressOfLocalAPIC;

    g_qwAPICIDAddress = qwLocalAPICBaseAddress + APIC_REGISTER_APICID;

    // 하위 인터럽트 커맨드 레지스터에 초기화 IPI와 시작 IPI를 전송하여 AP를 깨움
    // 초기화(INIT) IPI 전송
    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_ICR_LOWER) =
        APIC_DESTINATIONSHORTHAND_ALLEXCLUDINGSELF | APIC_TRIGGERMODE_EDGE |
        APIC_LEVEL_ASSERT | APIC_DESTINATIONMODE_PHYSICAL | APIC_DELIVERYMODE_INIT;

    kWaitUsingDirectPIT(MSTOCOUNT(10));

    if(*(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_ICR_LOWER) & APIC_DELIVERYSTATUS_PENDING)
    {
        kInitializePIT(MSTOCOUNT(1), TRUE);

        kSetInterruptFlag(bInterruptFlag);
        return FALSE;
    }

    // 시작(Start Up) IPI 전송, 2회 반복 전송함
    for(i = 0; i < 2; i++)
    {
        *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_ICR_LOWER) =
            APIC_DESTINATIONSHORTHAND_ALLEXCLUDINGSELF | APIC_TRIGGERMODE_EDGE |
            APIC_LEVEL_ASSERT | APIC_DESTINATIONMODE_PHYSICAL | APIC_DELIVERYMODE_STARTUP | 0x10;

        kWaitUsingDirectPIT(USTOCOUNT(200));

        if(*(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_ICR_LOWER) & APIC_DELIVERYSTATUS_PENDING)
        {
            kInitializePIT(MSTOCOUNT(1), TRUE);

            kSetInterruptFlag(bInterruptFlag);
            return FALSE;
        }
    }

    kInitializePIT(MSTOCOUNT(1), TRUE);

    kSetInterruptFlag(bInterruptFlag);

    while(g_iWakeUpApplicationProcessorCount < (pstMPManager->iProcessorCount - 1))
    {
        kSleep(50);
    }
    return TRUE;
}

BYTE kGetAPICID(void)
{
    MPCONFIGURATIONTABLEHEADER* pstMPHeader;
    QWORD qwLocalAPICBaseAddress;

    if(g_qwAPICIDAddress == 0)
    {
        pstMPHeader = kGetMPConfigurationManager()->pstMPConfigurationTableHeader;
        if(pstMPHeader == NULL)
        {
            return 0;
        }

        qwLocalAPICBaseAddress = pstMPHeader->dwMemoryMapIOAddressOfLocalAPIC;
        g_qwAPICIDAddress = qwLocalAPICBaseAddress + APIC_REGISTER_APICID;
    }

    return *((DWORD*) g_qwAPICIDAddress) >> 24;
}