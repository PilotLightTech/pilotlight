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

#include "pilotlight.h"
#include "pl_os.h"
#include <stdio.h> // file api

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct plWin32SharedLibrary_t
{
    HMODULE   handle;
    FILETIME  lastWriteTime;
} plWin32SharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static FILETIME
pl__get_last_write_time(const char* filename)
{
    FILETIME LastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA data = {0};
    if(GetFileAttributesExA(filename, GetFileExInfoStandard, &data))
        LastWriteTime = data.ftLastWriteTime;
    
    return LastWriteTime;
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
    CopyFile(source, destination, FALSE);
}

bool
pl_has_library_changed(plSharedLibrary* library)
{
    FILETIME newWriteTime = pl__get_last_write_time(library->path);
    plWin32SharedLibrary* win32Library = library->_platformData;
    return CompareFileTime(&newWriteTime, &win32Library->lastWriteTime) != 0;
}

bool
pl_load_library(plSharedLibrary* library, const char* name, const char* transitionalName, const char* lockFile)
{
    if(library->path[0] == 0)             strncpy(library->path, name, PL_MAX_NAME_LENGTH);
    if(library->transitionalName[0] == 0) strncpy(library->transitionalName, transitionalName, PL_MAX_NAME_LENGTH);
    if(library->lockFile[0] == 0)         strncpy(library->lockFile, lockFile, PL_MAX_NAME_LENGTH);
    library->valid = false;

    if(library->_platformData == NULL)
        library->_platformData = malloc(sizeof(plWin32SharedLibrary));
    plWin32SharedLibrary* win32Library = library->_platformData;

    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if(!GetFileAttributesExA(library->lockFile, GetFileExInfoStandard, &Ignored))  // lock file gone
    {
        char temporaryName[2024] = {0};
        win32Library->lastWriteTime = pl__get_last_write_time(library->path);
        
        pl_sprintf(temporaryName, "%s%u%s", library->transitionalName, library->tempIndex, ".dll");
        if(++library->tempIndex >= 1024)
        {
            library->tempIndex = 0;
        }
        pl_copy_file(library->path, temporaryName, NULL, NULL);

        win32Library->handle = NULL;
        win32Library->handle = LoadLibraryA(temporaryName);
        if(win32Library->handle)
            library->valid = true;
    }

    return library->valid;
}

void
pl_reload_library(plSharedLibrary* library)
{
    library->valid = false;
    for(uint32_t i = 0; !library->valid && (i < 100); i++)
    {
        if(pl_load_library(library, library->path, library->transitionalName, library->lockFile))
            break;
        pl_sleep(100);
    }
}

void*
pl_load_library_function(plSharedLibrary* library, const char* name)
{
    PL_ASSERT(library->valid && "Library not valid");
    void* loadedFunction = NULL;
    if(library->valid)
    {
        plWin32SharedLibrary* win32Library = library->_platformData;
        loadedFunction = (void*)GetProcAddress(win32Library->handle, name);
    }
    return loadedFunction;
}

int
pl_sleep(uint32_t millisec)
{
    Sleep((long)millisec);
    return 0;
}
