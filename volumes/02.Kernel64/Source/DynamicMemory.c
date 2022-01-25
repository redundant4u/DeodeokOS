#include "DynamicMemory.h"
#include "Utility.h"
#include "Task.h"

static DYNAMICMEMORY gs_stDynamicMemory;

void kInitializeDynamicMemory(void)
{
    QWORD qwDynamicMemorySize;
    int i, j;
    BYTE* pbCurrentBitmapPosition;
    int iBlockCountOfLevel, iMetaBlockCount;

    qwDynamicMemorySize = kCalculateDynamicMemorySize();
    iMetaBlockCount = kCalculateMetaBlockCount(qwDynamicMemorySize);

    gs_stDynamicMemory.iBlockCountOfSmallestBlock =
        (qwDynamicMemorySize / DYNAMICMEMORY_MIN_SIZE) - iMetaBlockCount;

    // 최대 몇 개의 블록 리스트로 구성되는지를 계산
    for(i = 0; (gs_stDynamicMemory.iBlockCountOfSmallestBlock >> i) > 0; i++)
    {
        ;
    }
    gs_stDynamicMemory.iMaxLevelCount = i;

    // 할당된 메모리가 속한 블록 리스트의 인덱스를 저장하는 영역을 초기화
    gs_stDynamicMemory.pbAllocatedBlockListIndex = (BYTE*) DYNAMICMEMORY_START_ADDRESS;
    for(i = 0; i < gs_stDynamicMemory.iBlockCountOfSmallestBlock; i++)
    {
        gs_stDynamicMemory.pbAllocatedBlockListIndex[i] = 0xFF;
    }

    // 비트맵 자료구조의 시작 주소 지정
    gs_stDynamicMemory.pstBitmapOfLevel = (BITMAP*) (DYNAMICMEMORY_START_ADDRESS +
        (sizeof(BYTE) * gs_stDynamicMemory.iBlockCountOfSmallestBlock));
    // 실제 비트맵의 주소 지정
    pbCurrentBitmapPosition = ((BYTE*) gs_stDynamicMemory.pstBitmapOfLevel) +
        (sizeof(BITMAP) * gs_stDynamicMemory.iMaxLevelCount);

    // 블록 리트스 별로 루프를 돌면서 비트맵을 생성
    for(j = 0; j < gs_stDynamicMemory.iMaxLevelCount; j++)
    {
        gs_stDynamicMemory.pstBitmapOfLevel[j].pbBitmap = pbCurrentBitmapPosition;
        gs_stDynamicMemory.pstBitmapOfLevel[j].qwExistBitCount = 0;
        iBlockCountOfLevel = gs_stDynamicMemory.iBlockCountOfSmallestBlock >> j;

        // 8개 이상 남았으면 상위 블록으로 모두 결합할 수 있으므로 모두 비어 있는 것으로 설정
        for(i = 0; i < iBlockCountOfLevel / 8; i++)
        {
            *pbCurrentBitmapPosition = 0x00;
            pbCurrentBitmapPosition++;
        }

        if((iBlockCountOfLevel % 8) != 0)
        {
            *pbCurrentBitmapPosition = 0x00;

            // 남은 블록이 홀수라면 마지막 한 블록은 결합되어 상위 블록으로 이동하지 못함
            // 따라서 마지막 블록은 현재 블록 리스트에 존재하는 자투리 블록으로 설정
            i = iBlockCountOfLevel % 8;
            if((i % 2) == 1)
            {
                *pbCurrentBitmapPosition |= (DYNAMICMEMORY_EXIST << (i - 1));
                gs_stDynamicMemory.pstBitmapOfLevel[j].qwExistBitCount = 1;
            }
            pbCurrentBitmapPosition++;
        }
    }
    // 블록 풀의 주소와 사용된 메모리 크기 설정
    gs_stDynamicMemory.qwStartAddress = DYNAMICMEMORY_START_ADDRESS + iMetaBlockCount * DYNAMICMEMORY_MIN_SIZE;
    gs_stDynamicMemory.qwEndAddress = kCalculateDynamicMemorySize() + DYNAMICMEMORY_START_ADDRESS;
    gs_stDynamicMemory.qwUsedSize = 0;

    kInitializeSpinLock(&(gs_stDynamicMemory.stSpinLock));
}

