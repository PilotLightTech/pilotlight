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
// [SECTION] internal api
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
    static const plFileI*   gptFile   = NULL;
    static const plPakI*    gptPak    = NULL;
    static       plIO*      gptIO     = NULL;

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

#define PL_VFS_MAX_PATH_LENGTH 255

//-----------------------------------------------------------------------------
// [SECTION] internal forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plVfsMemoryFile            plVfsMemoryFile;
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
    PL_FILE_SYSTEM_TYPE_PHYSICAL  = 0,
    PL_FILE_SYSTEM_TYPE_DIRECTORY = 1,
    PL_FILE_SYSTEM_TYPE_PAK       = 101,
    PL_FILE_SYSTEM_TYPE_MEMORY    = 102,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVfsMemoryFile
{
    char     acName[PL_VFS_MAX_PATH_LENGTH];
    size_t   szSize;
    uint8_t* puData;
} plVfsMemoryFile;

typedef struct _plVfsFile
{
    bool            bActive;
    bool            bOpen;
    size_t          szOffsetPointer; // per-open cursor for memory files
    plVfsFileMode   tMode;
    uint32_t        uFileSystemIndex;
    uint32_t        uGeneration;
    char            acPath[PL_VFS_MAX_PATH_LENGTH]; // actually used to retrieve file
    char            acRealPath[PL_VFS_MAX_PATH_LENGTH];
    plFileStream*   ptFile;
    plPakChildFile* ptChildFile;
} plVfsFile;

typedef struct _plVfsFileSystem
{
    plFileSystemType tType;
    plVfsMountFlags  tFlags;
    uint32_t         uIndex;
    char             acMountDirectory[PL_VFS_MAX_PATH_LENGTH];
    char             acPhysicalDirectory[PL_VFS_MAX_PATH_LENGTH];
    plPakFile*       ptPakFile;
    plVfsMemoryFile* sbtMemoryFiles;
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
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool
pl__str_get_root_directory(const char* pcFilePath, char* pcDirectoryOut, size_t szOutSize)
{
    size_t szLen = strlen(pcFilePath);
    strncpy(pcDirectoryOut, pcFilePath, szOutSize);

    if(szLen > szOutSize || szOutSize < 2)
        return false;

    if(pcDirectoryOut[0] == '/' || pcDirectoryOut[0] == '\\')
    {
        size_t szCurrentLocation = 1;
        bool bHit = false;
        while(szCurrentLocation < szLen)
        {
            if(pcDirectoryOut[szCurrentLocation] == '/' || pcDirectoryOut[szCurrentLocation] == '\\')
            {
                pcDirectoryOut[szCurrentLocation + 1] = 0;
                bHit = true;
                break;
            }

            szCurrentLocation++;
        }
        if(!bHit)
        {
            pcDirectoryOut[1] = 0;
        }
        return true;
    }
    else if(pcDirectoryOut[1] == ':')
    {
        size_t szCurrentLocation = 3;
        while(szCurrentLocation < szLen)
        {
            if(pcDirectoryOut[szCurrentLocation] == '/' || pcDirectoryOut[szCurrentLocation] == '\\')
            {
                pcDirectoryOut[szCurrentLocation + 1] = 0;
                break;
            }

            szCurrentLocation++;
        }
        return true;  
    }
    else
    {
        pcDirectoryOut[0] = '.';
        pcDirectoryOut[1] = '/';
        pcDirectoryOut[2] = 0;
    }
    return true;
}

static inline plVfsMemoryFile*
pl__vfs_get_memory_file(plVfsFileSystem* ptSystem, const char* pcFile)
{
    const uint32_t uFileCount = pl_sb_size(ptSystem->sbtMemoryFiles);
    for(uint32_t i = 0; i < uFileCount; i++)
    {
        if(pl_str_equal(pcFile, ptSystem->sbtMemoryFiles[i].acName))
        {
            return &ptSystem->sbtMemoryFiles[i];
        }
    }
    return NULL;
}

static inline uint32_t
pl__vfs_get_memory_file_index(plVfsFileSystem* ptSystem, const char* pcFile)
{
    const uint32_t uFileCount = pl_sb_size(ptSystem->sbtMemoryFiles);
    for(uint32_t i = 0; i < uFileCount; i++)
    {
        if(pl_str_equal(pcFile, ptSystem->sbtMemoryFiles[i].acName))
            return i;
    }
    return UINT32_MAX;
}

static inline plVfsFile*
pl__vfs_get_file(plVfsFileHandle tHandle)
{
    if(tHandle.uData == UINT64_MAX || tHandle.uIndex >= pl_sb_size(gptVfsCtx->sbtFiles))
        return NULL;
    if(!gptVfsCtx->sbtFiles[tHandle.uIndex].bActive || gptVfsCtx->sbtFiles[tHandle.uIndex].uGeneration != tHandle.uGeneration)
        return NULL;
    return &gptVfsCtx->sbtFiles[tHandle.uIndex];
}

static inline plVfsFileSystem*
pl__vfs_get_system(const char* pcFile)
{
    // retrieve root directory in order to find expected file system
    char acDirectory[PL_VFS_MAX_PATH_LENGTH] = {0};
    pl__str_get_root_directory(pcFile, acDirectory, PL_VFS_MAX_PATH_LENGTH);

    // find file system
    const uint32_t uFileSystemIndexCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 1; i < uFileSystemIndexCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(pl_str_equal(acDirectory, ptFileSystem->acMountDirectory))
        {
            return ptFileSystem;
        }
    }
    return &gptVfsCtx->sbtFileSystems[0];
}

