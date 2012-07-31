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
// Include headers from C modules under test.
extern "C"
{
    #include "DiskImage.h"
    #include "BinaryBuffer.h"
    #include "MallocFailureInject.h"
    #include <FileFailureInject.h>
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

static const char* g_imageFilename = "DiskImageTest.nib";
static const char* g_savFilename = "DiskImageTest.sav";


TEST_GROUP(DiskImage)
{
    DiskImage*           m_pDiskImage;
    const unsigned char* m_pCurr;
    FILE*                m_pFile;
    unsigned char*       m_pImageOnDisk;
    unsigned char        m_checksum;
    
    void setup()
    {
        clearExceptionCode();
        m_pDiskImage = NULL;
        m_pFile = NULL;
        m_pCurr = NULL;
        m_pImageOnDisk = NULL;
    }

    void teardown()
    {
        LONGS_EQUAL(noException, getExceptionCode());
        MallocFailureInject_Restore();
        DiskImage_Free(m_pDiskImage);
        if (m_pFile)
            fclose(m_pFile);
        free(m_pImageOnDisk);
        remove(g_imageFilename);
        remove(g_savFilename);
    }
    
    void validateAllZeroes(const unsigned char* pBuffer, size_t bufferSize)
    {
        for (size_t i = 0 ; i < bufferSize ; i++)
            LONGS_EQUAL(0, *pBuffer++);
    }
    
    void validateRWTS16SectorsAreClear(const unsigned char* pImage, 
                                       unsigned int         startTrack, 
                                       unsigned int         startSector,
                                       unsigned int         endTrack,
                                       unsigned int         endSector)
    {
        unsigned int startOffset = DISK_IMAGE_NIBBLES_PER_TRACK * startTrack + 
                                   DISK_IMAGE_RWTS16_NIBBLES_PER_SECTOR * startSector;
        unsigned int endOffset = DISK_IMAGE_NIBBLES_PER_TRACK * endTrack + 
                                 DISK_IMAGE_RWTS16_NIBBLES_PER_SECTOR * (endSector + 1);
        unsigned int length = endOffset - startOffset;
        
        validateAllZeroes(pImage + startOffset, length);
    }

    void validateRWTS16SectorContainsNibbles(const unsigned char* pImage,
                                             const unsigned char* pExpectedContent, 
                                             unsigned int         track, 
                                             unsigned int         sector)
    {
        static const unsigned char volume = 0;
        unsigned int               startOffset = DISK_IMAGE_NIBBLES_PER_TRACK * track + 
                                                 DISK_IMAGE_RWTS16_NIBBLES_PER_SECTOR * sector;
        m_pCurr = pImage + startOffset;
        
        validateSyncBytes(21);
        validateAddressField(volume, track, sector);
        validateSyncBytes(10);
        validateDataField(pExpectedContent);
        validateSyncBytes(22);
    }
    
    void validateSyncBytes(size_t syncByteCount)
    {
        for (size_t i = 0 ; i < syncByteCount ; i++)
            LONGS_EQUAL(0xFF, *m_pCurr++);
    }
    
    void validateAddressField(unsigned char volume, unsigned char track, unsigned char sector)
    {
        validateAddressFieldProlog();
        
        initChecksum();
        validate4by4Data(volume);
        validate4by4Data(track);
        validate4by4Data(sector);
        validateChecksum();
        
        validateFieldEpilog();
    }
    
    void validateAddressFieldProlog()
    {
        CHECK_TRUE(0 == memcmp(m_pCurr, "\xD5\xAA\x96", 3));
        m_pCurr += 3;
    }
    
    void initChecksum()
    {
        m_checksum = 0;
    }
    
    void validate4by4Data(unsigned char byte)
    {
        unsigned char oddBits = byte & 0xAA;
        unsigned char evenBits = byte & 0x55;
        unsigned char encodedOddByte = 0xAA | (oddBits >> 1);
        unsigned char encodedEvenByte = 0xAA | evenBits;
        
        updateChecksum(byte);
        LONGS_EQUAL(encodedOddByte, *m_pCurr++);
        LONGS_EQUAL(encodedEvenByte, *m_pCurr++);
    }
    
    void updateChecksum(unsigned char byte)
    {
        m_checksum ^= byte;
    }
    
    void validateChecksum()
    {
        validate4by4Data(m_checksum);
    }
    
    void validateFieldEpilog()
    {
        CHECK_TRUE(0 == memcmp(m_pCurr, "\xDE\xAA\xEB", 3));
        m_pCurr += 3;
    }
    
    void validateDataField(const unsigned char* pExpectedContent)
    {
        validateDataFieldProlog();
        
        validateData(pExpectedContent);
        
        validateFieldEpilog();
    }
    
    void validateDataFieldProlog()
    {
        CHECK_TRUE(0 == memcmp(m_pCurr, "\xD5\xAA\xAD", 3));
        m_pCurr += 3;
    }
    
    void validateData(const unsigned char* pExpectedContent)
    {
        CHECK_TRUE(0 == memcmp(m_pCurr, pExpectedContent, 343));
        m_pCurr += 343;
    }
    
    void writeZeroSectors(unsigned int startTrack, unsigned int startSector, unsigned int sectorCount)
    {
        unsigned int   totalSectorSize = sectorCount * DISK_IMAGE_BYTES_PER_SECTOR;
        unsigned char* pSectorData = (unsigned char*)malloc(totalSectorSize);
        CHECK_TRUE(pSectorData != NULL);
        memset(pSectorData, 0, totalSectorSize);
        
        DiskImageObject object;
        object.startOffset = 0;
        object.length = totalSectorSize;
        object.track = startTrack;
        object.sector = startSector;

        DiskImage_InsertObjectAsRWTS16(m_pDiskImage, pSectorData, &object);
        free(pSectorData);
    }
    
    const unsigned char* readDiskImageIntoMemory(void)
    {
        m_pFile = fopen(g_imageFilename, "r");
        CHECK(m_pFile != NULL);
        LONGS_EQUAL(DISK_IMAGE_SIZE, getFileSize(m_pFile));
        
        m_pImageOnDisk = (unsigned char*)malloc(DISK_IMAGE_SIZE);
        CHECK(m_pImageOnDisk != NULL);
        LONGS_EQUAL(DISK_IMAGE_SIZE, fread(m_pImageOnDisk, 1, DISK_IMAGE_SIZE, m_pFile));

        fclose(m_pFile);
        m_pFile = NULL;
        
        return m_pImageOnDisk;
    }
    
    long getFileSize(FILE* pFile)
    {
        fseek(pFile, 0, SEEK_END);
        long size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);
        return size;
    }
    
    void validateOutOfMemoryExceptionThrown()
    {
        validateExceptionThrown(outOfMemoryException);
    }
    
    void validateInvalidArgumentExceptionThrown()
    {
        validateExceptionThrown(invalidArgumentException);
    }
    
    void validateFileExceptionThrown()
    {
        validateExceptionThrown(fileException);
    }
    
    void validateExceptionThrown(int expectedExceptionCode)
    {
        LONGS_EQUAL(expectedExceptionCode, getExceptionCode());
        clearExceptionCode();
    }
    
    void createZeroSectorObjectFile()
    {
        SavFileHeader header;
        char          sectorData[DISK_IMAGE_BYTES_PER_SECTOR];
    
        memcpy(header.signature, BINARY_BUFFER_SAV_SIGNATURE, sizeof(header.signature));
        header.address = 0;
        header.length = DISK_IMAGE_BYTES_PER_SECTOR;
        memset(sectorData, 0, sizeof(sectorData));
    
        FILE* pFile = fopen(g_savFilename, "w");
        fwrite(&header, 1, sizeof(header), pFile);
        fwrite(sectorData, 1, sizeof(sectorData), pFile);
        fclose(pFile);
    }
};


