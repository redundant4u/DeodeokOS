#ifndef __MPCONFIGURATIONTABLE__
#define __MPCONFIGURATIONTABLE__

#include "Types.h"

// MP 플로팅 포인터의 특성 바이트
#define MP_FLOATINGPOINTER_FEATRUEBYTE1_USEMPTABLE  0x00
#define MP_FLOATINGPOINTER_FEATUREBYTE2_PICMODE     0x80

#define MP_ENTRYTYPE_PROCESSOR                  0
#define MP_ENTRYTYPE_BUS                        1
#define MP_ENTRYTYPE_IOAPIC                     2
#define MP_ENTRYTYPE_IOINTERRUPTASSIGNMENT      3
#define MP_ENTRYTYPE_LOCALINTERRUPTASSIGNMENT   4

#define MP_PROCESSOR_CPUFLAGS_ENABLE    0x01
#define MP_PROCESSOR_CPUFLAGS_BSP       0x02

#define MP_BUS_TYPESTRING_ISA          "ISA"
#define MP_BUS_TYPESTRING_PCI          "PCI"
#define MP_BUS_TYPESTRING_PCMCIA        "PCMCIA"
#define MP_BUS_TYPESTRING_VESALOCALBUS  "VL"

#define MP_INTERRUPTTYPE_INT    0
#define MP_INTERRUPTTYPE_NMI    1
#define MP_INTERRUPTTYPE_SMI    2
#define MP_INTERRUPTTYPE_EXTINT 3

#define MP_INTERRUPT_FLAGS_CONFORMPOLARITY  0x00
#define MP_INTERRUPT_FLAGS_ACTIVEHIGH       0x01
#define MP_INTERRUPT_FLAGS_ACTIVELOW        0x03
#define MP_INTERRUPT_FLAGS_CONFORMTRIGGER   0x00
#define MP_INTERRUPT_FLAGS_EDGETRIGGERER    0x04
#define MP_INTERRUPT_FLAGS_LEVELTRIGGERED   0x0C

#pragma pack(push, 1)

typedef struct kMPFloatingPointerStruct
{
    char vcSignature[4];
    DWORD dwMPConfigurationTableAddress;
    BYTE bLength;
    BYTE bRevision;
    BYTE bCheckSum;
    BYTE vbMPFeatureByte[5];
} MPFLOATINGPOINTER;

typedef struct kMPConfigurationTableHeaderStruct
{
    char vcSignature[4];
    WORD wBaseTableLength;
    BYTE bRevision;
    BYTE bCheckSum;
    char vcOEMIDString[8];
    char vcProductIDString[12];
    DWORD dwOEMTablePointerAddress;
    WORD wOEMTableSize;
    WORD wEntryCount;
    DWORD dwMemoryMapIOAddressOfLocalAPIC;
    WORD wExtendedTableLength;
    BYTE bExtendedTableCheckSum;
    BYTE bReserved;
} MPCONFIGURATIONTABLEHEADER;

typedef struct kProcessorEntryStruct
{
    BYTE bEntryType;
    BYTE bLocalAPICID;
    BYTE bLocalAPICVersion;
    BYTE bCPUFlags;
    BYTE vbCPUSignature[4];
    DWORD dwFeatureFlags;
    DWORD vdwReserved[2];
} PROCESSORENTRY;

typedef struct kBusEntryStruct
{
    BYTE bEntryType;
    BYTE bBusID;
    char vcBusTypeString[6];
} BUSENTRY;

typedef struct kIOAPICEntryStruct
{
    BYTE bEntryType;
    BYTE bIOAPICID;
    BYTE bIOAPICVersion;
    BYTE bIOAPICFlags;
    DWORD dwMemoryMapAddress;
} IOAPICENTRY;

typedef struct kIOInterruptAssignmentEntryStruct
{
    BYTE bEntryType;
    BYTE bInterruptType;
    WORD wInterruptFlags;
    BYTE bSourceBUSID;
    BYTE bSourceBUSIRQ;
    BYTE bDestinationIOAPICID;
    BYTE bDestinationIOAPICINTIN;
} IOINTERRUPTASSIGNMENTENTRY;

typedef struct kLocalInterruptEntryStruct
{
    BYTE bEntryType;
    BYTE bInterruptType;
    WORD wInterruptFlags;
    BYTE bSourceBUSID;
    BYTE bSourceBUSIRQ;
    BYTE bDestinationLocalAPICID;
    BYTE bDestinationLocalAPICINTIN;
} LOCALINTERRUPTASSIGNMENTENTRY;

#pragma pack(pop)

typedef struct kMPConfigurationManagerStruct
{
    MPFLOATINGPOINTER* pstMPFloatingPointer;
    MPCONFIGURATIONTABLEHEADER* pstMPConfigurationTableHeader;
    QWORD qwBaseEntryStartAddress;
    int iProcessorCount;
    BOOL bUsePICMode;
    BYTE bISABusID;
} MPCONFIGURATIONMANAGER;

BOOL kFindMPFloatingPointerAddress(QWORD* pstAddress);
BOOL kAnalysisMPConfigurationTable(void);
MPCONFIGURATIONMANAGER* kGetMPConfigurationManager(void);
void kPrintMPConfigurationTable(void);
int kGetProcessorCount(void);
IOAPICENTRY* kFindIOAPICEntryForISA(void);

#endif