// Registered paths and open streams occupy distinct slots. Reuse preserves generation.
static plVfsFileHandle
pl__vfs_allocate_file(plVfsFile tFile)
{
    (void)pl_hm32_get_free_index(&gptVfsCtx->tFileHashmap);
    uint32_t uIndex = 0;
    while(uIndex < pl_sb_size(gptVfsCtx->sbtFiles) && gptVfsCtx->sbtFiles[uIndex].bActive)
        uIndex++;
    if(uIndex == pl_sb_size(gptVfsCtx->sbtFiles))
    {
        plVfsFile tEmpty = {0};
        pl_sb_push(gptVfsCtx->sbtFiles, tEmpty);
    }
    tFile.uGeneration = gptVfsCtx->sbtFiles[uIndex].uGeneration;
    tFile.bActive = true;
    gptVfsCtx->sbtFiles[uIndex] = tFile;
    return (plVfsFileHandle){.uIndex = uIndex, .uGeneration = tFile.uGeneration};
}

bool pl_vfs_does_file_exist(const char* pcFile);

plVfsFileHandle
pl_vfs_register_file(const char* pcFile, bool bMustExist)
{
    if(!pcFile || !pcFile[0] || strlen(pcFile) >= PL_VFS_MAX_PATH_LENGTH)
        return (plVfsFileHandle){.uData = UINT64_MAX};

    // check if file has already been registered
    uint32_t uKey = PL_DS_HASH32_INVALID;
    if(pl_hm32_has_key_str_ex(&gptVfsCtx->tFileHashmap, pcFile, &uKey))
    {
        if(bMustExist && !pl_vfs_does_file_exist(pcFile))
            return (plVfsFileHandle){.uData = UINT64_MAX};
        plVfsFileHandle tResult = {0};
        tResult.uIndex = uKey;
        tResult.uGeneration = gptVfsCtx->sbtFiles[uKey].uGeneration;
        return tResult;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~register new file~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // figure out target file system to use
    plVfsFileSystem* ptTargetFileSystem = pl__vfs_get_system(pcFile);

    // get "usable" part of file path
    size_t szRootLength = 0;
    if(ptTargetFileSystem->tType != PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {
        char acDirectory[PL_VFS_MAX_PATH_LENGTH] = {0};
        pl__str_get_root_directory(pcFile, acDirectory, PL_VFS_MAX_PATH_LENGTH);
        szRootLength = strnlen(acDirectory, PL_VFS_MAX_PATH_LENGTH);
    }

    // create internal file
    plVfsFile tFile = {0};
    tFile.bOpen = false;
    tFile.uFileSystemIndex = ptTargetFileSystem->uIndex;

    // copy addressable path
    strncpy(tFile.acPath, pcFile, PL_VFS_MAX_PATH_LENGTH);
    const char* pcRelativeFile = NULL;

    // find actual path needed for retrieval
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            strncpy(tFile.acRealPath, pcFile, PL_VFS_MAX_PATH_LENGTH);
            if(bMustExist && !gptFile->exists(tFile.acRealPath))
                return (plVfsFileHandle){.uData = UINT64_MAX};
            break;

        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
            pcRelativeFile = &pcFile[szRootLength - 1];
            pl_str_concatenate(ptTargetFileSystem->acPhysicalDirectory, pcRelativeFile, tFile.acRealPath, PL_VFS_MAX_PATH_LENGTH);
            if(bMustExist && !gptFile->exists(tFile.acRealPath))
                return (plVfsFileHandle){.uData = UINT64_MAX};
            break;
        
        case PL_FILE_SYSTEM_TYPE_PAK:
            strncpy(tFile.acRealPath, &pcFile[szRootLength], PL_VFS_MAX_PATH_LENGTH);

            // check if file really exist
            if(!gptPak->read_file(ptTargetFileSystem->ptPakFile, tFile.acRealPath, NULL, NULL))
                return (plVfsFileHandle){.uData = UINT64_MAX};
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, pcFile);
            if(bMustExist && ptMemoryFile == NULL)
                return (plVfsFileHandle){.uData = UINT64_MAX};
            
            strncpy(tFile.acRealPath, pcFile, PL_VFS_MAX_PATH_LENGTH);
            break;
        }

        default:
            break;
    }

    plVfsFileHandle tHandle = pl__vfs_allocate_file(tFile);
    pl_hm32_insert_str(&gptVfsCtx->tFileHashmap, pcFile, tHandle.uIndex);
    return tHandle;
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plVfsResult
pl_vfs_mount_directory(const char* pcDirectory, const char* pcPhysicalDirectory, plVfsMountFlags tFlags)
{

    const uint32_t uFileSystemIndexCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 1; i < uFileSystemIndexCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(pl_str_equal(pcDirectory, ptFileSystem->acMountDirectory))
        {
            return PL_VFS_RESULT_FAIL;
        }
    }

    plVfsFileSystem tSystem = {
        .tType  = PL_FILE_SYSTEM_TYPE_DIRECTORY,
        .tFlags = tFlags,
        .uIndex = pl_sb_size(gptVfsCtx->sbtFileSystems)
    };
    char* pcResult = strncpy(tSystem.acMountDirectory, pcDirectory, PL_VFS_MAX_PATH_LENGTH);

    // add forward slash on end if not already present
    size_t szLen = strnlen(pcResult, PL_VFS_MAX_PATH_LENGTH);
    if(pcResult[szLen - 1] != '/')
    {
        pcResult[szLen] = '/';
        pcResult[szLen + 1] = 0;
    }

    // remove forward slash if present here
    strncpy(tSystem.acPhysicalDirectory, pcPhysicalDirectory, PL_VFS_MAX_PATH_LENGTH);
    szLen = strnlen(tSystem.acPhysicalDirectory, PL_VFS_MAX_PATH_LENGTH);
    if(tSystem.acPhysicalDirectory[szLen - 1] == '/')
        tSystem.acPhysicalDirectory[szLen - 1] = 0;
    pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);
    return PL_VFS_RESULT_SUCCESS;
}

