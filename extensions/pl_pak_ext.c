/*
   pl_pak_ext.c
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The implementation is simple. The only caveats here are that we store
        the file entries so that they can be added at the end of the file at
        the end of packing.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>  // files
#include <string.h> // memset
#include "pl.h"
#include "pl_pak_ext.h"

// extensions
#include "pl_compress_ext.h"
#include "pl_vfs_ext.h"
#include "pl_platform_ext.h"

// libs
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

    // apis
    static const plMemoryI*   gptMemory   = NULL;
    static const plCompressI* gptCompress = NULL;
    static const plVfsI*      gptVfs      = NULL;
    static const plFileI*     gptFile     = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    #include "pl_ds.h"
#endif

#define PL_PAK_MAX_PATH_LENGTH 255
#define PL_PAK_VERSION 2
#define PL_PAK_HEADER_SIZE 32
#define PL_PAK_ENTRY_SIZE 280

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

// Disk v2 uses explicit little-endian integers, never pointers or struct padding.
// Legacy v1 support is restricted to the historical little-endian 64-bit layout.
typedef struct _plPakChildFile
{
    char                    acFilePath[PL_PAK_MAX_PATH_LENGTH];
    bool                    bCompressed;
    uint64_t                uUncompressedSize;
    uint64_t                uCompressedSize;
    uint64_t                uOffset;
    uint64_t                uPosition;
    uint8_t*                puCompressionStreamBuffer;
    plPakFile*              ptParentPak;
    struct _plPakChildFile* ptNext;
} plPakChildFile;

typedef struct _plPakFile
{
    plFileStream*   ptFile;
    plPakChildFile* sbtEntries;
    plPakChildFile* ptOpenFiles;
    plPakEntryInfo* atEntries;
    plHashMap       tHashmap;
    uint32_t        uContentVersion;
    uint32_t        uPakVersion;
    char*           pcDestination;
    char*           pcTemporary;
    bool            bPacking;
    bool            bFailed;
} plPakFile;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline uint64_t        pl__pak_read_uint(const uint8_t* puData, uint32_t uBytes);
static inline void            pl__pak_write_uint(uint8_t* puData, uint64_t uValue, uint32_t uBytes);
static inline bool            pl__pak_valid_path(const char* pcPath);
static inline plPakChildFile* pl__pak_find(plPakFile* ptPak, const char* pcPath);
static inline bool            pl__pak_load_failure(plPakFile* ptPak);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_pak_close_file(plPakChildFile* ptEntry)
{
    if(!ptEntry)
        return;
    plPakChildFile** pptLink = &ptEntry->ptParentPak->ptOpenFiles;
    while(*pptLink && *pptLink != ptEntry)
        pptLink = &(*pptLink)->ptNext;
    if(*pptLink)
        *pptLink = ptEntry->ptNext;
    if(ptEntry->puCompressionStreamBuffer)
        PL_FREE(ptEntry->puCompressionStreamBuffer);
    PL_FREE(ptEntry);
}

void
pl_pak_unload(plPakFile** pptPak)
{
    if(!pptPak || !*pptPak)
        return;
    plPakFile* ptPak = *pptPak;
    while(ptPak->ptOpenFiles)
        pl_pak_close_file(ptPak->ptOpenFiles);
    if(ptPak->ptFile)
        gptFile->close_stream(ptPak->ptFile);
    if(ptPak->pcTemporary && ptPak->pcTemporary[0])
        gptFile->remove(ptPak->pcTemporary);
    if(ptPak->pcTemporary)
    {
        PL_FREE(ptPak->pcTemporary);
    }
    if(ptPak->pcDestination)
    {
        PL_FREE(ptPak->pcDestination);
    }
    pl_sb_free(ptPak->sbtEntries);
    pl_hm_free(&ptPak->tHashmap);
    if(ptPak->atEntries)
    {
        PL_FREE(ptPak->atEntries);
    }
    PL_FREE(ptPak);
    *pptPak = NULL;
}

bool
pl_pak_begin_packing(const char* pcFile, uint32_t uVersion, plPakFile** pptPak)
{
    if(!pptPak)
        return false;
    *pptPak = NULL;
    if(!pcFile || !pcFile[0] || strlen(pcFile) > SIZE_MAX - 64)
        return false;
    plPakFile* ptPak = PL_ALLOC(sizeof(plPakFile));
    if(!ptPak)
        return false;
    memset(ptPak, 0, sizeof(*ptPak));
    *pptPak = ptPak;
    size_t szPath = strlen(pcFile) + 1;
    ptPak->pcDestination = PL_ALLOC(szPath);
    ptPak->pcTemporary = PL_ALLOC(szPath + 64);
    if(ptPak->pcTemporary)
        ptPak->pcTemporary[0] = 0;
    if(!ptPak->pcDestination || !ptPak->pcTemporary)
    {
        pl_pak_unload(pptPak);
        return false;
    }
    memcpy(ptPak->pcDestination, pcFile, szPath);
    ptPak->ptFile = gptFile->open_temp_stream(pcFile, ptPak->pcTemporary, szPath + 64);
    if(!ptPak->ptFile)
    {
        pl_pak_unload(pptPak);
        return false;
    }
    ptPak->uContentVersion = uVersion;
    ptPak->uPakVersion = PL_PAK_VERSION;
    ptPak->bPacking = true;
    // Leave an invalid header until successful finalization.
    uint8_t auHeader[PL_PAK_HEADER_SIZE] = {0};
    *pptPak = ptPak;
    if(gptFile->write_stream(ptPak->ptFile, auHeader, sizeof(auHeader)) != sizeof(auHeader))
    {
        pl_pak_unload(pptPak);
        return false;
    }
    return true;
}

bool
pl_pak_add_from_memory(plPakFile* ptPak, const char* pcPakPath, uint8_t* puData, size_t szSize, bool bCompress)
{
    if(!ptPak || !ptPak->bPacking || ptPak->bFailed)
        return false;
    if(!pl__pak_valid_path(pcPakPath) || pl__pak_find(ptPak, pcPakPath) || (szSize && !puData))
    {
        ptPak->bFailed = true;
        return false;
    }
    plPakChildFile tEntry = {0};
    strcpy(tEntry.acFilePath, pcPakPath);
    tEntry.uUncompressedSize = szSize;
    if(!gptFile->tell_stream(ptPak->ptFile, &tEntry.uOffset))
    {
        ptPak->bFailed = true;
        return false;
    }
    // The codec is 32-bit; large entries and empty entries remain uncompressed.
    uint8_t* puCompressed = NULL;
    size_t szWrite = szSize;
    uint8_t* puWrite = puData;
    if(bCompress && szSize && szSize <= UINT32_MAX / 2)
    {
        uint32_t uNeeded = gptCompress->compress(puData, (uint32_t)szSize, NULL, 0);
        if(uNeeded && uNeeded < szSize)
        {
            puCompressed = PL_ALLOC(uNeeded);
            if(!puCompressed)
            {
                ptPak->bFailed = true;
                return false;
            }
            uint32_t uActual = gptCompress->compress(puData, (uint32_t)szSize, puCompressed, uNeeded);
            if(uActual != uNeeded)
            {
                PL_FREE(puCompressed);
                ptPak->bFailed = true;
                return false;
            }
            tEntry.bCompressed = true;
            tEntry.uCompressedSize = uActual;
            puWrite = puCompressed;
            szWrite = uActual;
        }
    }
    bool bSuccess = gptFile->write_stream(ptPak->ptFile, puWrite, szWrite) == szWrite;
    if(puCompressed)
    {
        PL_FREE(puCompressed);
    }
    if(!bSuccess)
    {
        ptPak->bFailed = true;
        return false;
    }
    pl_sb_push(ptPak->sbtEntries, tEntry);
    pl_hm_insert_str(&ptPak->tHashmap, tEntry.acFilePath, pl_sb_size(ptPak->sbtEntries) - 1);
    return true;
}

bool
pl_pak_add_from_disk(plPakFile* ptPak, const char* pcPakPath, const char* pcFilePath, bool bCompress)
{
    if(!ptPak || !ptPak->bPacking || ptPak->bFailed)
        return false;
    if(!pl__pak_valid_path(pcPakPath) || pl__pak_find(ptPak, pcPakPath) || !pcFilePath || !gptVfs->does_file_exist(pcFilePath))
    {
        ptPak->bFailed = true;
        return false;
    }
    plVfsFileHandle tFile = gptVfs->open_file(pcFilePath, PL_VFS_FILE_MODE_READ);
    bool bSuccess = gptVfs->is_file_valid(tFile) && gptVfs->is_file_open(tFile);
    if(!bSuccess)
    {
        ptPak->bFailed = true;
        return false;
    }
    size_t szSize = 0;
    bSuccess = gptVfs->read_file(tFile, NULL, &szSize) == PL_VFS_RESULT_SUCCESS;
    uint8_t* puData = NULL;
    if(bSuccess && bCompress && szSize && szSize <= UINT32_MAX / 2)
    {
        puData = PL_ALLOC(szSize);
        bSuccess = puData && gptVfs->read_file_stream(tFile, 1, szSize, puData) == szSize;
        if(bSuccess)
            bSuccess = pl_pak_add_from_memory(ptPak, pcPakPath, puData, szSize, true);
    }
    else if(bSuccess)
    {
        // Uncompressed/large sources need only one MiB of temporary memory.
        const size_t szChunkSize = 1024 * 1024;
        puData = PL_ALLOC(szChunkSize);
        plPakChildFile tEntry = {0};
        strcpy(tEntry.acFilePath, pcPakPath);
        tEntry.uUncompressedSize = szSize;
        bSuccess = puData && gptFile->tell_stream(ptPak->ptFile, &tEntry.uOffset);
        size_t szRemaining = szSize;
        while(bSuccess && szRemaining)
        {
            size_t szChunk = szRemaining < szChunkSize ? szRemaining : szChunkSize;
            bSuccess = gptVfs->read_file_stream(tFile, 1, szChunk, puData) == szChunk &&
                gptFile->write_stream(ptPak->ptFile, puData, szChunk) == szChunk;
            szRemaining -= szChunk;
        }
        if(bSuccess)
        {
            pl_sb_push(ptPak->sbtEntries, tEntry);
            pl_hm_insert_str(&ptPak->tHashmap, tEntry.acFilePath, pl_sb_size(ptPak->sbtEntries) - 1);
        }
    }
    gptVfs->close_file(tFile);
    if(puData)
        PL_FREE(puData);
    if(!bSuccess)
        ptPak->bFailed = true;
    return bSuccess;
}

bool
pl_pak_end_packing(plPakFile** pptPak)
{
    if(!pptPak || !*pptPak)
        return false;
    plPakFile* ptPak = *pptPak;
    if(!ptPak->bPacking)
        return false;
    uint64_t uTableOffset = 0;
    bool bSuccess = ptPak->bPacking && !ptPak->bFailed && gptFile->tell_stream(ptPak->ptFile, &uTableOffset);
    for(uint32_t i = 0; bSuccess && i < pl_sb_size(ptPak->sbtEntries); i++)
    {
        plPakChildFile* ptEntry = &ptPak->sbtEntries[i];
        uint8_t auEntry[PL_PAK_ENTRY_SIZE] = {0};
        memcpy(auEntry, ptEntry->acFilePath, strlen(ptEntry->acFilePath));
        auEntry[255] = ptEntry->bCompressed ? 1 : 0;
        pl__pak_write_uint(auEntry + 256, ptEntry->uUncompressedSize, 8);
        pl__pak_write_uint(auEntry + 264, ptEntry->uCompressedSize, 8);
        pl__pak_write_uint(auEntry + 272, ptEntry->uOffset, 8);
        bSuccess = gptFile->write_stream(ptPak->ptFile, auEntry, sizeof(auEntry)) == sizeof(auEntry);
    }
    if(bSuccess)
    {
        uint8_t auHeader[PL_PAK_HEADER_SIZE] = {'P', 'A', 'K', 0};
        pl__pak_write_uint(auHeader + 4, PL_PAK_VERSION, 4);
        pl__pak_write_uint(auHeader + 8, ptPak->uContentVersion, 4);
        pl__pak_write_uint(auHeader + 12, pl_sb_size(ptPak->sbtEntries), 4);
        pl__pak_write_uint(auHeader + 16, uTableOffset, 8);
        bSuccess = gptFile->seek_stream(ptPak->ptFile, 0, PL_FILE_SEEK_START) &&
            gptFile->write_stream(ptPak->ptFile, auHeader, sizeof(auHeader)) == sizeof(auHeader) &&
            gptFile->flush_stream(ptPak->ptFile);
    }
    if(!gptFile->close_stream(ptPak->ptFile))
        bSuccess = false;
    ptPak->ptFile = NULL;
    if(bSuccess)
        bSuccess = gptFile->replace_file(ptPak->pcTemporary, ptPak->pcDestination) == PL_FILE_RESULT_SUCCESS;
    if(bSuccess)
        ptPak->pcTemporary[0] = 0;
    pl_pak_unload(pptPak);
    return bSuccess;
}

bool
pl_pak_load(const char* pcFile, plPakInfo* ptInfoOut, plPakFile** pptPak)
{
    if(ptInfoOut)
        memset(ptInfoOut, 0, sizeof(*ptInfoOut));
    if(!pptPak)
        return false;
    *pptPak = NULL;
    if(!pcFile || !gptVfs->does_file_exist(pcFile))
        return false;
    plVfsFileHandle tHandle = gptVfs->register_file(pcFile, true);
    if(!gptVfs->is_file_valid(tHandle))
        return false;
    const char* pcRealPath = gptVfs->get_real_path(tHandle);
    plFileStream* ptFile = gptFile->open_stream(pcRealPath, PL_FILE_MODE_READ);
    if(!ptFile)
        return false;
    plPakFile* ptPak = PL_ALLOC(sizeof(plPakFile));
    if(!ptPak)
    {
        gptFile->close_stream(ptFile);
        return false;
    }
    memset(ptPak, 0, sizeof(*ptPak));
    ptPak->ptFile = ptFile;
    uint8_t auHeader[68] = {0};
    uint64_t uFileSize = 0;
    uint64_t uTableOffset = 0;
    uint32_t uEntryCount = 0;
    uint32_t uEntrySize = PL_PAK_ENTRY_SIZE;
    uint32_t uHeaderSize = PL_PAK_HEADER_SIZE;
    if(!gptFile->size_stream(ptFile, &uFileSize) || uFileSize < PL_PAK_HEADER_SIZE || gptFile->read_stream(ptFile, auHeader, PL_PAK_HEADER_SIZE) != PL_PAK_HEADER_SIZE || memcmp(auHeader, "PAK\0", 4) != 0)
        return pl__pak_load_failure(ptPak);
    ptPak->uPakVersion = (uint32_t)pl__pak_read_uint(auHeader + 4, 4);
    ptPak->uContentVersion = (uint32_t)pl__pak_read_uint(auHeader + 8, 4);
    if(ptPak->uPakVersion == 1)
    {
        uHeaderSize = 68;
        uEntrySize = 304;
        if(uFileSize < uHeaderSize || gptFile->read_stream(ptFile, auHeader + 32, 36) != 36)
            return pl__pak_load_failure(ptPak);
        uEntryCount = (uint32_t)pl__pak_read_uint(auHeader + 64, 4);
        uint64_t uTableBytes = (uint64_t)uEntryCount * uEntrySize;
        if(uTableBytes > uFileSize - uHeaderSize)
            return pl__pak_load_failure(ptPak);
        uTableOffset = uFileSize - uTableBytes;
    }
    else if(ptPak->uPakVersion == PL_PAK_VERSION)
    {
        uEntryCount = (uint32_t)pl__pak_read_uint(auHeader + 12, 4);
        uTableOffset = pl__pak_read_uint(auHeader + 16, 8);
        if(pl__pak_read_uint(auHeader + 24, 8) != 0)
            return pl__pak_load_failure(ptPak);
    }
    else
        return pl__pak_load_failure(ptPak);
    #if SIZE_MAX <= UINT32_MAX
    if(uEntryCount > SIZE_MAX / sizeof(plPakChildFile))
        return pl__pak_load_failure(ptPak);
    #endif
    if(uTableOffset < uHeaderSize || uTableOffset > uFileSize || (uint64_t)uEntryCount * uEntrySize != uFileSize - uTableOffset || !gptFile->seek_stream(ptFile, (int64_t)uTableOffset, PL_FILE_SEEK_START))
        return pl__pak_load_failure(ptPak);
    uint64_t uPreviousEnd = uHeaderSize;
    for(uint32_t i = 0; i < uEntryCount; i++)
    {
        uint8_t auEntry[304];
        plPakChildFile tEntry = {0};
        if(gptFile->read_stream(ptFile, auEntry, uEntrySize) != uEntrySize || !memchr(auEntry, 0, 255) || auEntry[255] > 1)
            return pl__pak_load_failure(ptPak);
        memcpy(tEntry.acFilePath, auEntry, 255);
        tEntry.bCompressed = auEntry[255] != 0;
        tEntry.uUncompressedSize = pl__pak_read_uint(auEntry + 256, 8);
        tEntry.uCompressedSize = pl__pak_read_uint(auEntry + 264, 8);
        tEntry.uOffset = pl__pak_read_uint(auEntry + 272, 8);
        uint64_t uStoredSize = tEntry.bCompressed ? tEntry.uCompressedSize : tEntry.uUncompressedSize;
        if(!pl__pak_valid_path(tEntry.acFilePath) || pl__pak_find(ptPak, tEntry.acFilePath) || tEntry.uOffset < uPreviousEnd || tEntry.uOffset > uTableOffset || uStoredSize > uTableOffset - tEntry.uOffset || tEntry.uUncompressedSize > SIZE_MAX || (tEntry.bCompressed && (!tEntry.uUncompressedSize || tEntry.uCompressedSize < 22 || tEntry.uCompressedSize > UINT32_MAX || tEntry.uUncompressedSize > UINT32_MAX)))
            return pl__pak_load_failure(ptPak);
        uPreviousEnd = tEntry.uOffset + uStoredSize;
        pl_sb_push(ptPak->sbtEntries, tEntry);
        pl_hm_insert_str(&ptPak->tHashmap, tEntry.acFilePath, pl_sb_size(ptPak->sbtEntries) - 1);
    }
    if(ptInfoOut)
    {
        if(uEntryCount)
        {
            ptPak->atEntries = PL_ALLOC(sizeof(plPakEntryInfo) * uEntryCount);
            if(!ptPak->atEntries)
                return pl__pak_load_failure(ptPak);
            for(uint32_t i = 0; i < uEntryCount; i++)
            {
                ptPak->atEntries[i].pcFilePath = ptPak->sbtEntries[i].acFilePath;
                ptPak->atEntries[i].szSize = (size_t)ptPak->sbtEntries[i].uUncompressedSize;
            }
        }
        *ptInfoOut = (plPakInfo){ptPak->uPakVersion, ptPak->uContentVersion, uEntryCount, ptPak->atEntries};
    }
    *pptPak = ptPak;
    return true;
}

bool
pl_pak_read_file(plPakFile* ptPak, const char* pcFile, uint8_t* puBuffer, size_t* pszSize)
{
    plPakChildFile* ptEntry = pl__pak_find(ptPak, pcFile);
    if(!ptEntry || ptPak->bPacking)
        return false;
    if(pszSize)
        *pszSize = (size_t)ptEntry->uUncompressedSize;
    if(!puBuffer)
        return true;
    if(!gptFile->seek_stream(ptPak->ptFile, (int64_t)ptEntry->uOffset, PL_FILE_SEEK_START))
        return false;
    if(!ptEntry->bCompressed)
        return gptFile->read_stream(ptPak->ptFile, puBuffer, (size_t)ptEntry->uUncompressedSize) == ptEntry->uUncompressedSize;
    uint8_t* puCompressed = PL_ALLOC((size_t)ptEntry->uCompressedSize);
    if(!puCompressed)
        return false;
    bool bSuccess = gptFile->read_stream(ptPak->ptFile, puCompressed, (size_t)ptEntry->uCompressedSize) == ptEntry->uCompressedSize &&
        gptCompress->decompress(puCompressed, (uint32_t)ptEntry->uCompressedSize, puBuffer, (uint32_t)ptEntry->uUncompressedSize) == ptEntry->uUncompressedSize;
    PL_FREE(puCompressed);
    return bSuccess;
}

plPakChildFile*
pl_pak_open_file(plPakFile* ptPak, const char* pcFile)
{
    plPakChildFile* ptSource = pl__pak_find(ptPak, pcFile);
    if(!ptSource || ptPak->bPacking)
        return NULL;
    plPakChildFile* ptEntry = PL_ALLOC(sizeof(*ptEntry));
    if(!ptEntry)
        return NULL;
    *ptEntry = *ptSource;
    ptEntry->ptParentPak = ptPak;
    if(ptEntry->bCompressed)
    {
        ptEntry->puCompressionStreamBuffer = PL_ALLOC((size_t)ptEntry->uUncompressedSize + (ptEntry->uUncompressedSize == 0));
        if(!ptEntry->puCompressionStreamBuffer || !pl_pak_read_file(ptPak, pcFile, ptEntry->puCompressionStreamBuffer, NULL))
        {
            if(ptEntry->puCompressionStreamBuffer)
            {
                PL_FREE(ptEntry->puCompressionStreamBuffer);
            }
            PL_FREE(ptEntry);
            return NULL;
        }
    }
    ptEntry->ptNext = ptPak->ptOpenFiles;
    ptPak->ptOpenFiles = ptEntry;
    return ptEntry;
}

size_t
pl_pak_read_file_stream(plPakChildFile* ptEntry, size_t szElementSize, size_t szElementCount, void* pData)
{
    if(!ptEntry || !pData || !szElementSize || !szElementCount || ptEntry->uPosition > ptEntry->uUncompressedSize)
        return 0;
    uint64_t uAvailable = (ptEntry->uUncompressedSize - ptEntry->uPosition) / szElementSize;
    if(szElementCount > uAvailable)
        szElementCount = (size_t)uAvailable;
    if(szElementCount > SIZE_MAX / szElementSize)
        return 0;
    size_t szBytes = szElementCount * szElementSize;
    if(!szBytes)
        return 0;
    if(ptEntry->bCompressed)
        memcpy(pData, ptEntry->puCompressionStreamBuffer + (size_t)ptEntry->uPosition, szBytes);
    else
    {
        if(!gptFile->seek_stream(ptEntry->ptParentPak->ptFile, (int64_t)(ptEntry->uOffset + ptEntry->uPosition), PL_FILE_SEEK_START))
            return 0;
        szBytes = gptFile->read_stream(ptEntry->ptParentPak->ptFile, pData, szBytes);
    }
    ptEntry->uPosition += szBytes;
    return szBytes / szElementSize;
}

static inline size_t
pl_pak_get_file_stream_position(plPakChildFile* ptEntry)
{
    return ptEntry ? (size_t)ptEntry->uPosition : 0;
}
static inline void
pl_pak_reset_file_stream_position(plPakChildFile* ptEntry)
{
    if(ptEntry)
        ptEntry->uPosition = 0;
}
static inline bool
pl_pak_set_file_stream_position(plPakChildFile* ptEntry, size_t szOffset)
{
    if(!ptEntry || szOffset > ptEntry->uUncompressedSize)
        return false;
    ptEntry->uPosition = szOffset;
    return true;
}
static inline bool
pl_pak_increment_file_stream_position(plPakChildFile* ptEntry, size_t szDelta)
{
    if(!ptEntry || ptEntry->uPosition > ptEntry->uUncompressedSize || szDelta > ptEntry->uUncompressedSize - ptEntry->uPosition)
        return false;
    ptEntry->uPosition += szDelta;
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static inline uint64_t
pl__pak_read_uint(const uint8_t* puData, uint32_t uBytes)
{
    uint64_t uValue = 0;
    for(uint32_t i = 0; i < uBytes; i++)
    {
        uValue |= (uint64_t)puData[i] << (8 * i);
    }
    return uValue;
}

static inline void
pl__pak_write_uint(uint8_t* puData, uint64_t uValue, uint32_t uBytes)
{
    for(uint32_t i = 0; i < uBytes; i++) puData[i] = (uint8_t)(uValue >> (8 * i));
}

static inline bool
pl__pak_valid_path(const char* pcPath)
{
    if(!pcPath || !pcPath[0] || pcPath[0] == '/' || strlen(pcPath) >= PL_PAK_MAX_PATH_LENGTH)
        return false;
    const char* pcStart = pcPath;
    for(const char* p = pcPath; ; p++)
    {
        if(*p == '\\' || *p == ':' || ((unsigned char)*p < 32 && *p))
            return false;
        if(*p == '/' || !*p)
        {
            size_t n = (size_t)(p - pcStart);
            if(n == 0 || (n == 1 && pcStart[0] == '.') || (n == 2 && pcStart[0] == '.' && pcStart[1] == '.'))
                return false;
            if(!*p)
                break;
            pcStart = p + 1;
        }
    }
    return true;
}

static inline plPakChildFile*
pl__pak_find(plPakFile* ptPak, const char* pcPath)
{
    if(!ptPak || !pcPath)
        return NULL;
    uint64_t uIndex = 0;
    if(!pl_hm_has_key_str_ex(&ptPak->tHashmap, pcPath, &uIndex))
        return NULL;
    if(strcmp(ptPak->sbtEntries[uIndex].acFilePath, pcPath) == 0)
        return &ptPak->sbtEntries[uIndex];
    // A hash collision must not alias another entry.
    for(uint32_t i = 0; i < pl_sb_size(ptPak->sbtEntries); i++)
        if(strcmp(ptPak->sbtEntries[i].acFilePath, pcPath) == 0)
            return &ptPak->sbtEntries[i];
    return NULL;
}

static inline bool
pl__pak_load_failure(plPakFile* ptPak)
{
    pl_pak_unload(&ptPak);
    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_pak_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plPakI tApi = {
        .begin_packing                  = pl_pak_begin_packing,
        .add_from_disk                  = pl_pak_add_from_disk,
        .add_from_memory                = pl_pak_add_from_memory,
        .end_packing                    = pl_pak_end_packing,
        .load                           = pl_pak_load,
        .unload                         = pl_pak_unload,
        .open_file                      = pl_pak_open_file,
        .close_file                     = pl_pak_close_file,
        .read_file                      = pl_pak_read_file,
        .read_file_stream               = pl_pak_read_file_stream,
        .get_file_stream_position       = pl_pak_get_file_stream_position,
        .reset_file_stream_position     = pl_pak_reset_file_stream_position,
        .set_file_stream_position       = pl_pak_set_file_stream_position,
        .increment_file_stream_position = pl_pak_increment_file_stream_position,
    };
    pl_set_api(ptApiRegistry, plPakI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptVfs      = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptFile     = pl_get_api_latest(ptApiRegistry, plFileI);
        gptCompress = pl_get_api_latest(ptApiRegistry, plCompressI);
    #endif
}

void
pl_unload_pak_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plPakI* ptApi = pl_get_api_latest(ptApiRegistry, plPakI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif
