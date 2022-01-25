#include "InterruptHandler.h"
#include "PIC.h"
#include "Keyboard.h"
#include "Console.h"
#include "Utility.h"
#include "Task.h"
#include "Descriptor.h"
#include "AssemblyUtility.h"
#include "HardDisk.h"
#include "LocalAPIC.h"
#include "IOAPIC.h"

static INTERRUPTMANAGER gs_stInterruptManager;

void kInitializeHandler(void)
{
    kMemSet(&gs_stInterruptManager, 0, sizeof(gs_stInterruptManager));
}

void kSetSymmetricIOMode(BOOL bSymmetricIOMode)
{
    gs_stInterruptManager.bSymmetricIOMode = bSymmetricIOMode;
}

void kSetInterruptLoadBalancing(BOOL bUseLoadBalancing)
{
    gs_stInterruptManager.bUseLoadBalancing = bUseLoadBalancing;
}

void kIncreaseInterruptCount(int iIRQ)
{
    gs_stInterruptManager.vvqwCoreInterruptCount[kGetAPICID()][iIRQ]++;
}

void kSendEOI(int iIRQ)
{
    if(gs_stInterruptManager.bSymmetricIOMode == FALSE)
    {
        kSendEOIToPIC(iIRQ);
    }
    else
    {
        kSendEOIToLocalAPIC();
    }
}

INTERRUPTMANAGER* kGetInterruptManager(void)
{
    return &gs_stInterruptManager;
}

void kProcessLoadBalancing(int iIRQ)
{
    QWORD qwMinCount = 0xFFFFFFFFFFFFFFFF;
    int iMinCountCoreIndex;
    int iCoreCount;
    int i;
    BOOL bResetCount = FALSE;
    BYTE bAPICID;

    bAPICID = kGetAPICID();

    // 부하 분산 기능이 꺼져 있거나, 부한 분산을 처리할 시점이 아니면 할 필요가 없음
    if((gs_stInterruptManager.vvqwCoreInterruptCount[bAPICID][iIRQ] == 0) ||
        ((gs_stInterruptManager.vvqwCoreInterruptCount[bAPICID][iIRQ] % INTERRUPT_LOADBALANCINGDIVIDOR) != 0) ||
        (gs_stInterruptManager.bUseLoadBalancing == FALSE))
    {
        return;
    }

    // 코어의 개수를 구해서 루프를 수행하여 인터럽트 처리 횟수가 가장 작은 코어를 선택
    iMinCountCoreIndex = 0;
    iCoreCount = kGetProcessorCount();
    for(i = 0; i < iCoreCount; i++)
    {
        if((gs_stInterruptManager.vvqwCoreInterruptCount[i][iIRQ] < qwMinCount))
        {
            qwMinCount = gs_stInterruptManager.vvqwCoreInterruptCount[i][iIRQ];
            iMinCountCoreIndex = i;
        }
        // 전체 카운트가 거의 최댓값에 근접했다면 나중에 카운트를 모두 0으로 설정
        else if(gs_stInterruptManager.vvqwCoreInterruptCount[i][iIRQ] >= 0xFFFFFFFFFFFFFFFE)
        {
            bResetCount = TRUE;
        }
    }

    kRoutingIRQToAPICID(iIRQ, iMinCountCoreIndex);

    if(bResetCount == TRUE)
    {
        for(i = 0; i < iCoreCount; i++)
        {
            gs_stInterruptManager.vvqwCoreInterruptCount[i][iIRQ] = 0;
        }
    }
}

void kCommonExceptionHandler(int iVectorNumber, QWORD qwErrorCode)
{
    char vcBuffer[3] = { 0, };
    
    kPrintStringXY(0, 0, "=========================================");
    kPrintStringXY(0, 1, "               Exception Occur           ");
    kPrintStringXY(0, 2, "         Vector:           Core ID:      ");
    vcBuffer[0] = '0' + iVectorNumber / 10;
    vcBuffer[1] = '0' + iVectorNumber % 10;
    kPrintStringXY(18, 2, vcBuffer);
    kSPrintf(vcBuffer, "0x%X", kGetAPICID());
    kPrintStringXY(36, 2, vcBuffer);
    kPrintStringXY(0, 3, "=========================================");

    while(1) ;
}

void kCommonInterruptHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iCommonInterruptCount = 0;
    int iIRQ;
    
    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;
    vcBuffer[8] = '0' + g_iCommonInterruptCount;

    g_iCommonInterruptCount = (g_iCommonInterruptCount + 1) % 10;
    kPrintStringXY(70, 0, vcBuffer);

    iIRQ = iVectorNumber - PIC_IRQSTARTVECTOR;

    kSendEOI(iIRQ);

    kIncreaseInterruptCount(iIRQ);

    kProcessLoadBalancing(iIRQ);
}

void kKeyboardHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iKeyboardInterruptCount = 0;
    BYTE bTemp;
    int iIRQ;

    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;
    vcBuffer[8] = '0' + g_iKeyboardInterruptCount;

    g_iKeyboardInterruptCount = (g_iKeyboardInterruptCount + 1) % 10;
    kPrintStringXY(0, 0, vcBuffer);

    if(kIsOutputBufferFull() == TRUE)
    {
        bTemp = kGetKeyboardScanCode();
        kConvertScanCodeAndPutQueue(bTemp);
    }

    iIRQ = iVectorNumber - PIC_IRQSTARTVECTOR;

    kSendEOI(iIRQ);

    kIncreaseInterruptCount(iIRQ);

    kProcessLoadBalancing(iIRQ);
}

void kTimerHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iTimerInterruptCount = 0;
    int iIRQ;

    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;

    vcBuffer[8] = '0' + g_iTimerInterruptCount;
    g_iTimerInterruptCount = (g_iTimerInterruptCount + 1) % 10;
    kPrintStringXY(70, 0, vcBuffer);

    iIRQ = iVectorNumber - PIC_IRQSTARTVECTOR;

    kSendEOI(iIRQ);

    kIncreaseInterruptCount(iIRQ);

    if(kGetAPICID() == 0)
    {
        g_qwTickCount++;

        kDecreaseProcessorTime();

        if(kIsProcessorTimeExpired() == TRUE)
        {
            kScheduleInInterrupt();
        }
    }
}

void kDeviceNotAvailableHandler(int iVectorNumber)
{
    TCB* pstFPUTask, *pstCurrentTask;
    QWORD qwLastFPUTaskID;

    // =======================================================
    char vcBuffer[] = "[EXC:  , ]";
    static int g_iFPUInterruptCount = 0;

    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;
    vcBuffer[8] = '0' + g_iFPUInterruptCount;
    g_iFPUInterruptCount = (g_iFPUInterruptCount + 1) % 10;
    kPrintStringXY(0, 0, vcBuffer);
    // =======================================================

    kClearTS();

    qwLastFPUTaskID = kGetLastFPUUsedTaskID();
    pstCurrentTask = kGetRunningTask();

    if(qwLastFPUTaskID == pstCurrentTask->stLink.qwID)
    {
        return ;
    }
    // FPU를 사용한 태스크가 있으면 FPU 상태를 저장
    else if(qwLastFPUTaskID != TASK_INVALIDID)
    {
        pstFPUTask = kGetTCBInTCBPool(GETTCBOFFSET(qwLastFPUTaskID));
        if((pstFPUTask != NULL) && (pstFPUTask->stLink.qwID == qwLastFPUTaskID))
        {
            kSaveFPUContext(pstFPUTask->vqwFPUContext);
        }
    }

    if(pstCurrentTask->bFPUUsed == FALSE)
    {
        kInitializeFPU();
        pstCurrentTask->bFPUUsed = TRUE;
    }
    else
    {
        kLoadFPUContext(pstCurrentTask->vqwFPUContext);
    }

    kSetLastFPUUsedTaskID(pstCurrentTask->stLink.qwID);
}

void kHDDHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iHDDInterruptCount = 0;
    BYTE bTemp;
    int iIRQ;

    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;

    vcBuffer[8] = '0' + g_iHDDInterruptCount;
    g_iHDDInterruptCount = (g_iHDDInterruptCount + 1) % 10;
    kPrintStringXY(10, 0, vcBuffer);

    iIRQ = iVectorNumber - PIC_IRQSTARTVECTOR;

    if(iIRQ == 14)
    {
        kSetHDDInterruptFlag(TRUE, TRUE);
    }
    else
    {
        kSetHDDInterruptFlag(FALSE, TRUE);
    }

    kSendEOI(iIRQ);

    kIncreaseInterruptCount(iIRQ);

    kProcessLoadBalancing(iIRQ);
}