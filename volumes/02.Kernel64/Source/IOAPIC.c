#include "IOAPIC.h"
#include "MPConfigurationTable.h"
#include "PIC.h"

static IOAPICMANAGER gs_stIOAPICManager;

QWORD kGetIOAPICBaseAddressOfISA(void)
{
    MPCONFIGURATIONMANAGER* pstMPManager;
    IOAPICENTRY* pstIOAPICEntry;

    if(gs_stIOAPICManager.qwIOAPICBaseAddressOfISA == NULL)
    {
        pstIOAPICEntry = kFindIOAPICEntryForISA();
        if(pstIOAPICEntry != NULL)
        {
            gs_stIOAPICManager.qwIOAPICBaseAddressOfISA =
                pstIOAPICEntry->dwMemoryMapAddress & 0xFFFFFFFF;
        }
    }

    return gs_stIOAPICManager.qwIOAPICBaseAddressOfISA;
}

void kSetIOAPICRedirectionEntry(IOREDIRECTIONTABLE* pstEntry, BYTE bAPICID,
    BYTE bInterruptMask, BYTE bFlagsAndDeliveryMode, BYTE bVector)
{
    kMemSet(pstEntry, 0, sizeof(IOREDIRECTIONTABLE));

    pstEntry->bDestination = bAPICID;
    pstEntry->bFlagsAndDeliveryMode = bFlagsAndDeliveryMode;
    pstEntry->bInterruptMask = bInterruptMask;
    pstEntry->bVector = bVector;
}

void kReadIOAPICRedirectionTable(int iINTIN, IOREDIRECTIONTABLE* pstEntry)
{
    QWORD* pqwData;
    QWORD qwIOAPICBaseAddress;

    qwIOAPICBaseAddress = kGetIOAPICBaseAddressOfISA();

    pqwData = (QWORD*) pstEntry;

    // I/O 리다이렉션 테이블의 상위/하위 4바이트를 읽음
    // I/O 리다이렉션 테이블은 상위/하위 레지스터와 하위 레지스터가 한 쌍이므로 INTIN에 2를 곱하여
    // 해당 I/O 리다이렉션 테이블 레지스터의 인덱스를 계산
    *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOREGISTERSELECTOR) =
        IOAPIC_REGISTERINDEX_HIGHIOREDIRECTIONTABLE + iINTIN * 2;
    *pqwData = *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOWINDOW);
    *pqwData = *pqwData << 32;

    *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOREGISTERSELECTOR) =
        IOAPIC_REGISTERINDEX_LOWIOREDIRECTIONTABLE + iINTIN * 2;
    *pqwData |= *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOWINDOW);
}

void kWriteIOAPICRedirectionTable(int iINTIN, IOREDIRECTIONTABLE* pstEntry)
{
    QWORD* pqwData;
    QWORD qwIOAPICBaseAddress;

    qwIOAPICBaseAddress = kGetIOAPICBaseAddressOfISA();

    pqwData = (QWORD*) pstEntry;

    *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOREGISTERSELECTOR) =
        IOAPIC_REGISTERINDEX_HIGHIOREDIRECTIONTABLE + iINTIN * 2;
    *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOWINDOW) = *pqwData >> 32;

    *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOREGISTERSELECTOR) =
        IOAPIC_REGISTERINDEX_LOWIOREDIRECTIONTABLE + iINTIN * 2;
    *(DWORD*) (qwIOAPICBaseAddress + IOAPIC_REGISTER_IOWINDOW) = *pqwData;
}

void kMaskAllInterruptInIOAPIC(void)
{
    IOREDIRECTIONTABLE stEntry;
    int i;

    for(i = 0; i < IOAPIC_MAXIOREDIRECTIONTABLECOUNT; i++)
    {
        kReadIOAPICRedirectionTable(i, &stEntry);
        stEntry.bInterruptMask = IOAPIC_INTERRUPT_MASK;
        kWriteIOAPICRedirectionTable(i, &stEntry);
    }
}

