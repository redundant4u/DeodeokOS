#include "FileSystem.h"
#include "HardDisk.h"
#include "DynamicMemory.h"
#include "Task.h"
#include "Utility.h"
#include "CacheManager.h"
#include "RAMDisk.h"

static FILESYSTEMMANAGER gs_stFileSystemManager;
static BYTE gs_vbTempBuffer[FILESYSTEM_SECTORSPERCLUSTER * 512];

fReadHDDInformation gs_pfReadHDDInformation = NULL;
fReadHDDSector gs_pfReadHDDSector = NULL;
fWriteHDDSector gs_pfWriteHDDSector = NULL;

BOOL kInitializeFileSystem(void)
{
    BOOL bCacheEnable = FALSE;

    kMemSet(&gs_stFileSystemManager, 0, sizeof(gs_stFileSystemManager));
    kInitializeMutex(&(gs_stFileSystemManager.stMutex));

    // 초기화가 성공하면 함수 포인터를 하드 디스크용 함수로 설정
    if(kInitializeHDD() == TRUE)
    {
        gs_pfReadHDDInformation = kReadHDDInformation;
        gs_pfReadHDDSector = kReadHDDSector;
        gs_pfWriteHDDSector = kWriteHDDSector;

        bCacheEnable = TRUE;
    }
    else if(kInitializeRDD(RDD_TOTALSECTORCOUNT) == TRUE)
    {
        gs_pfReadHDDInformation = kReadRDDInformation;
        gs_pfReadHDDSector = kReadRDDSector;
        gs_pfWriteHDDSector = kWriteRDDSector;

        if(kFormat() == FALSE)
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    if(kMount() == FALSE)
    {
        return FALSE;
    }

    gs_stFileSystemManager.pstHandlePool =(FILE*) kAllocateMemory(FILESYSTEM_HANDLE_MAXCOUNT * sizeof(FILE));

    if(gs_stFileSystemManager.bMounted == NULL)
    {
        gs_stFileSystemManager.bMounted = FALSE;
        return FALSE;
    }

    kMemSet(gs_stFileSystemManager.pstHandlePool, 0, FILESYSTEM_HANDLE_MAXCOUNT * sizeof(FILE));

    if(bCacheEnable == TRUE)
    {
        gs_stFileSystemManager.bCacheEnable = kInitializeCacheManager();
    }

    return TRUE;
}

// 하드 디스크의 MBR을 읽어서 MINT 파일 시스템인지 확인
BOOL kMount(void)
{
    MBR* pstMBR;

    kLock(&(gs_stFileSystemManager.stMutex));

    if(gs_pfReadHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return FALSE;
    }

    pstMBR = (MBR*) gs_vbTempBuffer;
    if(pstMBR->dwSignature != FILESYSTEM_SIGNATURE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return FALSE;
    }

    gs_stFileSystemManager.bMounted = TRUE;

    gs_stFileSystemManager.dwReservedSectorCount = pstMBR->dwReservedSectorCount;
    gs_stFileSystemManager.dwClusterLinkAreaStartAddress = pstMBR->dwReservedSectorCount + 1;
    gs_stFileSystemManager.dwClusterLinkAreaSize = pstMBR->dwClusterLinkSectorCount;
    gs_stFileSystemManager.dwDataAreaStartAddress = pstMBR->dwReservedSectorCount + pstMBR->dwClusterLinkSectorCount + 1;
    gs_stFileSystemManager.dwTotalClusterCount = pstMBR->dwTotalClusterCount;

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return TRUE;
}

BOOL kFormat(void)
{
    HDDINFORMATION* pstHDD;
    MBR* pstMBR;
    DWORD dwTotalSectorCount, dwRemainSectorCount;
    DWORD dwMaxClusterCount, dwClusterCount;
    DWORD dwClusterLinkSectorCount;
    DWORD i;

    kLock(&(gs_stFileSystemManager.stMutex));

    pstHDD = (HDDINFORMATION*) gs_vbTempBuffer;
    if(gs_pfReadHDDInformation(TRUE, TRUE, pstHDD) == FALSE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return FALSE;
    }
    dwTotalSectorCount = pstHDD->dwTotalSectors;

    dwMaxClusterCount = dwTotalSectorCount / FILESYSTEM_SECTORSPERCLUSTER;

    dwClusterLinkSectorCount = (dwMaxClusterCount + 127) / 128;

    dwRemainSectorCount = dwTotalSectorCount - dwClusterLinkSectorCount - 1;
    dwClusterCount = dwRemainSectorCount / FILESYSTEM_SECTORSPERCLUSTER;

    dwClusterLinkSectorCount = (dwClusterCount + 127) / 128;

    // MBR 영역 읽기
    if(gs_pfReadHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return FALSE;
    }

    pstMBR = (MBR*) gs_vbTempBuffer;
    kMemSet(pstMBR->vstPartition, 0, sizeof(pstMBR->vstPartition));
    pstMBR->dwSignature = FILESYSTEM_SIGNATURE;
    pstMBR->dwReservedSectorCount = 0;
    pstMBR->dwClusterLinkSectorCount = dwClusterLinkSectorCount;
    pstMBR->dwTotalClusterCount = dwClusterCount;

    if(gs_pfWriteHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return FALSE;
    }

    kMemSet(gs_vbTempBuffer, 0, 512);
    for(i = 0; i < (dwClusterLinkSectorCount + FILESYSTEM_SECTORSPERCLUSTER); i++)
    {
        if(i == 0)
        {
            ((DWORD*) (gs_vbTempBuffer))[0] = FILESYSTEM_LASTCLUSTER;
        }
        else
        {
            ((DWORD*) (gs_vbTempBuffer))[0] = FILESYSTEM_FREECLUSTER;
        }

        if(gs_pfWriteHDDSector(TRUE, TRUE, i+1, 1, gs_vbTempBuffer) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return FALSE;
        }
    }

    if(gs_stFileSystemManager.bCacheEnable == TRUE)
    {
        kDiscardAllCacheBuffer(CACHE_CLUSTERLINKTABLEAREA);
        kDiscardAllCacheBuffer(CACHE_DATAAREA);
    }

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return TRUE;
}

BOOL kGetHDDInformation(HDDINFORMATION* pstInformation)
{
    BOOL bResult;

    kLock(&(gs_stFileSystemManager.stMutex));
    bResult = gs_pfReadHDDInformation(TRUE, TRUE, pstInformation);
    kUnlock(&(gs_stFileSystemManager.stMutex));

    return bResult;
}

// 저수준 함수
static BOOL kReadClusterLinkTable(DWORD dwOffset, BYTE* pbBuffer)
{
    if(gs_stFileSystemManager.bCacheEnable == FALSE)
    {
        kInternalReadClusterLinkTableWithoutCache(dwOffset, pbBuffer);
    }
    else
    {
        kInternalReadClusterLinkTableWithCache(dwOffset, pbBuffer);
    }
}

static BOOL kInternalReadClusterLinkTableWithoutCache(DWORD dwOffset, BYTE* pbBuffer)
{
    return gs_pfReadHDDSector(TRUE, TRUE, dwOffset + gs_stFileSystemManager.dwClusterLinkAreaStartAddress, 1, pbBuffer);
}

static BOOL kInternalReadClusterLinkTableWithCache(DWORD dwOffset, BYTE* pbBuffer)
{
    CACHEBUFFER* pstCacheBuffer;
    pstCacheBuffer = kFindCacheBuffer(CACHE_CLUSTERLINKTABLEAREA, dwOffset);

    if(pstCacheBuffer != NULL)
    {
        kMemCpy(pbBuffer, pstCacheBuffer->pbBuffer, 512);
        return TRUE;
    }

    // 캐시 버퍼에 없다면 하드디스크에서 직접 읽음
    if(kInternalReadClusterLinkTableWithoutCache(dwOffset, pbBuffer) == FALSE)
    {
        return FALSE;
    }

    // 캐시를 할당받아서 캐시 내용 갱신
    pstCacheBuffer = kAllocateCacheBufferWithFlush(CACHE_CLUSTERLINKTABLEAREA);
    if(pstCacheBuffer == NULL)
    {
        return FALSE;
    }

    kMemCpy(pstCacheBuffer->pbBuffer, pbBuffer, 512);
    pstCacheBuffer->dwTag = dwOffset;

    pstCacheBuffer->bChanged = FALSE;
    return TRUE;
}

static CACHEBUFFER* kAllocateCacheBufferWithFlush(int iCacheTableIndex)
{
    CACHEBUFFER* pstCacheBuffer;

    pstCacheBuffer = kAllocateCacheBuffer(iCacheTableIndex);
    if(pstCacheBuffer == NULL)
    {
        pstCacheBuffer = kGetVictimInCacheBuffer(iCacheTableIndex);

        if(pstCacheBuffer == NULL)
        {
            kPrintf("Cache Allocate Fail\n");
            return NULL;
        }
        if(pstCacheBuffer->bChanged == TRUE)
        {
            switch(iCacheTableIndex)
            {
            case CACHE_CLUSTERLINKTABLEAREA:
                if(kInternalWriteClusterLinkTableWithoutCache(pstCacheBuffer->dwTag, pstCacheBuffer->pbBuffer) == FALSE)
                {
                    kPrintf("Cache Buffer Write Fail\n");
                    return NULL;
                }
                break;
            
            case CACHE_DATAAREA:
                if(kInternalWriteClusterWithoutCache(pstCacheBuffer->dwTag, pstCacheBuffer->pbBuffer) == FALSE)
                {
                    kPrintf("Cache Buffer Write Fail\n");
                    return NULL;
                }
                break;

            default:
                kPrintf("kAllocateCacheBufferWithFlush Fail\n");
                return NULL;
                break;
            }
        }
    }
    return pstCacheBuffer;
}

static BOOL kWriteClusterLinkTable(DWORD dwOffset, BYTE* pbBuffer)
{
    if(gs_stFileSystemManager.bCacheEnable == FALSE)
    {
        return kInternalWriteClusterLinkTableWithoutCache(dwOffset, pbBuffer);
    }
    else
    {
        return kInternalWriteClusterLinkTableWithCache(dwOffset, pbBuffer);
    }
}

static BOOL kInternalWriteClusterLinkTableWithoutCache(DWORD dwOffset, BYTE* pbBuffer)
{
    return gs_pfWriteHDDSector(TRUE, TRUE, dwOffset + gs_stFileSystemManager.dwClusterLinkAreaStartAddress, 1, pbBuffer);
}

static BOOL kInternalWriteClusterLinkTableWithCache(DWORD dwOffset, BYTE* pbBuffer)
{
    CACHEBUFFER* pstCacheBuffer;
    pstCacheBuffer = kFindCacheBuffer(CACHE_CLUSTERLINKTABLEAREA, dwOffset);

    if(pstCacheBuffer != NULL)
    {
        kMemCpy(pstCacheBuffer->pbBuffer, pbBuffer, 512);
        pstCacheBuffer->bChanged = TRUE;

        return TRUE;
    }

    pstCacheBuffer = kAllocateCacheBufferWithFlush(CACHE_CLUSTERLINKTABLEAREA);
    if(pstCacheBuffer == NULL)
    {
        return FALSE;
    }

    kMemCpy(pstCacheBuffer->pbBuffer, pbBuffer, 512);
    pstCacheBuffer->dwTag = dwOffset;

    pstCacheBuffer->bChanged = TRUE;

    return TRUE;
}

static BOOL kReadCluster(DWORD dwOffset, BYTE* pbBuffer)
{
    if(gs_stFileSystemManager.bCacheEnable == FALSE)
    {
        kInternalReadClusterWithoutCache(dwOffset, pbBuffer);
    }
    else
    {
        kInternalReadClusterWithCache(dwOffset, pbBuffer);
    }
}

static BOOL kInternalReadClusterWithoutCache(DWORD dwOffset, BYTE* pbBuffer)
{
    return  gs_pfReadHDDSector(TRUE, TRUE, (dwOffset * FILESYSTEM_SECTORSPERCLUSTER) +
        gs_stFileSystemManager.dwDataAreaStartAddress, FILESYSTEM_SECTORSPERCLUSTER, pbBuffer);
}

static BOOL kInternalReadClusterWithCache(DWORD dwOffset, BYTE* pbBuffer)
{
    CACHEBUFFER* pstCacheBuffer;

    pstCacheBuffer = kFindCacheBuffer(CACHE_DATAAREA, dwOffset);

    if(pstCacheBuffer != NULL)
    {
        kMemCpy(pbBuffer, pstCacheBuffer->pbBuffer, FILESYSTEM_CLUSTERSIZE);
        return TRUE;
    }

    if(kInternalReadClusterWithoutCache(dwOffset, pbBuffer) == FALSE)
    {
        return FALSE;
    }

    pstCacheBuffer = kAllocateCacheBufferWithFlush(CACHE_DATAAREA);
    if(pstCacheBuffer == NULL)
    {
        return FALSE;
    }

    kMemCpy(pstCacheBuffer->pbBuffer, pbBuffer, FILESYSTEM_CLUSTERSIZE);
    pstCacheBuffer->dwTag = dwOffset;

    pstCacheBuffer->bChanged = FALSE;
    return TRUE;
}

static BOOL kWriteCluster(DWORD dwOffset, BYTE* pbBuffer)
{
    if(gs_stFileSystemManager.bCacheEnable == FALSE)
    {
        kInternalWriteClusterWithoutCache(dwOffset, pbBuffer);
    }
    else
    {
        kInternalWriteClusterWithCache(dwOffset, pbBuffer);
    }
}

static BOOL kInternalWriteClusterWithoutCache(DWORD dwOffset, BYTE* pbBuffer)
{
    return gs_pfWriteHDDSector(TRUE, TRUE, (dwOffset * FILESYSTEM_SECTORSPERCLUSTER) +
        gs_stFileSystemManager.dwDataAreaStartAddress, FILESYSTEM_SECTORSPERCLUSTER, pbBuffer);
}

static BOOL kInternalWriteClusterWithCache(DWORD dwOffset, BYTE* pbBuffer)
{
    CACHEBUFFER* pstCacheBuffer;

    pstCacheBuffer = kFindCacheBuffer(CACHE_DATAAREA, dwOffset);

    if(pstCacheBuffer != NULL)
    {
        kMemCpy(pstCacheBuffer->pbBuffer, pbBuffer, FILESYSTEM_CLUSTERSIZE);
        pstCacheBuffer->bChanged = TRUE;

        return TRUE;
    }

    pstCacheBuffer = kAllocateCacheBufferWithFlush(CACHE_DATAAREA);
    if(pstCacheBuffer == NULL)
    {
        return FALSE;
    }

    kMemCpy(pstCacheBuffer->pbBuffer, pbBuffer, FILESYSTEM_CLUSTERSIZE);
    pstCacheBuffer->dwTag = dwOffset;

    pstCacheBuffer->bChanged = TRUE;

    return TRUE;
}

static DWORD kFindFreeCluster(void)
{
    DWORD dwLinkCountInSector;
    DWORD dwLastSectorOffset, dwCurrentSectorOffset;
    DWORD i, j;

    if(gs_stFileSystemManager.bMounted == FALSE)
    {
        return FILESYSTEM_LASTCLUSTER;
    }

    dwLastSectorOffset = gs_stFileSystemManager.dwLastAllocatedClusterLinkSectorOffset;

    for(i = 0; i < gs_stFileSystemManager.dwClusterLinkAreaSize; i++)
    {
        if((dwLastSectorOffset + i) == (gs_stFileSystemManager.dwClusterLinkAreaSize - 1))
        {
            dwLinkCountInSector = gs_stFileSystemManager.dwTotalClusterCount % 128;
        }
        else
        {
            dwLinkCountInSector = 128;
        }

        dwCurrentSectorOffset = (dwLastSectorOffset + i) % gs_stFileSystemManager.dwClusterLinkAreaSize;
        if(kReadClusterLinkTable(dwCurrentSectorOffset, gs_vbTempBuffer) == FALSE)
        {
            return FILESYSTEM_LASTCLUSTER;
        }

        for(j = 0; j < dwLinkCountInSector; j++)
        {
            if(((DWORD*) gs_vbTempBuffer)[j] == FILESYSTEM_FREECLUSTER)
            {
                break;
            }
        }

        if(j != dwLinkCountInSector)
        {
            gs_stFileSystemManager.dwLastAllocatedClusterLinkSectorOffset = dwCurrentSectorOffset;
            return (dwCurrentSectorOffset * 128) + j;
        }
    }

    return FILESYSTEM_LASTCLUSTER;
}

static BOOL kSetClusterLinkData(DWORD dwClusterIndex, DWORD dwData)
{
    DWORD dwSectorOffset;

    if(gs_stFileSystemManager.bMounted == FALSE)
    {
        return FALSE;
    }

    dwSectorOffset = dwClusterIndex / 128;

    if(kReadClusterLinkTable(dwSectorOffset, gs_vbTempBuffer) == FALSE)
    {
        return FALSE;
    }

    ((DWORD*) gs_vbTempBuffer)[dwClusterIndex % 128] = dwData;

    if(kWriteClusterLinkTable(dwSectorOffset, gs_vbTempBuffer) == FALSE)
    {
        return FALSE;
    }

    return TRUE;
}

static BOOL kGetClusterLinkData(DWORD dwClusterIndex, DWORD* pdwData)
{
    DWORD dwSectorOffset;

    if(gs_stFileSystemManager.bMounted == FALSE)
    {
        return FALSE;
    }

    dwSectorOffset = dwClusterIndex / 128;

    if(dwSectorOffset > gs_stFileSystemManager.dwClusterLinkAreaSize)
    {
        return FALSE;
    }

    if(kReadClusterLinkTable(dwSectorOffset, gs_vbTempBuffer) == FALSE)
    {
        return FALSE;
    }

    *pdwData = ((DWORD*) gs_vbTempBuffer)[dwClusterIndex % 128];
    return TRUE;
}

static int kFindFreeDirectoryEntry(void)
{
    DIRECTORYENTRY* pstEntry;
    int i;

    if(gs_stFileSystemManager.bMounted == FALSE)
    {
        return -1;
    }

    // 루트 디렉터리를 읽음
    if(kReadCluster(0, gs_vbTempBuffer) == FALSE)
    {
        return -1;
    }

    pstEntry = (DIRECTORYENTRY*) gs_vbTempBuffer;
    for(i = 0; i < FILESYSTEM_MAXDIRECTORYENTRYCOUNT; i++)
    {
        if(pstEntry[i].dwStartClusterIndex == 0)
        {
            return i;
        }
    }
    return -1;   
}

static BOOL kSetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry)
{
    DIRECTORYENTRY* pstRootEntry;

    if((gs_stFileSystemManager.bMounted == FALSE) ||
        (iIndex < 0) || (iIndex >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
    {
        return FALSE;
    }

    if(kReadCluster(0, gs_vbTempBuffer) == FALSE)
    {
        return FALSE;
    }

    pstRootEntry = (DIRECTORYENTRY*) gs_vbTempBuffer;
    kMemCpy(pstRootEntry + iIndex, pstEntry, sizeof(DIRECTORYENTRY));

    if(kWriteCluster(0, gs_vbTempBuffer) == FALSE)
    {
        return FALSE;
    }

    return TRUE;
}

static BOOL kGetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry)
{
    DIRECTORYENTRY* pstRootEntry;

    if((gs_stFileSystemManager.bMounted == FALSE) ||
        (iIndex < 0) || (iIndex >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
    {
        return FALSE;
    }

    if(kReadCluster(0, gs_vbTempBuffer) == FALSE)
    {
        return FALSE;
    }

    pstRootEntry = (DIRECTORYENTRY*) gs_vbTempBuffer;
    kMemCpy(pstEntry, pstRootEntry + iIndex, sizeof(DIRECTORYENTRY));
    return TRUE;
}

static int kFindDirectoryEntry(const char* pcFileName, DIRECTORYENTRY* pstEntry)
{
    DIRECTORYENTRY* pstRootEntry;
    int i;
    int iLength;

    if(gs_stFileSystemManager.bMounted == FALSE)
    {
        return -1;
    }

    if(kReadCluster(0, gs_vbTempBuffer) == FALSE)
    {
        return -1;
    }

    iLength = kStrLen(pcFileName);
    pstRootEntry = (DIRECTORYENTRY*) gs_vbTempBuffer;
    for(i = 0; i < FILESYSTEM_MAXDIRECTORYENTRYCOUNT; i++)
    {
        if(kMemCmp(pstRootEntry[i].vcFileName, pcFileName, iLength) == 0)
        {
            kMemCpy(pstEntry, pstRootEntry + i, sizeof(DIRECTORYENTRY));
            return i;
        }
    }
    return -1;
}

void kGetFileSystemInformation(FILESYSTEMMANAGER* pstManager)
{
    kMemCpy(pstManager, &gs_stFileSystemManager, sizeof(gs_stFileSystemManager));
}

// 고수준 함수
static void* kAllocateFileDirectoryHandle(void)
{
    int i;
    FILE* pstFile;

    pstFile = gs_stFileSystemManager.pstHandlePool;
    for(i = 0; i < FILESYSTEM_HANDLE_MAXCOUNT; i++)
    {
        if(pstFile->bType == FILESYSTEM_TYPE_FREE)
        {
            pstFile->bType = FILESYSTEM_TYPE_FILE;
            return pstFile;
        }

        pstFile++;
    }
    return NULL;
}

static void kFreeFileDirectoryHandle(FILE* pstFile)
{
    kMemSet(pstFile, 0, sizeof(FILE));
    pstFile->bType = FILESYSTEM_TYPE_FREE;
}

static BOOL kCreateFile(const char* pcFileName, DIRECTORYENTRY* pstEntry, int* piDirectoryEntryIndex)
{
    DWORD dwCluster;

    dwCluster = kFindFreeCluster();
    if((dwCluster == FILESYSTEM_LASTCLUSTER) ||
        (kSetClusterLinkData(dwCluster, FILESYSTEM_LASTCLUSTER) == FALSE))
    {
        return FALSE;
    }

    *piDirectoryEntryIndex = kFindFreeDirectoryEntry();
    if(*piDirectoryEntryIndex == -1)
    {
        kSetClusterLinkData(dwCluster, FILESYSTEM_FREECLUSTER);
        return FALSE;
    }

    kMemCpy(pstEntry->vcFileName, pcFileName, kStrLen(pcFileName) + 1);
    pstEntry->dwStartClusterIndex = dwCluster;
    pstEntry->dwFileSize = 0;

    if(kSetDirectoryEntryData(*piDirectoryEntryIndex, pstEntry) == FALSE)
    {
        kSetClusterLinkData(dwCluster, FILESYSTEM_FREECLUSTER);
        return FALSE;
    }
    return TRUE;
}

static BOOL kFreeClusterUntilEnd(DWORD dwClusterIndex)
{
    DWORD dwCurrentClusterIndex;
    DWORD dwNextClusterIndex;

    dwCurrentClusterIndex = dwClusterIndex;

    while(dwCurrentClusterIndex != FILESYSTEM_LASTCLUSTER)
    {
        if(kGetClusterLinkData(dwCurrentClusterIndex, &dwNextClusterIndex) == FALSE)
        {
            return FALSE;
        }

        if(kSetClusterLinkData(dwCurrentClusterIndex, FILESYSTEM_FREECLUSTER) == FALSE)
        {
            return FALSE;
        }

        dwCurrentClusterIndex = dwNextClusterIndex;
    }

    return TRUE;
}

FILE* kOpenFile(const char* pcFileName, const char* pcMode)
{
    DIRECTORYENTRY stEntry;
    int iDirectoryEntryOffset;
    int iFileNameLength;
    DWORD dwSecondCluster;
    FILE* pstFile;

    iFileNameLength = kStrLen(pcFileName);
    if((iFileNameLength > (sizeof(stEntry.vcFileName) - 1)) || (iFileNameLength == 0))
    {
        return NULL;
    }

    kLock(&(gs_stFileSystemManager.stMutex));

    // 파일이 먼저 존재하는지 확인하고, 없다면 옵션을 보고 파일을 생성
    iDirectoryEntryOffset = kFindDirectoryEntry(pcFileName, &stEntry);
    if(iDirectoryEntryOffset == -1)
    {
        if(pcMode[0] == 'r')
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return NULL;
        }

        if(kCreateFile(pcFileName, &stEntry, &iDirectoryEntryOffset) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return NULL;
        }
    }
    // 파일의 내용을 비워야 하는 옵션이면 파일에 연결된 클러스터를 모두 제거하고 파일 크기를 0으로 설정
    else if(pcMode[0] == 'w')
    {
        if(kGetClusterLinkData(stEntry.dwStartClusterIndex, &dwSecondCluster) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return NULL;
        }

        if(kSetClusterLinkData(stEntry.dwStartClusterIndex, FILESYSTEM_LASTCLUSTER) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return NULL;
        }

        if(kFreeClusterUntilEnd(dwSecondCluster) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return NULL;
        }

        stEntry.dwFileSize = 0;
        if(kSetDirectoryEntryData(iDirectoryEntryOffset, &stEntry) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return NULL;
        }
    }

    // 파일 핸들을 할당받아 데이터를 설정한 후 반환
    pstFile = kAllocateFileDirectoryHandle();
    if(pstFile == NULL)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return NULL;
    }

    pstFile->bType = FILESYSTEM_TYPE_FILE;
    pstFile->stFileHandle.iDirectoryEntryOffset = iDirectoryEntryOffset;
    pstFile->stFileHandle.dwFileSize = stEntry.dwFileSize;
    pstFile->stFileHandle.dwStartClusterIndex = stEntry.dwStartClusterIndex;
    pstFile->stFileHandle.dwCurrentClusterIndex = stEntry.dwStartClusterIndex;
    pstFile->stFileHandle.dwPreviousClusterIndex = stEntry.dwStartClusterIndex;
    pstFile->stFileHandle.dwCurrentOffset = 0;

    if(pcMode[0] == 'a')
    {
        kSeekFile(pstFile, 0, FILESYSTEM_SEEK_END);
    }

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return pstFile;
}

DWORD kReadFile(void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile)
{
    DWORD dwTotalCount;
    DWORD dwReadCount;
    DWORD dwOffsetInCluster;
    DWORD dwCopySize;
    FILEHANDLE* pstFileHandle;
    DWORD dwNextClusterIndex;

    if((pstFile == NULL) || (pstFile->bType != FILESYSTEM_TYPE_FILE))
    {
        return 0;
    }
    pstFileHandle = &(pstFile->stFileHandle);

    if((pstFileHandle->dwCurrentOffset == pstFileHandle->dwFileSize) ||
        (pstFileHandle->dwCurrentClusterIndex == FILESYSTEM_LASTCLUSTER))
    {
        return 0;
    }

    dwTotalCount = MIN(dwSize * dwCount, pstFileHandle->dwFileSize - pstFileHandle->dwCurrentOffset);
    kLock(&(gs_stFileSystemManager.stMutex));

    dwReadCount = 0;
    while(dwReadCount != dwTotalCount)
    {
        // 클러스트를 읽어서 버퍼에 복사
        if(kReadCluster(pstFileHandle->dwCurrentClusterIndex, gs_vbTempBuffer) == FALSE)
        {
            break;
        }

        dwOffsetInCluster = pstFileHandle->dwCurrentOffset % FILESYSTEM_CLUSTERSIZE;
        dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE - dwOffsetInCluster, dwTotalCount - dwReadCount);
        kMemCpy((char*) pvBuffer + dwReadCount, gs_vbTempBuffer + dwOffsetInCluster, dwCopySize);

        dwReadCount += dwCopySize;
        pstFileHandle->dwCurrentOffset += dwCopySize;

        // 현재 클러스터를 끝까지 다 읽었으면 다음 클러스터로 이동
        if((pstFileHandle->dwCurrentOffset % FILESYSTEM_CLUSTERSIZE) == 0)
        {
            if(kGetClusterLinkData(pstFileHandle->dwCurrentClusterIndex, &dwNextClusterIndex) == FALSE)
            {
                break;
            }

            pstFileHandle->dwPreviousClusterIndex = pstFileHandle->dwCurrentClusterIndex;
            pstFileHandle->dwCurrentClusterIndex = dwNextClusterIndex;
        }
    }
    kUnlock(&(gs_stFileSystemManager.stMutex));

    return (dwReadCount / dwSize);
}

