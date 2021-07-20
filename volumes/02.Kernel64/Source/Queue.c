#include "Queue.h"

void kInitializeQueue(QUEUE* pstQueue, void* pvQueueBuffer,
    int iMaxDataCount, int iDataSize)
{
    pstQueue->iMaxDataCount = iMaxDataCount;
    pstQueue->iDataSize = iDataSize;
    pstQueue->pvQueueArrary = pvQueueBuffer;

    pstQueue->iPutIndex = 0;
    pstQueue->iGetIndex = 0;
    pstQueue->bLastOperationPut = FALSE;
}

BOOL kIsQueueFull(const QUEUE* pstQueue)
{
    if((pstQueue->iGetIndex == pstQueue->iPutIndex) &&
        (pstQueue->bLastOperationPut == TRUE))
    {
        return TRUE;
    }
    return FALSE;
}

BOOL kIsQueueEmpty(const QUEUE* pstQueue)
{
    if((pstQueue->iGetIndex == pstQueue->iPutIndex) &&
        (pstQueue->bLastOperationPut == FALSE))
    {
        return TRUE;
    }
    return FALSE;
}

BOOL kPutQueue(QUEUE* pstQueue, const void* pvData)
{
    if(kIsQueueFull(pstQueue) == TRUE)
    {
        return FALSE;
    }

    kMemCpy((char*) pstQueue->pvQueueArrary + (pstQueue->iDataSize *
            pstQueue->iPutIndex), pvData, pstQueue->iDataSize);
    
    pstQueue->iPutIndex = (pstQueue->iPutIndex + 1) % pstQueue->iMaxDataCount;
    pstQueue->bLastOperationPut = TRUE;

    return TRUE;
}

BOOL kGetQueue(QUEUE* pstQueue, void* pvData)
{
    if(kIsQueueEmpty(pstQueue) == TRUE)
    {
        return FALSE;
    }

    kMemCpy(pvData, (char*) pstQueue->pvQueueArrary + (pstQueue->iDataSize *
            pstQueue->iGetIndex), pstQueue->iDataSize);

    pstQueue->iGetIndex = (pstQueue->iGetIndex + 1) % pstQueue->iMaxDataCount;
    pstQueue->bLastOperationPut = FALSE;

    return TRUE;       
}