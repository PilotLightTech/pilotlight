/*
   pl_os_win32.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_os.h"
#include <stdio.h> // file api

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct
{
    HMODULE   tHandle;
    FILETIME  tLastWriteTime;
} plWin32SharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_read_file(const char* pcFile, unsigned* puSizeIn, char* pcBuffer, const char* pcMode)
{
    PL_ASSERT(puSizeIn);

    FILE* ptDataFile = fopen(pcFile, pcMode);
    unsigned uSize = 0u;

    if (ptDataFile == NULL)
    {
        PL_ASSERT(false && "File not found.");
        *puSizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    fseek(ptDataFile, 0, SEEK_SET);

    if(pcBuffer == NULL)
    {
        *puSizeIn = uSize;
        fclose(ptDataFile);
        return;
    }

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
        PL_ASSERT(false && "File not read.");
    }

    fclose(ptDataFile);
}

void
pl_copy_file(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer)
{
    CopyFile(pcSource, pcDestination, FALSE);
}

bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
    plWin32SharedLibrary* win32Library = ptLibrary->_pPlatformData;
    return CompareFileTime(&newWriteTime, &win32Library->tLastWriteTime) != 0;
}

bool
pl_load_library(plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile)
{
    if(ptLibrary->acPath[0] == 0)             strncpy(ptLibrary->acPath, pcName, PL_MAX_NAME_LENGTH);
    if(ptLibrary->acTransitionalName[0] == 0) strncpy(ptLibrary->acTransitionalName, pcTransitionalName, PL_MAX_NAME_LENGTH);
    if(ptLibrary->acLockFile[0] == 0)         strncpy(ptLibrary->acLockFile, pcLockFile, PL_MAX_NAME_LENGTH);
    ptLibrary->bValid = false;

    if(ptLibrary->_pPlatformData == NULL)
        ptLibrary->_pPlatformData = malloc(sizeof(plWin32SharedLibrary));
    plWin32SharedLibrary* ptWin32Library = ptLibrary->_pPlatformData;

    WIN32_FILE_ATTRIBUTE_DATA tIgnored;
    if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
    {
        char acTemporaryName[2024] = {0};
        ptWin32Library->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dll");
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl_copy_file(ptLibrary->acPath, acTemporaryName, NULL, NULL);

        ptWin32Library->tHandle = NULL;
        ptWin32Library->tHandle = LoadLibraryA(acTemporaryName);
        if(ptWin32Library->tHandle)
            ptLibrary->bValid = true;
    }

    return ptLibrary->bValid;
}

void
pl_reload_library(plSharedLibrary* ptLibrary)
{
    ptLibrary->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(ptLibrary, ptLibrary->acPath, ptLibrary->acTransitionalName, ptLibrary->acLockFile))
            break;
        pl_sleep(100);
    }
}

void*
pl_load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "Library not valid");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        plWin32SharedLibrary* ptWin32Library = ptLibrary->_pPlatformData;
        pLoadedFunction = (void*)GetProcAddress(ptWin32Library->tHandle, name);
    }
    return pLoadedFunction;
}

int
pl_sleep(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
    return 0;
}