static BOOL kUpdateDirectoryEntry(FILEHANDLE* pstFileHandle)
{
    DIRECTORYENTRY stEntry;

    if((pstFileHandle == NULL) || kGetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry) == FALSE)
    {
        return FALSE;
    }

    stEntry.dwFileSize = pstFileHandle->dwFileSize;
    stEntry.dwStartClusterIndex = pstFileHandle->dwStartClusterIndex;

    if(kSetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry) == FALSE)
    {
        return FALSE;
    }
    return TRUE;
}

DWORD kWriteFile(const void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile)
{
    DWORD dwWriteCount;
    DWORD dwTotalCount;
    DWORD dwOffsetInCluster;
    DWORD dwCopySize;
    DWORD dwAllocatedClusterIndex;
    DWORD dwNextClutserIndex;
    FILEHANDLE* pstFileHandle;

    if((pstFile == NULL) || (pstFile->bType != FILESYSTEM_TYPE_FILE))
    {
        return 0;
    }
    pstFileHandle = &(pstFile->stFileHandle);

    dwTotalCount = dwSize * dwCount;

    kLock(&(gs_stFileSystemManager.stMutex));

    dwWriteCount = 0;
    while(dwWriteCount != dwTotalCount)
    {
        // 현재 클러스터가 파일의 끝이면 클러스터를 할당하여 연결
        if(pstFileHandle->dwCurrentClusterIndex == FILESYSTEM_LASTCLUSTER)
        {
            dwAllocatedClusterIndex = kFindFreeCluster();
            if(dwAllocatedClusterIndex == FILESYSTEM_LASTCLUSTER)
            {
                break;
            }

            // 검색된 클러스터를 마지막 클러스터를 설정
            if(kSetClusterLinkData(dwAllocatedClusterIndex, FILESYSTEM_LASTCLUSTER) == FALSE)
            {
                break;
            }

            // 파일의 마지막 클러스터에 할당된 클러스터를 연결
            if(kSetClusterLinkData(pstFileHandle->dwPreviousClusterIndex, dwAllocatedClusterIndex) == FALSE)
            {
                kSetClusterLinkData(dwAllocatedClusterIndex, FILESYSTEM_FREECLUSTER);
                break;
            }

            pstFileHandle->dwCurrentClusterIndex = dwAllocatedClusterIndex;
            kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_LASTCLUSTER);
        }
        // 한 클러스터를 채우지 못하면 클러스터를 읽어서 임시 클러스터 버퍼로 복사
        else if(((pstFileHandle->dwCurrentOffset % FILESYSTEM_CLUSTERSIZE) != 0) ||
                ((dwTotalCount - dwWriteCount) < FILESYSTEM_CLUSTERSIZE))
        {
            // 전체 클러스터를 덮어쓰는 경우가 아니면 부분만 덮어써야 하므로 현재 클러스터를 읽음
            if(kReadCluster(pstFileHandle->dwCurrentClusterIndex, gs_vbTempBuffer) == FALSE)
            {
                break;
            }
        }

        dwOffsetInCluster = pstFileHandle->dwCurrentOffset % FILESYSTEM_CLUSTERSIZE;
        dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE - dwOffsetInCluster, dwTotalCount - dwWriteCount);
        kMemCpy(gs_vbTempBuffer + dwOffsetInCluster, (char*) pvBuffer + dwWriteCount, dwCopySize);

        if(kWriteCluster(pstFileHandle->dwCurrentClusterIndex, gs_vbTempBuffer) == FALSE)
        {
            break;
        }

        dwWriteCount += dwCopySize;
        pstFileHandle->dwCurrentOffset += dwCopySize;

        // 현재 클러스터의 끝까지 다 썼으면 다음 클러스터로 이동
        if((pstFileHandle->dwCurrentOffset % FILESYSTEM_CLUSTERSIZE) == 0)
        {
            // 현재 클러스터의 링크 데이터로 다음 클러스터를 얻음
            if(kGetClusterLinkData(pstFileHandle->dwCurrentClusterIndex, &dwNextClutserIndex) == FALSE)
            {
                break;
            }

            pstFileHandle->dwPreviousClusterIndex = pstFileHandle->dwCurrentClusterIndex;
            pstFileHandle->dwCurrentClusterIndex = dwNextClutserIndex;
        }
    }

    // 파일 크기가 변했다면 루트 디렉터리에 있는 디렉터리 엔트리 정보를 갱신
    if(pstFileHandle->dwFileSize < pstFileHandle->dwCurrentOffset)
    {
        pstFileHandle->dwFileSize = pstFileHandle->dwCurrentOffset;
        kUpdateDirectoryEntry(pstFileHandle);
    }

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return (dwWriteCount / dwSize);
}

