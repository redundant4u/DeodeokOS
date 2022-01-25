#include "Task.h"
#include "Descriptor.h"
#include "Utility.h"

static SCHEDULER gs_stScheduler;
static TCBPOOLMANAGER gs_stTCBPoolManager;

// 태스크 풀과 태스크 관련
static void kInitializeTCBPool(void)
{
    int i;

    kMemSet(&(gs_stTCBPoolManager), 0, sizeof(gs_stTCBPoolManager));

    gs_stTCBPoolManager.pstStartAddress = (TCB*) TASK_TCBPOOLADDRESS;
    kMemSet(TASK_TCBPOOLADDRESS, 0, sizeof(TCB) * TASK_MAXCOUNT);

    for(i = 0; i < TASK_MAXCOUNT; i++)
    {
        gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID = i;
    }

    gs_stTCBPoolManager.iMaxCount = TASK_MAXCOUNT;
    gs_stTCBPoolManager.iAllocatedCount = 1;
}

static TCB* kAllocateTCB(void)
{
    TCB* pstEmptyTCB;
    int i;

    if(gs_stTCBPoolManager.iUseCount == gs_stTCBPoolManager.iMaxCount)
    {
        return NULL;
    }

    for(i = 0; i < gs_stTCBPoolManager.iMaxCount; i++)
    {
        // ID의 상위 32비트가 0이면 할당되지 않은 TCB
        if((gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID >> 32) == 0)
        {
            pstEmptyTCB = &(gs_stTCBPoolManager.pstStartAddress[i]);
            break;
        }
    }

    pstEmptyTCB->stLink.qwID = ((QWORD) gs_stTCBPoolManager.iAllocatedCount << 32) | i;
    gs_stTCBPoolManager.iUseCount++;
    gs_stTCBPoolManager.iAllocatedCount++;

    if(gs_stTCBPoolManager.iAllocatedCount == 0)
    {
        gs_stTCBPoolManager.iAllocatedCount = 1;
    }

    return pstEmptyTCB;
}

static void kFreeTCB(QWORD qwID)
{
    int i;

    i = GETTCBOFFSET(qwID);

    kMemSet(&(gs_stTCBPoolManager.pstStartAddress[i].stContext), 0, sizeof(CONTEXT));
    gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID = i;

    gs_stTCBPoolManager.iUseCount--;
}

TCB* kCreateTask(QWORD qwFlags, void* pvMemoryAddress, QWORD qwMemorySize,
    QWORD qwEntryPointAddress)
{
    TCB* pstTask, *pstProcess;
    void* pvStackAddress;

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));

    pstTask = kAllocateTCB();
    if(pstTask == NULL)
    {
        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
        return NULL;
    }

    pstProcess = kGetProcessByThread(kGetRunningTask());
    if(pstProcess == NULL)
    {
        kFreeTCB(pstTask->stLink.qwID);
        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
        return NULL;
    }

    if(qwFlags & TASK_FLAGS_THREAD)
    {
        pstTask->qwParentProcessID = pstProcess->stLink.qwID;
        pstTask->pvMemoryAddress = pstProcess->pvMemoryAddress;
        pstTask->qwMemorySize = pstProcess->qwMemorySize;

        kAddListToTail(&(pstProcess->stChildThreadList), &(pstTask->stThreadLink));
    }
    else
    {
        pstTask->qwParentProcessID = pstProcess->stLink.qwID;
        pstTask->pvMemoryAddress = pvMemoryAddress;
        pstTask->qwMemorySize = qwMemorySize;
    }

    pstTask->stThreadLink.qwID = pstTask->stLink.qwID;
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

    // 태스크 ID로 스택 주소 계산, 하위 32비트가 스택 풀의 오프셋 역할 수행
    pvStackAddress = (void*) (TASK_STACKPOOLADDRESS + (TASK_STACKSIZE *
        GETTCBOFFSET(pstTask->stLink.qwID)));

    kSetUpTask(pstTask, qwFlags, qwEntryPointAddress, pvStackAddress, TASK_STACKSIZE);

    kInitializeList(&(pstTask->stChildThreadList));

    pstTask->bFPUUsed = FALSE;

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));
    kAddTaskToReadyList(pstTask);
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

    return pstTask;
}

