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
#include <stdlib.h>   // malloc
#include <stdio.h>    // file api
#include <string.h>   // strncpy
#include <sys/stat.h> // timespec
#include <copyfile.h> // copyfile
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <time.h>     // nanosleep

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plAppleSharedLibrary
{
    void*           handle;
    struct timespec lastWriteTime;
} plAppleSharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_read_file(const char* file, unsigned* sizeIn, char* buffer, const char* mode)
{
    PL_ASSERT(sizeIn);

    FILE* dataFile = fopen(file, mode);
    unsigned size = 0u;

    if (dataFile == NULL)
    {
        PL_ASSERT(false && "File not found.");
        *sizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(dataFile, 0, SEEK_END);
    size = ftell(dataFile);
    fseek(dataFile, 0, SEEK_SET);

    if(buffer == NULL)
    {
        *sizeIn = size;
        fclose(dataFile);
        return;
    }

    // copy the file into the buffer:
    size_t result = fread(buffer, sizeof(char), size, dataFile);
    if (result != size)
    {
        if (feof(dataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(dataFile)) {
            perror("Error reading test.bin");
        }
        PL_ASSERT(false && "File not read.");
    }

    fclose(dataFile);
}

void
pl_copy_file(const char* source, const char* destination, unsigned* size, char* buffer)
{
    copyfile_state_t s;
    s = copyfile_state_alloc();
    copyfile(source, destination, s, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(s);
}

bool
pl_has_library_changed(plSharedLibrary* library)
{
    struct timespec newWriteTime = pl__get_last_write_time(library->acPath);
    plAppleSharedLibrary* appleLibrary = library->_pPlatformData;
    return newWriteTime.tv_sec != appleLibrary->lastWriteTime.tv_sec;
}

bool
pl_load_library(plSharedLibrary* library, const char* name, const char* transitionalName, const char* lockFile)
{
    if(library->acPath[0] == 0)             strncpy(library->acPath, name, PL_MAX_NAME_LENGTH);
    if(library->acTransitionalName[0] == 0) strncpy(library->acTransitionalName, transitionalName, PL_MAX_NAME_LENGTH);
    if(library->acLockFile[0] == 0)         strncpy(library->acLockFile, lockFile, PL_MAX_NAME_LENGTH);
    library->bValid = false;

    if(library->_pPlatformData == NULL)
        library->_pPlatformData = malloc(sizeof(plAppleSharedLibrary));
    plAppleSharedLibrary* appleLibrary = library->_pPlatformData;

    struct stat attr2;
    if(stat(library->acLockFile, &attr2) == -1)  // lock file gone
    {
        char temporaryName[2024] = {0};
        appleLibrary->lastWriteTime = pl__get_last_write_time(library->acPath);
        
        pl_sprintf(temporaryName, "%s%u%s", library->acTransitionalName, library->uTempIndex, ".so");
        if(++library->uTempIndex >= 1024)
        {
            library->uTempIndex = 0;
        }
        pl_copy_file(library->acPath, temporaryName, NULL, NULL);

        appleLibrary->handle = NULL;
        appleLibrary->handle = dlopen(temporaryName, RTLD_NOW);
        if(appleLibrary->handle)
            library->bValid = true;
    }

    return library->bValid;
}

void
pl_reload_library(plSharedLibrary* library)
{
    library->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(library, library->acPath, library->acTransitionalName, library->acLockFile))
            break;
        pl_sleep(100);
    }
}

void*
pl_load_library_function(plSharedLibrary* library, const char* name)
{
    PL_ASSERT(library->bValid && "Library not valid");
    void* loadedFunction = NULL;
    if(library->bValid)
    {
        plAppleSharedLibrary* appleLibrary = library->_pPlatformData;
        loadedFunction = dlsym(appleLibrary->handle, name);
    }
    return loadedFunction;
}

int
pl_sleep(uint32_t millisec)
{
    struct timespec ts;
    int res;

    ts.tv_sec = millisec / 1000;
    ts.tv_nsec = (millisec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } 
    while (res);

    return res;
}