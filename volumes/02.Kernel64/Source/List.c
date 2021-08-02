#include "List.h"

void kInitializeList(LIST* pstList)
{
    pstList->iItemCount = 0;
    pstList->pvHeader = NULL;
    pstList->pvTail = NULL;
}

int kGetListCount(const LIST* pstList)
{
    return pstList->iItemCount;
}

void kAddListToTail(LIST* pstList, void* pvItem)
{
    LISTLINK* pstLink;

    pstLink = (LISTLINK*) pvItem;
    pstLink->pvNext = NULL;

    if(pstList->pvHeader == NULL)
    {
        pstList->pvHeader = pvItem;
        pstList->pvTail = pvItem;
        pstList->iItemCount = 1;

        return;
    }

    pstLink = (LISTLINK*) pstList->pvTail;
    pstLink->pvNext = pvItem;

    pstList->pvTail = pvItem;
    pstList->iItemCount++;
}

void kAddListToHeader(LIST* pstList, void* pvItem)
{
    LISTLINK* pstLink;

    pstLink = (LISTLINK*) pvItem;
    pstLink->pvNext = pstList->pvHeader;

    if(pstList->pvHeader == NULL)
    {
        pstList->pvHeader = pvItem;
        pstList->pvTail = pvItem;
        pstList->iItemCount = 1;

        return;
    }

    pstList->pvHeader = pvItem;
    pstList->iItemCount++;
}

void* kRemoveList(LIST* pstList, QWORD qwID)
{
    LISTLINK* pstLink;
    LISTLINK* pstPreviousLink;

    pstPreviousLink = (LISTLINK*) pstList->pvHeader;
    for(pstLink = pstPreviousLink; pstLink != NULL; pstLink = pstLink->pvNext)
    {
        if(pstLink->qwID == qwID)
        {
            // 데이터가 하나밖에 없다면 리스트 초기화
            if((pstLink == pstList->pvHeader) &&
               (pstLink == pstList->pvTail))
            {
                pstList->pvHeader = NULL;
                pstList->pvTail = NULL;
            }
            // 리스트의 첫 번째 데이터면 Header를 두 번째 데이터로 변경
            else if(pstLink == pstList->pvHeader)
            {
                pstList->pvHeader = pstLink->pvNext;
            }
            // 리스트의 마지막 데이터면 Tail을 마지막 이전의 데이터로 변경
            else if(pstLink == pstList->pvTail)
            {
                pstList->pvTail = pstPreviousLink;
            }
            else
            {
                pstPreviousLink->pvNext = pstLink->pvNext;
            }

            pstList->iItemCount--;
            return pstLink;
        }
        pstPreviousLink = pstLink;
    }
    return NULL;
}

void* kRemoveListFromHeader(LIST* pstList)
{
    LISTLINK* pstLink;

    if(pstList->iItemCount == 0)
    {
        return NULL;
    }

    pstLink = (LISTLINK*) pstList->pvHeader;
    return kRemoveList(pstList, pstLink->qwID);
}

void* kRemoveListFromTail(LIST* pstList)
{
    LISTLINK* pstLink;
    if(pstList->iItemCount == 0)
    {
        return NULL;
    }

    pstLink = (LISTLINK*) pstList->pvTail;
    return kRemoveList(pstList, pstLink->qwID);
}

void* kFindList(const LIST* pstList, QWORD qwID)
{
    LISTLINK* pstLink;

    for(pstLink = (LISTLINK*) pstList->pvHeader; pstLink != NULL; pstLink = pstLink->pvNext)
    {
        if(pstLink->qwID == qwID)
        {
            return pstLink;
        }
    }
    return NULL;
}

void* kGetHeaderFromList(const LIST* pstList)
{
    return pstList->pvHeader;
}

void* kGetTailFromList(const LIST* pstList)
{
    return pstList->pvTail;
}

void* kGetNextFromList(const LIST* pstList, void* pstCurrent)
{
    LISTLINK* pstLink;
    pstLink = (LISTLINK*) pstCurrent;
    return pstLink->pvNext;
}