BOOL kWriteZero(FILE* pstFile, DWORD dwCount)
{
    BYTE* pbBuffer;
    DWORD dwRemainCount;
    DWORD dwWriteCount;

    if(pstFile == NULL)
    {
        return FALSE;
    }

    pbBuffer = (BYTE*) kAllocateMemory(FILESYSTEM_CLUSTERSIZE);
    if(pbBuffer == NULL)
    {
        return FALSE;
    }

    kMemSet(pbBuffer, 0, FILESYSTEM_CLUSTERSIZE);
    dwRemainCount = dwCount;

    // 클러스터 단위로 반복해서 쓰기 수행
    while(dwRemainCount != 0)
    {
        dwWriteCount = MIN(dwRemainCount, FILESYSTEM_CLUSTERSIZE);
        if(kWriteFile(pbBuffer, 1, dwWriteCount, pstFile) != dwWriteCount)
        {
            kFreeMemory(pbBuffer);
            return FALSE;
        }
        dwRemainCount -= dwWriteCount;
    }
    kFreeMemory(pbBuffer);
    return TRUE;
}

int kSeekFile(FILE* pstFile, int iOffset, int iOrigin)
{
    DWORD dwRealOffset;
    DWORD dwClusterOffsetToMove;
    DWORD dwCurrentClusterOffset;
    DWORD dwLastClusterOffset;
    DWORD dwMoveCount;
    DWORD i;
    DWORD dwStartClusterIndex;
    DWORD dwPreviousClusterIndex;
    DWORD dwCurrentClusterIndex;
    FILEHANDLE* pstFileHandle;

    if((pstFile == NULL) || (pstFile->bType != FILESYSTEM_TYPE_FILE))
    {
        return 0;
    }
    pstFileHandle = &(pstFile->stFileHandle);

    // Origin과 Offset을 조합하여 파일 시작을 기준으로 파일 포인털르 옮겨야 할 위치 계산
    switch(iOrigin)
    {
        case FILESYSTEM_SEEK_SET:
            if(iOffset <= 0)
            {
                dwRealOffset = 0;
            }
            else
            {
                dwRealOffset = iOffset;
            }
            break;
        case FILESYSTEM_SEEK_CUR:
            if((iOffset < 0) && (pstFileHandle->dwCurrentOffset <= (DWORD) -iOffset))
            {
                dwRealOffset = 0;
            }
            else
            {
                dwRealOffset = pstFileHandle->dwCurrentOffset + iOffset;
            }
            break;
        case FILESYSTEM_SEEK_END:
            if((iOffset < 0) && (pstFileHandle->dwFileSize <= (DWORD) -iOffset))
            {
                dwRealOffset = 0;
            }
            else
            {
                dwRealOffset = pstFileHandle->dwFileSize + iOffset;
            }
            break;
    }

    // 파일을 구성하는 클러스터의 개수와 현재 파일 포인터의 위치를 고려하여
    // 옮겨질 파일 포인터가 위치하는 클러스터까지 클러스터 링크를 검색
    dwLastClusterOffset = pstFileHandle->dwFileSize / FILESYSTEM_CLUSTERSIZE;
    dwClusterOffsetToMove = dwRealOffset / FILESYSTEM_CLUSTERSIZE;
    dwCurrentClusterOffset = pstFileHandle->dwCurrentOffset / FILESYSTEM_CLUSTERSIZE;

    if(dwLastClusterOffset < dwClusterOffsetToMove)
    {
        dwMoveCount = dwLastClusterOffset - dwCurrentClusterOffset;
        dwStartClusterIndex = pstFileHandle->dwCurrentClusterIndex;
    }
    else if(dwCurrentClusterOffset <= dwClusterOffsetToMove)
    {
        dwMoveCount = dwClusterOffsetToMove - dwCurrentClusterOffset;
        dwStartClusterIndex = pstFileHandle->dwCurrentClusterIndex;
    }
    else
    {
        dwMoveCount = dwClusterOffsetToMove;
        dwStartClusterIndex = pstFileHandle->dwStartClusterIndex;
    }

    kLock(&(gs_stFileSystemManager.stMutex));

    dwCurrentClusterIndex = dwStartClusterIndex;
    for(i = 0; i < dwMoveCount; i++)
    {
        dwPreviousClusterIndex = dwCurrentClusterIndex;

        if(kGetClusterLinkData(dwPreviousClusterIndex, &dwCurrentClusterIndex) == FALSE)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return -1;
        }
    }

    if(dwMoveCount > 0)
    {
        pstFileHandle->dwPreviousClusterIndex = dwPreviousClusterIndex;
        pstFileHandle->dwCurrentClusterIndex = dwCurrentClusterIndex;
    }
    else if(dwStartClusterIndex == pstFileHandle->dwStartClusterIndex)
    {
        pstFileHandle->dwPreviousClusterIndex = pstFileHandle->dwStartClusterIndex;
        pstFileHandle->dwCurrentClusterIndex = pstFileHandle->dwStartClusterIndex;
    }

    // 파일 포인터를 갱신하고 파일 오프셋이 파일의 크기를 넘었다면 나머지 부분을
    // 0으로 채워서 파일의 크기를 늘림
    if(dwLastClusterOffset < dwClusterOffsetToMove)
    {
        pstFileHandle->dwCurrentOffset = pstFileHandle->dwFileSize;
        kUnlock(&(gs_stFileSystemManager.stMutex));

        if(kWriteZero(pstFile, dwRealOffset - pstFileHandle->dwFileSize) == FALSE)
        {
            return 0;
        }
    }
    pstFileHandle->dwCurrentOffset = dwRealOffset;
    kUnlock(&(gs_stFileSystemManager.stMutex));

    return 0;
}

