/*  Copyright (C) 2013  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#ifndef _NIBBLE_DISK_IMAGE_H_
#define _NIBBLE_DISK_IMAGE_H_

#include "try_catch.h"
#include "DiskImage.h"

#define NIBBLE_DISK_IMAGE_RWTS16_NIBBLES_PER_SECTOR 384
#define NIBBLE_DISK_IMAGE_RWTS16_GAP1_SYNC_BYTES    528
#define NIBBLE_DISK_IMAGE_RWTS16_GAP2_SYNC_BYTES    5
#define NIBBLE_DISK_IMAGE_RWTS16_GAP3_SYNC_BYTES    16
#define NIBBLE_DISK_IMAGE_RWTS16_SECTORS_PER_TRACK  16
#define NIBBLE_DISK_IMAGE_SIZE                      232960
#define NIBBLE_DISK_IMAGE_NIBBLES_PER_TRACK         (NIBBLE_DISK_IMAGE_SIZE / DISK_IMAGE_TRACKS_PER_SIDE)


typedef struct NibbleDiskImage NibbleDiskImage;


__throws NibbleDiskImage* NibbleDiskImage_Create(void);

__throws void             NibbleDiskImage_ProcessScriptFile(NibbleDiskImage* pThis, const char* pScriptFilename);
__throws void             NibbleDiskImage_ProcessScript(NibbleDiskImage* pThis, char* pScriptText);
__throws void             NibbleDiskImage_ReadObjectFile(NibbleDiskImage* pThis, const char* pFilename);
__throws void             NibbleDiskImage_InsertObjectFile(NibbleDiskImage* pThis, DiskImageInsert* pInsert);
__throws void             NibbleDiskImage_InsertData(NibbleDiskImage*     pThis, 
                                                     const unsigned char* pData, 
                                                     DiskImageInsert*     pInsert);
                                                   
__throws void             NibbleDiskImage_WriteImage(NibbleDiskImage* pThis, const char* pImageFilename);
         
         const unsigned char* NibbleDiskImage_GetImagePointer(NibbleDiskImage* pThis);
         size_t               NibbleDiskImage_GetImageSize(NibbleDiskImage* pThis);

__throws void             NibbleDiskImage_ReadRW18Track(NibbleDiskImage* pThis,
                                                        unsigned int track,
                                                        unsigned int side,
                                                        unsigned char* pTrackData,
                                                        size_t trackDataSize);

#endif /* _NIBBLE_DISK_IMAGE_H_ */