TEST(DiskImage, FailAllocationInCreate)
{
    MallocFailureInject_FailAllocation(1);
    m_pDiskImage = DiskImage_Create();
    POINTERS_EQUAL(NULL, m_pDiskImage);
    validateOutOfMemoryExceptionThrown();
}

TEST(DiskImage, VerifyCreateStartsWithZeroesInImage)
{
    m_pDiskImage = DiskImage_Create();
    CHECK_TRUE(NULL != m_pDiskImage);
    
    const unsigned char* pImage = DiskImage_GetImagePointer(m_pDiskImage);
    CHECK_TRUE(NULL != pImage);
    LONGS_EQUAL(DISK_IMAGE_SIZE, DiskImage_GetImageSize(m_pDiskImage));
    validateAllZeroes(pImage, DISK_IMAGE_SIZE);
}

TEST(DiskImage, InsertZeroSectorAt0_0AsRWTS16)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(0, 0, 1);

    unsigned char expectedEncodedData[343];
    memset(expectedEncodedData, 0x96, sizeof(expectedEncodedData));
    const unsigned char* pImage = DiskImage_GetImagePointer(m_pDiskImage);
    validateRWTS16SectorsAreClear(pImage, 0, 1, 34, 15);
    validateRWTS16SectorContainsNibbles(pImage, expectedEncodedData, 0, 0);
}