int kCloseFile(FILE* pstFile)
{
    if((pstFile == NULL) || (pstFile->bType != FILESYSTEM_TYPE_FILE))
    {
        return -1;
    }

    kFreeFileDirectoryHandle(pstFile);
    return 0;
}

BOOL kIsFileOpened(const DIRECTORYENTRY* pstEntry)
{
    int i;
    FILE* pstFile;

    pstFile = gs_stFileSystemManager.pstHandlePool;
    for(i = 0; i < FILESYSTEM_HANDLE_MAXCOUNT; i++)
    {
        if((pstFile[i].bType == FILESYSTEM_TYPE_FILE) &&
            (pstFile[i].stFileHandle.dwStartClusterIndex == pstEntry->dwStartClusterIndex))
        {
            return TRUE;
        }
    }
    return FALSE;
}

int kRemoveFile(const char* pcFileName)
{
    DIRECTORYENTRY stEntry;
    int iDirectoryEntryOffset;
    int iFileNameLength;

    iFileNameLength = kStrLen(pcFileName);
    if((iFileNameLength > (sizeof(stEntry.vcFileName) - 1)) ||
        (iFileNameLength == 0))
    {
        return NULL;
    }

    kLock(&(gs_stFileSystemManager.stMutex));

    iDirectoryEntryOffset = kFindDirectoryEntry(pcFileName, &stEntry);
    if(iDirectoryEntryOffset == -1)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return -1;
    }

    if(kIsFileOpened(&stEntry) == TRUE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return -1;
    }

    // 파일을 구성하는 클러스터를 모두 해제
    if(kFreeClusterUntilEnd(stEntry.dwStartClusterIndex) == FALSE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return -1;
    }

    kMemSet(&stEntry, 0, sizeof(stEntry));
    if(kSetDirectoryEntryData(iDirectoryEntryOffset, &stEntry) == FALSE)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return -1;
    }

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return 0;
}

