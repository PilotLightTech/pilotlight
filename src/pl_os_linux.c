/*
   linux_pl_os.c
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

#include <time.h>         // nanosleep
#include "pl_os.h"
#include <stdio.h>        // file api
#include <dlfcn.h>        // dlopen, dlsym, dlclose
#include <sys/stat.h>     // stat, timespec
#include <fcntl.h>        // O_RDONLY, O_WRONLY ,O_CREAT
#include <sys/sendfile.h> // sendfile

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plLinuxSharedLibrary
{
    void*  handle;
    time_t lastWriteTime;
} plLinuxSharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
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
    uint32_t bufferSize = 0u;
    pl_read_file(source, &bufferSize, NULL, "rb");

    struct stat stat_buf;
    int fromfd = open(source, O_RDONLY);
    fstat(fromfd, &stat_buf);
    int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
    int n = 1;
    while (n > 0)
        n = sendfile(tofd, fromfd, 0, bufferSize * 2);
}

bool
pl_has_library_changed(plSharedLibrary* library)
{
    time_t newWriteTime = pl__get_last_write_time(library->acPath);
    plLinuxSharedLibrary* linuxLibrary = library->_pPlatformData;
    return newWriteTime != linuxLibrary->lastWriteTime;
}

bool
pl_load_library(plSharedLibrary* library, const char* name, const char* transitionalName, const char* lockFile)
{
    if(library->acPath[0] == 0)             strncpy(library->acPath, name, PL_MAX_NAME_LENGTH);
    if(library->acTransitionalName[0] == 0) strncpy(library->acTransitionalName, transitionalName, PL_MAX_NAME_LENGTH);
    if(library->acLockFile[0] == 0)         strncpy(library->acLockFile, lockFile, PL_MAX_NAME_LENGTH);
    library->bValid = false;

    if(library->_pPlatformData == NULL)
        library->_pPlatformData = malloc(sizeof(plLinuxSharedLibrary));
    plLinuxSharedLibrary* linuxLibrary = library->_pPlatformData;

    if(linuxLibrary)
    {
        struct stat attr2;
        if(stat(library->acLockFile, &attr2) == -1)  // lock file gone
        {
            char temporaryName[2024] = {0};
            linuxLibrary->lastWriteTime = pl__get_last_write_time(library->acPath);
            
            pl_sprintf(temporaryName, "%s%u%s", library->acTransitionalName, library->uTempIndex, ".so");
            if(++library->uTempIndex >= 1024)
            {
                library->uTempIndex = 0;
            }
            pl_copy_file(library->acPath, temporaryName, NULL, NULL);

            linuxLibrary->handle = NULL;
            linuxLibrary->handle = dlopen(temporaryName, RTLD_NOW);
            if(linuxLibrary->handle)
                library->bValid = true;
            else
            {
                printf("\n\n%s\n\n", dlerror());
            }
        }
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
        plLinuxSharedLibrary* linuxLibrary = library->_pPlatformData;
        loadedFunction = dlsym(linuxLibrary->handle, name);
    }
    return loadedFunction;
}

int
pl_sleep(uint32_t millisec)
{
    struct timespec ts = {0};
    int res;

    ts.tv_sec = millisec / 1000;
    ts.tv_nsec = (millisec % 1000) * 1000000;

    do 
    {
        res = nanosleep(&ts, &ts);
    } 
    while (res);

    return res;
}