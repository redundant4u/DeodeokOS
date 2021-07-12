#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
// #include <sys/uio.h>
// #include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define BYTESOFSECTOR 512

int AdjustInSectorSize(int iFd, int iSourceSize);
void WriteKernelInformation(int iTargetFd,
    int iTotalKernelSectorCount, int iKernel32SectorCount);
int CopyFile(int iSourceFd, int iTargetFd);

int main(int argc, char* argv[])
{
    int iSourceFd;
    int iTargetFd;
    int iBootLoaderSize;
    int iKernel32SectorCount;
    int iKernel64SectorCount;
    int iSourceSize;

    if(argc < 4)
    {
        fprintf(stderr, "[ERROR] ImageMaker BootLoader.bin Kernel32.bin Kernel64.bin\n");
        exit(-1);
    }

    // Disk.img 파일 생성
    if((iTargetFd = open("Disk.img", O_RDWR | O_CREAT |
        O_TRUNC, S_IREAD | S_IWRITE)) == -1)
    {
        fprintf(stderr, "[ERROR] Disk.img open fail\n");
        exit(-1);
    }

    // 부트로더 파일을 열어서 모든 내용을 디스크 이미지 파일로 복사
    printf("[INFO] Copy boot loader to image file\n");
    if((iSourceFd = open(argv[1], O_RDONLY)) == -1)
    {
        fprintf(stderr, "[ERROR] %s open fail\n", argv[1]);
        exit(-1);
    }

    iSourceSize = CopyFile(iSourceFd, iTargetFd);
    close(iSourceFd);

    iBootLoaderSize = AdjustInSectorSize(iTargetFd, iSourceSize);
    printf("[INFO] %s size = [%d] and sector count = [%d]\n",
        argv[1], iSourceSize, iBootLoaderSize);
    
    // 32비트 커널 파일을 열어서 모든 내용을 디스크 이미지 파일로 복사
    printf("[INFO] Copy protected mode kernel to image file\n");
    if((iSourceFd = open(argv[2], O_RDONLY)) == -1)
    {
        fprintf(stderr, "[ERROR] %s open fail\n", argv[2]);
        exit(-1);
    }

    iSourceSize = CopyFile(iSourceFd, iTargetFd);
    close(iSourceFd);

    iKernel32SectorCount = AdjustInSectorSize(iTargetFd, iSourceSize);
    printf("[INFO] %s size = [%d] and sector count = [%d]\n",
        argv[2], iSourceSize, iKernel32SectorCount);
    
    // 64비트 커널 파일을 열어서 모든 내용을 디스크 이미지 파일로 복사
    printf("[INFO] Copy IA-32e model kernel to image file\n");
    if((iSourceFd = open(argv[3], O_RDONLY)) == -1)
    {
        fprintf(stderr, "[ERROR] %s open fail\n", argv[3]);
        exit(-1);
    }

    iSourceSize = CopyFile(iSourceFd, iTargetFd);
    close(iSourceFd);

    iKernel64SectorCount = AdjustInSectorSize(iTargetFd, iSourceSize);
    printf("[INFO] %s size = [%d] and sector count = [%d]\n",
        argv[3], iSourceSize, iKernel64SectorCount);

    // 디스크 이미지에 커널 정보 갱신
    printf("[INFO] Start to write kernel information\n");
    WriteKernelInformation(iTargetFd,
        iKernel32SectorCount + iKernel64SectorCount, iKernel32SectorCount);
    printf("[INFO] Image file create complete\n");

    close(iTargetFd);
    return 0;
}

// 현재 위치부터 512바이트 배수 위치까지 맞추어 0x00으로 채움
int AdjustInSectorSize(int iFd, int iSourceSize)
{
    int i;
    int iAdjustSizeToSector;
    char cCh;
    int iSectorCount;

    iAdjustSizeToSector = iSourceSize % BYTESOFSECTOR;
    cCh = 0x00;

    if(iAdjustSizeToSector != 0)
    {
        iAdjustSizeToSector = 512 - iAdjustSizeToSector;
        printf("[INFO] File size [%lu] and fill [%u] byte\n",
            iSourceSize, iAdjustSizeToSector);
        for(i = 0; i < iAdjustSizeToSector; i++)
        {
            write(iFd, &cCh, 1);
        }
    }
    else
    {
        printf("[INFO] File size is aligned 512 byte\n");
    }

    iSectorCount = (iSourceSize + iAdjustSizeToSector) / BYTESOFSECTOR;
    return iSectorCount;
}

void WriteKernelInformation(int iTargetFd, int iTotallKernelSectorCount, int iKernel32SectorCount)
{
    unsigned short usData;
    long lPosition;

    lPosition = lseek(iTargetFd, (off_t)5, SEEK_SET);
    if(lPosition == -1)
    {
        fprintf(stderr, "lseek fail. Return value = %d, errno = %d, %d\n",
            lPosition, errno, SEEK_SET);
        exit(-1);
    }
    usData = (unsigned short) iTotallKernelSectorCount;
    write(iTargetFd, &usData, 2);
    usData = (unsigned short) iKernel32SectorCount;
    write(iTargetFd, &usData, 2);

    printf("[INFO] Total sector count except boot loader [%d]\n",
        iTotallKernelSectorCount);
    printf("[INFO] Total sector count of protected mode kernel [%d]\n",
        iKernel32SectorCount);
}

int CopyFile(int iSourceFd, int iTargetFd)
{
    int iSourceFileSize;
    int iRead;
    int iWrite;
    char vcBuffer[BYTESOFSECTOR];

    iSourceFileSize = 0;
    while(1)
    {
        iRead = read(iSourceFd, vcBuffer, sizeof(vcBuffer));
        iWrite = write(iTargetFd, vcBuffer, iRead);

        if(iRead != iWrite)
        {
            fprintf(stderr, "[ERROR] iRead != iWrite...\n");
            exit(-1);
        }
        iSourceFileSize += iRead;

        if(iRead != sizeof(vcBuffer))
        {
            break;
        }
    }
    return iSourceFileSize;
}