#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "Types.h"
#include "Synchronization.h"
#include "HardDisk.h"
#include "CacheManager.h"

#define FILESYSTEM_SIGNATURE                0x7E38CF10
#define FILESYSTEM_SECTORSPERCLUSTER        8
#define FILESYSTEM_LASTCLUSTER              0xFFFFFFFF
#define FILESYSTEM_FREECLUSTER              0x00
#define FILESYSTEM_MAXDIRECTORYENTRYCOUNT   ((FILESYSTEM_SECTORSPERCLUSTER * 512) / sizeof(DIRECTORYENTRY))
#define FILESYSTEM_CLUSTERSIZE              (FILESYSTEM_SECTORSPERCLUSTER * 512)

#define FILESYSTEM_HANDLE_MAXCOUNT          (TASK_MAXCOUNT * 3)

#define FILESYSTEM_MAXFILENAMELENGTH        24

#define FILESYSTEM_TYPE_FREE                0
#define FILESYSTEM_TYPE_FILE                1
#define FILESYSTEM_TYPE_DIRECTORY           2

#define FILESYSTEM_SEEK_SET                 0
#define FILESYSTEM_SEEK_CUR                 1
#define FILESYSTEM_SEEK_END                 2

typedef BOOL(*fReadHDDInformation) (BOOL bPrimary, BOOL bMaster, HDDINFORMATION* pstHDDInformation);
typedef int(*fReadHDDSector) (BOOL bPrimary, BOOL bMaster, DWORD dwLBA, int iSectorCount, char* pcBuffer);
typedef int(*fWriteHDDSector) (BOOL bPrimary, BOOL bMaster, DWORD dwLBA, int iSectorCount, char* pcBuffer);

#define fopen       kOpenFile
#define fread       kReadFile
#define fwrite      kWriteFile
#define fseek       kSeekFile
#define fclose      kCloseFile
#define remove      kRemoveFile
#define opendir     kOpenDirectory
#define readdir     kReadDirectory
#define rewinddir   kRewindDirectory
#define closedir    kCloseDirectory

#define SEEK_SET    FILESYSTEM_SEEK_SET
#define SEEK_CUR    FILESYSTEM_SEEK_CUR
#define SEEK_END    FILESYSTEM_SEEK_END

#define size_t      DWORD
#define dirent      kDirectoryEntryStruct
#define d_name      vcFileName

#pragma pack(push, 1)

typedef struct kPartitionStruct
{
    BYTE bBootableFlag;
    BYTE vbStartingCHSAdress[3];
    BYTE bPartitionType;
    BYTE vbEndingCHSAddress[3];
    DWORD dwStartingLBAAddress;
    DWORD dwSizeInSector;
} PARTITION;

typedef struct kMBRStruct
{
    BYTE vbBootCode[430];

    DWORD dwSignature;
    DWORD dwReservedSectorCount;
    DWORD dwClusterLinkSectorCount;
    DWORD dwTotalClusterCount;

    PARTITION vstPartition[4];

    BYTE vbBootLoaderSignature[2];
} MBR;

typedef struct kDirectoryEntryStruct
{
    char vcFileName[FILESYSTEM_MAXFILENAMELENGTH];
    DWORD dwFileSize;
    DWORD dwStartClusterIndex;
} DIRECTORYENTRY;

typedef struct kFileHandleStruct
{
    int iDirectoryEntryOffset;
    DWORD dwFileSize;
    DWORD dwStartClusterIndex;
    DWORD dwCurrentClusterIndex;
    DWORD dwPreviousClusterIndex;
    DWORD dwCurrentOffset;
} FILEHANDLE;

typedef struct kDirectoryHandleStruct
{
    DIRECTORYENTRY* pstDirectoryBuffer;
    int iCurrentOffset;
} DIRECTORYHANDLE;

typedef struct kFileDirectoryHandleStruct
{
    BYTE bType;
    union
    {
        FILEHANDLE stFileHandle;
        DIRECTORYHANDLE stDirectoryHandle;
    };
} FILE, DIR;

typedef struct kFileSystemManagerStruct
{
    BOOL bMounted;

    DWORD dwReservedSectorCount;
    DWORD dwClusterLinkAreaStartAddress;
    DWORD dwClusterLinkAreaSize;
    DWORD dwDataAreaStartAddress;
    DWORD dwTotalClusterCount;

    DWORD dwLastAllocatedClusterLinkSectorOffset;

    MUTEX stMutex;

    FILE* pstHandlePool;

    BOOL bCacheEnable;
} FILESYSTEMMANAGER;

BOOL kInitializeFileSystem(void);
BOOL kFormat(void);
BOOL kMount(void);
BOOL kGetHDDInformation(HDDINFORMATION* pstInformation);

// 저수준 함수
static BOOL kReadClusterLinkTable(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kWriteClusterLinkTable(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kReadCluster(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kWriteCluster(DWORD dwOffset, BYTE* pbBuffer);
static DWORD kFindFreeCluster(void);
static BOOL kSetClusterLinkData(DWORD dwClusterIndex, DWORD dwData);
static BOOL kGetClusterLinkData(DWORD dwClusterIndex, DWORD* pdwData);
static int kFindFreeDirectoryEntry(void);
static BOOL kSetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry);
static BOOL kGetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry);
static int kFindDirectoryEntry(const char* pcFileName, DIRECTORYENTRY* pstEntry);
void kGetFileSystemInformation(FILESYSTEMMANAGER* pstManager);

// 캐시 관련 함수
static BOOL kInternalReadClusterLinkTableWithoutCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalReadClusterLinkTableWithCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalWriteClusterLinkTableWithoutCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalWriteClusterLinkTableWithCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalReadClusterWithoutCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalReadClusterWithCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalWriteClusterWithoutCache(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kInternalWriteClusterWithCache(DWORD dwOffset, BYTE* pbBuffer);

static CACHEBUFFER* kAllocateCacheBufferWithFlush(int iCacheTableIndex);
BOOL kFlushFileSystemCache(void);

// 고수준 함수
FILE* kOpenFile(const char* pcFileName, const char* pcMode);
DWORD kReadFile(void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile);
DWORD kWriteFile(const void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFILE);
int kSeekFile(FILE* pstFile, int iOffset, int iOrigin);
int kCloseFile(FILE* pstFile);
int kRemoveFile(const char* pcFileName);
DIR* kOpenDirectory(const char* pcDirectoryName);
struct kDirectoryEntryStruct* kReadDirectory(DIR* pstDirectory);
void kRewindDirectory(DIR* pstDirectory);
int kCloseDirectory(DIR* pstDirectory);
BOOL kWriteZero(FILE* pstFile, DWORD dwCount);
BOOL kIsFileOpened(const DIRECTORYENTRY* pstEntry);

static void* kAllocateFileDirectoryHandle(void);
static void kFreeFileDirectoryHandle(FILE* pstFile);
static BOOL kCreateFile(const char* pcFileName, DIRECTORYENTRY* pstEntry, int* piDirectoryEntryIndex);
static BOOL kFreeClusterUntilEnd(DWORD dwClusterIndex);
static BOOL kUpdateDirectoryEntry(FILEHANDLE* pstFileHandle);

#endif