plVfsResult
pl_vfs_mount_pak(const char* pcDirectory, const char* pcPakFilePath, plVfsMountFlags tFlags)
{
    if(!pcDirectory || !pcPakFilePath || !pcDirectory[0] || strlen(pcDirectory) > PL_VFS_MAX_PATH_LENGTH - 2 || strlen(pcPakFilePath) >= PL_VFS_MAX_PATH_LENGTH)
        return PL_VFS_RESULT_FAIL;
    char acMount[PL_VFS_MAX_PATH_LENGTH] = {0};
    snprintf(acMount, sizeof(acMount), "%s%s", pcDirectory, pcDirectory[strlen(pcDirectory) - 1] == '/' ? "" : "/");

    const uint32_t uFileSystemIndexCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 1; i < uFileSystemIndexCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(pl_str_equal(acMount, ptFileSystem->acMountDirectory))
        {
            return PL_VFS_RESULT_FAIL;
        }
    }

    plVfsFileSystem tSystem = {
        .tType  = PL_FILE_SYSTEM_TYPE_PAK,
        .tFlags = tFlags,
        .uIndex = pl_sb_size(gptVfsCtx->sbtFileSystems)
    };

    // add forward slash if not present
    char* pcResult = strncpy(tSystem.acMountDirectory, pcDirectory, PL_VFS_MAX_PATH_LENGTH);
    size_t szLen = strnlen(pcResult, PL_VFS_MAX_PATH_LENGTH);
    if(pcResult[szLen - 1] != '/')
    {
        pcResult[szLen] = '/';
        pcResult[szLen + 1] = 0;
    }

    // attempt to load pak file
    plPakInfo tPakInfo = {0};
    if(gptPak->load(pcPakFilePath, &tPakInfo, &tSystem.ptPakFile))
    {
        strncpy(tSystem.acPhysicalDirectory, pcPakFilePath, PL_VFS_MAX_PATH_LENGTH);

        pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);

        // pre-register files
        for(uint32_t i = 0; i < tPakInfo.uEntryCount; i++)
        {
            char acMountedPath[PL_VFS_MAX_PATH_LENGTH];
            int iLength = snprintf(acMountedPath, sizeof(acMountedPath), "%s%s", tSystem.acMountDirectory, tPakInfo.atEntries[i].pcFilePath);
            if(iLength >= 0 && (size_t)iLength < sizeof(acMountedPath))
                pl_vfs_register_file(acMountedPath, false);
        }
        return PL_VFS_RESULT_SUCCESS;
    }
    return PL_VFS_RESULT_FAIL;
}