TEST(DiskImage, InsertZeroSectorAt34_15AsRWTS16)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(34, 15, 1);

    unsigned char expectedEncodedData[343];
    memset(expectedEncodedData, 0x96, sizeof(expectedEncodedData));
    const unsigned char* pImage = DiskImage_GetImagePointer(m_pDiskImage);
    validateRWTS16SectorsAreClear(pImage, 0, 0, 34, 14);
    validateRWTS16SectorContainsNibbles(pImage, expectedEncodedData, 34, 15);
}

TEST(DiskImage, InsertTwoZeroSectorsAt0_15AsRWTS16)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(0, 15, 2);
 
    unsigned char expectedEncodedData[343];
    memset(expectedEncodedData, 0x96, sizeof(expectedEncodedData));
    const unsigned char* pImage = DiskImage_GetImagePointer(m_pDiskImage);
    validateRWTS16SectorsAreClear(pImage, 0, 0, 0, 14);
    validateRWTS16SectorContainsNibbles(pImage, expectedEncodedData, 0, 15);
    validateRWTS16SectorContainsNibbles(pImage, expectedEncodedData, 1, 0);
    validateRWTS16SectorsAreClear(pImage, 1, 1, 34, 15);
}

TEST(DiskImage, FailToInsertSector16AsRWTS16)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(0, 16, 1);
    validateInvalidArgumentExceptionThrown();
}

TEST(DiskImage, FailToInsertTrack35AsRWTS16)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(35, 0, 1);
    validateInvalidArgumentExceptionThrown();
}

TEST(DiskImage, FailToInsertSecondSectorAsRWTS16)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(34, 15, 2);
    validateInvalidArgumentExceptionThrown();
}

