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
// [SECTION] public api forward declaration
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
    size_t   szOffsetPointer;
} plVfsMemoryFile;

typedef struct _plVfsFile
{
    bool            bActive;
    bool            bOpen;
    uint32_t        uFileSystemIndex;
    uint32_t        uGeneration;
    char            acPath[PL_VFS_MAX_PATH_LENGTH]; // actually used to retrieve file
    char            acRealPath[PL_VFS_MAX_PATH_LENGTH];
    FILE*           ptFile;
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
// [SECTION] public api forward declaration
//-----------------------------------------------------------------------------

plVfsFileHandle pl_vfs_open           (const char*, plVfsFileMode);
void            pl_vfs_close          (plVfsFileHandle);
size_t          pl_vfs_write          (plVfsFileHandle, const void*, size_t);
plVfsResult     pl_vfs_read           (plVfsFileHandle, void*, size_t*);
bool            pl_vfs_is_file_valid  (plVfsFileHandle);
plVfsFileHandle pl_vfs_register_file  (const char*, bool);
size_t          pl_vfs_file_size      (const char*);
const char*     pl_vfs_get_real_path  (plVfsFileHandle);
plVfsResult     pl_vfs_mount_directory(const char* directory, const char* physicalDirectory, plVfsMountFlags);
plVfsResult     pl_vfs_mount_pak      (const char* directory, const char* pakFilePath, plVfsMountFlags);
plVfsResult     pl_vfs_mount_memory   (const char* directory, plVfsMountFlags);

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
    if(tHandle.uData == UINT64_MAX)
        return NULL;
    if(gptVfsCtx->sbtFiles[tHandle.uIndex].uGeneration != tHandle.uGeneration)
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

plVfsFileHandle
pl_vfs_register_file(const char* pcFile, bool bMustExist)
{

    // check if file has already been registered
    uint32_t uKey = PL_DS_HASH32_INVALID;
    if(pl_hm32_has_key_str_ex(&gptVfsCtx->tFileHashmap, pcFile, &uKey))
    {
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

    // find available slot
    uint32_t uNewKey = pl_hm32_get_free_index(&gptVfsCtx->tFileHashmap);
    if(uNewKey == PL_DS_HASH32_INVALID)
    {
        uNewKey = pl_sb_size(gptVfsCtx->sbtFiles);
        pl_sb_add(gptVfsCtx->sbtFiles);
    }
    pl_hm32_insert_str(&gptVfsCtx->tFileHashmap, pcFile, uNewKey);
    gptVfsCtx->sbtFiles[uNewKey] = tFile;
    gptVfsCtx->sbtFiles[uNewKey].bActive = true;
    return (plVfsFileHandle){.uIndex = uNewKey, .uGeneration = gptVfsCtx->sbtFiles[uNewKey].uGeneration};
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
            pl_vfs_register_file(tPakInfo.atEntries[i].pcFilePath, false);
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

    ptFile->bActive = false;
    ptFile->uGeneration++;

    const char* pcFileName = ptFile->acRealPath;
    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    // unregister file for vfs
    pl_hm32_remove_str(&gptVfsCtx->tFileHashmap, ptFile->acPath);

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
            PL_ASSERT(false && "can't remove file within pak file");
            break;
        
        default:
            break;
    }

    return tResult;
}

bool
pl_vfs_does_file_exist(const char* pcFile)
{
    // check if file already managed by vfs system
    uint32_t uKey = PL_DS_HASH32_INVALID;
    if(pl_hm32_has_key_str_ex(&gptVfsCtx->tFileHashmap, pcFile, &uKey))
    {
        return true;
    }

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
pl_vfs_open(const char* pcFile, plVfsFileMode tMode)
{

    plVfsFileHandle tHandle = pl_vfs_register_file(pcFile, false);

    // get file mode
    const char* pcFileMode = NULL;
    switch (tMode)
    {
        case PL_VFS_FILE_MODE_READ:        pcFileMode = "rb"; break;
        case PL_VFS_FILE_MODE_WRITE:       pcFileMode = "wb"; break;
        case PL_VFS_FILE_MODE_APPEND:      pcFileMode = "ab"; break;
        case PL_VFS_FILE_MODE_READ_WRITE:  pcFileMode = "wb+"; break;
        case PL_VFS_FILE_MODE_READ_APPEND: pcFileMode = "ab+"; break;
        
        default:
            break;
    }

    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return (plVfsFileHandle){.uData = UINT64_MAX};

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];

    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            ptFile->ptFile = fopen(ptFile->acRealPath, pcFileMode);
            ptFile->bOpen = ptFile->ptFile != NULL;
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            ptFile->ptChildFile = gptPak->open_file(ptTargetFileSystem->ptPakFile, ptFile->acRealPath);
            ptFile->bOpen = ptFile->ptChildFile != NULL;
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {

            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            
            ptFile->bOpen = false;
            if(ptMemoryFile)
            {
                ptFile->bOpen = true;
            }
            else if(tMode > 0)
            {
                pl_sb_add(ptTargetFileSystem->sbtMemoryFiles);
                strncpy(pl_sb_top(ptTargetFileSystem->sbtMemoryFiles).acName, pcFile, PL_VFS_MAX_PATH_LENGTH);
                ptFile->bOpen = true;
            }
            break;
        }


        default:
            break;
    }

    if(ptFile->bOpen)
        return tHandle;
    return (plVfsFileHandle){.uData = UINT64_MAX};
}

