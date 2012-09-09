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
#include <stdio.h>
#include "TextFile.h"
#include "TextFileTest.h"
#include "util.h"

struct TextFile
{
    char*        pFileBuffer;
    char*        pText;
    char*        pCurr;
    char*        pFilename;
    unsigned int lineNumber;
};


__throws static void initObject(TextFile* pThis, char* pText);
__throws static char* allocateStringAndCopyMergedFilename(const char* pFilename, const char* pFilenameSuffix);
__throws TextFile* TextFile_CreateFromString(char* pText)
{
    static const char defaultFilename[] = "filename";
    TextFile*         pThis = NULL;
    
    __try
    {
        pThis = allocateAndZero(sizeof(*pThis));
        initObject(pThis, pText);
        pThis->pFilename = allocateStringAndCopyMergedFilename(defaultFilename, NULL);
    }
    __catch
    {
        TextFile_Free(pThis);
        __rethrow;
    }
    
    return pThis;
}

__throws static void initObject(TextFile* pThis, char* pText)
{
    pThis->pText = pText;
    pThis->pCurr = pText;
}

__throws static char* allocateStringAndCopyMergedFilename(const char* pFilename, const char* pFilenameSuffix)
{
    size_t filenameLength = strlen(pFilename);
    size_t suffixLength = pFilenameSuffix ? strlen(pFilenameSuffix) : 0;
    size_t fullFilenameLength = filenameLength + suffixLength + 1;
    char*  pFullFilename = malloc(fullFilenameLength);
    if (!pFullFilename)
        __throw(outOfMemoryException);

    memcpy(pFullFilename, pFilename, filenameLength);
    memcpy(pFullFilename + filenameLength, pFilenameSuffix, suffixLength);
    pFullFilename[fullFilenameLength - 1] = '\0';
    
    return pFullFilename;
}


static FILE* openFile(const char* pFilename);
static long getTextLength(FILE* pFile);
static long adjustFileSizeToAllowForTrailingNull(long actualFileSize);
static char* allocateTextBuffer(long textLength);
static void readFileContentIntoTextBufferAndNullTerminate(char* pTextBuffer, long textBufferSize, FILE* pFile);
__throws TextFile* TextFile_CreateFromFile(const char* pFilename, const char* pFilenameSuffix)
{
    FILE*     pFile = NULL;
    long      textLength = -1;
    TextFile* pThis = NULL;
    
    __try
    {
        pThis = allocateAndZero(sizeof(*pThis));
        pThis->pFilename = allocateStringAndCopyMergedFilename(pFilename, pFilenameSuffix);
        pFile = openFile(pThis->pFilename);
        textLength = getTextLength(pFile);
        pThis->pFileBuffer = allocateTextBuffer(textLength);
        readFileContentIntoTextBufferAndNullTerminate(pThis->pFileBuffer, textLength, pFile);
        initObject(pThis, pThis->pFileBuffer);
    }
    __catch
    {
        TextFile_Free(pThis);
        fclose(pFile);
        __rethrow;
    }
    
    fclose(pFile);
    return pThis;
}

static FILE* openFile(const char* pFilename)
{
    FILE* pFile = NULL;
    
    pFile = fopen(pFilename, "r");
    if (!pFile)
        __throw(fileNotFoundException);
    return pFile;
}

static long getTextLength(FILE* pFile)
{
    long fileSize;
    int  result;
    
    result = fseek(pFile, 0, SEEK_END);
    if (result != 0)
        __throw(fileException);
        
    fileSize = ftell(pFile);
    if (fileSize == -1)
        __throw(fileException);
        
    result = fseek(pFile, 0, SEEK_SET);
    if (result != 0)
        __throw(fileException);
        
    return adjustFileSizeToAllowForTrailingNull(fileSize);
}

static long adjustFileSizeToAllowForTrailingNull(long actualFileSize)
{
    return actualFileSize + 1;
}

static char* allocateTextBuffer(long textLength)
{
    char* pTextBuffer;
    
    pTextBuffer = malloc(textLength);
    if (!pTextBuffer)
        __throw(outOfMemoryException);
        
    return pTextBuffer;
}

static void readFileContentIntoTextBufferAndNullTerminate(char* pTextBuffer, long textBufferSize, FILE* pFile)
{
    int fileContentSize = textBufferSize - 1;
    int result;
    
    result = fread(pTextBuffer, 1, fileContentSize, pFile);
    if (result != fileContentSize)
        __throw(fileException);
        
    pTextBuffer[textBufferSize - 1] = '\0';
}


void TextFile_Free(TextFile* pThis)
{
    if (!pThis)
        return;
    
    free(pThis->pFileBuffer);
    free(pThis->pFilename);
    free(pThis);
}


static void nullTerminateLineAndAdvanceToNextLine(TextFile* pThis);
static char* findEndOfLine(TextFile* pThis);
static int isLineEndCharacter(char ch);
static char* findStartOfNextLine(char* pEndOfCurrentLine);
char* TextFile_GetNextLine(TextFile* pThis)
{
    char* pStartOfLine = pThis->pCurr;
    
    if (*pStartOfLine == '\0')
        return NULL;
    
    nullTerminateLineAndAdvanceToNextLine(pThis);
    pThis->lineNumber++;
    
    return pStartOfLine;
}

static void nullTerminateLineAndAdvanceToNextLine(TextFile* pThis)
{
    char* pLineEnd = findEndOfLine(pThis);
    char* pNextLineStart = findStartOfNextLine(pLineEnd);
    
    *pLineEnd = '\0';
    pThis->pCurr = pNextLineStart;
}

static char* findEndOfLine(TextFile* pThis)
{
    char* pCurr = pThis->pCurr;
    
    while (*pCurr && !isLineEndCharacter(*pCurr))
        pCurr++;
    
    /* Get here if *pCurr is '\0' or '\r' or '\n' */
    return pCurr;
}

static int isLineEndCharacter(char ch)
{
    return ch == '\r' || ch == '\n';
}

static char* findStartOfNextLine(char* pEndOfCurrentLine)
{
    char  prev = *pEndOfCurrentLine;
    char  curr;
    
    if (prev == '\0')
        return pEndOfCurrentLine;
    
    curr = pEndOfCurrentLine[1];
    
    if ((prev == '\r' && curr == '\n') ||
        (prev == '\n' && curr == '\r'))
    {
        return pEndOfCurrentLine + 2;
    }
    
    return pEndOfCurrentLine + 1;
}

unsigned int TextFile_GetLineNumber(TextFile* pThis)
{
    return pThis->lineNumber;
}

const char* TextFile_GetFilename(TextFile* pThis)
{
    return pThis->pFilename;
}