TEST(DiskImage, InsertTestSectorAt0_0AsRWTS16)
{
    m_pDiskImage = DiskImage_Create();

    unsigned char sectorData[256] =
    {
        0x00, 0x00, 0x03, 0x00, 0xfa, 0x55, 0x53, 0x45,
        0x52, 0x53, 0x2e, 0x44, 0x49, 0x53, 0x4b, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xc3, 0x27, 0x0d, 0x09, 0x00, 0x06,
        0x00, 0x18, 0x01, 0x26, 0x50, 0x52, 0x4f, 0x44,
        0x4f, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff, 0x08, 0x00, 0x1f, 0x00,
        0x00, 0x3c, 0x00, 0x21, 0xa8, 0x00, 0x00, 0x00,
        0x00, 0x21, 0x00, 0x20, 0x21, 0xa8, 0x00, 0x00,
        0x02, 0x00, 0x2c, 0x42, 0x41, 0x53, 0x49, 0x43,
        0x2e, 0x53, 0x59, 0x53, 0x54, 0x45, 0x4d, 0x00,
        0x00, 0x00, 0xff, 0x27, 0x00, 0x15, 0x00, 0x00,
        0x28, 0x00, 0x6f, 0xa7, 0x00, 0x00, 0x00, 0x00,
        0x21, 0x00, 0x20, 0x6f, 0xa7, 0x00, 0x00, 0x02,
        0x00, 0x25, 0x46, 0x49, 0x4c, 0x45, 0x52, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x3c, 0x00, 0x33, 0x00, 0x00, 0x64,
        0x00, 0x21, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x21,
        0x6e, 0x01, 0x21, 0xa8, 0x00, 0x00, 0x02, 0x00,
        0x27, 0x43, 0x4f, 0x4e, 0x56, 0x45, 0x52, 0x54,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x6f, 0x00, 0x2a, 0x00, 0x01, 0x50, 0x00,
        0x61, 0xa7, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00,
        0x20, 0x61, 0xa7, 0x00, 0x00, 0x02, 0x00, 0x27,
        0x53, 0x54, 0x41, 0x52, 0x54, 0x55, 0x50, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
        0x99, 0x00, 0x18, 0x00, 0xc9, 0x2c, 0x00, 0x4f,
        0xa7, 0x00, 0x00, 0x00, 0x00, 0x21, 0x01, 0x08,
        0x4f, 0xa7, 0x00, 0x00, 0x02, 0x00, 0x25, 0x4d,
        0x4f, 0x49, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xb1
    };
    DiskImageObject object;
    object.startOffset = 0;
    object.length = 256;
    object.track = 0;
    object.sector = 0;
    DiskImage_InsertObjectAsRWTS16(m_pDiskImage, sectorData, &object);

    unsigned char expectedEncodedData[343] =
    {
        0xa7, 0x9d, 0xad, 0xad, 0xf4, 0xa6, 0xfd, 0xbe,
        0xb7, 0xe6, 0xd9, 0x97, 0xeb, 0xb5, 0xfc, 0xad,
        0xa7, 0x96, 0xd6, 0xd6, 0xae, 0xd6, 0xcd, 0xed,
        0x96, 0xb4, 0xbd, 0xf7, 0x96, 0xfc, 0xd6, 0xfc,
        0xb4, 0xdb, 0xde, 0xa7, 0xaf, 0xae, 0xac, 0xab,
        0xaf, 0x9d, 0x9a, 0x9b, 0xd7, 0xd7, 0x9a, 0x9b,
        0xda, 0xd6, 0x9b, 0xfc, 0xae, 0xed, 0xae, 0xae,
        0x96, 0xd6, 0x96, 0xe7, 0xfb, 0x96, 0xf2, 0x9b,
        0xb4, 0xbd, 0xe9, 0xb2, 0xb6, 0xbd, 0xed, 0xed,
        0xdb, 0x9f, 0xb2, 0x96, 0x9a, 0xac, 0x96, 0xae,
        0xaf, 0x9e, 0x96, 0xd7, 0xda, 0x97,
        0x9b, 0x96, 0x96, 0x96, 0xfe, 0xe7, 0x97, 0x9e,
        0x9e, 0x96, 0xd3, 0xbf, 0x9b, 0x9f, 0x9f, 0xb6,
        0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
        0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
        0x96, 0x96, 0xed, 0xf9, 0xac, 0x97, 0x9a, 0x97,
        0x97, 0x9f, 0x9f, 0xab, 0xce, 0x96, 0xa6, 0x9a,
        0x9a, 0xa6, 0xb9, 0x96, 0x96, 0x96, 0x96, 0x96,
        0x96, 0x96, 0x96, 0xff, 0xfd, 0x9a, 0xa6, 0xa6,
        0x96, 0xb3, 0xb3, 0xa7, 0xd9, 0xe6, 0x96, 0x96,
        0x96, 0xa7, 0xa7, 0xa7, 0x96, 0xd9, 0xe6, 0x96,
        0x96, 0x96, 0xad, 0xcb, 0x96, 0x9d, 0x9f, 0x9a,
        0xcb, 0xd3, 0x9a, 0x9a, 0x97, 0x9d, 0x9a, 0xb7,
        0x96, 0x96, 0xff, 0xf5, 0xab, 0x9e, 0x9e, 0x96,
        0xac, 0xac, 0xcb, 0xef, 0xe5, 0x96, 0x96, 0x96,
        0xa7, 0xa7, 0xa7, 0xb7, 0xef, 0xe5, 0x96, 0x96,
        0x96, 0xab, 0xbd, 0x9b, 0x97, 0x9a, 0x9e, 0xb9,
        0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
        0x96, 0xff, 0xed, 0xb3, 0xae, 0xae, 0x96, 0xbe,
        0xbe, 0xa7, 0xd9, 0xe6, 0x96, 0x96, 0x96, 0xa7,
        0xb7, 0xcb, 0xa7, 0xd9, 0xe6, 0x96, 0x96, 0x96,
        0xab, 0xbe, 0x9b, 0x96, 0x9f, 0x9d, 0x9e, 0x97,
        0xba, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
        0xff, 0xdb, 0xcb, 0xac, 0xac, 0x96, 0xb9, 0xb9,
        0xbd, 0xee, 0xe5, 0x96, 0x96, 0x96, 0xa7, 0xa7,
        0xa7, 0xb4, 0xee, 0xe5, 0x96, 0x96, 0x96, 0xab,
        0xce, 0x97, 0x9e, 0x9d, 0x97, 0x96, 0x97, 0xb9,
        0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0xff,
        0xbe, 0xdd, 0x9f, 0x9f, 0xef, 0xf9, 0xad, 0xb7,
        0xfa, 0xe5, 0x96, 0x96, 0x96, 0xa7, 0xa7, 0x9a,
        0xb5, 0xfa, 0xe5, 0x96, 0x96, 0x96, 0xab, 0xbf,
        0x96, 0x97, 0x9f, 0x9e, 0xb5, 0x96, 0x96, 0x96,
        0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0xff, 0xb7,
        0xe9
    };
    const unsigned char* pImage = DiskImage_GetImagePointer(m_pDiskImage);
    validateRWTS16SectorsAreClear(pImage, 0, 1, 34, 15);
    validateRWTS16SectorContainsNibbles(pImage, expectedEncodedData, 0, 0);
}