static QWORD kCalculateDynamicMemorySize(void)
{
    QWORD qwRAMSize;

    // 3GB 이상 메모리는 비디오 메모리와 프로세서가 사용하는 레지스터가
    // 있으므로 최대 3GB 까지만 사용
    qwRAMSize = (kGetTotalRAMSize() * 1024 * 1024);
    if(qwRAMSize > (QWORD) 3 * 1024 * 1024 * 1024)
    {
        qwRAMSize = (QWORD) 3 * 1024 * 1024 * 1024;
    }

    return qwRAMSize - DYNAMICMEMORY_START_ADDRESS;
}

static int kCalculateMetaBlockCount(QWORD qwDynamicRAMSize)
{
    long lBlockCountOfSmallestBlock;
    DWORD dwSizeOfAllocateBlockListIndex;
    DWORD dwSizeOfBitmap;
    long i;

    lBlockCountOfSmallestBlock = qwDynamicRAMSize / DYNAMICMEMORY_MIN_SIZE;
    dwSizeOfAllocateBlockListIndex = lBlockCountOfSmallestBlock * sizeof(BYTE);

    dwSizeOfBitmap = 0;
    for(i = 0; (lBlockCountOfSmallestBlock >> i) > 0; i++)
    {
        dwSizeOfBitmap += sizeof(BITMAP);
        dwSizeOfBitmap += ((lBlockCountOfSmallestBlock >> i) + 7) / 8;
    }

    return (dwSizeOfAllocateBlockListIndex + dwSizeOfBitmap +
        DYNAMICMEMORY_MIN_SIZE - 1) / DYNAMICMEMORY_MIN_SIZE;
}

void* kAllocateMemory(QWORD qwSize)
{
    QWORD qwAlignedSize;
    QWORD qwReleativeAddress;
    long lOffset;
    int iSizeArrayOffset;
    int iIndexOfBlockList;

    // 메모리 크기를 버디 블록의 크기로 맞춤
    qwAlignedSize = kGetBuddyBlockSize(qwSize);
    if(qwAlignedSize == 0)
    {
        return NULL;
    }

    // 여유 공간이 충분하지 않다면
    if(gs_stDynamicMemory.qwStartAddress + gs_stDynamicMemory.qwUsedSize +
        qwAlignedSize > gs_stDynamicMemory.qwEndAddress)
    {
        return NULL;
    }

    // 버디 블록 할당하고 할당된 블록이 속한 블록 리스트의 인덱스를 반환
    lOffset = kAllocationBuddyBlock(qwAlignedSize);
    if(lOffset == -1)
    {
        return NULL;
    }

    iIndexOfBlockList = kGetBlockListIndexOfMatchSize(qwAlignedSize);

    qwReleativeAddress = qwAlignedSize * lOffset;
    iSizeArrayOffset = qwReleativeAddress / DYNAMICMEMORY_MIN_SIZE;

    gs_stDynamicMemory.pbAllocatedBlockListIndex[iSizeArrayOffset] = (BYTE) iIndexOfBlockList;
    gs_stDynamicMemory.qwUsedSize += qwAlignedSize;

    return (void*) (qwReleativeAddress + gs_stDynamicMemory.qwStartAddress);
}

static QWORD kGetBuddyBlockSize(QWORD qwSize)
{
    long i;
    for(i = 0; i < gs_stDynamicMemory.iMaxLevelCount; i++)
    {
        if(qwSize <= (DYNAMICMEMORY_MIN_SIZE << i))
        {
            return (DYNAMICMEMORY_MIN_SIZE << i);
        }
    }
    return 0;
}