static void kSetUpTask(TCB* pstTCB, QWORD qwFlags, QWORD qwEntryPointAddress,
    void* pvStackAddress, QWORD qwStackSize)
{
    // 콘텍스트 초기화
    kMemSet(pstTCB->stContext.vqRegister, 0, sizeof(pstTCB->stContext.vqRegister));

    // 스택에 관련된 RSP, RBP 레지스터 설정
    pstTCB->stContext.vqRegister[TASK_RSPOFFSET] = (QWORD) pvStackAddress + qwStackSize - 8;
    pstTCB->stContext.vqRegister[TASK_RBPOFFSET] = (QWORD) pvStackAddress + qwStackSize - 8;

    *(QWORD *) ((QWORD) pvStackAddress + qwStackSize - 8) = (QWORD) kExitTask;

    // 세그먼트 설렉터 설정
    pstTCB->stContext.vqRegister[TASK_CSOFFSET] = GDT_KERNELCODESEGMENT;
    pstTCB->stContext.vqRegister[TASK_DSOFFSET] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[TASK_ESOFFSET] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[TASK_FSOFFSET] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[TASK_GSOFFSET] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[TASK_SSOFFSET] = GDT_KERNELDATASEGMENT;

    pstTCB->stContext.vqRegister[TASK_RIPOFFSET] = qwEntryPointAddress;
    pstTCB->stContext.vqRegister[TASK_RFLAGSOFFSET] |= 0x0200;

    pstTCB->pvStackAddress = pvStackAddress;
    pstTCB->qwStackSize = qwStackSize;
    pstTCB->qwFlags = qwFlags;
}

// 스케줄러 관련
void kInitializeScheduler(void)
{
    int i;
    TCB* pstTask;

    kInitializeTCBPool();

    for(i = 0; i < TASK_MAXREADYLISTCOUNT; i++)
    {
        kInitializeList(&(gs_stScheduler.vstReadyList[i]));
        gs_stScheduler.viExecuteCount[i] = 0;
    }
    kInitializeList(&(gs_stScheduler.stWaitList));

    pstTask = kAllocateTCB();
    gs_stScheduler.pstRunningTask = pstTask;
    pstTask->qwFlags = TASK_FLAGS_HIGHEST | TASK_FLAGS_PROCESS | TASK_FLAGS_SYSTEM;
    pstTask->qwParentProcessID = pstTask->stLink.qwID;
    pstTask->pvMemoryAddress = (void*) 0x100000;
    pstTask->qwMemorySize = 0x500000;
    pstTask->pvStackAddress = (void*) 0x600000;
    pstTask->qwStackSize = 0x100000;

    gs_stScheduler.qwSpendProcessorTimeInIdleTask = 0;
    gs_stScheduler.qwProcessorLoad = 0;
    gs_stScheduler.qwLastFPUUsedTaskID = TASK_INVALIDID;

    kInitializeSpinLock(&(gs_stScheduler.stSpinLock));
}

void kSetRunningTask(TCB* pstTask)
{
    kLockForSpinLock(&(gs_stScheduler.stSpinLock));
    gs_stScheduler.pstRunningTask = pstTask;
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
}

TCB* kGetRunningTask(void)
{
    TCB* pstRunningTask;

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));
    pstRunningTask = gs_stScheduler.pstRunningTask;
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

    return pstRunningTask;
}

static TCB* kGetNextTaskToRun(void)
{
    TCB* pstTarget = NULL;
    int iTaskCount, i, j;

    // 큐에 태스크가 있으나 모든 큐의 태스크 1회씩 실행된 경우, 모든 큐가 프로세서를
    // 양보하여 태스크를 선택하지 못할 수 있으니 NULL일 경우 한 번더 수행
    for(j = 0; j < 2; j++)
    {
        // 높은 우선순위에서 낮은 우선순위까지 리스트를 확인하여 스케줄링할 태스크를 선택
        for(i = 0; i < TASK_MAXREADYLISTCOUNT; i++)
        {
            iTaskCount = kGetListCount(&(gs_stScheduler.vstReadyList[i]));

            // 만약 실행한 횟수보다 리스트의 태스크 수가 더 많으면 현재 우선순위의 태스크를 실행함
            if(gs_stScheduler.viExecuteCount[i] < iTaskCount)
            {
                pstTarget = (TCB*) kRemoveListFromHeader(
                    &(gs_stScheduler.vstReadyList[i])
                );
                gs_stScheduler.viExecuteCount[i]++;
                break;
            }
            // 실행한 횟수가 더 많으면 실행 횟수를 초기화하고 다음 우선순위로 양보
            else
            {
                gs_stScheduler.viExecuteCount[i] = 0;
            }
        }

        if(pstTarget != NULL)
        {
            break;
        }
    }

    return pstTarget;
}

