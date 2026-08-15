/*
   pl_main_macos.c
     * null platform backend (for testing on macos, not complete)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
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

#include <time.h> // clock_gettime_nsec_np
#include <sys/stat.h> // timespec
#include <copyfile.h> // copyfile
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <unistd.h> // close
#include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
#include <pthread.h>


//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------


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
const char* gpcLibraryPrefix    = "lib";

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    printf("here\n");
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
        pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(()) *)(void*, void*))                 ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");


        gptIOCtx->pl_app_resize = pl_app_resize;
        gptIOCtx->pl_app_update = pl_app_update;
        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        gptIOCtx->pAppUserData = pl_app_load(gptApiRegistry, NULL);
        // if(gptIOCtx->platform_setup)
        //     gptIOCtx->pBackendPlatformData = gptIOCtx->platform_setup();
        bool bApisFound = pl__check_apis();
        if(!bApisFound)
            return 3;
    }
    else
        return 2;

    // main loop
    while (gptIOCtx->bRunning)
    {

        // gptIOCtx->platform_new_frame(gptIOCtx->pBackendPlatformData);

        pl__garbage_collect_data_reg();

        if(!gptIOCtx->bViewportMinimized)
        {
            pl_app_update(gptIOCtx->pAppUserData);
            // pl__handle_extension_reloads();
        }
    
        if(gbApisDirty)
            pl__check_apis();

    }

    // app cleanup
    pl_app_shutdown(gptIOCtx->pAppUserData);

    // if(gptIOCtx->platform_cleanup)
    //     gptIOCtx->platform_cleanup(gptIOCtx->pBackendPlatformData);

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
// [SECTION] file ext
//-----------------------------------------------------------------------------

void
pl_file_binary_read(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
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
pl_file_copy(const char* source, const char* destination)
{
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(source, destination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
}

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}

bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    PL_ASSERT(ptLibrary);
    if(ptLibrary)
    {
        struct timespec newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime.tv_sec != ptLibrary->tLastWriteTime.tv_sec;
    }
    return false;
}

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

        pl_sprintf(ptLibrary->acPath, "%slib%s.%s", ptLibrary->acDirectory, ptLibrary->acName, ptLibrary->acFileExtension);
        if(gbHotReloadActive)
        {
            pl_sprintf(ptLibrary->acLockFile, "%slib%s", ptLibrary->acDirectory, pcLockFile);
            pl_sprintf(ptLibrary->acTransitionalPath, "%s/lib%s_", pcCacheDirectory, ptLibrary->acName);
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
                pl_file_copy(ptLibrary->acPath, acTemporaryPath);

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
            struct timespec ts = {0};
            int res;
        
            ts.tv_sec = 100 / 1000;
            ts.tv_nsec = (100 % 1000) * 1000000;
        
            do 
            {
                res = nanosleep(&ts, &ts);
            } 
            while (res);
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
        pLoadedFunction = dlsym(ptLibrary->handle, name);
    }
    return pLoadedFunction;
}


//-----------------------------------------------------------------------------
// [SECTION] threads ext
//-----------------------------------------------------------------------------

typedef struct _plMutex
{
    pthread_mutex_t tHandle;
} plMutex;

void
pl_create_mutex(plMutex** pptMutexOut)
{
    *pptMutexOut = malloc(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
    }
}

void
pl_lock_mutex(plMutex* ptMutex)
{
    pthread_mutex_lock(&ptMutex->tHandle);
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
    pthread_mutex_unlock(&ptMutex->tHandle);
}

void
pl_destroy_mutex(plMutex** pptMutex)
{
    pthread_mutex_destroy(&(*pptMutex)->tHandle);
    free(*pptMutex);
    *pptMutex = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"