plVfsResult
pl_vfs_mount_memory(const char* pcDirectory, plVfsMountFlags tFlags)
{

    const uint32_t uFileSystemIndexCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 1; i < uFileSystemIndexCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(pl_str_equal(pcDirectory, ptFileSystem->acMountDirectory))
        {
            return PL_VFS_RESULT_FAIL;
        }
    }

    plVfsFileSystem tSystem = {
        .tType  = PL_FILE_SYSTEM_TYPE_MEMORY,
        .tFlags = tFlags,
        .uIndex = pl_sb_size(gptVfsCtx->sbtFileSystems)
    };

    // add forward slash if not present
    char* pcResult = strncpy(tSystem.acMountDirectory, pcDirectory, PL_VFS_MAX_PATH_LENGTH);
    size_t szLen = strnlen(pcResult, PL_VFS_MAX_PATH_LENGTH);
    if(pcResult[szLen - 1] != '/')
    {
        pcResult[szLen] = '/';
        pcResult[szLen + 1] = 0;
    }

    pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);
    return PL_VFS_RESULT_SUCCESS;
}

plVfsResult
pl_vfs_delete_file(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return PL_VFS_RESULT_FAIL;

    // Do not invalidate shared metadata or remove storage while a stream uses it.
    for(uint32_t i = 0; i < pl_sb_size(gptVfsCtx->sbtFiles); i++)
        if(gptVfsCtx->sbtFiles[i].bActive && gptVfsCtx->sbtFiles[i].bOpen &&
           strcmp(gptVfsCtx->sbtFiles[i].acPath, ptFile->acPath) == 0)
            return PL_VFS_RESULT_FAIL;
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];

    // attempt to actually delete file if possible
    plVfsResult tResult = PL_VFS_RESULT_FAIL;
    switch(ptTargetFileSystem->tType)
    {

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            uint32_t uMemoryFileIndex = pl__vfs_get_memory_file_index(ptTargetFileSystem, ptFile->acRealPath);

            if(uMemoryFileIndex == UINT32_MAX)
                tResult = PL_VFS_RESULT_FAIL;
            else
            {
                if(ptTargetFileSystem->sbtMemoryFiles[uMemoryFileIndex].puData)
                {
                    PL_FREE(ptTargetFileSystem->sbtMemoryFiles[uMemoryFileIndex].puData);
                    ptTargetFileSystem->sbtMemoryFiles[uMemoryFileIndex].puData = NULL;
                }
                pl_sb_del_swap(ptTargetFileSystem->sbtMemoryFiles, uMemoryFileIndex);
                tResult = PL_VFS_RESULT_SUCCESS;
            }
            break;
        }

        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        {
            
            plFileResult tResult2 = gptFile->remove(ptFile->acRealPath);
            if(tResult2 == PL_FILE_RESULT_SUCCESS)
                tResult = PL_VFS_RESULT_SUCCESS;
            break;
        }

        case PL_FILE_SYSTEM_TYPE_PAK:
            break;
        
        default:
            break;
    }

    if(tResult == PL_VFS_RESULT_SUCCESS)
    {
        pl_hm32_remove_str(&gptVfsCtx->tFileHashmap, ptFile->acPath);
        ptFile->bActive = false;
        ptFile->uGeneration++;
    }
    return tResult;
}