static BOOL kAddTaskToReadyList(TCB* pstTask)
{
    BYTE bPriority;

    bPriority = GETPRIORITY(pstTask->qwFlags);
    if(bPriority == TASK_FLAGS_WAIT)
    {
        kAddListToTail(&(gs_stScheduler.stWaitList), pstTask);
        return TRUE;
    }
    else if(bPriority >= TASK_MAXREADYLISTCOUNT)
    {
        return FALSE;
    }

    kAddListToTail(&(gs_stScheduler.vstReadyList[bPriority]), pstTask);
    return TRUE;
}

static TCB* kRemoveTaskFromReadyList(QWORD qwTaskID)
{
    TCB* pstTarget;
    BYTE bPriority;

    if(GETTCBOFFSET(qwTaskID) >= TASK_MAXCOUNT)
    {
        return NULL;
    }

    pstTarget = &(gs_stTCBPoolManager.pstStartAddress[GETTCBOFFSET(qwTaskID)]);
    if(pstTarget->stLink.qwID != qwTaskID)
    {
        return NULL;
    }

    bPriority = GETPRIORITY(pstTarget->qwFlags);
    if(bPriority >= TASK_MAXREADYLISTCOUNT)
    {
        return NULL;
    }

    pstTarget = kRemoveList(&(gs_stScheduler.vstReadyList[bPriority]), qwTaskID);
    return pstTarget;
}

BOOL kChangePriority(QWORD qwTaskID, BYTE bPriority)
{
    TCB* pstTarget;

    if(bPriority > TASK_MAXREADYLISTCOUNT)
    {
        return FALSE;
    }

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));
    pstTarget = gs_stScheduler.pstRunningTask;
    if(pstTarget->stLink.qwID == qwTaskID)
    {
        SETPRIORITY(pstTarget->qwFlags, bPriority);
    }
    // 실행 중인 태스크가 아니면 준비 리스트에서 찾아서 해당 우선순위의 리스트로 이동
    else
    {
        pstTarget = kRemoveTaskFromReadyList(qwTaskID);
        if(pstTarget == NULL)
        {
            pstTarget = kGetTCBInTCBPool(GETTCBOFFSET(qwTaskID));
            if(pstTarget != NULL)
            {
                SETPRIORITY(pstTarget->qwFlags, bPriority);
            }
        }
        else
        {
            SETPRIORITY(pstTarget->qwFlags, bPriority);
            kAddTaskToReadyList(pstTarget);
        }
    }
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

    return TRUE;
}

void kSchedule(void)
{
    TCB* pstRunningTask, *pstNextTask;
    BOOL bPreviousFlag;

    if(kGetReadyTaskCount() < 1)
    {
        return;
    }

    bPreviousFlag = kSetInterruptFlag(FALSE);
    kLockForSpinLock(&(gs_stScheduler.stSpinLock));

    pstNextTask = kGetNextTaskToRun();
    if(pstNextTask == NULL)
    {
        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
        kSetInterruptFlag(bPreviousFlag);
        return;
    }

    // 현재 수행 중인 태스크의 정보를 수정한 뒤 context 전환
    pstRunningTask = gs_stScheduler.pstRunningTask;
    gs_stScheduler.pstRunningTask = pstNextTask;

    // 유휴 대스크에서 전환되었다면 사용한 프로세서 시간 증가
    if((pstRunningTask->qwFlags & TASK_FLAGS_IDLE) == TASK_FLAGS_IDLE)
    {
        gs_stScheduler.qwSpendProcessorTimeInIdleTask +=
            TASK_PROCESSORTIME - gs_stScheduler.iProcessorTime;
    }

    // 다음에 수행할 태스크가 FPU를 쓴 태스크가 아니라면 TS 비트 설정
    if(gs_stScheduler.qwLastFPUUsedTaskID != pstNextTask->stLink.qwID)
    {
        kSetTS();
    }
    else
    {
        kClearTS();
    }

    if(pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK)
    {
        kAddListToTail(&(gs_stScheduler.stWaitList), pstRunningTask);
        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
        kSwitchContext(NULL, &(pstNextTask->stContext));
    }
    else
    {
        kAddTaskToReadyList(pstRunningTask);
        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
        kSwitchContext(&(pstRunningTask->stContext), &(pstNextTask->stContext));
    }

    gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;

    kSetInterruptFlag(bPreviousFlag);
}

