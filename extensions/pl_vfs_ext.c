/*
   pl_vfs_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal forward declarations
// [SECTION] enums
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pl.h"
#include "pl_vfs_ext.h"

// extensions
#include "pl_platform_ext.h"
#include "pl_pak_ext.h"

// libs
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

    // apis
    static const plMemoryI* gptMemory = NULL;
    static const plFileI*   gptFile = NULL;
    static const plPakI*    gptPak = NULL;

    static plIO* gptIO = NULL;
    #ifndef PL_DS_ALLOC
        
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    #include "pl_ds.h"
#endif

#define PL_VFS_MAX_PATH_LENGTH 255

//-----------------------------------------------------------------------------
// [SECTION] internal forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plVfsFile                  plVfsFile;
typedef struct _plVfsFileSystem            plVfsFileSystem;
typedef struct _plVirtualFileSystemContext plVirtualFileSystemContext;

// enums/flags
typedef int plFileSystemType;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plFileSystemType
{
    PL_FILE_SYSTEM_TYPE_PHYSICAL = 0,
    PL_FILE_SYSTEM_TYPE_DIRECTORY,
    PL_FILE_SYSTEM_TYPE_PAK,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVfsFile
{
    uint32_t uFileSystem;
    uint32_t _uGeneration;
    char     acPath[PL_VFS_MAX_PATH_LENGTH];
    bool     bOpen;

    // physical data
    char  acPhysicalPath[PL_VFS_MAX_PATH_LENGTH];
    FILE* ptFile;
} plVfsFile;

typedef struct _plVfsFileSystem
{
    plFileSystemType tType;
    char             acDirectory[PL_VFS_MAX_PATH_LENGTH];
    uint32_t*        sbuFileIndices;

    // physical directory data
    char acPhysicalDirectory[PL_VFS_MAX_PATH_LENGTH];
    
    // pak directory data
    plPakFile* ptPakFile;
} plVfsFileSystem;

typedef struct _plVirtualFileSystemContext
{
    plHashMap32      tFileHashmap;
    plVfsFile*       sbtFiles;
    plVfsFileSystem* sbtFileSystems;
} plVirtualFileSystemContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plVirtualFileSystemContext* gptVfsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_vfs_initialize(void)
{
    plVfsFileSystem tSystem = {
        .tType = PL_FILE_SYSTEM_TYPE_PHYSICAL
    };
    pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);
}

void
pl_vfs_cleanup(void)
{

    const uint32_t uFileSystemCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 0; i < uFileSystemCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(ptFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
        {
            gptPak->unload(&ptFileSystem->ptPakFile);
        }
        pl_sb_free(ptFileSystem->sbuFileIndices);
    }

    pl_sb_free(gptVfsCtx->sbtFiles);
    pl_hm32_free(&gptVfsCtx->tFileHashmap);
    pl_sb_free(gptVfsCtx->sbtFileSystems);
}

bool
pl_vfs_mount_directory(const char* pcDirectory, const char* pcPhysicalDirectory)
{
    plVfsFileSystem tSystem = {
        .tType = PL_FILE_SYSTEM_TYPE_DIRECTORY
    };
    char* pcResult = strncpy(tSystem.acDirectory, pcDirectory, PL_VFS_MAX_PATH_LENGTH);
    size_t szLen = strnlen(pcResult, PL_VFS_MAX_PATH_LENGTH);
    pcResult[szLen] = '/';
    pcResult[szLen + 1] = 0;
    strncpy(tSystem.acPhysicalDirectory, pcPhysicalDirectory, PL_VFS_MAX_PATH_LENGTH);
    pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);
    return true;
}

bool
pl_vfs_mount_pak(const char* pcDirectory, const char* pcPakFilePath)
{
    plVfsFileSystem tSystem = {
        .tType = PL_FILE_SYSTEM_TYPE_PAK
    };
    char* pcResult = strncpy(tSystem.acDirectory, pcDirectory, PL_VFS_MAX_PATH_LENGTH);
    size_t szLen = strnlen(pcResult, PL_VFS_MAX_PATH_LENGTH);
    pcResult[szLen] = '/';
    pcResult[szLen + 1] = 0;

    if(gptPak->load(pcPakFilePath, NULL, &tSystem.ptPakFile))
    {
        pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);
        return true;
    }
    return false;
}

plFileHandle
pl_vfs_get_file(const char* pcFile)
{
    uint32_t uKey = PL_DS_HASH32_INVALID;
    if(pl_hm32_has_key_str_ex(&gptVfsCtx->tFileHashmap, pcFile, &uKey))
    {
        return (plFileHandle){.uIndex = uKey};
    }

    char acDirectory[PL_VFS_MAX_PATH_LENGTH] = {0};
    pl_str_get_root_directory(pcFile, acDirectory, PL_VFS_MAX_PATH_LENGTH);
    size_t szRootLength = strnlen(acDirectory, PL_VFS_MAX_PATH_LENGTH);

    // find file system
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[0];
    uint32_t uFileSystemIndex = 0;
    const uint32_t uFileSystemCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 1; i < uFileSystemCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(pl_str_equal(acDirectory, ptFileSystem->acDirectory))
        {
            uFileSystemIndex = i;
            ptTargetFileSystem = ptFileSystem;
            break;
        }
    }

    uint32_t uNewKey = pl_hm32_get_free_index(&gptVfsCtx->tFileHashmap);
    if(uNewKey == PL_DS_HASH32_INVALID)
    {
        uNewKey = pl_sb_size(gptVfsCtx->sbtFiles);
        pl_sb_add(gptVfsCtx->sbtFiles);
    }
    gptVfsCtx->sbtFiles[uNewKey].bOpen = false;
    gptVfsCtx->sbtFiles[uNewKey]._uGeneration++;
    strncpy(gptVfsCtx->sbtFiles[uNewKey].acPath, pcFile, PL_VFS_MAX_PATH_LENGTH);
    gptVfsCtx->sbtFiles[uNewKey].uFileSystem = uFileSystemIndex;

    if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {
        strncpy(gptVfsCtx->sbtFiles[uNewKey].acPhysicalPath, pcFile, PL_VFS_MAX_PATH_LENGTH);
    }
    else if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_DIRECTORY)
    {
        const char* pcRelativeFile = &pcFile[szRootLength - 1];
        pl_str_concatenate(ptTargetFileSystem->acPhysicalDirectory, pcRelativeFile, gptVfsCtx->sbtFiles[uNewKey].acPhysicalPath, PL_VFS_MAX_PATH_LENGTH);
    }
    else if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
    {
        strncpy(gptVfsCtx->sbtFiles[uNewKey].acPhysicalPath, &pcFile[szRootLength], PL_VFS_MAX_PATH_LENGTH);
    }
    pl_sb_push(ptTargetFileSystem->sbuFileIndices, uNewKey);
    pl_hm32_insert_str(&gptVfsCtx->tFileHashmap, pcFile, uNewKey);
    return (plFileHandle){.uIndex = uNewKey, .uGeneration = gptVfsCtx->sbtFiles[uNewKey]._uGeneration};

}

bool
pl_vfs_open(plFileHandle tHandle, plOpenFileFlags tFlags)
{
    const char* pcFileName = gptVfsCtx->sbtFiles[tHandle.uIndex].acPhysicalPath;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[gptVfsCtx->sbtFiles[tHandle.uIndex].uFileSystem];
    
    if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_DIRECTORY || ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {
        gptVfsCtx->sbtFiles[tHandle.uIndex].bOpen = true;
        if(tFlags == PL_OPEN_FILE_FLAGS_WRITE)
            gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile = fopen(pcFileName, "wb");
        else if(tFlags == PL_OPEN_FILE_FLAGS_READ)
            gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile = fopen(pcFileName, "rb");
        else if(tFlags == (PL_OPEN_FILE_FLAGS_READ | PL_OPEN_FILE_FLAGS_WRITE))
            gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile = fopen(pcFileName, "wb+");
        else
        {
            gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile = NULL;
            gptVfsCtx->sbtFiles[tHandle.uIndex].bOpen = false;
        }
        return gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile != NULL;
    }
    else if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
    {
        gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile = NULL;
        gptVfsCtx->sbtFiles[tHandle.uIndex].bOpen = true;
    }


    return false;
}

void
pl_vfs_close(plFileHandle tHandle)
{
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[gptVfsCtx->sbtFiles[tHandle.uIndex].uFileSystem];
    
    if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_DIRECTORY || ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {

        FILE* ptFile = gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile;
        if(ptFile)
        {
            fclose(ptFile);
            gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile = NULL;
        }
    }
}

size_t
pl_vfs_write(plFileHandle tHandle, const void* pData, size_t szSize)
{
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[gptVfsCtx->sbtFiles[tHandle.uIndex].uFileSystem];
    
    if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_DIRECTORY || ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {
        FILE* ptFile = gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile;
        if(ptFile)
        {
            return fwrite(pData, 1, szSize, ptFile);
        }
    }
    else if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
    {
        PL_ASSERT(false && "can not write to pak file");
    }
    return 0;
}

bool
pl_vfs_read(plFileHandle tHandle, void* pData, size_t* pszSizeOut)
{
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[gptVfsCtx->sbtFiles[tHandle.uIndex].uFileSystem];
    
    if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_DIRECTORY || ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {

        FILE* ptFile = gptVfsCtx->sbtFiles[tHandle.uIndex].ptFile;

        size_t uSize = 0;

        // obtain file size
        fseek(ptFile, 0, SEEK_END);
        uSize = ftell(ptFile);

        if(pszSizeOut)
            *pszSizeOut = uSize;

        if(pData == NULL)
        {
            return true;
        }
        fseek(ptFile, 0, SEEK_SET);

        size_t szResult = fread(pData, sizeof(char), uSize, ptFile);
        if (szResult != uSize)
        {
            if (feof(ptFile))
                printf("Error reading test.bin: unexpected end of file\n");
            else if (ferror(ptFile)) {
                perror("Error reading test.bin");
            }
            return false;
        }
        return true;
    }
    else if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
    {
        return gptPak->get_file(ptTargetFileSystem->ptPakFile, gptVfsCtx->sbtFiles[tHandle.uIndex].acPhysicalPath, pData, pszSizeOut);
    }
    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_vfs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plVfsI tApi = {
        .initialize      = pl_vfs_initialize,
        .cleanup         = pl_vfs_cleanup,
        .get_file        = pl_vfs_get_file,
        .open            = pl_vfs_open,
        .close           = pl_vfs_close,
        .mount_directory = pl_vfs_mount_directory,
        .mount_pak       = pl_vfs_mount_pak,
        .write           = pl_vfs_write,
        .read            = pl_vfs_read,
    };
    pl_set_api(ptApiRegistry, plVfsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptFile   = pl_get_api_latest(ptApiRegistry, plFileI);
        gptPak    = pl_get_api_latest(ptApiRegistry, plPakI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptVfsCtx = ptDataRegistry->get_data("plVirtualFileSystemContext");
    }
    else
    {
        static plVirtualFileSystemContext gtVfsCtx = {0};
        gptVfsCtx = &gtVfsCtx;
        ptDataRegistry->set_data("plVirtualFileSystemContext", gptVfsCtx);
    }
}

PL_EXPORT void
pl_unload_vfs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plVfsI* ptApi = pl_get_api_latest(ptApiRegistry, plVfsI);
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