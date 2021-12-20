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

void kCommonExceptionHandler(int iVectorNumber, QWORD qwErrorCode)
{
    char vcBuffer[3] = { 0, };
    
    vcBuffer[0] = '0' + iVectorNumber / 10;
    vcBuffer[1] = '0' + iVectorNumber % 10;

    kPrintStringXY(0, 0, "===========================");
    kPrintStringXY(0, 1, "      Exception Occur      ");
    kPrintStringXY(0, 2, "         Vector:           ");
    kPrintStringXY(18, 2, vcBuffer);
    kPrintStringXY(0, 3, "===========================");

    while(1) ;
}

void kCommonInterruptHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iCommonInterruptCount = 0;
    
    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;
    vcBuffer[8] = '0' + g_iCommonInterruptCount;

    g_iCommonInterruptCount = (g_iCommonInterruptCount + 1) % 10;
    kPrintStringXY(70, 0, vcBuffer);

    kSendEOIToPIC(iVectorNumber - PIC_IRQSTARTVECTOR);

    kSendEOIToLocalAPIC();
}

void kKeyboardHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iKeyboardInterruptCount = 0;
    BYTE bTemp;

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

    kSendEOIToPIC(iVectorNumber - PIC_IRQSTARTVECTOR);

    kSendEOIToLocalAPIC();
}

void kTimerHandler(int iVectorNumber)
{
    char vcBuffer[] = "[INT:  , ]";
    static int g_iTimerInterruptCount = 0;

    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;

    vcBuffer[8] = '0' + g_iTimerInterruptCount;
    g_iTimerInterruptCount = (g_iTimerInterruptCount + 1) % 10;
    kPrintStringXY(70, 0, vcBuffer);

    kSendEOIToPIC(iVectorNumber - PIC_IRQSTARTVECTOR);

    kSendEOIToLocalAPIC();

    g_qwTickCount++;

    kDecreaseProcessorTime();
    if(kIsProcessorTimeExpired() == TRUE)
    {
        kScheduleInInterrupt();
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

    vcBuffer[5] = '0' + iVectorNumber / 10;
    vcBuffer[6] = '0' + iVectorNumber % 10;

    vcBuffer[8] = '0' + g_iHDDInterruptCount;
    g_iHDDInterruptCount = (g_iHDDInterruptCount + 1) % 10;
    kPrintStringXY(10, 0, vcBuffer);

    if(iVectorNumber - PIC_IRQSTARTVECTOR == 14)
    {
        kSetHDDInterruptFlag(TRUE, TRUE);
    }
    else
    {
        kSetHDDInterruptFlag(FALSE, TRUE);
    }

    kSendEOIToPIC(iVectorNumber - PIC_IRQSTARTVECTOR);

    kSendEOIToLocalAPIC();
}