BOOL kScheduleInInterrupt(void)
{
    TCB* pstRunningTask, *pstNextTask;
    char* pcContextAddress;

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));

    pstNextTask = kGetNextTaskToRun();
    if(pstNextTask == NULL)
    {
        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
        return FALSE;
    }

    // 태스크 전환 처리
    // 인터럽트 핸들러에서 저장한 context를 다른 context로 덮어쓰는 방법으로 처리
    pcContextAddress = (char*) IST_STARTADDRESS + IST_SIZE - sizeof(CONTEXT);

    // 현재 수행 중인 태스크의 정보를 수정한 뒤 context 전환
    pstRunningTask = gs_stScheduler.pstRunningTask;
    gs_stScheduler.pstRunningTask = pstNextTask;

    if((pstRunningTask->qwFlags & TASK_FLAGS_IDLE) == TASK_FLAGS_IDLE)
    {
        gs_stScheduler.qwSpendProcessorTimeInIdleTask += TASK_PROCESSORTIME;
    }

    // 태스크 종료 플래그가 설정된 경우 context를 저장하지 않고 대기 리스트에만 삽입
    if(pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK)
    {
        kAddListToTail(&(gs_stScheduler.stWaitList), pstRunningTask);
    }
    // 태스크가 종료되지 않으면 IST에 있는 context를 복사하고 현재 대스크를 준비 리스트로 옮김
    else
    {
        kMemCpy(&(pstRunningTask->stContext), pcContextAddress, sizeof(CONTEXT));
        kAddTaskToReadyList(pstRunningTask);
    }
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

    if(gs_stScheduler.qwLastFPUUsedTaskID != pstNextTask->stLink.qwID)
    {
        kSetTS();
    }
    else
    {
        kClearTS();
    }

    kMemCpy(pcContextAddress, &(pstNextTask->stContext), sizeof(CONTEXT));

    return TRUE;
}

void kDecreaseProcessorTime(void)
{
    if(gs_stScheduler.iProcessorTime > 0)
    {
        gs_stScheduler.iProcessorTime--;
    }
}

BOOL kIsProcessorTimeExpired(void)
{
    if(gs_stScheduler.iProcessorTime <= 0)
    {
        return TRUE;
    }
    return FALSE;
}

BOOL kEndTask(QWORD qwTaskID)
{
    TCB* pstTarget;
    BYTE bPriority;

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));

    pstTarget = gs_stScheduler.pstRunningTask;
    if(pstTarget->stLink.qwID == qwTaskID)
    {
        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY(pstTarget->qwFlags, TASK_FLAGS_WAIT);

        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

        kSchedule();

        while(1);
    }
    else
    {
        pstTarget = kRemoveTaskFromReadyList(qwTaskID);
        if(pstTarget == NULL)
        {
            pstTarget = kGetTCBInTCBPool(GETTCBOFFSET(qwTaskID));
            if(pstTarget != NULL)
            {
                pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
                SETPRIORITY(pstTarget->qwFlags, TASK_FLAGS_WAIT);
            }
            kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

            return TRUE;
        }

        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY(pstTarget->qwFlags, TASK_FLAGS_WAIT);
        kAddListToTail(&(gs_stScheduler.stWaitList), pstTarget);
    }
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

    return TRUE;
}

void kExitTask(void)
{
    kEndTask(gs_stScheduler.pstRunningTask->stLink.qwID);
}

int kGetReadyTaskCount(void)
{
    int iTotalCount = 0;
    int i;

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));

    for(i = 0; i < TASK_MAXREADYLISTCOUNT; i++)
    {
        iTotalCount += kGetListCount(&(gs_stScheduler.vstReadyList[i]));
    }
    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
    
    return iTotalCount;
}

int kGetTaskCount(void)
{
    int iTotalCount;

    iTotalCount = kGetReadyTaskCount();

    kLockForSpinLock(&(gs_stScheduler.stSpinLock));

    iTotalCount += kGetListCount(&(gs_stScheduler.stWaitList)) + 1;

    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
    
    return iTotalCount;
}

TCB* kGetTCBInTCBPool(int iOffset)
{
    if((iOffset < -1) && (iOffset > TASK_MAXCOUNT))
    {
        return NULL;
    }

    return &(gs_stTCBPoolManager.pstStartAddress[iOffset]);
}

BOOL kIsTaskExist(QWORD qwID)
{
    TCB* pstTCB;
    
    pstTCB = kGetTCBInTCBPool(GETTCBOFFSET(qwID));
    if((pstTCB == NULL) || (pstTCB->stLink.qwID != qwID))
    {
        return FALSE;
    }
    return TRUE;
}

QWORD kGetProcessorLoad(void)
{
    return gs_stScheduler.qwProcessorLoad;
}