bool
pl_vfs_does_file_exist(const char* pcFile)
{
    if(!pcFile || !pcFile[0] || strlen(pcFile) >= PL_VFS_MAX_PATH_LENGTH)
        return false;

    // retrieve root directory in order to find expected file system
    char acDirectory[PL_VFS_MAX_PATH_LENGTH] = {0};
    pl__str_get_root_directory(pcFile, acDirectory, PL_VFS_MAX_PATH_LENGTH);
    size_t szRootLength = strnlen(acDirectory, PL_VFS_MAX_PATH_LENGTH);

    // find file system
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[0];
    uint32_t uFileSystemIndexIndex = 0;
    const uint32_t uFileSystemIndexCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 1; i < uFileSystemIndexCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        if(pl_str_equal(acDirectory, ptFileSystem->acMountDirectory))
        {
            uFileSystemIndexIndex = i;
            ptTargetFileSystem = ptFileSystem;
            break;
        }
    }

    char acFullPath[PL_VFS_MAX_PATH_LENGTH] = {0};

    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            return gptFile->exists(pcFile);
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
            pl_str_concatenate(ptTargetFileSystem->acPhysicalDirectory, &pcFile[szRootLength - 1], acFullPath, PL_VFS_MAX_PATH_LENGTH);
            return gptFile->exists(acFullPath);
        case PL_FILE_SYSTEM_TYPE_PAK:
            return gptPak->read_file(ptTargetFileSystem->ptPakFile, &pcFile[szRootLength], NULL, NULL);
        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, pcFile);
            if(ptMemoryFile)
                return true;
            return false;
        }
        default: break;
    }
    return false;
}

bool
pl_vfs_is_file_open(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return false;
    return ptFile->bOpen;
}

plVfsFileHandle
pl_vfs_open_file(const char* pcFile, plVfsFileMode tMode)
{
    static const plFileMode atModes[] = {
        PL_FILE_MODE_READ,
        PL_FILE_MODE_WRITE,
        PL_FILE_MODE_APPEND,
        PL_FILE_MODE_WRITE_READ,
        PL_FILE_MODE_APPEND_READ
    };

    const plVfsFileHandle tInvalid = {.uData = UINT64_MAX};
    if(tMode < PL_VFS_FILE_MODE_READ || tMode > PL_VFS_FILE_MODE_READ_APPEND)
        return tInvalid;
    plVfsFileHandle tRegistered = pl_vfs_register_file(pcFile, false);
    plVfsFile* ptRegistered = pl__vfs_get_file(tRegistered);
    if(!ptRegistered) return tInvalid;
    plVfsFile tFile = *ptRegistered;
    tFile.tMode = tMode;
    tFile.szOffsetPointer = 0;
    plVfsFileSystem* ptSystem = &gptVfsCtx->sbtFileSystems[tFile.uFileSystemIndex];
    switch(ptSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            tFile.ptFile = gptFile->open_stream(tFile.acRealPath, atModes[tMode]);
            if(!tFile.ptFile)
                return tInvalid;
            break;
        case PL_FILE_SYSTEM_TYPE_PAK:
            if(tMode != PL_VFS_FILE_MODE_READ) return tInvalid;
            tFile.ptChildFile = gptPak->open_file(ptSystem->ptPakFile, tFile.acRealPath);
            if(!tFile.ptChildFile)
                return tInvalid;
            break;
        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemory = pl__vfs_get_memory_file(ptSystem, tFile.acRealPath);
            if(!ptMemory)
            {
                if(tMode == PL_VFS_FILE_MODE_READ) return tInvalid;
                plVfsMemoryFile tMemory = {0};
                strcpy(tMemory.acName, pcFile);
                pl_sb_push(ptSystem->sbtMemoryFiles, tMemory);
                ptMemory = &pl_sb_top(ptSystem->sbtMemoryFiles);
            }
            if(tMode == PL_VFS_FILE_MODE_WRITE || tMode == PL_VFS_FILE_MODE_READ_WRITE)
            {
                if(ptMemory->puData)
                {
                    PL_FREE(ptMemory->puData);
                }
                ptMemory->puData = NULL;
                ptMemory->szSize = 0;
            }
            if(tMode == PL_VFS_FILE_MODE_APPEND || tMode == PL_VFS_FILE_MODE_READ_APPEND)
                tFile.szOffsetPointer = ptMemory->szSize;
            break;
        }
        default: return tInvalid;
    }
    tFile.bOpen = true;
    return pl__vfs_allocate_file(tFile);
}

void
pl_vfs_close_file(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return;

    if(!ptFile->bOpen) return;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];

    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            PL_ASSERT(ptFile->ptFile);
            gptFile->close_stream(ptFile->ptFile);
            ptFile->ptFile = NULL;
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->close_file(ptFile->ptChildFile);
            ptFile->ptChildFile = NULL;
            break;

        default:
            break;
    }
    ptFile->bOpen = false;
    ptFile->bActive = false;
    ptFile->uGeneration++;
}