static int kAllocationBuddyBlock(QWORD qwAlignedSize)
{
    int iBlockListIndex, iFreeOffset;
    int i;

    iBlockListIndex = kGetBlockListIndexOfMatchSize(qwAlignedSize);
    if(iBlockListIndex == -1)
    {
        return -1;
    }

    kLockForSpinLock(&(gs_stDynamicMemory.stSpinLock));

    for(i = iBlockListIndex; i < gs_stDynamicMemory.iMaxLevelCount; i++)
    {
        iFreeOffset = kFindFreeBlockInBitmap(i);
        if(iFreeOffset != -1)
        {
            break;
        }
    }

    if(iFreeOffset == -1)
    {
        kUnlockForSpinLock(&(gs_stDynamicMemory.stSpinLock));
        return -1;
    }

    kSetFlagInBitmap(i, iFreeOffset, DYNAMICMEMORY_EMPTY);

    // 상위 블록에서 블록을 찾았다면 상위 블록을 분할
    if(i > iBlockListIndex)
    {
        for(i = i - 1; i >= iBlockListIndex; i--)
        {
            kSetFlagInBitmap(i, iFreeOffset * 2, DYNAMICMEMORY_EMPTY);
            kSetFlagInBitmap(i, iFreeOffset * 2 + 1, DYNAMICMEMORY_EXIST);
            iFreeOffset = iFreeOffset * 2;
        }
    }
    kUnlockForSpinLock(&(gs_stDynamicMemory.stSpinLock));

    return iFreeOffset;
}

static int kGetBlockListIndexOfMatchSize(QWORD qwAlignedSize)
{
    int i;

    for(i = 0; i < gs_stDynamicMemory.iMaxLevelCount; i++)
    {
        if(qwAlignedSize <= (DYNAMICMEMORY_MIN_SIZE << i))
        {
            return i;
        }
    }
    return -1;
}

static int kFindFreeBlockInBitmap(int iBlockListIndex)
{
    int i, iMaxCount;
    BYTE* pbBitmap;
    QWORD* pqwBitmap;

    if(gs_stDynamicMemory.pstBitmapOfLevel[iBlockListIndex].qwExistBitCount == 0)
    {
        return -1;
    }

    iMaxCount = gs_stDynamicMemory.iBlockCountOfSmallestBlock >> iBlockListIndex;
    pbBitmap = gs_stDynamicMemory.pstBitmapOfLevel[iBlockListIndex].pbBitmap;
    for(i = 0; i < iMaxCount;)
    {
        if(((iMaxCount - i) / 64) > 0)
        {
            pqwBitmap = (QWORD*) &(pbBitmap[i / 8]);
            if(*pqwBitmap == 0)
            {
                i += 64;
                continue;
            }
        }

        if((pbBitmap[i / 8] & (DYNAMICMEMORY_EXIST << (i % 8))) != 0)
        {
            return i;
        }
        i++;
    }
    return -1;
}

static void kSetFlagInBitmap(int iBlockListIndex, int iOffset, BYTE bFlag)
{
    BYTE* pbBitmap;
    
    pbBitmap = gs_stDynamicMemory.pstBitmapOfLevel[iBlockListIndex].pbBitmap;
    if(bFlag == DYNAMICMEMORY_EXIST)
    {
        // 해당 위치에 데이터가 비어 있다면 개수 증가
        if((pbBitmap[iOffset / 8] & (0x01 << (iOffset % 8))) == 0)
        {
            gs_stDynamicMemory.pstBitmapOfLevel[iBlockListIndex].qwExistBitCount++;
        }
        pbBitmap[iOffset / 8] |= (0x01 << (iOffset % 8));
    }
    else
    {
        // 해당 위치에 데이터가 있다면 개수 감소
        if((pbBitmap[iOffset / 8] & (0x01 << (iOffset % 8))) != 0)
        {
            gs_stDynamicMemory.pstBitmapOfLevel[iBlockListIndex].qwExistBitCount--;
        }
        pbBitmap[iOffset / 8] &= ~(0x01 << (iOffset % 8));
    }
}