static TCB* kGetProcessByThread(TCB* pstThread)
{
    TCB* pstProcess;

    if(pstThread->qwFlags & TASK_FLAGS_PROCESS)
    {
        return pstThread;
    }

    // TCB풀에서 태스크 자료구조 추출
    pstProcess = kGetTCBInTCBPool(GETTCBOFFSET(pstThread->qwParentProcessID));

    if((pstProcess == NULL) || (pstProcess->stLink.qwID != pstThread->qwParentProcessID))
    {
        return NULL;
    }

    return pstProcess;
}

// 유휴 태스크 관련
void kIdleTask(void)
{
    TCB* pstTask, *pstChildThread, *pstProcess;
    QWORD qwLastMeasureTickCount, qwLastSpendTickInIdleTask;
    QWORD qwCurrentMeasureTickCount, qwCurrentSpendTickInIdleTask;
    int i, iCount;
    QWORD qwTaskID;
    void* pstThreadLink;

    qwLastSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;
    qwLastMeasureTickCount = kGetTickCount();

    while(1)
    {
        qwCurrentMeasureTickCount = kGetTickCount();
        qwCurrentSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;

        if(qwCurrentMeasureTickCount - qwLastMeasureTickCount == 0)
        {
            gs_stScheduler.qwProcessorLoad = 0;
        }
        else
        {
            gs_stScheduler.qwProcessorLoad = 100 -
                (qwCurrentSpendTickInIdleTask - qwLastSpendTickInIdleTask) *
                100 / (qwCurrentMeasureTickCount - qwLastMeasureTickCount);
        }

        qwLastMeasureTickCount = qwCurrentMeasureTickCount;
        qwLastSpendTickInIdleTask = qwCurrentSpendTickInIdleTask;

        kHaltProcessorByLoad();

        // 대기큐에 대기 중인 태스크가 있다면 태스크 종료
        if(kGetListCount(&(gs_stScheduler.stWaitList)) >= 0)
        {
            while(1)
            {
                kLockForSpinLock(&(gs_stScheduler.stSpinLock));
                pstTask = kRemoveListFromHeader(&(gs_stScheduler.stWaitList));
                if(pstTask == NULL)
                {
                    kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
                    break;
                }

                if(pstTask->qwFlags & TASK_FLAGS_PROCESS)
                {
                    // 프로세스를 종료할 때 자식 스레드가 존재하면 스레드를 모두 종료하고
                    // 자식 스레드 삽입
                    iCount = kGetListCount(&(pstTask->stChildThreadList));
                    for(i = 0; i < iCount; i++)
                    {
                        pstThreadLink = (TCB*) kRemoveListFromHeader(&(pstTask->stChildThreadList));
                        if(pstThreadLink == NULL)
                        {
                            break;
                        }

                        pstChildThread = GETTCBFROMTHREADLINK(pstThreadLink);

                        kAddListToTail(&(pstTask->stChildThreadList), &(pstChildThread->stThreadLink));

                        kEndTask(pstChildThread->stLink.qwID);
                    }

                    // 자식 스레드가 남아있다면 자식 스레드가 다 종료될 때까지
                    // 기다려야 하므로 대기 리스트에 삽입
                    if(kGetListCount(&(pstTask->stChildThreadList)) > 0)
                    {
                        kAddListToTail(&(gs_stScheduler.stWaitList), pstTask);

                        kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));
                        continue;
                    }
                    else
                    {

                    }
                }
                else if(pstTask->qwFlags & TASK_FLAGS_THREAD)
                {
                    // 스레드라면 프로세스의 자식 스레드 리스트에서 제거
                    pstProcess = kGetProcessByThread(pstTask);
                    if(pstProcess != NULL)
                    {
                        kRemoveList(&(pstProcess->stChildThreadList), pstTask->stLink.qwID);
                    }
                }
                
                qwTaskID = pstTask->stLink.qwID;
                kFreeTCB(qwTaskID);
                kUnlockForSpinLock(&(gs_stScheduler.stSpinLock));

                kPrintf("IDLE: Task ID[0x%q] is completely ended.\n", qwTaskID);
            }
        }
        kSchedule();
    }
}

void kHaltProcessorByLoad(void)
{
    if(gs_stScheduler.qwProcessorLoad < 40)
    {
        kHlt();
        kHlt();
        kHlt();
    }
    else if(gs_stScheduler.qwProcessorLoad < 80)
    {
        kHlt();
        kHlt();
    }
    else if(gs_stScheduler.qwProcessorLoad < 95)
    {
        kHlt();
    }
}

// FPU 관련
QWORD kGetLastFPUUsedTaskID(void)
{
    return gs_stScheduler.qwLastFPUUsedTaskID;
}

void kSetLastFPUUsedTaskID(QWORD qwTaskID)
{
    gs_stScheduler.qwLastFPUUsedTaskID = qwTaskID;
}