size_t
pl_vfs_write_file(plVfsFileHandle tHandle, const void* pData, size_t szSize)
{

    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return 0;

    if(!ptFile->bOpen)
        return 0;

    if(ptFile->tMode == PL_VFS_FILE_MODE_READ || (szSize && !pData))
        return 0;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            PL_ASSERT(ptFile->ptFile);
            return gptFile->write_stream(ptFile->ptFile, pData, szSize);

        case PL_FILE_SYSTEM_TYPE_PAK:
            PL_ASSERT(false && "can not write to pak file in this version");
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
            {
                uint8_t* puNew = szSize ? PL_ALLOC(szSize) : NULL;
                if(szSize && !puNew)
                    return 0;
                if(szSize)
                    memcpy(puNew, pData, szSize);
                if(ptMemoryFile->puData)
                {
                    PL_FREE(ptMemoryFile->puData);
                }
                ptMemoryFile->puData = puNew;
                ptMemoryFile->szSize = szSize;
                return szSize;
            }
            break;
        }

        default:
            break;
    }
    return 0;
}

plVfsResult
pl_vfs_read_file(plVfsFileHandle tHandle, void* pData, size_t* pszSizeOut)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return PL_VFS_RESULT_FAIL;

    if(!ptFile->bOpen)
        return 0;

    if(pData && (ptFile->tMode == PL_VFS_FILE_MODE_WRITE || ptFile->tMode == PL_VFS_FILE_MODE_APPEND))
        return PL_VFS_RESULT_FAIL;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
        {

            uint64_t uSize = 0;
            if(!gptFile->size_stream(ptFile->ptFile, &uSize) || uSize > SIZE_MAX)
                return PL_VFS_RESULT_FAIL;
            if(pszSizeOut)
                *pszSizeOut = (size_t)uSize;
            if(!pData)
                return PL_VFS_RESULT_SUCCESS;
            if(!gptFile->seek_stream(ptFile->ptFile, 0, PL_FILE_SEEK_START))
                return PL_VFS_RESULT_FAIL;
            return gptFile->read_stream(ptFile->ptFile, pData, (size_t)uSize) == uSize ? PL_VFS_RESULT_SUCCESS : PL_VFS_RESULT_FAIL;
        }

        case PL_FILE_SYSTEM_TYPE_PAK:
        {
            bool bResult = gptPak->read_file(ptTargetFileSystem->ptPakFile, ptFile->acRealPath, pData, pszSizeOut);
            return bResult ? PL_VFS_RESULT_SUCCESS : PL_VFS_RESULT_FAIL;
        }

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
            {
                if(pszSizeOut)
                    *pszSizeOut = ptMemoryFile->szSize;
                if(pData && ptMemoryFile->szSize)
                    memcpy(pData, ptMemoryFile->puData, ptMemoryFile->szSize);
                return PL_VFS_RESULT_SUCCESS;
            }
            return PL_VFS_RESULT_FAIL;
        }

        default:
            break;
    }
    return PL_VFS_RESULT_FAIL;
}

bool
pl_vfs_is_file_valid(plVfsFileHandle tHandle)
{
    if(tHandle.uData == UINT64_MAX)
        return false;
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    return ptFile != NULL;
}

size_t
pl_vfs_file_size_str(const char* pcFile)
{
    plVfsFileHandle tHandle = pl_vfs_register_file(pcFile, true);
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
    {
        return 0;
    }
    size_t szFileSize = 0;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            gptFile->binary_read(ptFile->acRealPath, &szFileSize, NULL);
            break;
        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->read_file(ptTargetFileSystem->ptPakFile, ptFile->acRealPath, NULL, &szFileSize);
            break;
        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                szFileSize = ptMemoryFile->szSize;
            break;
        }
        default:
            break;
    }
    return szFileSize;
}

const char*
pl_vfs_get_real_path(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
    {
        PL_ASSERT(false && "file doesn't exist");
        return NULL;
    }
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    if(ptTargetFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
        return ptTargetFileSystem->acPhysicalDirectory;
    return ptFile->acRealPath;
}

size_t
pl_vfs_get_file_stream_position(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return 0;

    if(!ptFile->bOpen)
        return 0;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
        {
            uint64_t uPosition = 0;
            return gptFile->tell_stream(ptFile->ptFile, &uPosition) && uPosition <= SIZE_MAX ? (size_t)uPosition : 0;
        }


        case PL_FILE_SYSTEM_TYPE_PAK:
            return gptPak->get_file_stream_position(ptFile->ptChildFile);

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                return ptFile->szOffsetPointer;
        }

        default:
            break;
    }
    return 0;
}

