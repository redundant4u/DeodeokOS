#include "Synchronization.h"
#include "Utility.h"
#include "Task.h"

BOOL kLockForSystemData(void)
{
    return kSetInterruptFlag(FALSE);
}

void kUnlockForSystemData(BOOL bInterruptFlag)
{
    kSetInterruptFlag(bInterruptFlag);
}

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