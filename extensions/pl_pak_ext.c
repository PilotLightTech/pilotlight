/*
   pl_pak_ext.c
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] public api implementation
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

// libs
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

    // apis
    static const plMemoryI* gptMemory = NULL;
    static const plCompressI* gptCompress = NULL;
    static const plVfsI* gptVfs = NULL;

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
#define PL_PAK_VERSION 1

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plPakFileHeader
{
    char     acID[4];         // used to quickly verify file format
    uint32_t uPakVersion;     // version of the format. This should match with the code that parses it
    uint32_t uContentVersion; // version of the content. Used to possibly do patches and updates to files
    char     acPakName[50];   // name of the pak file
    uint32_t uNumEntries;     // number of directories/files
} plPakFileHeader;

typedef struct _plPakChildFile
{
    char       acFilePath[PL_PAK_MAX_PATH_LENGTH]; // path to the file (relative to the pak directory)
    bool       bCompressed;                        // true if compressed
    uint64_t   uUncompressedSize;                  // size before compression
    uint64_t   uCompressedSize;                    // size after compression
    uint64_t   uOffset;                            // offset pointing to start of binary data
    size_t     szOffsetPointer;
    uint8_t*   puCompressionStreamBuffer;
    plPakFile* ptParentPak;
} plPakChildFile;

typedef struct _plPakFile
{
    FILE*           ptFile;
    plPakChildFile* sbtEntries;
    plPakFileHeader tHeader;
    plPakEntryInfo* atEntries;
    uint8_t*        puCompressionBuffer;
    uint64_t        uCompressionBufferSize;
    plHashMap       tHashmap;
} plPakFile;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_pak_begin_packing(const char* pcFile, uint32_t uVersion, plPakFile** pptPak)
{
    plPakFile* ptPak = NULL;
    FILE* ptFile = fopen(pcFile, "wb");
    if(ptFile)
    {
        ptPak = PL_ALLOC(sizeof(plPakFile));
        memset(ptPak, 0, sizeof(plPakFile));
        ptPak->ptFile = ptFile;
        ptPak->tHeader.uPakVersion = PL_PAK_VERSION;
        ptPak->tHeader.uContentVersion = uVersion;
        strncpy(ptPak->tHeader.acID, "PAK", 4);
        pl_str_get_file_name(pcFile, ptPak->tHeader.acPakName, 50);

        // going ahead a writing header to file, though we will rewrite
        // it at the end of packing to update the entry count
        fwrite(&ptPak->tHeader, 1, sizeof(plPakFileHeader), ptFile);
    }
    *pptPak = ptPak;
    return ptPak != NULL;
}

bool      
pl_pak_add_from_disk(plPakFile* ptPak, const char* pcPakPath, const char* pcFilePath, bool bCompress)
{

    if(!gptVfs->does_file_exist(pcFilePath))
        return false;

    pl_sb_add(ptPak->sbtEntries);
    plPakChildFile* ptEntry = &pl_sb_back(ptPak->sbtEntries);
    ptEntry->bCompressed = bCompress;
    strncpy(ptEntry->acFilePath, pcPakPath, PL_PAK_MAX_PATH_LENGTH);

    // load file frome disk temporarily
    size_t szFileSize = gptVfs->get_file_size_str(pcFilePath);
    uint8_t* puBuffer = PL_ALLOC(szFileSize);
    memset(puBuffer, 0, szFileSize);
    plVfsFileHandle tHandle = gptVfs->open_file(pcFilePath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tHandle, puBuffer, &szFileSize);
    gptVfs->close_file(tHandle);

    // mark offset in file, then write
    ptEntry->uOffset = (uint32_t)ftell(ptPak->ptFile);
    ptEntry->uUncompressedSize = (uint32_t)szFileSize;

    if(bCompress)
    {
        if(szFileSize * 2 > ptPak->uCompressionBufferSize)
        {

            if(ptPak->puCompressionBuffer) // first allocation
            {
                PL_FREE(ptPak->puCompressionBuffer);
                ptPak->puCompressionBuffer = NULL;
            }
            
            ptPak->uCompressionBufferSize = (uint32_t)szFileSize * 2;
            ptPak->puCompressionBuffer = PL_ALLOC(ptPak->uCompressionBufferSize);
            memset(ptPak->puCompressionBuffer, 0, ptPak->uCompressionBufferSize);
        }

        uint64_t uCompressedSize = (uint64_t)gptCompress->compress(puBuffer, (uint32_t)szFileSize, ptPak->puCompressionBuffer, (uint32_t)ptPak->uCompressionBufferSize);
        PL_ASSERT(uCompressedSize < ptPak->uCompressionBufferSize);
        ptEntry->uCompressedSize = uCompressedSize;
        fwrite(ptPak->puCompressionBuffer, 1, (size_t)uCompressedSize, ptPak->ptFile);
    }
    else
    {
        fwrite(puBuffer, 1, szFileSize, ptPak->ptFile);
    }
    PL_FREE(puBuffer);

    return true;
}

void      
pl_pak_add_from_memory(plPakFile* ptPak, const char* pcPakPath, uint8_t* puFileData, size_t szFileByteSize, bool bCompress)
{

    PL_ASSERT(bCompress == false && "compression not supported yet");

    pl_sb_add(ptPak->sbtEntries);
    plPakChildFile* ptEntry = &pl_sb_back(ptPak->sbtEntries);
    ptEntry->bCompressed = bCompress;
    strncpy(ptEntry->acFilePath, pcPakPath, PL_PAK_MAX_PATH_LENGTH);

    // mark offset in file, then write
    ptEntry->uOffset = (uint32_t)ftell(ptPak->ptFile);
    ptEntry->uUncompressedSize = (uint32_t)szFileByteSize;

    if(bCompress)
    {

        if(szFileByteSize * 2 > ptPak->uCompressionBufferSize)
        {

            if(ptPak->puCompressionBuffer) // first allocation
            {
                PL_FREE(ptPak->puCompressionBuffer);
                ptPak->puCompressionBuffer = NULL;
            }
            
            ptPak->uCompressionBufferSize = (uint32_t)szFileByteSize * 2;
            ptPak->puCompressionBuffer = PL_ALLOC(ptPak->uCompressionBufferSize);
            memset(ptPak->puCompressionBuffer, 0, ptPak->uCompressionBufferSize);
        }

        uint64_t uCompressedSize = gptCompress->compress(puFileData, (uint32_t)szFileByteSize, ptPak->puCompressionBuffer, (uint32_t)ptPak->uCompressionBufferSize);
        PL_ASSERT(uCompressedSize < szFileByteSize * 2);
        ptEntry->uCompressedSize = uCompressedSize;
        fwrite(ptPak->puCompressionBuffer, 1, (size_t)uCompressedSize, ptPak->ptFile);
    }
    else
    {
        fwrite(puFileData, 1, szFileByteSize, ptPak->ptFile);
    }
}

void
pl_pak_unload(plPakFile** pptPak)
{
    plPakFile* ptPak = *pptPak;
    fclose(ptPak->ptFile);
    pl_sb_free(ptPak->sbtEntries);
    pl_hm_free(&ptPak->tHashmap);
    if(ptPak->atEntries)
    {
        PL_FREE(ptPak->atEntries);
        ptPak->atEntries = NULL;
    }

    if(ptPak->puCompressionBuffer)
    {
        PL_FREE(ptPak->puCompressionBuffer);
    }
    
    PL_FREE(ptPak);
    *pptPak = NULL;
}

void      
pl_pak_end_packing(plPakFile** pptPak)
{
    plPakFile* ptPak = *pptPak;
    uint32_t uEndLocation = (uint32_t)ftell(ptPak->ptFile);

    // jump back to beginning so we can update the entry count
    fseek(ptPak->ptFile, 0, SEEK_SET);
    ptPak->tHeader.uNumEntries = pl_sb_size(ptPak->sbtEntries);
    fwrite(&ptPak->tHeader, 1, sizeof(plPakFileHeader),  ptPak->ptFile);

    // jump to end to start writing entries
    fseek(ptPak->ptFile, 0, SEEK_END); // back to end

    for(uint32_t i = 0; i < ptPak->tHeader.uNumEntries; i++)
        fwrite(&ptPak->sbtEntries[i], 1, sizeof(plPakChildFile), ptPak->ptFile);

    pl_pak_unload(pptPak);
}

bool
pl_pak_load(const char* pcFile, plPakInfo* ptInfoOut, plPakFile** pptPak)
{
    // ensure file exists
    if(!gptVfs->does_file_exist(pcFile))
    {
        if(ptInfoOut)
            memset(ptInfoOut, 0, sizeof(plPakInfo));
        return false;
    }

    plVfsFileHandle tHandle = gptVfs->register_file(pcFile, true);
    pcFile = gptVfs->get_real_path(tHandle);
    plPakFile* ptPak = NULL;
    FILE* ptFile = fopen(pcFile, "rb");
    if(ptFile)
    {
        ptPak = PL_ALLOC(sizeof(plPakFile));
        memset(ptPak, 0, sizeof(plPakFile));
        ptPak->ptFile = ptFile;

        // pull in header
        fread(&ptPak->tHeader, sizeof(plPakFileHeader), 1, ptFile);

        // jump to start of file entries
        fseek(ptPak->ptFile, -(long)sizeof(plPakChildFile) * (long)ptPak->tHeader.uNumEntries, SEEK_END); // back to end

        // load entries
        pl_sb_resize(ptPak->sbtEntries, ptPak->tHeader.uNumEntries);
        for(uint32_t i = 0; i < ptPak->tHeader.uNumEntries; i++)
        {
            fread(&ptPak->sbtEntries[i], sizeof(plPakChildFile), 1, ptFile);
            ptPak->sbtEntries[i].ptParentPak = ptPak;
            pl_hm_insert_str(&ptPak->tHashmap, ptPak->sbtEntries[i].acFilePath, i);
        }

        // update info if requested
        if(ptInfoOut)
        {
            ptInfoOut->uContentVersion = ptPak->tHeader.uContentVersion;
            ptInfoOut->uEntryCount = ptPak->tHeader.uNumEntries;
            ptInfoOut->uPakVersion = ptPak->tHeader.uPakVersion;
            ptPak->atEntries = PL_ALLOC(sizeof(plPakChildFile) * ptInfoOut->uEntryCount);
            for(uint32_t i = 0; i < ptPak->tHeader.uNumEntries; i++)
            {
                ptPak->atEntries[i].pcFilePath = ptPak->sbtEntries[i].acFilePath;
                ptPak->atEntries[i].szSize = ptPak->sbtEntries[i].uUncompressedSize;
            }
            ptInfoOut->atEntries = ptPak->atEntries;
        }
    }

    *pptPak = ptPak;

    return ptPak != NULL;
}

bool
pl_pak_read_file(plPakFile* ptPak, const char* pcFile, uint8_t* puBufferOut, size_t* pSzFileByteSizeOut)
{

    plPakChildFile* ptEntry = NULL;

    uint64_t uIndex = 0;
    if(pl_hm_has_key_str_ex(&ptPak->tHashmap, pcFile, &uIndex))
    {
        ptEntry = &ptPak->sbtEntries[uIndex];
        PL_ASSERT(pl_str_equal(ptEntry->acFilePath, pcFile));
    }
    else
        return false;

    if(pSzFileByteSizeOut)
        *pSzFileByteSizeOut = ptEntry->uUncompressedSize;
    if(puBufferOut)
    {
        fseek(ptPak->ptFile, (long)ptEntry->uOffset, SEEK_SET);
        if(ptEntry->bCompressed)
        {

            if(ptEntry->uUncompressedSize > ptPak->uCompressionBufferSize)
            {

                if(ptPak->puCompressionBuffer) // first allocation
                {
                    PL_FREE(ptPak->puCompressionBuffer);
                    ptPak->puCompressionBuffer = NULL;
                }
                
                ptPak->uCompressionBufferSize = ptEntry->uUncompressedSize;
                ptPak->puCompressionBuffer = PL_ALLOC(ptPak->uCompressionBufferSize);
                memset(ptPak->puCompressionBuffer, 0, ptPak->uCompressionBufferSize);
            }
            fread(ptPak->puCompressionBuffer, 1, ptEntry->uCompressedSize, ptPak->ptFile);
            uint64_t uDecompressedSize = gptCompress->decompress(ptPak->puCompressionBuffer, (uint32_t)ptEntry->uCompressedSize, puBufferOut, (uint32_t)ptPak->uCompressionBufferSize);
            return uDecompressedSize == ptEntry->uUncompressedSize;
        }
        else
        {
            fread(puBufferOut, 1, ptEntry->uUncompressedSize, ptPak->ptFile);
        }
    }
    return true;
}

plPakChildFile*
pl_pak_open_file(plPakFile* ptPak, const char* pcFile)
{
    plPakChildFile* ptEntry = NULL;

    uint64_t uIndex = 0;
    if(pl_hm_has_key_str_ex(&ptPak->tHashmap, pcFile, &uIndex))
    {
        ptEntry = &ptPak->sbtEntries[uIndex];
        PL_ASSERT(pl_str_equal(ptEntry->acFilePath, pcFile));
    }
    else
        return NULL;


    fseek(ptPak->ptFile, (long)ptEntry->uOffset, SEEK_SET);
    if(ptEntry->bCompressed)
    {

        ptEntry->puCompressionStreamBuffer = PL_ALLOC(ptEntry->uUncompressedSize);
        memset(ptEntry->puCompressionStreamBuffer, 0, ptEntry->uUncompressedSize);

        if(ptEntry->uUncompressedSize > ptPak->uCompressionBufferSize)
        {

            if(ptPak->puCompressionBuffer) // first allocation
            {
                PL_FREE(ptPak->puCompressionBuffer);
                ptPak->puCompressionBuffer = NULL;
            }
            
            ptPak->uCompressionBufferSize = ptEntry->uUncompressedSize;
            ptPak->puCompressionBuffer = PL_ALLOC(ptPak->uCompressionBufferSize);
            memset(ptPak->puCompressionBuffer, 0, ptPak->uCompressionBufferSize);
        }
        fread(ptPak->puCompressionBuffer, 1, ptEntry->uCompressedSize, ptPak->ptFile);
        uint64_t uDecompressedSize = gptCompress->decompress(ptPak->puCompressionBuffer, (uint32_t)ptEntry->uCompressedSize, ptEntry->puCompressionStreamBuffer, (uint32_t)ptEntry->uUncompressedSize);
    }
    else
    {
        fseek(ptPak->ptFile, (long)ptEntry->uOffset, SEEK_SET);
    }
    return ptEntry;
}

void
pl_pak_close_file(plPakChildFile* ptEntry)
{
    if(ptEntry->puCompressionStreamBuffer)
    {
        PL_FREE(ptEntry->puCompressionStreamBuffer);
        ptEntry->puCompressionStreamBuffer = NULL;
    }
    ptEntry->szOffsetPointer = 0;
}

size_t
pl_pak_read_file_stream(plPakChildFile* ptEntry, size_t szElementSize, size_t szElementCount, void* pDataOut)
{
    if(pDataOut == NULL)
        return 0;

    fseek(ptEntry->ptParentPak->ptFile, (long)ptEntry->uOffset, SEEK_SET);
    if(ptEntry->bCompressed)
    {
        size_t szPotentialOffsetPointer = ptEntry->szOffsetPointer + szElementCount * szElementSize;
        if(szPotentialOffsetPointer >= ptEntry->uUncompressedSize)
            return 0;
        memcpy(pDataOut, &ptEntry->puCompressionStreamBuffer[ptEntry->szOffsetPointer], szElementCount * szElementSize);
        ptEntry->szOffsetPointer = szPotentialOffsetPointer;
        return szElementCount * szElementSize;
    }
    else
    {
        fseek(ptEntry->ptParentPak->ptFile, (long)(ptEntry->uOffset + ptEntry->szOffsetPointer), SEEK_SET);
        return fread(pDataOut, szElementSize, szElementCount, ptEntry->ptParentPak->ptFile);
    }
}

size_t
pl_pak_get_file_stream_position(plPakChildFile* ptEntry)
{
    return ptEntry->szOffsetPointer;
}

void
pl_pak_reset_file_stream_position(plPakChildFile* ptEntry)
{
    ptEntry->szOffsetPointer = 0;
}

void
pl_pak_set_file_stream_position(plPakChildFile* ptEntry, size_t szOffset)
{
    ptEntry->szOffsetPointer = szOffset;
}

void
pl_pak_increment_file_stream_position(plPakChildFile* ptEntry, size_t szDelta)
{
    ptEntry->szOffsetPointer += szDelta;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
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

        #ifndef PL_DISABLE_OBSOLETE
        .get_file = pl_pak_read_file,
        #endif // PL_DISABLE_OBSOLETE
    };
    pl_set_api(ptApiRegistry, plPakI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptVfs      = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptCompress = pl_get_api_latest(ptApiRegistry, plCompressI);
    #endif
}

PL_EXPORT void
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