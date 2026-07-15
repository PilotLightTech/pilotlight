/*
   pl_main_win32.c
     * win32 main backend
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] library api
// [SECTION] threads
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
// #include "pl_ds.h"    // hashmap & stretchy buffer
#include "pl_string.h"
#include <stdlib.h>   // exit
#include <stdio.h>    // printf
#include <windows.h>

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// win32 stuff
bool        gbEnableVirtualTerminalProcessing = true;
INT64       ilTime                            = 0;
INT64       ilTicksPerSecond                  = 0;
const char* gpcLibraryPrefix                  = "";
const char* gpcLibraryExtension               = "dll";

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    const char* pcAppName = "app";

    for(int i = 1; i < argc; i++)
    { 
        if(strcmp(argv[i], "--disable_vt") == 0)
        {
            gbEnableVirtualTerminalProcessing = false;
        }
        else if(strcmp(argv[i], "-hr") == 0 || strcmp(argv[i], "--hot-reload") == 0)
        {
            gbHotReloadActive = true;
        }
        else if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--app") == 0)
        {
            pcAppName = argv[i + 1];
            i++;
        }
        else if(strcmp(argv[i], "--version") == 0)
        {
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
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
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
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
            printf("--apis          %s\n", "Displays embedded apis.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'app'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'app'.");
            printf("-hr             %s\n", "Activates hot reloading.");
            printf("--hot-reload    %s\n", "Activates hot reloading.");

            printf("\nWin32 Only:\n");
            printf("--disable_vt:   %s\n", "Disables escape characters.");
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

    // setup info for clock
    QueryPerformanceFrequency((LARGE_INTEGER*)&ilTicksPerSecond);
    QueryPerformanceCounter((LARGE_INTEGER*)&ilTime);
    
    // setup console
    DWORD tCurrentMode = 0;
    DWORD tOriginalMode = 0;
    HANDLE tStdOutHandle = NULL;

    if(gbEnableVirtualTerminalProcessing)
    {
        tStdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        if(tStdOutHandle == INVALID_HANDLE_VALUE)
            exit(GetLastError());
        if(!GetConsoleMode(tStdOutHandle, &tCurrentMode))
            exit(GetLastError());
        tOriginalMode = tCurrentMode;
        tCurrentMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // enable ANSI escape codes
        if(!SetConsoleMode(tStdOutHandle, tCurrentMode))
            exit(GetLastError());
    }

    // load app library
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName,
        .tFlags = PL_LIBRARY_FLAGS_RELOADABLE
    };
    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__cdecl  *)(void*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        pl_app_info     = (bool  (__cdecl  *)(const plApiRegistryI*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
        gptIOCtx->pl_app_resize = pl_app_resize;
        gptIOCtx->pl_app_update = pl_app_update;
        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        gptIOCtx->pAppUserData = pl_app_load(gptApiRegistry, NULL);
        if(gptIOCtx->platform_setup)
            gptIOCtx->pBackendPlatformData = gptIOCtx->platform_setup();
        gptIOCtx->_bFirstLoadComplete = true;
        bool bApisFound = pl__check_apis();
        if(!bApisFound)
            return 3;
    }
    else
        return 2;

    // main loop
    while (gptIOCtx->bRunning)
    {

        // while queue has messages, remove and dispatch them (but do not block on empty queue)
        MSG tMsg = {0};
        while (PeekMessage(&tMsg, NULL, 0, 0, PM_REMOVE))
        {
            // check for quit because peekmessage does not signal this via return val
            if (tMsg.message == WM_QUIT)
            {
                gptIOCtx->bRunning = false;
                break;
            }
            // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
            TranslateMessage(&tMsg);
            DispatchMessage(&tMsg);
        }

        if(gptIOCtx->platform_new_frame)
            gptIOCtx->platform_new_frame(gptIOCtx->pBackendPlatformData);

        // reload library
        if(gbHotReloadActive && ptLibraryApi->has_changed(gptAppLibrary))
        {
            pl_reload_library(gptAppLibrary);
            pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl  *)(void*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");

            gptIOCtx->pl_app_resize = pl_app_resize;
            gptIOCtx->pl_app_update = pl_app_update;
            pl__handle_extension_reloads();
            gptIOCtx->pAppUserData = pl_app_load(gptApiRegistry, gptIOCtx->pAppUserData);
            
        }

        // render frame
        if(gptIOCtx->bRunning)
        {
            pl__garbage_collect_data_reg();

            // setup time step
            INT64 ilCurrentTime = 0;
            QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
            gptIOCtx->fDeltaTime = (float)(ilCurrentTime - ilTime) / ilTicksPerSecond;
            ilTime = ilCurrentTime;
            if(!gptIOCtx->bViewportMinimized)
            {
                pl_app_update(gptIOCtx->pAppUserData);
                pl__handle_extension_reloads();
            }

            if(gbApisDirty)
                pl__check_apis();
        }
    }

    // app cleanup
    pl_app_shutdown(gptIOCtx->pAppUserData);

    if(gptIOCtx->platform_cleanup)
        gptIOCtx->platform_cleanup(gptIOCtx->pBackendPlatformData);

    // unload extensions & APIs
    pl__unload_all_extensions();
    pl__unload_core_apis();

    // return console to original mode
    if(gbEnableVirtualTerminalProcessing)
    {
        if(!SetConsoleMode(tStdOutHandle, tOriginalMode))
            exit(GetLastError());
    }

    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    pl__check_for_leaks();
}

//-----------------------------------------------------------------------------
// [SECTION] library api
//-----------------------------------------------------------------------------

static inline FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}

bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    PL_ASSERT(ptLibrary);
    if(ptLibrary)
    {
        FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return CompareFileTime(&newWriteTime, &ptLibrary->tLastWriteTime) != 0;
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
                printf("LoadLibaryA() failed with error code : %d for %s\n", iLastError, acTemporaryName);
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
                printf("LoadLibaryA() failed with error code : %d for %s\n", iLastError, acTemporaryName);
            }
            SetDllDirectoryA(NULL);
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
            Sleep((long)100);
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
        pLoadedFunction = (void*)GetProcAddress(ptLibrary->tHandle, name);
    }
    return pLoadedFunction;
}

//-----------------------------------------------------------------------------
// [SECTION] threads
//-----------------------------------------------------------------------------

typedef struct _plMutex
{
    HANDLE tHandle;
} plMutex;

void
pl_create_mutex(plMutex** ppMutexOut)
{
    HANDLE tHandle = CreateMutex(NULL, FALSE, NULL);
    if(tHandle)
    {
        (*ppMutexOut) = (plMutex*)malloc(sizeof(plMutex));
        (*ppMutexOut)->tHandle = tHandle;
    }
}

void
pl_destroy_mutex(plMutex** ptMutex)
{
    CloseHandle((*ptMutex)->tHandle);
    free((*ptMutex));
    (*ptMutex) = NULL;
}

void
pl_lock_mutex(plMutex* ptMutex)
{
    DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
    if(!ReleaseMutex(ptMutex->tHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"