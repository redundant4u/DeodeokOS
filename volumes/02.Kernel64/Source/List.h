#ifndef __LIST_H__
#define __LIST_H__

#include "Types.h"

#pragma pack(push, 1)

typedef struct kListLinkStruct
{
    void* pvNext;
    QWORD qwID;
} LISTLINK;

struct kListItemExampleStruct
{
    LISTLINK stLink;
    
    int iData;
    char cData;
};

typedef struct kListManagerStruct
{
    int iItemCount;
    void* pvHeader;
    void* pvTail;
} LIST;

#pragma pack(pop)

void kInitializeList(LIST* pstList);
int kGetListCount(const LIST* pstList);
void kAddListToTail(LIST* pstList, void* pvItem);
void kAddListToHeader(LIST* pstList, void* pvItem);
void* kRemoveList(LIST* pstList, QWORD qwID);
void* kRemoveListFromHeader(LIST* pstList);
void* kRemoveListFromTail(LIST* pstList);
void* kFindList(const LIST* pstList, QWORD qwID);
void* kGetHeaderFromList(const LIST* pstList);
void* kGetTailFromList(const LIST* pstList);
void* kGetNextFromList(const LIST* pstList, void* pstCount);

#endif