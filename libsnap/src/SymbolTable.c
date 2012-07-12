/*  Copyright (C) 2012  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#include <string.h>
#include "SymbolTable.h"
#include "SymbolTableTest.h"
#include "util.h"

struct SymbolTable
{
    Symbol**  ppBuckets;
    Symbol**  ppEnumBucket;
    Symbol*   pEnumBucketItem;
    size_t    bucketCount;
    size_t    symbolCount;
};

struct SymbolLineReference
{
    LineInfo*                   pLineInfo;
    struct SymbolLineReference* pNext;
};

typedef struct LineReferenceFind
{
    Symbol*              pSymbol;
    LineInfo*            pLineInfo;
    SymbolLineReference* pPrev;
    SymbolLineReference* pFound;
} LineReferenceFind;



static SymbolTable* allocateSymbolTable(void);
static void allocateBuckets(SymbolTable* pThis, size_t bucketCount);
__throws SymbolTable* SymbolTable_Create(size_t bucketCount)
{
    SymbolTable* pThis = NULL;
    
    __try
    {
        __throwing_func( pThis = allocateSymbolTable() );
        __throwing_func( allocateBuckets(pThis, bucketCount) );
    }
    __catch
    {
        SymbolTable_Free(pThis);
        __rethrow_and_return(NULL);
    }

    return pThis;
}

static SymbolTable* allocateSymbolTable(void)
{
    SymbolTable* pThis = malloc(sizeof(*pThis));
    if (!pThis)
        __throw_and_return(outOfMemoryException, NULL);
    memset(pThis, 0, sizeof(*pThis));
    return pThis;
}

static void allocateBuckets(SymbolTable* pThis, size_t bucketCount)
{
    size_t allocationSize = bucketCount * sizeof(*pThis->ppBuckets);
    pThis->ppBuckets = malloc(allocationSize);
    if (!pThis->ppBuckets)
        __throw(outOfMemoryException);
    memset(pThis->ppBuckets, 0, allocationSize);
    pThis->bucketCount = bucketCount;
}


static void freeBuckets(SymbolTable* pThis);
static void freeBucket(Symbol* pEntry);
static void freeSymbol(Symbol* pSymbol);
static void freeLineReferences(Symbol* pSymbol);
void SymbolTable_Free(SymbolTable* pThis)
{
    if (!pThis)
        return;
    
    freeBuckets(pThis);
    free(pThis);
}

static void freeBuckets(SymbolTable* pThis)
{
    size_t i;
    
    if (!pThis->ppBuckets)
        return;
        
    for (i = 0 ; i < pThis->bucketCount ; i++)
        freeBucket(pThis->ppBuckets[i]);
    free(pThis->ppBuckets);
}

static void freeBucket(Symbol* pEntry)
{
    while (pEntry)
    {
        Symbol* pNext = pEntry->pNext;
        freeSymbol(pEntry);
        pEntry = pNext;
    }
}

static void freeSymbol(Symbol* pSymbol)
{
    free((char*)pSymbol->pKey);
    freeLineReferences(pSymbol);
    free(pSymbol);
}

static void freeLineReferences(Symbol* pSymbol)
{
    SymbolLineReference* pCurr = pSymbol->pLineReferences;
    
    while(pCurr)
    {
        SymbolLineReference* pNext = pCurr->pNext;
        free(pCurr);
        pCurr = pNext;
    }
}


size_t SymbolTable_GetSymbolCount(SymbolTable* pThis)
{
    return pThis->symbolCount;
}


static Symbol* allocateSymbol(const char* pKey);
static size_t hashString(const char* pString);
static char* stringDuplicate(const char* pString);
Symbol* SymbolTable_Add(SymbolTable* pThis, const char* pKey)
{
    Symbol*  pElement;
    Symbol** ppBucket;    
    
    __try
        pElement = allocateSymbol(pKey);
    __catch
        __rethrow_and_return(NULL);

    ppBucket = &pThis->ppBuckets[hashString(pKey) % pThis->bucketCount];
    pElement->pNext = *ppBucket;
    *ppBucket = pElement;
    pThis->symbolCount++;
    
    return pElement;
}

static Symbol* allocateSymbol(const char* pKey)
{
    Symbol* pSymbol = NULL;
    
    pSymbol = malloc(sizeof(*pSymbol));
    if (!pSymbol)
        __throw_and_return(outOfMemoryException, NULL);
    
    memset(pSymbol, 0, sizeof(*pSymbol));
    pSymbol->pKey = stringDuplicate(pKey);
    if (!pSymbol->pKey)
    {
        free(pSymbol);
        __throw_and_return(outOfMemoryException, NULL);
    }
    
    return pSymbol;
}

static char* stringDuplicate(const char* pString)
{
    char* pCopy = malloc(strlen(pString) + 1);
    
    if (!pCopy)
        return pCopy;
    return strcpy(pCopy, pString);
}

static size_t hashString(const char* pString)
{
    static const size_t hashMultiplier = 31;
    size_t              hash = 0;
    const char*         pCurr;
    
    for (pCurr = pString ; *pCurr ; pCurr++)
        hash = hash * hashMultiplier + (size_t)*pCurr;
        
    return hash;
}


static Symbol* searchBucket(Symbol* pBucketHead, const char* pKey, size_t keyLength);
Symbol* SymbolTable_FindSized(SymbolTable* pThis, const char* pKey, size_t keyLength)
{
    return searchBucket(pThis->ppBuckets[hashString(pKey) % pThis->bucketCount], pKey, keyLength);
}

static Symbol* searchBucket(Symbol* pBucketHead, const char* pKey, size_t keyLength)
{
    Symbol* pCurr = pBucketHead;
    
    if (!pBucketHead)
        return NULL;
        
    while (pCurr)
    {
        if (0 == strncmp(pKey, pCurr->pKey, keyLength))
            return pCurr;
            
        pCurr = pCurr->pNext;
    }
    return NULL;
}


Symbol* SymbolTable_Find(SymbolTable* pThis, const char* pKey)
{
    return searchBucket(pThis->ppBuckets[hashString(pKey) % pThis->bucketCount], pKey, strlen(pKey));
}


static void walkToNextNonEmptyBucket(SymbolTable* pThis);
void SymbolTable_EnumStart(SymbolTable* pThis)
{
    pThis->ppEnumBucket = pThis->ppBuckets - 1;
    walkToNextNonEmptyBucket(pThis);
}

static void walkToNextNonEmptyBucket(SymbolTable* pThis)
{
    Symbol** ppEnd = pThis->ppBuckets + pThis->bucketCount;
    Symbol** ppCurr = pThis->ppEnumBucket;
    
    while (++ppCurr < ppEnd)
    {
        if (*ppCurr)
        {
            pThis->ppEnumBucket = ppCurr;
            pThis->pEnumBucketItem = *ppCurr;
            return;
        }
    }
}


static void walkToNextSymbol(SymbolTable* pThis);
static int encounteredEndOfBucketList(SymbolTable* pThis);
Symbol* SymbolTable_EnumNext(SymbolTable* pThis)
{
    Symbol* pCurr = pThis->pEnumBucketItem;

    if (!pCurr)
        return NULL;
    
    walkToNextSymbol(pThis);
    return pCurr;
}

static void walkToNextSymbol(SymbolTable* pThis)
{
    pThis->pEnumBucketItem = pThis->pEnumBucketItem->pNext;
    if (encounteredEndOfBucketList(pThis))
        walkToNextNonEmptyBucket(pThis);
}

static int encounteredEndOfBucketList(SymbolTable* pThis)
{
    return pThis->pEnumBucketItem == NULL;
}


__throws void Symbol_LineReferenceAdd(Symbol* pSymbol, LineInfo* pLineInfo)
{
    SymbolLineReference* pLineReference;
    
    if (Symbol_LineReferenceExist(pSymbol, pLineInfo))
        return;
        
    pLineReference = malloc(sizeof(*pLineReference));
    if (!pLineReference)
        __throw(outOfMemoryException);
        
    memset(pLineReference, 0, sizeof(*pLineReference));
    pLineReference->pLineInfo = pLineInfo;
    pLineReference->pNext = pSymbol->pLineReferences;
    pSymbol->pLineReferences = pLineReference;
}


static int findLineReference(LineReferenceFind* pFind);
int Symbol_LineReferenceExist(Symbol* pSymbol, LineInfo* pLineInfo)
{
    LineReferenceFind find;
    
    find.pSymbol = pSymbol;
    find.pLineInfo = pLineInfo;
    return findLineReference(&find);
}

static int findLineReference(LineReferenceFind* pFind)
{
    SymbolLineReference* pPrev = NULL;
    SymbolLineReference* pCurr = pFind->pSymbol->pLineReferences;
    LineInfo*            pLineInfo = pFind->pLineInfo;
    
    while (pCurr)
    {
        if (pCurr->pLineInfo == pLineInfo)
        {
            pFind->pPrev = pPrev;
            pFind->pFound = pCurr;
            return TRUE;
        }
        pPrev = pCurr;
        pCurr = pCurr->pNext;
    }
    
    pFind->pPrev = NULL;
    pFind->pFound = NULL;
    return FALSE;
}


void Symbol_LineReferenceRemove(Symbol* pSymbol, LineInfo* pLineInfo)
{
    LineReferenceFind find;
    
    find.pSymbol = pSymbol;
    find.pLineInfo = pLineInfo;
    if (findLineReference(&find))
    {
        if (!find.pPrev)
            pSymbol->pLineReferences = find.pFound->pNext;
        else
            find.pPrev->pNext = find.pFound->pNext;
        free(find.pFound);
    }
}


void Symbol_LineReferenceEnumStart(Symbol* pSymbol)
{
    pSymbol->pEnumLineReference = pSymbol->pLineReferences;
}

LineInfo* Symbol_LineReferenceEnumNext(Symbol* pSymbol)
{
    SymbolLineReference* pLineReference = pSymbol->pEnumLineReference;
    LineInfo*            pReturn;
    
    if (!pLineReference)
        return NULL;
        
    pReturn = pLineReference->pLineInfo;
    pSymbol->pEnumLineReference = pLineReference->pNext;
    return pReturn;
}
