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