void
pl_vfs_close(plVfsFileHandle tHandle)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return;

    PL_ASSERT(ptFile->bOpen && "file was never open");

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];

    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            PL_ASSERT(ptFile->ptFile);
            fclose(ptFile->ptFile);
            ptFile->ptFile = NULL;
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->close_file(ptFile->ptChildFile);
            break;

        default:
            break;
    }
    ptFile->bOpen = false;
}

size_t
pl_vfs_write(plVfsFileHandle tHandle, const void* pData, size_t szSize)
{

    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return 0;

    PL_ASSERT(ptFile->bOpen && "file not open");

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
            PL_ASSERT(ptFile->ptFile);
            return fwrite(pData, 1, szSize, ptFile->ptFile);

        case PL_FILE_SYSTEM_TYPE_PAK:
            PL_ASSERT(false && "can not write to pak file in this version");
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
            {
                if(ptMemoryFile->puData)
                {
                    PL_FREE(ptMemoryFile->puData);
                }
                ptMemoryFile->puData = PL_ALLOC(szSize);
                memset(ptMemoryFile->puData, 0, szSize);
                memcpy(ptMemoryFile->puData, pData, szSize);
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
pl_vfs_read(plVfsFileHandle tHandle, void* pData, size_t* pszSizeOut)
{
    plVfsFile* ptFile = pl__vfs_get_file(tHandle);
    if(ptFile == NULL)
        return PL_VFS_RESULT_FAIL;

    PL_ASSERT(ptFile->bOpen && "file not open");

    plVfsFileSystem* ptTargetFileSystem = &gptVfsCtx->sbtFileSystems[ptFile->uFileSystemIndex];
    
    switch (ptTargetFileSystem->tType)
    {
        case PL_FILE_SYSTEM_TYPE_DIRECTORY:
        case PL_FILE_SYSTEM_TYPE_PHYSICAL:
        {

            size_t uSize = 0;

            // obtain file size
            fseek(ptFile->ptFile, 0, SEEK_END);
            uSize = ftell(ptFile->ptFile);

            if(pszSizeOut)
                *pszSizeOut = uSize;

            if(pData == NULL)
            {
                return PL_VFS_RESULT_SUCCESS;
            }
            fseek(ptFile->ptFile, 0, SEEK_SET);

            size_t szResult = fread(pData, sizeof(char), uSize, ptFile->ptFile);
            if (szResult != uSize)
            {
                if (feof(ptFile->ptFile))
                    printf("Error reading %s: unexpected end of file\n", ptFile->acPath);
                else if (ferror(ptFile->ptFile)) {
                    printf("Error reading t%s\n", ptFile->acPath);
                }
                return PL_VFS_RESULT_FAIL;
            }
            return PL_VFS_RESULT_SUCCESS;
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
                if(pData && *pszSizeOut >= ptMemoryFile->szSize)
                {
                    memcpy(pData, ptMemoryFile->puData, ptMemoryFile->szSize);
                    return PL_VFS_RESULT_SUCCESS;
                }
                if(pszSizeOut)
                    *pszSizeOut = ptMemoryFile->szSize;
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
pl_vfs_file_size(const char* pcFile)
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
            return (size_t)ftell(ptFile->ptFile);


        case PL_FILE_SYSTEM_TYPE_PAK:
            return gptPak->get_file_stream_position(ptFile->ptChildFile);

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                return ptMemoryFile->szOffsetPointer;
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
            fseek(ptFile->ptFile, 0, SEEK_SET);
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->reset_file_stream_position(ptFile->ptChildFile);
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                ptMemoryFile->szOffsetPointer = 0;
            break;
        }

        default:
            break;
    }
}

void
pl_vfs_set_file_stream_position(plVfsFileHandle tHandle, size_t szPosition)
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
            fseek(ptFile->ptFile, (long)szPosition, SEEK_SET);
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->set_file_stream_position(ptFile->ptChildFile, szPosition);
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                ptMemoryFile->szOffsetPointer = szPosition;
            break;
        }

        default:
            break;
    }
}