DIR* kOpenDirectory(const char* pcDirectoryName)
{
    DIR* pstDirectory;
    DIRECTORYENTRY* pstDirectoryBuffer;

    kLock(&(gs_stFileSystemManager.stMutex));

    pstDirectory = kAllocateFileDirectoryHandle();
    if(pstDirectory == NULL)
    {
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return NULL;
    }

    pstDirectoryBuffer = (DIRECTORYENTRY*) kAllocateMemory(FILESYSTEM_CLUSTERSIZE);
    if(pstDirectory == NULL)
    {
        kFreeFileDirectoryHandle(pstDirectory);
        kUnlock(&(gs_stFileSystemManager.stMutex));
        return NULL;
    }

    if(kReadCluster(0, (BYTE*) pstDirectoryBuffer) == FALSE)
    {
        kFreeFileDirectoryHandle(pstDirectory);
        kFreeMemory(pstDirectoryBuffer);

        kUnlock(&(gs_stFileSystemManager.stMutex));
        return NULL;
    }

    pstDirectory->bType = FILESYSTEM_TYPE_DIRECTORY;
    pstDirectory->stDirectoryHandle.iCurrentOffset = 0;
    pstDirectory->stDirectoryHandle.pstDirectoryBuffer = pstDirectoryBuffer;

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return pstDirectory;
}

