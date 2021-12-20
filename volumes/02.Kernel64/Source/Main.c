#include "Types.h"
#include "Keyboard.h"
#include "Descriptor.h"
#include "PIC.h"
#include "Console.h"
#include "ConsoleShell.h"
#include "Task.h"
#include "PIT.h"
#include "DynamicMemory.h"
#include "HardDisk.h"
#include "FileSystem.h"
#include "SerialPort.h"
#include "MultiProcessor.h"
#include "LocalAPIC.h"

void MainForApplicationProcessor(void);

void Main(void)
{
    int iCursorX, iCursorY;

    // BSP 플래그를 읽어서 Application Processor이면 코어용 초기화 함수로 이동
    if(*((BYTE*) BOOTSTRAPPROCESSOR_FLAGADDRESS) == 0)
    {
        MainForApplicationProcessor();
    }

    // Bootstrap Processor가 부팅을 완료했으므로 0x7C09에 있는 Application Processor를
    // 나타내는 플래그를 0으로 설정하여 Application Processor용으로 코드 실행 경로 변경
    *((BYTE*) BOOTSTRAPPROCESSOR_FLAGADDRESS) = 0;

    kInitializeConsole(0, 10);

    kPrintf("Switch To IA-32e Mode Success\n");
    kPrintf("IA-32e C Language Kernel Start..............[Pass]\n");
    kPrintf("Initialize Console..........................[Pass]\n");

    kGetCursor(&iCursorX, &iCursorY);
    kPrintf("GDT Initialize And Switch For IA-32e Mode...[    ]");
    kInitializeGDTTableAndTSS();
    kLoadGDTR(GDTR_STARTADDRESS);
    kSetCursor(45, iCursorY++);
    kPrintf("Pass\n");

    kPrintf("TSS Segment Load............................[    ]");
    kLoadTR(GDT_TSSSEGMENT);
    kSetCursor(45, iCursorY++);
    kPrintf("Pass\n");

    kPrintf("IDT Initalize...............................[    ]");
    kInitializeIDTTables();
    kLoadIDTR(IDTR_STARTADDRESS);
    kSetCursor(45, iCursorY++);
    kPrintf("Pass\n");

    kPrintf("Total RAM Size Check........................[    ]");
    kCheckTotalRAMSize();
    kSetCursor(45, iCursorY++);
    kPrintf("Pass], Size = %d MB\n", kGetTotalRAMSize());

    kPrintf("TCB Pool And Scheduler Initialize...........[Pass]\n");
    iCursorX++;
    kInitializeScheduler();

    kPrintf("Dynamic Memory Initialize...................[Pass]\n");
    iCursorY++;
    kInitializeDynamicMemory();

    kInitializePIT(MSTOCOUNT(1), 1);

    kPrintf("Keyboard Activate And Queue Initialize......[    ]");

    if(kInitializeKeyboard() == TRUE)
    {
        kSetCursor(45, iCursorY++);
        kPrintf("Pass\n");
        kChangeKeyboardLED(FALSE, FALSE, FALSE);
    }
    else
    {
        kSetCursor(45, iCursorY++);
        kPrintf("Fail\n");
        while(1);
    }

    kPrintf("PIC Controller And Interrupt Initailize.....[    ]");
    kInitializePIC();
    kMaskPICInterrupt(0);
    kEnableInterrupt();
    kSetCursor(45, iCursorY++);
    kPrintf("Pass\n");

    kPrintf("File System Initialize......................[    ]");
    if(kInitializeFileSystem() == TRUE)
    {
        kSetCursor(45, iCursorY++);
        kPrintf("Pass\n");
    }
    else
    {
        kSetCursor(45, iCursorY++);
        kPrintf("Fail\n");
    }

    kPrintf("Serial Port Initialize......................[Pass]\n");
    iCursorY++;
    kInitializeSerialPort();

    kCreateTask(TASK_FLAGS_LOWEST | TASK_FLAGS_THREAD | TASK_FLAGS_SYSTEM | TASK_FLAGS_IDLE,
                0, 0, (QWORD) kIdleTask);
    kStartConsoleShell();
}

void MainForApplicationProcessor(void)
{
    QWORD qwTickCount;

    kLoadGDTR(GDTR_STARTADDRESS);

    kLoadTR(GDT_TSSSEGMENT + (kGetAPICID() * sizeof(GDTENTRY16)));

    kLoadIDTR(IDTR_STARTADDRESS);

    kEnableSoftwareLocalAPIC();

    kSetTaskPriority(0);

    kInitializeLocalVectorTable();

    kEnableInterrupt();

    qwTickCount = kGetTickCount();
    while(1)
    {
        if(kGetTickCount() - qwTickCount > 1000)
        {
            qwTickCount = kGetTickCount();
            // kPrintfstarstartsymmetricio("Application Processor [APIC ID: %d] is Activated\n", kGetAPICID());
        }
    }
}