void
pl_vfs_increment_file_stream_position(plVfsFileHandle tHandle, size_t szDelta)
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
            fseek(ptFile->ptFile, (long)szDelta, SEEK_CUR);
            break;

        case PL_FILE_SYSTEM_TYPE_PAK:
            gptPak->increment_file_stream_position(ptFile->ptChildFile, szDelta);
            break;

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
                ptMemoryFile->szOffsetPointer = szDelta;
            break;
        }

        default:
            break;
    }
}

size_t
pl_vfs_read_file_stream(plVfsFileHandle tHandle, size_t szElementSize, size_t szElementCount, void* pBufferOut)
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
            return fread(pBufferOut, szElementSize, szElementCount, ptFile->ptFile);

        case PL_FILE_SYSTEM_TYPE_PAK:
        {
            return gptPak->read_file_stream(ptFile->ptChildFile, szElementSize, szElementCount, pBufferOut);
        }

        case PL_FILE_SYSTEM_TYPE_MEMORY:
        {
            plVfsMemoryFile* ptMemoryFile = pl__vfs_get_memory_file(ptTargetFileSystem, ptFile->acRealPath);
            if(ptMemoryFile)
            {
                memcpy(pBufferOut, &ptMemoryFile->puData[ptMemoryFile->szOffsetPointer], szElementCount * szElementSize);
                ptMemoryFile->szOffsetPointer += szElementCount * szElementSize;
                return szElementCount * szElementSize;
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
            return fwrite(pBuffer, szElementSize, szElementCount, ptFile->ptFile);

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
                memcpy(&ptMemoryFile->puData[ptMemoryFile->szOffsetPointer], pBuffer, szElementCount * szElementSize);
                ptMemoryFile->szOffsetPointer += szElementCount * szElementSize;
                return szElementCount * szElementSize;
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

PL_EXPORT void
pl_load_vfs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plVfsI tApi = { 
        .register_file                  = pl_vfs_register_file,
        .open_file                      = pl_vfs_open,
        .close_file                     = pl_vfs_close,
        .mount_directory                = pl_vfs_mount_directory,
        .mount_pak                      = pl_vfs_mount_pak,
        .mount_memory                   = pl_vfs_mount_memory,
        .write_file                     = pl_vfs_write,
        .read_file                      = pl_vfs_read,
        .delete_file                    = pl_vfs_delete_file,
        .does_file_exist                = pl_vfs_does_file_exist,
        .is_file_open                   = pl_vfs_is_file_open,
        .get_file_size_str              = pl_vfs_file_size,
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

PL_EXPORT void
pl_unload_vfs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
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