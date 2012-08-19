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
#ifndef _DISK_IMAGE_PRIV_H_
#define _DISK_IMAGE_PRIV_H_

#include "DiskImage.h"
#include "TextFile.h"
#include "ParseCSV.h"
#include "ByteBuffer.h"


typedef struct DiskImageVTable
{
    void (*insertData)(void* pThis, const unsigned char* pData, DiskImageInsert* pInsert);

} DiskImageVTable;


typedef struct DiskImageScriptEngine
{
    DiskImage*      pDiskImage;
    TextFile*       pTextFile;
    ParseCSV*       pParser;
    const char*     pScriptFilename;
    DiskImageInsert insert;
    unsigned int    lineNumber;
} DiskImageScriptEngine;


struct DiskImage
{
    DiskImageVTable*      pVTable;
    ByteBuffer            image;
    ByteBuffer            object;
    DiskImageScriptEngine script;
    unsigned int          objectFileLength;
};


__throws DiskImage DiskImage_Init(DiskImageVTable* pVTable, unsigned int imageSize);

#endif /* _DISK_IMAGE_PRIV_H_ */
