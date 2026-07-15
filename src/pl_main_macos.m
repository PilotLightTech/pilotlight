/*
   pl_main_macos.m
     * MacOS platform backend
     *
     * This remains a single unity-build translation unit, but application
     * lifecycle/frame driving and Cocoa window management are intentionally
     * separated into independent sections. The window backend never calls
     * pl_app_update(), performs hot reloads, or owns application shutdown.
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs & interfaces
// [SECTION] globals
// [SECTION] entry point
// [SECTION] application host
// [SECTION] Cocoa application delegate
// [SECTION] Cocoa view
// [SECTION] Cocoa view controller
// [SECTION] input
// [SECTION] library ext
// [SECTION] window ext
// [SECTION] clipboard
// [SECTION] thread ext
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
#include "pl_ds.h"
#include "pl_string.h"
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#import <dispatch/dispatch.h>
#import <time.h>
#include <stdlib.h>   // malloc, calloc, free
#include <string.h>   // strncpy
#include <sys/stat.h> // timespec
#include <stdio.h>    // file api
#include <copyfile.h> // copyfile
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <unistd.h>   // close
#include <fcntl.h>    // O_RDONLY, O_WRONLY, O_CREAT
#include <pthread.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}

static inline CFTimeInterval
pl__get_absolute_time(void)
{
    return (CFTimeInterval)((double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9);
}



static void          pl__load_application_functions(void);
static void          pl__application_frame(void);
static void          pl__application_shutdown_once(void);
static bool          pl__start_application_render_loop(void);
static void          pl__stop_application_render_loop(void);
static CVReturn      pl__dispatch_render_loop(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void*);

id                gtAppDelegate = nil;

//-----------------------------------------------------------------------------
// [SECTION] structs & interfaces
//-----------------------------------------------------------------------------

typedef struct _plPlatformExtData
{
    NSNumber* tScreen;
    id<MTLDevice> tDevice;
    CAMetalLayer* ptLayer;
    id gtAppDelegate;
} plPlatformExtData;

typedef struct _plSharedLibrary
{
    bool            bValid;
    uint32_t        uTempIndex;
    char            acFileExtension[16];
    char            acName[PL_MAX_NAME_LENGTH];
    char            acPath[PL_MAX_PATH_LENGTH];
    char            acDirectory[PL_MAX_PATH_LENGTH];
    char            acTransitionalPath[PL_MAX_PATH_LENGTH];
    char            acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc   tDesc;
    void*           handle;
    struct timespec tLastWriteTime;
} plSharedLibrary;

typedef struct _plMacApplicationState
{
    CVDisplayLinkRef  tDisplayLink;
    dispatch_source_t tDisplaySource;
    CFTimeInterval    tLastFrameTime;
    bool              bShutdown;
} plMacApplicationState;



typedef struct _plRuntimeMutex
{
    pthread_mutex_t tHandle;
} plRuntimeMutex;


//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------



const plLibraryI* gptLibraryApi = NULL;

static plMacApplicationState gtApplication = {0};


const char* gpcLibraryExtension = "dylib";
const char* gpcLibraryPrefix    = "lib";

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int
main(int argc, char* argv[])
{
    const char* pcAppName = "app";

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--app") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "Missing application name after '%s'.\n", argv[i]);
                return 1;
            }
            pcAppName = argv[++i];
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
            printf("Usage: pilot_light [options]\n\n");
            printf("Options:\n");
            printf("-h              %s\n", "Displays this information.");
            printf("--help          %s\n", "Displays this information.");
            printf("--version       %s\n", "Displays Pilot Light version information.");
            printf("--apis          %s\n", "Displays embedded apis.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'app'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'app'.");
            printf("-hr             %s\n", "Activates hot reloading.");
            printf("--hot-reload    %s\n", "Activates hot reloading.");
            return 0;
        }
    }

    #if __has_feature(objc_arc)
        NSLog(@"ARC on");
    #else
        NSLog(@"ARC off");
    #endif

    // Application host setup.
    pl__load_core_apis();

    gptIOCtx = gptIOI->get_io();
    gptIOCtx->bHotReloadActive = gbHotReloadActive;
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    [NSApplication sharedApplication];
    
    

    // Load the application library. The application is free to create its
    // windows during pl_app_load().
    gptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    const plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName,
        .tFlags = PL_LIBRARY_FLAGS_RELOADABLE
    };

    if(!gptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        pl__application_shutdown_once();
        NSApplication.sharedApplication.delegate = nil;
        [gtAppDelegate release];
        gtAppDelegate = nil;
        return 2;
    }

    pl__load_application_functions();

    if(pl_app_info && !pl_app_info(gptApiRegistry))
    {
        pl__application_shutdown_once();
        NSApplication.sharedApplication.delegate = nil;
        [gtAppDelegate release];
        gtAppDelegate = nil;
        return 0;
    }

    gptIOCtx->pAppUserData = pl_app_load(gptApiRegistry, NULL);
    if(gptIOCtx->platform_setup)
        gptIOCtx->pBackendPlatformData = gptIOCtx->platform_setup();
    gptIOCtx->_bFirstLoadComplete = true;
    plPlatformExtData* ptWindowData = gptIOCtx->pBackendPlatformData;
    NSApplication.sharedApplication.delegate = ptWindowData->gtAppDelegate;
    gtAppDelegate = ptWindowData->gtAppDelegate;
    if(!pl__check_apis())
    {
        pl__application_shutdown_once();
        NSApplication.sharedApplication.delegate = nil;
        [gtAppDelegate release];
        gtAppDelegate = nil;
        return 3;
    }

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    if(!pl__start_application_render_loop())
    {
        pl__application_shutdown_once();
        NSApplication.sharedApplication.delegate = nil;
        [gtAppDelegate release];
        gtAppDelegate = nil;
        return 4;
    }

    [NSApp run];

    // applicationWillTerminate normally performs this. Keep the call here so
    // an unusual run-loop exit still receives deterministic cleanup.
    pl__application_shutdown_once();

    NSApplication.sharedApplication.delegate = nil;
    [gtAppDelegate release];
    gtAppDelegate = nil;
    return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] application host
//-----------------------------------------------------------------------------

static void
pl__load_application_functions(void)
{
    pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
    pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        gptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
    pl_app_resize   = (void  (__attribute__(()) *)(void*, void*))             gptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
    pl_app_update   = (void  (__attribute__(()) *)(void*))                        gptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
    pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        gptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
    gptIOCtx->pl_app_resize = pl_app_resize;
    gptIOCtx->pl_app_update = pl_app_update;
}

static bool
pl__start_application_render_loop(void)
{
    if(gtApplication.tDisplayLink)
        return true;

    gtApplication.tDisplaySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, dispatch_get_main_queue());
    if(!gtApplication.tDisplaySource)
        return false;

    dispatch_source_set_event_handler(gtApplication.tDisplaySource, ^{
        @autoreleasepool
        {
            pl__application_frame();
        }
    });
    dispatch_resume(gtApplication.tDisplaySource);

    CVReturn tResult = CVDisplayLinkCreateWithActiveCGDisplays(&gtApplication.tDisplayLink);
    if(tResult != kCVReturnSuccess)
    {
        dispatch_source_cancel(gtApplication.tDisplaySource);
        gtApplication.tDisplaySource = NULL;
        return false;
    }

    tResult = CVDisplayLinkSetOutputCallback(gtApplication.tDisplayLink,
                                              pl__dispatch_render_loop,
                                              (__bridge void*)gtApplication.tDisplaySource);
    if(tResult != kCVReturnSuccess)
    {
        pl__stop_application_render_loop();
        return false;
    }



    plPlatformExtData* ptWindowData = gptIOCtx->pBackendPlatformData;

    // Follow the main window's display when one exists. Failure is not fatal;
    // the display link created above is still valid for active displays.
    if(ptWindowData->tScreen)
    {
        NSNumber* ptScreenNumber = ptWindowData->tScreen;
        if(ptScreenNumber)
            CVDisplayLinkSetCurrentCGDisplay(gtApplication.tDisplayLink, (CGDirectDisplayID)ptScreenNumber.unsignedIntegerValue);
    }

    tResult = CVDisplayLinkStart(gtApplication.tDisplayLink);
    if(tResult != kCVReturnSuccess)
    {
        pl__stop_application_render_loop();
        return false;
    }

    return true;
}

static void
pl__stop_application_render_loop(void)
{
    if(gtApplication.tDisplayLink)
    {
        CVDisplayLinkStop(gtApplication.tDisplayLink);
        CVDisplayLinkRelease(gtApplication.tDisplayLink);
        gtApplication.tDisplayLink = NULL;
    }

    if(gtApplication.tDisplaySource)
    {
        dispatch_source_cancel(gtApplication.tDisplaySource);
        #if defined(OS_OBJECT_USE_OBJC) && !OS_OBJECT_USE_OBJC
            dispatch_release(gtApplication.tDisplaySource);
        #endif
        gtApplication.tDisplaySource = NULL;
    }
}

static CVReturn
pl__dispatch_render_loop(CVDisplayLinkRef displayLink,
                         const CVTimeStamp* now,
                         const CVTimeStamp* outputTime,
                         CVOptionFlags flagsIn,
                         CVOptionFlags* flagsOut,
                         void* displayLinkContext)
{
    (void)displayLink;
    (void)now;
    (void)outputTime;
    (void)flagsIn;
    (void)flagsOut;

    dispatch_source_t tSource = (__bridge dispatch_source_t)displayLinkContext;
    if(tSource)
        dispatch_source_merge_data(tSource, 1);
    return kCVReturnSuccess;
}

static void
pl__application_frame(void)
{
    if(gtApplication.bShutdown)
        return;

    if(gbHotReloadActive && gptLibraryApi->has_changed(gptAppLibrary))
    {
        pl_reload_library(gptAppLibrary);
        pl__load_application_functions();
        pl__handle_extension_reloads();
        gptIOCtx->pAppUserData = pl_app_load(gptApiRegistry, gptIOCtx->pAppUserData);
    }

    // The window backend applies native changes and records events. The
    // application host is the only layer that dispatches pl_app_resize().
    if(gptIOCtx->bViewportSizeChanged)
    {
        gptIOCtx->bViewportSizeChanged = false;
        return;
    }

    gptIOCtx->platform_new_frame(gptIOCtx->pBackendPlatformData);

    if(gtApplication.tLastFrameTime == 0.0)
        gtApplication.tLastFrameTime = pl__get_absolute_time();

    const double dCurrentTime = pl__get_absolute_time();
    gptIOCtx->fDeltaTime = (float)(dCurrentTime - gtApplication.tLastFrameTime);
    gtApplication.tLastFrameTime = dCurrentTime;

    pl__garbage_collect_data_reg();
    if(pl_app_update)
        pl_app_update(gptIOCtx->pAppUserData);
    pl__handle_extension_reloads();

    if(gbApisDirty)
        pl__check_apis();

    if(!gptIOCtx->bRunning)
        [NSApp terminate:nil];
}

static void
pl__application_shutdown_once(void)
{
    if(gtApplication.bShutdown)
        return;

    gtApplication.bShutdown = true;
    pl__stop_application_render_loop();

    if(pl_app_shutdown)
    {
        pl_app_shutdown(gptIOCtx->pAppUserData);
    }

    gptIOCtx->platform_cleanup(gptIOCtx->pBackendPlatformData);
    pl__unload_all_extensions();

    // gptAppLibrary was allocated through the core memory API, so release it
    // before unregistering the core APIs.
    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
        gptAppLibrary = NULL;
    }

    pl__unload_core_apis();
    pl__check_for_leaks();
}

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

void
pl_file_copy(const char* source, const char* destination)
{
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(source, destination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
}

bool
pl_has_library_changed(plSharedLibrary* library)
{
    PL_ASSERT(library);
    if(library)
    {
        struct timespec newWriteTime = pl__get_last_write_time(library->acPath);
        return newWriteTime.tv_sec != library->tLastWriteTime.tv_sec;
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
            strncpy(ptLibrary->acFileExtension, "dylib", 16);

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
pl_reload_library(plSharedLibrary* library)
{
    if(library->tDesc.tFlags & PL_LIBRARY_FLAGS_RELOADABLE)
    {
        library->bValid = false;
        for(uint32_t i = 0; i < 100; i++)
        {
            if(pl_load_library(library->tDesc, &library))
                break;
            // pl_sleep(100);
            struct timespec ts;
            int res;

            ts.tv_sec = 100 / 1000;
            ts.tv_nsec = (100 % 1000) * 1000000;

            do {
                res = nanosleep(&ts, &ts);
            } 
            while (res);
        }
    }
}

void*
pl_load_library_function(plSharedLibrary* library, const char* name)
{
    PL_ASSERT(library->bValid && "Library not valid");
    void* loadedFunction = NULL;
    if(library->bValid)
    {
        loadedFunction = dlsym(library->handle, name);
    }
    return loadedFunction;
}


//-----------------------------------------------------------------------------
// [SECTION] thread ext
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