BOOL kFreeMemory(void* pvAddress)
{
    QWORD qwReleativeAddress;
    int iSizeArrayOffset;
    QWORD qwBlockSize;
    int iBlockListIndex;
    int iBitmapOffset;

    if(pvAddress == NULL)
    {
        return FALSE;
    }

    qwReleativeAddress = ((QWORD) pvAddress) - gs_stDynamicMemory.qwStartAddress;
    iSizeArrayOffset = qwReleativeAddress / DYNAMICMEMORY_MIN_SIZE;

    if(gs_stDynamicMemory.pbAllocatedBlockListIndex[iSizeArrayOffset] == 0xFF)
    {
        return FALSE;
    }

    iBlockListIndex = (int) gs_stDynamicMemory.pbAllocatedBlockListIndex[iSizeArrayOffset];
    gs_stDynamicMemory.pbAllocatedBlockListIndex[iSizeArrayOffset] = 0xFF;

    qwBlockSize = DYNAMICMEMORY_MIN_SIZE << iBlockListIndex;

    iBitmapOffset = qwReleativeAddress / qwBlockSize;
    if(kFreeBuddyBlock(iBlockListIndex, iBitmapOffset) == TRUE)
    {
        gs_stDynamicMemory.qwUsedSize -= qwBlockSize;
        return TRUE;
    }

    return FALSE;
}

static BOOL kFreeBuddyBlock(int iBlockListIndex, int iBlockOffset)
{
    int iBuddyBlockOffset;
    int i;
    BOOL bFlag;

    kLockForSpinLock(&(gs_stDynamicMemory.stSpinLock));

    for(i = iBlockListIndex; i < gs_stDynamicMemory.iMaxLevelCount; i++)
    {
        kSetFlagInBitmap(i, iBlockOffset, DYNAMICMEMORY_EXIST);

        // 블록의 오프셋이 짝수면 홀수를 검사하고 홀수면 짝수의 비트맵을
        // 검사하여 인접한 블록이 존재한다면 결합
        if((iBlockOffset % 2) == 0)
        {
            iBuddyBlockOffset = iBlockOffset + 1;
        }
        else
        {
            iBuddyBlockOffset = iBlockOffset - 1;
        }
        bFlag = kGetFlagInBitmap(i, iBuddyBlockOffset);

        if(bFlag == DYNAMICMEMORY_EMPTY)
        {
            break;
        }

        // 인접한 블록 결합
        kSetFlagInBitmap(i, iBuddyBlockOffset, DYNAMICMEMORY_EMPTY);
        kSetFlagInBitmap(i, iBlockOffset, DYNAMICMEMORY_EMPTY);

        iBlockOffset = iBlockOffset / 2;
    }

    kUnlockForSpinLock(&(gs_stDynamicMemory.stSpinLock));
    return TRUE;
}

static BYTE kGetFlagInBitmap(int iBlockListIndex, int iOffset)
{
    BYTE* pbBitmap;

    pbBitmap = gs_stDynamicMemory.pstBitmapOfLevel[iBlockListIndex].pbBitmap;
    if((pbBitmap[iOffset / 8] & (0x01 << (iOffset % 8))) != 0x00)
    {
        return DYNAMICMEMORY_EXIST;
    }

    return DYNAMICMEMORY_EMPTY;
}

void kGetDynamicMemoryInformation(QWORD* pqwDynamicMemeoryStartAddress,
    QWORD* pqwDynamicMemoryTotalSize, QWORD* pqwMetaDataSize, QWORD* pqwUsedMemorySize)
{
    *pqwDynamicMemeoryStartAddress = DYNAMICMEMORY_START_ADDRESS;
    *pqwDynamicMemoryTotalSize = kCalculateDynamicMemorySize();
    *pqwMetaDataSize = kCalculateMetaBlockCount(*pqwDynamicMemoryTotalSize) * DYNAMICMEMORY_MIN_SIZE;
    *pqwUsedMemorySize = gs_stDynamicMemory.qwUsedSize;
}

DYNAMICMEMORY* kGetDynamicMemoryManager(void)
{
    return &gs_stDynamicMemory;
}