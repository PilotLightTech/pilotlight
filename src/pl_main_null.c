/*
   pl_main_null.c
     * null platform backend (for testing, not complete)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] window ext
// [SECTION] file ext
// [SECTION] library ext
// [SECTION] threads ext
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
#include "pl_ds.h"    // hashmap & stretchy buffer
#include "pl_string.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__APPLE__)
    #include <time.h> // clock_gettime_nsec_np
    #include <sys/stat.h> // timespec
    #include <copyfile.h> // copyfile
    #include <dlfcn.h>    // dlopen, dlsym, dlclose
    #include <unistd.h> // close
    #include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
#else // linux
    #include <sys/sendfile.h> // sendfile
    #include <sys/stat.h> // stat, timespec
    #include <dlfcn.h> // dlopen, dlsym, dlclose
    #include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
    #include <time.h> // clock_gettime, clock_getres
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifdef _WIN32
typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acFileExtension[16];                   // default: "dll"
    char          acName[PL_MAX_NAME_LENGTH];            // i.e. "app"
    char          acPath[PL_MAX_PATH_LENGTH];            // i.e. "app.dll" or "../out/app.dll"
    char          acDirectory[PL_MAX_PATH_LENGTH];       // i.e. "./" or "../out/"
    char          acTransitionalPath[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    HMODULE       tHandle;
    FILETIME      tLastWriteTime;
} plSharedLibrary;

const char* gpcLibraryExtension = "dll";

#elif defined(__APPLE__)

typedef struct _plSharedLibrary
{
    bool            bValid;
    uint32_t        uTempIndex;
    char            acFileExtension[16];                   // default: "dylib"
    char            acName[PL_MAX_NAME_LENGTH];            // i.e. "app"
    char            acPath[PL_MAX_PATH_LENGTH];            // i.e. "app.dylib" or "../out/app.dylib"
    char            acDirectory[PL_MAX_PATH_LENGTH];       // i.e. "./" or "../out/"
    char            acTransitionalPath[PL_MAX_PATH_LENGTH];
    char            acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc   tDesc;
    void*           handle;
    struct timespec tLastWriteTime;
} plSharedLibrary;

const char* gpcLibraryExtension = "dylib";

#else // linux

typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acFileExtension[16];                   // default: "so"
    char          acName[PL_MAX_NAME_LENGTH];            // i.e. "app"
    char          acPath[PL_MAX_PATH_LENGTH];            // i.e. "app.so" or "../out/app.so"
    char          acDirectory[PL_MAX_PATH_LENGTH];       // i.e. "./" or "../out/"
    char          acTransitionalPath[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    void*         handle;
    time_t        tLastWriteTime;
} plSharedLibrary;

const char* gpcLibraryExtension = "so";

#endif

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    const char* pcAppName = "app";

    for(int i = 1; i < argc; i++)
    { 
        if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--app") == 0)
        {
            pcAppName = argv[i + 1];
            i++;
        }
        else if(strcmp(argv[i], "-hr") == 0 || strcmp(argv[i], "--hot-reload") == 0)
        {
            gbHotReloadActive = true;
        }
        else if(strcmp(argv[i], "--version") == 0)
        {
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug (NULL)\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release (NULL)\n\n");
            #endif
            return 0;
        }
        else if(strcmp(argv[i], "--apis") == 0)
        {
            plVersion tWindowExtVersion = plWindowI_version;
            plVersion tLibraryVersion = plLibraryI_version;
            plVersion tDataRegistryVersion = plDataRegistryI_version;
            plVersion tExtensionRegistryVersion = plExtensionRegistryI_version;
            plVersion tIOIVersion = plIOI_version;
            plVersion tMemoryIVersion = plMemoryI_version;
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: v%s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug null\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release null\n\n");
            #endif
            printf("~~~~~~~~API Versions~~~~~~~~~\n\n");
            printf("plWindowI:            v%u.%u.%u\n", tWindowExtVersion.uMajor, tWindowExtVersion.uMinor, tWindowExtVersion.uPatch);
            printf("plLibraryI:           v%u.%u.%u\n", tLibraryVersion.uMajor, tLibraryVersion.uMinor, tLibraryVersion.uPatch);
            printf("plDataRegistryI:      v%u.%u.%u\n", tDataRegistryVersion.uMajor, tDataRegistryVersion.uMinor, tDataRegistryVersion.uPatch);
            printf("plExtensionRegistryI: v%u.%u.%u\n", tExtensionRegistryVersion.uMajor, tExtensionRegistryVersion.uMinor, tExtensionRegistryVersion.uPatch);
            printf("plIOI:                v%u.%u.%u\n", tIOIVersion.uMajor, tIOIVersion.uMinor, tIOIVersion.uPatch);
            printf("plMemoryI:            v%u.%u.%u\n", tMemoryIVersion.uMajor, tMemoryIVersion.uMinor, tMemoryIVersion.uPatch);

            return 0;
        }
        else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("\nPilot Light - light weight game engine\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            printf("Usage: pilot_light.exe [options]\n\n");
            printf("Options:\n");
            printf("-h              %s\n", "Displays this information.");
            printf("--help          %s\n", "Displays this information.");
            printf("--version       %s\n", "Displays Pilot Light version information.");
            printf("--extensions    %s\n", "Displays embedded extensions.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'app'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'app'.");
            printf("-hr             %s\n", "Activates hot reloading.");
            printf("--hot-reload    %s\n", "Activates hot reloading.");
            return 0;
        }
    }

    // load core apis
    pl__load_core_apis();

    gptIOCtx = gptIOI->get_io();
    gptIOCtx->bHotReloadActive = gbHotReloadActive;

    // command line args
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    // load app library
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName
    };

    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        #ifdef _WIN32
            pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl  *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl  *)(plWindow*, void*))             ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl  *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            pl_app_info     = (bool  (__cdecl  *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");

        #else
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(plWindow*, void*))             ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
        #endif

        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        gpUserData = pl_app_load(gptApiRegistry, NULL);
        bool bApisFound = pl__check_apis();
        if(!bApisFound)
            return 3;
    }

    // main loop
    while (gptIOCtx->bRunning)
    {

        pl__garbage_collect_data_reg();

        // setup time step
        // INT64 ilCurrentTime = 0;
        // QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
        // gptIOCtx->fDeltaTime = (float)(ilCurrentTime - ilTime) / ilTicksPerSecond;
        // ilTime = ilCurrentTime;
        if(!gptIOCtx->bViewportMinimized)
        {
            pl_app_update(gpUserData);
            // pl__handle_extension_reloads();
        }
    
        if(gbApisDirty)
            pl__check_apis();

    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    // unload extensions & APIs
    pl__unload_all_extensions();
    pl__unload_core_apis();

    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    pl__check_for_leaks();
}

//-----------------------------------------------------------------------------
// [SECTION] window ext
//-----------------------------------------------------------------------------

plWindowResult
pl_create_window(plWindowDesc tDesc, plWindow** pptWindowOut)
{
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_destroy_window(plWindow* ptWindow)
{
}

void
pl_show_window(plWindow* ptWindow)
{ 
}

//-----------------------------------------------------------------------------
// [SECTION] file ext
//-----------------------------------------------------------------------------

void
pl_binary_read_file(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{
    if(pszSizeIn == NULL)
        return;

    FILE* ptDataFile = fopen(pcFile, "rb");
    size_t uSize = 0u;

    if (ptDataFile == NULL)
    {
        *pszSizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    
    if(pcBuffer == NULL)
    {
        *pszSizeIn = uSize;
        fclose(ptDataFile);
        return;
    }
    fseek(ptDataFile, 0, SEEK_SET);

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
        return;
    }

    fclose(ptDataFile);
    return;
}

void
pl_copy_file(const char* source, const char* destination)
{
    #ifdef _WIN32
        BOOL bResult = CopyFile(source, destination, FALSE);
    #elif defined(__APPLE__)
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(source, destination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
    #else
        size_t bufferSize = 0u;
        pl_binary_read_file(source, &bufferSize, NULL);

        struct stat stat_buf;
        int fromfd = open(source, O_RDONLY);
        fstat(fromfd, &stat_buf);
        int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
        int n = 1;
        while (n > 0)
            n = sendfile(tofd, fromfd, 0, bufferSize * 2);
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

#ifdef _WIN32
static inline FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}
#elif defined(__APPLE__)
struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}
#else
static inline time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
}
#endif

bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    PL_ASSERT(ptLibrary);
    if(ptLibrary)
    {
        #ifdef _WIN32
        FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return CompareFileTime(&newWriteTime, &ptLibrary->tLastWriteTime) != 0;
        #elif defined(__APPLE__)
        struct timespec newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime.tv_sec != ptLibrary->tLastWriteTime.tv_sec;
        #else
        time_t newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime != ptLibrary->tLastWriteTime;
        #endif
    }
    return false;
}


#ifdef _WIN32
plLibraryResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{

    plSharedLibrary* ptLibrary = NULL;

    const char* pcLockFile = "lock.tmp";
    const char* pcCacheDirectory = "../out-temp";

    if(*pptLibraryOut == NULL)
    {

        if(gbHotReloadActive)
        {
            DWORD dwAttrib = GetFileAttributes(pcCacheDirectory);

            if(dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
            {
                // do nothing
            }
            else
            {
                CreateDirectoryA(pcCacheDirectory, NULL);
            }
        }

        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));
        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;
        pl_str_get_file_name_only(tDesc.pcName, ptLibrary->acName, PL_MAX_NAME_LENGTH);
        pl_str_get_directory(tDesc.pcName, ptLibrary->acDirectory, PL_MAX_PATH_LENGTH);

        if(pl_str_get_file_extension(tDesc.pcName, ptLibrary->acFileExtension, 16) == NULL)
            strncpy(ptLibrary->acFileExtension, "dll", 16);

        pl_sprintf(ptLibrary->acPath, "%s%s.%s", ptLibrary->acDirectory, ptLibrary->acName, ptLibrary->acFileExtension);
        
        if(gbHotReloadActive)
        {
            pl_sprintf(ptLibrary->acLockFile, "%s%s", ptLibrary->acDirectory, pcLockFile);
            pl_sprintf(ptLibrary->acTransitionalPath, "%s/%s_", pcCacheDirectory, ptLibrary->acName);
        }

        if(!gbHotReloadActive || !(tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE))
        {

            char acTemporaryName[PL_MAX_PATH_LENGTH] = {0};
            ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
            
            pl_sprintf(acTemporaryName, "%s.%s", ptLibrary->acName, ptLibrary->acFileExtension);

            SetDllDirectoryA(ptLibrary->acDirectory);
            ptLibrary->tHandle = NULL;
            ptLibrary->tHandle = LoadLibraryA(acTemporaryName);
            if(ptLibrary->tHandle)
                ptLibrary->bValid = true;
            else
            {
                DWORD iLastError = GetLastError();
                printf("LoadLibaryA() failed with error code : %d\n", iLastError);
            }
            SetDllDirectoryA(NULL);
    }
        
    }
    else
        ptLibrary = *pptLibraryOut;


    if(gbHotReloadActive && tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE)
    {

        ptLibrary->bValid = false;

        WIN32_FILE_ATTRIBUTE_DATA tIgnored;
        if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
        {
            char acTemporaryPath[PL_MAX_PATH_LENGTH] = {0};
            char acTemporaryName[PL_MAX_NAME_LENGTH] = {0};
            ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
            
            pl_sprintf(acTemporaryPath, "%s%u.%s", ptLibrary->acTransitionalPath, ptLibrary->uTempIndex, ptLibrary->acFileExtension);
            pl_sprintf(acTemporaryName, "%s_%u.%s", ptLibrary->acName, ptLibrary->uTempIndex, ptLibrary->acFileExtension);
            if(++ptLibrary->uTempIndex >= 1024)
            {
                ptLibrary->uTempIndex = 0;
            }
            CopyFile(ptLibrary->acPath, acTemporaryPath, FALSE);


            SetDllDirectoryA(pcCacheDirectory);

            ptLibrary->tHandle = NULL;
            ptLibrary->tHandle = LoadLibraryA(acTemporaryName);
            if(ptLibrary->tHandle)
                ptLibrary->bValid = true;
            else
            {
                DWORD iLastError = GetLastError();
                printf("LoadLibaryA() failed with error code : %d\n", iLastError);
            }
            SetDllDirectoryA(NULL);
        }
    }

    if(ptLibrary->bValid)
        return PL_LIBRARY_RESULT_SUCCESS;
    return PL_LIBRARY_RESULT_FAIL;
}
#elif defined(__APPLE__)
plLibraryResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{

    plSharedLibrary* ptLibrary = NULL;

    const char* pcLockFile = "lock.tmp";
    const char* pcCacheDirectory = "../out-temp";

    if(*pptLibraryOut == NULL)
    {
        struct stat st = {0};

        if (stat(pcCacheDirectory, &st) == -1)
            mkdir(pcCacheDirectory, 0700);

        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));

        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;
        pl_str_get_file_name_only(tDesc.pcName, ptLibrary->acName, PL_MAX_NAME_LENGTH);
        pl_str_get_directory(tDesc.pcName, ptLibrary->acDirectory, PL_MAX_PATH_LENGTH);

        if(pl_str_get_file_extension(tDesc.pcName, ptLibrary->acFileExtension, 16) == NULL)
            strncpy(ptLibrary->acFileExtension, gpcLibraryExtension, 16);

        pl_sprintf(ptLibrary->acPath, "%s%s.%s", ptLibrary->acDirectory, ptLibrary->acName, ptLibrary->acFileExtension);
        if(gbHotReloadActive)
        {
            pl_sprintf(ptLibrary->acLockFile, "%s%s", ptLibrary->acDirectory, pcLockFile);
            pl_sprintf(ptLibrary->acTransitionalPath, "%s/%s_", pcCacheDirectory, ptLibrary->acName);
        }

        if(!gbHotReloadActive || !(tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE))
        {
            ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
            ptLibrary->handle = NULL;
            ptLibrary->handle = dlopen(ptLibrary->acPath, RTLD_NOW);
            if(ptLibrary->handle)
                ptLibrary->bValid = true;
            else
            {
                printf("\n\n%s\n\n", dlerror());
            }
        }
    }
    else
        ptLibrary = *pptLibraryOut;

    if(gbHotReloadActive && tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE)
    {
        ptLibrary->bValid = false;

        if(ptLibrary)
        {
            struct stat attr2;
            if(stat(ptLibrary->acLockFile, &attr2) == -1)  // lock file gone
            {
                char acTemporaryPath[PL_MAX_PATH_LENGTH] = {0};
                ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
                
                pl_sprintf(acTemporaryPath, "%s%u.%s", ptLibrary->acTransitionalPath, ptLibrary->uTempIndex, ptLibrary->acFileExtension);
                if(++ptLibrary->uTempIndex >= 1024)
                {
                    ptLibrary->uTempIndex = 0;
                }
                pl_copy_file(ptLibrary->acPath, acTemporaryPath);

                ptLibrary->handle = NULL;
                ptLibrary->handle = dlopen(acTemporaryPath, RTLD_NOW);
                if(ptLibrary->handle)
                    ptLibrary->bValid = true;
                else
                {
                    printf("\n\n%s\n\n", dlerror());
                }

            }
        }
    }
    if(ptLibrary->bValid)
        return PL_LIBRARY_RESULT_SUCCESS;
    return PL_LIBRARY_RESULT_FAIL;
}
#else
plLibraryResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{
    plSharedLibrary* ptLibrary = NULL;

    const char* pcLockFile = "lock.tmp";
    const char* pcCacheDirectory = "../out-temp";

    if(*pptLibraryOut == NULL)
    {

        struct stat st = {0};

        if (stat(pcCacheDirectory, &st) == -1)
            mkdir(pcCacheDirectory, 0700);

        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));

        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;
        pl_str_get_file_name_only(tDesc.pcName, ptLibrary->acName, PL_MAX_NAME_LENGTH);
        pl_str_get_directory(tDesc.pcName, ptLibrary->acDirectory, PL_MAX_PATH_LENGTH);

        if(pl_str_get_file_extension(tDesc.pcName, ptLibrary->acFileExtension, 16) == NULL)
            strncpy(ptLibrary->acFileExtension, gpcLibraryExtension, 16);

        pl_sprintf(ptLibrary->acPath, "%s%s.%s", ptLibrary->acDirectory, ptLibrary->acName, ptLibrary->acFileExtension);
        if(gbHotReloadActive)
        {
            pl_sprintf(ptLibrary->acLockFile, "%s%s", ptLibrary->acDirectory, pcLockFile);
            pl_sprintf(ptLibrary->acTransitionalPath, "%s/%s_", pcCacheDirectory, ptLibrary->acName);
        }

        if(!gbHotReloadActive || !(tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE))
        {
            ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
            ptLibrary->handle = NULL;
            ptLibrary->handle = dlopen(ptLibrary->acPath, RTLD_NOW);
            if(ptLibrary->handle)
                ptLibrary->bValid = true;
            else
            {
                printf("\n\n%s\n\n", dlerror());
            }
        }
    }
    else
        ptLibrary = *pptLibraryOut;

    if(gbHotReloadActive && tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE)
    {
        ptLibrary->bValid = false;

        if(ptLibrary)
        {
            struct stat attr2;
            if(stat(ptLibrary->acLockFile, &attr2) == -1)  // lock file gone
            {
                char acTemporaryPath[PL_MAX_PATH_LENGTH] = {0};
                ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
                
                pl_sprintf(acTemporaryPath, "%s%u.%s", ptLibrary->acTransitionalPath, ptLibrary->uTempIndex, ptLibrary->acFileExtension);
                if(++ptLibrary->uTempIndex >= 1024)
                {
                    ptLibrary->uTempIndex = 0;
                }
                pl_copy_file(ptLibrary->acPath, acTemporaryPath);

                ptLibrary->handle = NULL;
                ptLibrary->handle = dlopen(acTemporaryPath, RTLD_NOW);
                if(ptLibrary->handle)
                    ptLibrary->bValid = true;
                else
                {
                    printf("\n\n%s\n\n", dlerror());
                }

            }
        }
    }
    if(ptLibrary->bValid)
        return PL_LIBRARY_RESULT_SUCCESS;
    return PL_LIBRARY_RESULT_FAIL;
}
#endif

void
pl_reload_library(plSharedLibrary* ptLibrary)
{
    if(ptLibrary->tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE)
    {
        ptLibrary->bValid = false;
        for(uint32_t i = 0; i < 100; i++)
        {
            if(pl_load_library(ptLibrary->tDesc, &ptLibrary))
                break;
            // pl_sleep(100);
            #ifdef _WIN32
            Sleep((long)100);
            #else
            struct timespec ts = {0};
            int res;
        
            ts.tv_sec = 100 / 1000;
            ts.tv_nsec = (100 % 1000) * 1000000;
        
            do 
            {
                res = nanosleep(&ts, &ts);
            } 
            while (res);
            #endif
        }
    }
}

void*
pl_load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "library not valid, should have been checked");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        #ifdef _WIN32
        pLoadedFunction = (void*)GetProcAddress(ptLibrary->tHandle, name);
        #else
        pLoadedFunction = dlsym(ptLibrary->handle, name);
        #endif
    }
    return pLoadedFunction;
}


//-----------------------------------------------------------------------------
// [SECTION] threads ext
//-----------------------------------------------------------------------------

void
pl_create_mutex(plMutex** ppMutexOut)
{
}

void
pl_destroy_mutex(plMutex** ptMutex)
{
}

void
pl_lock_mutex(plMutex* ptMutex)
{
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"