void
pl_vfs_reset_file_stream_position(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return;

    if(!ptFile->bOpen)
        return;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            gptFile->seek_stream(ptFile->ptFile, 0, PL_FILE_SEEK_START);
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->reset_file_stream_position(ptFile->ptChildFile);
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                ptFile->szOffsetPointer = 0;
            break;
        }

        default:
            break;
    }
}

bool
pl_vfs_set_file_stream_position(plVfsFileHandle tHandle, size_t szPosition)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(!ptFile || !ptFile->bOpen)
        return false;
    plVfsFileSystem* ptSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    switch(ptSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            return szPosition <= INT64_MAX && gptFile->seek_stream(ptFile->ptFile, (int64_t)szPosition, PL_FILE_SEEK_START) == PL_FILE_RESULT_SUCCESS;
        case PL_FILE_SYSTEM_TYPE_PAK:
            return gptPak->set_file_stream_position(ptFile->ptChildFile, szPosition);
        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemory = pl__vfs_get_memory_file(ptSystem, ptFile->acRealPath);
            if(!ptMemory || szPosition > ptMemory->szSize)
                return false;
            ptFile->szOffsetPointer = szPosition;
            return true;
        }
        default: return false;
    }
}

bool
pl_vfs_increment_file_stream_position(plVfsFileHandle tHandle, size_t szDelta)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(!ptFile || !ptFile->bOpen)
        return false;
    plVfsFileSystem* ptSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    if(ptSystem->tType == PL_FILE_SYSTEM_TYPE_DIRECTORY || ptSystem->tType == PL_FILE_SYSTEM_TYPE_PHYSICAL)
    {
        uint64_t uPosition = 0;
        if(!gptFile->tell_stream(ptFile->ptFile, &uPosition) || uPosition > INT64_MAX || szDelta > (uint64_t)INT64_MAX - uPosition)
           return false;
        return gptFile->seek_stream(ptFile->ptFile, (int64_t)(uPosition + szDelta), PL_FILE_SEEK_START) == PL_FILE_RESULT_SUCCESS;
    }
    if(ptSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
        return gptPak->increment_file_stream_position(ptFile->ptChildFile, szDelta);
    if(szDelta > SIZE_MAX - ptFile->szOffsetPointer)
        return false;
    return pl_vfs_set_file_stream_position(tHandle, ptFile->szOffsetPointer + szDelta);
}

size_t
pl_vfs_read_file_stream(plVfsFileHandle tHandle, size_t szElementSize, size_t szElementCount, void* pBufferOut)
{
    if(!pBufferOut || !szElementSize || szElementCount > SIZE_MAX / szElementSize)
        return 0;
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return 0;

    if(!ptFile->bOpen)
        return 0;

    if(ptFile->tMode == PL_VFS_FILE_MODE_WRITE || ptFile->tMode == PL_VFS_FILE_MODE_APPEND) return 0;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            return gptFile->read_stream(ptFile->ptFile, pBufferOut, szElementSize * szElementCount) / szElementSize;

        case PL_FILE_SYSTEM_TYPE_PAK:
        {
            return gptPak->read_file_stream(ptFile->ptChildFile, szElementSize, szElementCount, pBufferOut);
        }

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
            {
                if(ptFile->szOffsetPointer > ptMemoryFile->szSize)
                    return 0;
                size_t szAvailable = (ptMemoryFile->szSize - ptFile->szOffsetPointer) / szElementSize;
                if(szElementCount > szAvailable)
                    szElementCount = szAvailable;
                if(!szElementCount)
                    return 0;
                memcpy(pBufferOut, ptMemoryFile->puData + ptFile->szOffsetPointer, szElementCount * szElementSize);
                ptFile->szOffsetPointer += szElementCount * szElementSize;
                return szElementCount;
            }
            break;
        }

        default:
            break;
    }
    return 0;
}