void kInitializeIORedirectionTable(void)
{
    MPCONFIGURATIONMANAGER* pstMPManager;
    MPCONFIGURATIONTABLEHEADER* pstMPHeader;
    IOINTERRUPTASSIGNMENTENTRY* pstIOAssignmentEntry;
    IOREDIRECTIONTABLE stIORedirectionEntry;
    QWORD qwEntryAddress;
    BYTE bEntryType;
    BYTE bDestination;
    int i;

    // I/O APIC를 관리하는 자료구조 초기화
    kMemSet(&gs_stIOAPICManager, 0, sizeof(gs_stIOAPICManager));

    kGetIOAPICBaseAddressOfISA();

    for(i = 0; i < IOAPIC_MAXIRQTOINTINMAPCOUNT; i++)
    {
        gs_stIOAPICManager.vbIRQToINTINMap[i] = 0xFF;
    }

    // I/O APIC를 마스크하여 인터럽트가 발생하지 않도록 하고 I/O 리다이렉션 테이블 초기화
    kMaskAllInterruptInIOAPIC();

    pstMPManager = kGetMPConfigurationManager();
    pstMPHeader = pstMPManager->pstMPConfigurationTableHeader;
    qwEntryAddress = pstMPManager->qwBaseEntryStartAddress;

    for(i = 0; i < pstMPHeader->wEntryCount; i++)
    {
        bEntryType = *(BYTE*) qwEntryAddress;
        switch(bEntryType)
        {
        case MP_ENTRYTYPE_IOINTERRUPTASSIGNMENT:
            pstIOAssignmentEntry = (IOINTERRUPTASSIGNMENTENTRY*) qwEntryAddress;

            if((pstIOAssignmentEntry->bSourceBUSID == pstMPManager->bISABusID) &&
               (pstIOAssignmentEntry->bInterruptType == MP_INTERRUPTTYPE_INT))
            {
                if(pstIOAssignmentEntry->bSourceBUSIRQ == 0)
                {
                    bDestination = 0xFF;
                }
                else
                {
                    bDestination = 0x00;
                }

                kSetIOAPICRedirectionEntry(&stIORedirectionEntry, bDestination,
                    0x00, IOAPIC_TRIGGERMODE_EDGE | IOAPIC_POLARITY_ACTIVEHIGH |
                    IOAPIC_DESTINATIONMODE_PHYSICALMODE | IOAPIC_DELIVERYMODE_FIXED,
                    PIC_IRQSTARTVECTOR + pstIOAssignmentEntry->bSourceBUSIRQ);

                kWriteIOAPICRedirectionTable(pstIOAssignmentEntry->bDestinationIOAPICINTIN,
                    &stIORedirectionEntry);

                gs_stIOAPICManager.vbIRQToINTINMap[pstIOAssignmentEntry->bSourceBUSIRQ] =
                    pstIOAssignmentEntry->bDestinationIOAPICINTIN;
            }
            qwEntryAddress += sizeof(IOINTERRUPTASSIGNMENTENTRY);
            break;

        case MP_ENTRYTYPE_PROCESSOR:
            qwEntryAddress += sizeof(PROCESSORENTRY);
            break;

        case MP_ENTRYTYPE_BUS:
        case MP_ENTRYTYPE_IOAPIC:
        case MP_ENTRYTYPE_LOCALINTERRUPTASSIGNMENT:
            qwEntryAddress += 8;
            break;
        }
    }
}

void kPrintIRQToINTINMap(void)
{
    int i;

    kPrintf("============ IRQ To I/O APIC INT IN Mapping Table ============\n");

    for(i = 0; i < IOAPIC_MAXIRQTOINTINMAPCOUNT; i++)
    {
        kPrintf("IRQ [%d] -> INTIN [%d]\n", i, gs_stIOAPICManager.vbIRQToINTINMap[i]);
    }
}

void kRoutingIRQToAPICID(int iIRQ, BYTE bAPICID)
{
    int i;
    IOREDIRECTIONTABLE stEntry;

    if(iIRQ > IOAPIC_MAXIRQTOINTINMAPCOUNT)
    {
        return;
    }

    kReadIOAPICRedirectionTable(gs_stIOAPICManager.vbIRQToINTINMap[iIRQ], &stEntry);
    stEntry.bDestination = bAPICID;
    kWriteIOAPICRedirectionTable(gs_stIOAPICManager.vbIRQToINTINMap[iIRQ], &stEntry);
}