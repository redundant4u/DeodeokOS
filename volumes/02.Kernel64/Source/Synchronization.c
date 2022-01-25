#include "Synchronization.h"
#include "Utility.h"
#include "Task.h"
#include "AssemblyUtility.h"

#if 0
BOOL kLockForSystemData(void)
{
    return kSetInterruptFlag(FALSE);
}

void kUnlockForSystemData(BOOL bInterruptFlag)
{
    kSetInterruptFlag(bInterruptFlag);
}
#endif

void kInitializeMutex(MUTEX* pstMutex)
{
    pstMutex->bLockFlag = FALSE;
    pstMutex->dwLockCount = 0;
    pstMutex->qwTaskID = TASK_INVALIDID;
}

void kLock(MUTEX* pstMutex)
{
    // 이미 잠겨 있다면 내가 잠갔는지 확인하고 잠근 횟수를 증가 후 종료
    if(kTestAndSet(&(pstMutex->bLockFlag), 0, 1) == FALSE)
    {
        if(pstMutex->qwTaskID == kGetRunningTask()->stLink.qwID)
        {
            pstMutex->dwLockCount++;
            return ;
        }

        // 자신이 아닌 경우 잠긴 것이 해제될 때까지 대기
        while(kTestAndSet(&(pstMutex->bLockFlag), 0, 1) == FALSE)
        {
            kSchedule();
        }
    }

    pstMutex->dwLockCount = 1;
    pstMutex->qwTaskID = kGetRunningTask()->stLink.qwID;
}

void kUnlock(MUTEX* pstMutex)
{
    if((pstMutex->bLockFlag == FALSE) ||
       (pstMutex->qwTaskID != kGetRunningTask()->stLink.qwID))
    {
        return ;
    }

    if((pstMutex->dwLockCount) > 1)
    {
        pstMutex->dwLockCount--;
        return ;
    }

    pstMutex->qwTaskID = TASK_INVALIDID;
    pstMutex->dwLockCount = 0;
    pstMutex->bLockFlag = FALSE;
}

void kInitializeSpinLock(SPINLOCK* pstSpinLock)
{
    pstSpinLock->bLockFlag = FALSE;
    pstSpinLock->dwLockCount = 0;
    pstSpinLock->bAPICID = 0xFF;
    pstSpinLock->bInterruptFlag = FALSE;
}

void kLockForSpinLock(SPINLOCK* pstSpinLock)
{
    BOOL bInterruptFlag;

    bInterruptFlag = kSetInterruptFlag(FALSE);

    if(kTestAndSet(&(pstSpinLock->bLockFlag), 0, 1) == FALSE)
    {
        if(pstSpinLock->bAPICID == kGetAPICID())
        {
            pstSpinLock->dwLockCount++;
            return ;
        }

        while(kTestAndSet(&(pstSpinLock->bLockFlag), 0, 1) == FALSE)
        {
            while(pstSpinLock->bLockFlag == TRUE)
            {
                kPause();
            }
        }
    }

    pstSpinLock->dwLockCount = 1;
    pstSpinLock->bAPICID = kGetAPICID();

    pstSpinLock->bInterruptFlag = bInterruptFlag;
}

void kUnlockForSpinLock(SPINLOCK* pstSpinLock)
{
    BOOL bInterruptFlag;

    bInterruptFlag = kSetInterruptFlag(FALSE);

    if((pstSpinLock->bLockFlag == FALSE) || (pstSpinLock->bAPICID != kGetAPICID()))
    {
        kSetInterruptFlag(bInterruptFlag);
        return ;
    }

    if(pstSpinLock->dwLockCount > 1)
    {
        pstSpinLock->dwLockCount--;
        return ;
    }

    bInterruptFlag = pstSpinLock->bInterruptFlag;

    pstSpinLock->bAPICID = 0xFF;
    pstSpinLock->dwLockCount = 0;
    pstSpinLock->bInterruptFlag = FALSE;
    pstSpinLock->bLockFlag = FALSE;

    kSetInterruptFlag(bInterruptFlag);
}