size_t
pl_vfs_write_file_stream(plVfsFileHandle tHandle, size_t szElementSize, size_t szElementCount, void* pBuffer)
{
    if(!pBuffer || !szElementSize || szElementCount > SIZE_MAX / szElementSize)
        return 0;
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return 0;

    if(!ptFile->bOpen)
        return 0;

    if(ptFile->tMode == PL_VFS_FILE_MODE_READ)
        return 0;

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            return gptFile->write_stream(ptFile->ptFile, pBuffer, szElementSize * szElementCount) / szElementSize;

        case PL_FILE_SYSTEM_TYPE_PAK:
        {
            PL_ASSERT(false && "Can't write to pak files");
            return 0;
        }

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
            {
                if(ptFile->tMode == PL_VFS_FILE_MODE_APPEND || ptFile->tMode == PL_VFS_FILE_MODE_READ_APPEND)
                    ptFile->szOffsetPointer = ptMemoryFile->szSize;
                size_t szBytes = szElementCount * szElementSize;
                if(!szBytes || szBytes > SIZE_MAX - ptFile->szOffsetPointer)
                    return 0;
                size_t szEnd = ptFile->szOffsetPointer + szBytes;
                if(szEnd > ptMemoryFile->szSize)
                {
                    uint8_t* puNew = PL_REALLOC(ptMemoryFile->puData, szEnd);
                    if(!puNew)
                        return 0;
                    memset(puNew + ptMemoryFile->szSize, 0, szEnd - ptMemoryFile->szSize);
                    ptMemoryFile->puData = puNew;
                    ptMemoryFile->szSize = szEnd;
                }
                memcpy(ptMemoryFile->puData + ptFile->szOffsetPointer, pBuffer, szBytes);
                ptFile->szOffsetPointer = szEnd;
                return szElementCount;
            }
        }

        default:
            break;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_vfs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plVfsI tApi = { 
        .register_file                  = pl_vfs_register_file,
        .open_file                      = pl_vfs_open_file,
        .close_file                     = pl_vfs_close_file,
        .mount_directory                = pl_vfs_mount_directory,
        .mount_pak                      = pl_vfs_mount_pak,
        .mount_memory                   = pl_vfs_mount_memory,
        .write_file                     = pl_vfs_write_file,
        .read_file                      = pl_vfs_read_file,
        .delete_file                    = pl_vfs_delete_file,
        .does_file_exist                = pl_vfs_does_file_exist,
        .is_file_open                   = pl_vfs_is_file_open,
        .get_file_size_str              = pl_vfs_file_size_str,
        .get_real_path                  = pl_vfs_get_real_path,
        .is_file_valid                  = pl_vfs_is_file_valid,
        .get_file_stream_position       = pl_vfs_get_file_stream_position,
        .reset_file_stream_position     = pl_vfs_reset_file_stream_position,
        .set_file_stream_position       = pl_vfs_set_file_stream_position,
        .increment_file_stream_position = pl_vfs_increment_file_stream_position,
        .read_file_stream               = pl_vfs_read_file_stream,
        .write_file_stream              = pl_vfs_write_file_stream,
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

        plVfsFileSystem tSystem = {
            .tType  = PL_FILE_SYSTEM_TYPE_PHYSICAL,
            .uIndex = 0
        };
        pl_sb_push(gptVfsCtx->sbtFileSystems, tSystem);
    }
}

void
pl_unload_vfs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    for(uint32_t i = 0; i < pl_sb_size(gptVfsCtx->sbtFiles); i++)
    {
        if(gptVfsCtx->sbtFiles[i].bOpen)
            pl_vfs_close_file((plVfsFileHandle){.uIndex = i, .uGeneration = gptVfsCtx->sbtFiles[i].uGeneration});
    }

    const uint32_t uFileSystemIndexCount = pl_sb_size(gptVfsCtx->sbtFileSystems);
    for(uint32_t i = 0; i < uFileSystemIndexCount; i++)
    {
        plVfsFileSystem* ptFileSystem = &gptVfsCtx->sbtFileSystems[i];

        const uint32_t uMemoryFileCount = pl_sb_size(ptFileSystem->sbtMemoryFiles);
        for(uint32_t j = 0; j < uMemoryFileCount; j++)
        {
            if(ptFileSystem->sbtMemoryFiles[j].puData)
            {
                PL_FREE(ptFileSystem->sbtMemoryFiles[j].puData);
            }
        }
        pl_sb_free(ptFileSystem->sbtMemoryFiles);

        if(ptFileSystem->tType == PL_FILE_SYSTEM_TYPE_PAK)
        {
            gptPak->unload(&ptFileSystem->ptPakFile);
        }
    }

    pl_sb_free(gptVfsCtx->sbtFiles);
    pl_hm32_free(&gptVfsCtx->tFileHashmap);
    pl_sb_free(gptVfsCtx->sbtFileSystems);

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