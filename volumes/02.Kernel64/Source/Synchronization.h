#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__

#include "Types.h"

#pragma pack(push, 1)

typedef struct kMutexStruct
{
    volatile QWORD qwTaskID;
    volatile DWORD dwLockCount;

    volatile BOOL bLockFlag;

    // 자료구조 크기 8바이트 단위를 맞추기 위한 패딩
    BYTE vbPadding[3];
} MUTEX;

#pragma pack(pop)

BOOL kLockForSystemData(void);
void kUnlockForSystemData(BOOL bInterruptFlag);

void kInitializeMutex(MUTEX* pstMutex);
void kLock(MUTEX* pstMutex);
void kUnlock(MUTEX* pstMutex);

#endif