struct kDirectoryEntryStruct* kReadDirectory(DIR* pstDirectory)
{
    DIRECTORYHANDLE* pstDirectoryHandle;
    DIRECTORYENTRY* pstEntry;

    if((pstDirectory == NULL) || (pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
    {
        return NULL;
    }
    pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

    if((pstDirectoryHandle->iCurrentOffset < 0) || (pstDirectoryHandle->iCurrentOffset >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
    {
        return NULL;
    }

    kLock(&(gs_stFileSystemManager.stMutex));

    pstEntry = pstDirectoryHandle->pstDirectoryBuffer;
    while(pstDirectoryHandle->iCurrentOffset < FILESYSTEM_MAXDIRECTORYENTRYCOUNT)
    {
        if(pstEntry[pstDirectoryHandle->iCurrentOffset].dwStartClusterIndex != 0)
        {
            kUnlock(&(gs_stFileSystemManager.stMutex));
            return &(pstEntry[pstDirectoryHandle->iCurrentOffset++]);
        }

        pstDirectoryHandle->iCurrentOffset++;
    }

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return NULL;
}

void kRewindDirectory(DIR* pstDirectory)
{
    DIRECTORYHANDLE* pstDirectoryHandle;

    if((pstDirectory == NULL) || (pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
    {
        return;
    }
    pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

    kLock(&(gs_stFileSystemManager.stMutex));

    pstDirectoryHandle->iCurrentOffset = 0;

    kUnlock(&(gs_stFileSystemManager.stMutex));
}

int kCloseDirectory(DIR* pstDirectory)
{
    DIRECTORYHANDLE* pstDirectoryHandle;

    if((pstDirectory == NULL) || (pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
    {
        return -1;
    }
    pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

    kLock(&(gs_stFileSystemManager.stMutex));

    kFreeMemory(pstDirectoryHandle->pstDirectoryBuffer);
    kFreeFileDirectoryHandle(pstDirectory);

    kUnlock(&(gs_stFileSystemManager.stMutex));

    return 0;
}

BOOL kFlushFileSystemCache(void)
{
    CACHEBUFFER* pstCacheBuffer;
    int iCacheCount;
    int i;

    if(gs_stFileSystemManager.bCacheEnable == FALSE)
    {
        return TRUE;
    }

    kLock(&(gs_stFileSystemManager.stMutex));

   // 클러스터 링크 테이블 영역의 캐시 정보를 얻어서 내용이 변한 캐시 버퍼를 모두 디스크에 씀
    kGetCacheBufferAndCount(CACHE_CLUSTERLINKTABLEAREA, &pstCacheBuffer, &iCacheCount);
    for(i = 0; i < iCacheCount; i++)
    {
        if(pstCacheBuffer[i].bChanged == TRUE)
        {
            if(kInternalWriteClusterLinkTableWithoutCache(
                pstCacheBuffer[i].dwTag, pstCacheBuffer[i].pbBuffer) == FALSE)
            {
                return FALSE;
            }
            pstCacheBuffer[i].bChanged = FALSE;
        }
    }

    // 데이터 영역의 캐시 정보를 얻어서 내용이 변한 캐시 버퍼를 모두 디스크에 씀
    kGetCacheBufferAndCount(CACHE_DATAAREA, &pstCacheBuffer, &iCacheCount);
    for(i = 0; i < iCacheCount; i++)
    {
        if(pstCacheBuffer[i].bChanged == TRUE)
        {
            if(kInternalWriteClusterWithoutCache(
                pstCacheBuffer[i].dwTag, pstCacheBuffer[i].pbBuffer) == FALSE)
            {
                return FALSE;
            }
            pstCacheBuffer[i].bChanged = FALSE;
        }
    }

    kUnlock(&(gs_stFileSystemManager.stMutex));
    return TRUE;
}