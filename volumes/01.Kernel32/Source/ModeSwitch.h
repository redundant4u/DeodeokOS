#ifndef __MODESWITCH_H__
#define __MODESWITCH_H__

#include "Types.h"

void kReadCPUID(DWORD dwEAX, DWORD* pdwRAX, DWORD* pdwEBX, DWORD* pdwECX, DWORD* pdwEDX);
void kSwitchAndExecute64bitKernel(void);

#endif