TEST(DiskImage, WriteImage)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(0, 0, 1);
    DiskImage_WriteImage(m_pDiskImage, g_imageFilename);
    
    unsigned char expectedEncodedData[343];
    memset(expectedEncodedData, 0x96, sizeof(expectedEncodedData));
    const unsigned char* pImage = readDiskImageIntoMemory();
    validateRWTS16SectorsAreClear(pImage, 0, 1, 34, 15);
    validateRWTS16SectorContainsNibbles(pImage, expectedEncodedData, 0, 0);
}

TEST(DiskImage, FailFOpenInWriteImage)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(0, 0, 1);
    fopenFail(NULL);
        DiskImage_WriteImage(m_pDiskImage, g_imageFilename);
    fopenRestore();
    validateFileExceptionThrown();
}

TEST(DiskImage, FailFWriteInWriteImage)
{
    m_pDiskImage = DiskImage_Create();
    writeZeroSectors(0, 0, 1);
    fwriteFail(0);
        DiskImage_WriteImage(m_pDiskImage, g_imageFilename);
    fwriteRestore();
    validateFileExceptionThrown();
}

TEST(DiskImage, ReadObjectFile)
{
    m_pDiskImage = DiskImage_Create();
    createZeroSectorObjectFile();
    DiskImage_ReadObjectFile(m_pDiskImage, g_savFilename);
}

TEST(DiskImage, FailFOpenInReadObjectFile)
{
    m_pDiskImage = DiskImage_Create();
    DiskImage_ReadObjectFile(m_pDiskImage, g_savFilename);
    validateFileExceptionThrown();
}

TEST(DiskImage, FailHeaderReadInReadObjectFile)
{
    m_pDiskImage = DiskImage_Create();
    createZeroSectorObjectFile();
    
    freadFail(0);
        DiskImage_ReadObjectFile(m_pDiskImage, g_savFilename);
    freadRestore();
    validateFileExceptionThrown();
}

TEST(DiskImage, FailAllocationInReadObjectFile)
{
    m_pDiskImage = DiskImage_Create();
    createZeroSectorObjectFile();
    
    MallocFailureInject_FailAllocation(1);
    DiskImage_ReadObjectFile(m_pDiskImage, g_savFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(DiskImage, FailDataReadInReadObjectFile)
{
    m_pDiskImage = DiskImage_Create();
    createZeroSectorObjectFile();
    
    freadFail(0);
    freadToFail(2);
        DiskImage_ReadObjectFile(m_pDiskImage, g_savFilename);
    freadRestore();
    validateFileExceptionThrown();
}