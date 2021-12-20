#include "LocalAPIC.h"
#include "MPConfigurationTable.h"

QWORD kGetLocalAPICBaseAddress(void)
{
    MPCONFIGURATIONTABLEHEADER* pstMPHeader;

    pstMPHeader = kGetMPConfigurationManager()->pstMPConfigurationTableHeader;
    return pstMPHeader->dwMemoryMapIOAddressOfLocalAPIC;
}

void kEnableSoftwareLocalAPIC(void)
{
    QWORD qwLocalAPICBaseAddress;

    qwLocalAPICBaseAddress = kGetLocalAPICBaseAddress();
   // 의사 인터럽트 벡터 레지스터의 APIC 소프트웨어 활성/비활성 필드를 1로 설정
    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_SVR) |= 0x100;
}

void kSendEOIToLocalAPIC(void)
{
    QWORD qwLocalAPICBaseAddress;

    qwLocalAPICBaseAddress = kGetLocalAPICBaseAddress();

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_EOI) = 0;
}

void kSetTaskPriority(BYTE bPriority)
{
    QWORD qwLocalAPICBaseAddress;

    qwLocalAPICBaseAddress = kGetLocalAPICBaseAddress();

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_TASKPRIORITY) = bPriority;
}

void kInitializeLocalVectorTable(void)
{
    QWORD qwLocalAPICBaseAddress;
    DWORD dwTempValue;

    qwLocalAPICBaseAddress = kGetLocalAPICBaseAddress();

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_TIMER) |= APIC_INTERRUPT_MASK;

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_LINT0) |= APIC_INTERRUPT_MASK;

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_LINT1) = APIC_TRIGGERMODE_EDGE | APIC_POLARITY_ACTIVEHIGH | APIC_DELIVERYMODE_NMI;

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_ERROR) |= APIC_INTERRUPT_MASK;

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_PERFORMANCEMONITORINGCOUNTER) |= APIC_INTERRUPT_MASK;

    *(DWORD*) (qwLocalAPICBaseAddress + APIC_REGISTER_THERMALSENSOR) |= APIC_INTERRUPT_MASK;
}