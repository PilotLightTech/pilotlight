/*
   pl_main_macos.c
     * MacOS platform backend

   Implemented APIs:
     [X] Windows
     [X] Libraries
     [X] UDP
     [X] Threads
     [X] Atomics
     [X] Virtual Memory
     [X] Clipboard
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs & interfaces
// [SECTION] globals
// [SECTION] entry point
// [SECTION] plNSView
// [SECTION] plNSViewController
// [SECTION] file api
// [SECTION] udp api
// [SECTION] library api
// [SECTION] thread api
// [SECTION] atomic api
// [SECTION] virtual memory api
// [SECTION] window api
// [SECTION] clipboard
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_os.h"
#include "pl_ds.h"
#import <Cocoa/Cocoa.h>

#ifndef PL_HEADLESS_APP
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#endif
#import <time.h>
#include <stdlib.h>   // malloc
#include <string.h>   // strncpy
#include <sys/stat.h> // timespec
#include <stdio.h>    // file api
#include <copyfile.h> // copyfile
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <sys/socket.h> // sockets
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/mman.h> // virtual memory

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// window api

#ifndef PL_HEADLESS_APP
plWindow* pl_create_window(const plWindowDesc*);
void      pl_destroy_window(plWindow*);
#endif

// clip board
const char* pl_get_clipboard_text(void* user_data_ctx);
void        pl_set_clipboard_text(void* pUnused, const char* text);

// file api
bool pl_file_exists      (const char* pcFile);
void pl_file_delete      (const char* pcFile);
void pl_binary_read_file (const char* pcFile, size_t* pszSize, uint8_t* pcBuffer);
void pl_copy_file        (const char* pcSource, const char* pcDestination);
void pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer);

// udp api
void pl_create_udp_socket(plSocket** pptSocketOut, bool bNonBlocking);
void pl_bind_udp_socket  (plSocket*, int iPort);
bool pl_send_udp_data    (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
bool pl_get_udp_data     (plSocket*, void* pData, size_t);

// library api
bool  pl_has_library_changed  (plSharedLibrary*);
bool  pl_load_library         (const char* pcName, const char* pcTransitionalName, const char* pcLockFile, plSharedLibrary** pptLibraryOut);
void  pl_reload_library       (plSharedLibrary*);
void* pl_load_library_function(plSharedLibrary*, const char* pcName);

// thread api: misc
void     pl_sleep(uint32_t millisec);
uint32_t pl_get_hardware_thread_count(void);

// thread api: thread
void pl_create_thread (plThreadProcedure, void* pData, plThread** ppThreadOut);
void pl_destroy_thread(plThread**);
void pl_join_thread   (plThread*);
void pl_yield_thread  (void);

// thread api: mutex
void pl_create_mutex (plMutex** ppMutexOut);
void pl_lock_mutex   (plMutex*);
void pl_unlock_mutex (plMutex*);
void pl_destroy_mutex(plMutex**);

// thread api: critical section
void pl_create_critical_section (plCriticalSection** pptCriticalSectionOut);
void pl_destroy_critical_section(plCriticalSection**);
void pl_enter_critical_section  (plCriticalSection*);
void pl_leave_critical_section  (plCriticalSection*);

// thread api: semaphore
void pl_create_semaphore     (uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
void pl_wait_on_semaphore    (plSemaphore*);
bool pl_try_wait_on_semaphore(plSemaphore*);
void pl_release_semaphore    (plSemaphore*);
void pl_destroy_semaphore    (plSemaphore**);

// thread api: thread local storage
void  pl_allocate_thread_local_key (plThreadKey** pptKeyOut);
void  pl_free_thread_local_key     (plThreadKey**);
void* pl_allocate_thread_local_data(plThreadKey*, size_t szSize);
void* pl_get_thread_local_data     (plThreadKey*);
void  pl_free_thread_local_data    (plThreadKey*, void* pData);

// thread api: conditional variable
void pl_create_condition_variable  (plConditionVariable** pptConditionVariableOut);
void pl_destroy_condition_variable (plConditionVariable**);
void pl_wake_condition_variable    (plConditionVariable*);
void pl_wake_all_condition_variable(plConditionVariable*);
void pl_sleep_condition_variable   (plConditionVariable*, plCriticalSection*);

// thread api: barrier
void pl_create_barrier (uint32_t uThreadCount, plBarrier** pptBarrierOut);
void pl_destroy_barrier(plBarrier**);
void pl_wait_on_barrier(plBarrier*);

// atomics
void    pl_create_atomic_counter  (int64_t ilValue, plAtomicCounter** ptCounter);
void    pl_destroy_atomic_counter (plAtomicCounter**);
void    pl_atomic_store           (plAtomicCounter*, int64_t ilValue);
int64_t pl_atomic_load            (plAtomicCounter*);
bool    pl_atomic_compare_exchange(plAtomicCounter*, int64_t ilExpectedValue, int64_t ilDesiredValue);
void    pl_atomic_increment       (plAtomicCounter*);
void    pl_atomic_decrement       (plAtomicCounter*);

// virtual memory
size_t pl_get_page_size   (void);
void*  pl_virtual_alloc   (void* pAddress, size_t);
void*  pl_virtual_reserve (void* pAddress, size_t);
void*  pl_virtual_commit  (void* pAddress, size_t); 
void   pl_virtual_uncommit(void* pAddress, size_t);
void   pl_virtual_free    (void* pAddress, size_t); 

// memory
#define PL_ALLOC(x) gptMemory->realloc(NULL, x, __FILE__, __LINE__)
#define PL_FREE(x)  gptMemory->realloc(x, 0, __FILE__, __LINE__)

struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}

static inline CFTimeInterval pl__get_absolute_time(void) { return (CFTimeInterval)((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9); }

//-----------------------------------------------------------------------------
// [SECTION] structs & interfaces
//-----------------------------------------------------------------------------

#ifndef PL_HEADLESS_APP
@protocol plNSViewDelegate <NSObject>
- (void)drawableResize:(CGSize)size;
- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer;
- (void)shutdown;
@end

@interface plNSViewController : NSViewController <plNSViewDelegate>
@end

@interface plNSView : NSView <CALayerDelegate>
@property (nonatomic, nonnull, readonly) CAMetalLayer *metalLayer;
@property (nonatomic, getter=isPaused) BOOL paused;
@property (nonatomic, nullable,retain) plNSViewController* delegate;
- (void)initCommon;
- (void)resizeDrawable:(CGFloat)scaleFactor;
- (void)stopRenderLoop;
- (void)render;
@end

@interface plNSAppDelegate : NSObject <NSApplicationDelegate>
@end
@implementation plNSAppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { return YES;}
@end

@interface plKeyEventResponder: NSView<NSTextInputClient>
@end

#endif

typedef struct _plSharedLibrary
{
    bool     bValid;
    uint32_t uTempIndex;
    char     acPath[PL_MAX_PATH_LENGTH];
    char     acTransitionalName[PL_MAX_PATH_LENGTH];
    char     acLockFile[PL_MAX_PATH_LENGTH];
    void*           handle;
    struct timespec lastWriteTime;
} plSharedLibrary;

typedef struct _plAtomicCounter
{
    atomic_int_fast64_t ilValue;
} plAtomicCounter;

typedef int pthread_barrierattr_t;
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;

#ifndef PL_HEADLESS_APP
typedef struct _plWindowData
{
    NSWindow*           ptWindow;
    plNSViewController* ptViewController;
    CAMetalLayer*       ptLayer;
} plWindowData;
#endif

typedef struct _plSocket
{
    int iPort;
    int iSocket;
} plSocket;

typedef struct _plThread
{
    pthread_t tHandle;
} plThread;

typedef struct _plMutex
{
    pthread_mutex_t tHandle;
} plMutex;

typedef struct _plCriticalSection
{
    pthread_mutex_t tHandle;
} plCriticalSection;

typedef struct _plSemaphore
{
    dispatch_semaphore_t tHandle;
} plSemaphore;

typedef struct _plBarrier
{
    pthread_barrier_t tHandle;
} plBarrier;

typedef struct _plConditionVariable
{
    pthread_cond_t tHandle;
} plConditionVariable;

typedef struct _plThreadKey
{
    pthread_key_t tKey;
} plThreadKey;

// barrier api emulation
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count);
int pthread_barrier_destroy(pthread_barrier_t *barrier);
int pthread_barrier_wait(pthread_barrier_t *barrier);

#ifndef PL_HEADLESS_APP
// Undocumented methods for creating cursors. (from Dear ImGui)
@interface NSCursor()
+ (id)_windowResizeNorthWestSouthEastCursor;
+ (id)_windowResizeNorthEastSouthWestCursor;
+ (id)_windowResizeNorthSouthCursor;
+ (id)_windowResizeEastWestCursor;
@end

plKey pl__osx_key_to_pl_key(int iKey);
void  pl__add_osx_tracking_area(NSView* _Nonnull view);
bool  pl__handle_osx_event(NSEvent* event, NSView* view);

// clip board
const char* pl_get_clipboard_text(void* user_data_ctx);
void        pl_set_clipboard_text(void* pUnused, const char* text);

#endif
 
//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// general
plSharedLibrary* gptAppLibrary = NULL;
void*            gpUserData    = NULL;
plIO*            gptIOCtx      = NULL;
#ifndef PL_HEADLESS_APP
plWindow** gsbtWindows = NULL;
#endif

// MacOS stuff
CFTimeInterval gtTime;
id gtAppDelegate;

#ifndef PL_HEADLESS_APP
plKeyEventResponder* gKeyEventResponder = NULL;
NSTextInputContext*  gInputContext = NULL;
id                   gMonitor;
NSCursor* aptMouseCursors[PL_MOUSE_CURSOR_COUNT];
#endif

// apis
const plDataRegistryI*      gptDataRegistry      = NULL;
const plApiRegistryI*       gptApiRegistry       = NULL;
const plExtensionRegistryI* gptExtensionRegistry = NULL;
const plIOI*                gptIOI               = NULL;
const plMemoryI*            gptMemory            = NULL;
const plLibraryI*           gptLibraryApi        = NULL;

// app function pointers
void* (*pl_app_load)    (const plApiRegistryI*, void*);
void  (*pl_app_shutdown)(void*);
void  (*pl_app_resize)  (void*);
void  (*pl_app_update)  (void*);
bool  (*pl_app_info)    (const plApiRegistryI*);

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
        else if(strcmp(argv[i], "--version") == 0)
        {
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION);
            return 0;
        }
        else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("\nPilot Light - light weight game engine\n");
            printf("Version: %s\n\n", PILOT_LIGHT_VERSION);
            printf("Usage: pilot_light [options]\n\n");
            printf("Options:\n");
            printf("-h              %s\n", "Displays this information.");
            printf("--help          %s\n", "Displays this information.");
            printf("-version        %s\n", "Displays Pilot Light version information.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'app'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'app'.");
            return 0;
        }
    }

#if __has_feature(objc_arc)
    // ARC is On
    NSLog(@"ARC on");

#else
    // ARC is Off
    NSLog(@"ARC off");

#endif

    // load core apis
    gptApiRegistry = pl_load_core_apis();

    // os provided apis
    #ifndef PL_HEADLESS_APP
    static const plWindowI tWindowApi = {
        .create_window  = pl_create_window,
        .destroy_window = pl_destroy_window
    };
    #endif

    static const plLibraryI tLibraryApi = {
        .has_changed   = pl_has_library_changed,
        .load          = pl_load_library,
        .load_function = pl_load_library_function,
        .reload        = pl_reload_library
    };

    static const plFileI tFileApi = {
        .copy         = pl_copy_file,
        .exists       = pl_file_exists,
        .delete       = pl_file_delete,
        .binary_read  = pl_binary_read_file,
        .binary_write = pl_binary_write_file
    };
    
    static const plUdpI tUdpApi = {
        .create_socket = pl_create_udp_socket,
        .bind_socket   = pl_bind_udp_socket,  
        .get_data      = pl_get_udp_data,
        .send_data     = pl_send_udp_data
    };

    static const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl_get_hardware_thread_count,
        .create_thread               = pl_create_thread,
        .destroy_thread              = pl_destroy_thread,
        .join_thread                 = pl_join_thread,
        .yield_thread                = pl_yield_thread,
        .sleep_thread                = pl_sleep,
        .create_mutex                = pl_create_mutex,
        .destroy_mutex               = pl_destroy_mutex,
        .lock_mutex                  = pl_lock_mutex,
        .unlock_mutex                = pl_unlock_mutex,
        .create_semaphore            = pl_create_semaphore,
        .destroy_semaphore           = pl_destroy_semaphore,
        .wait_on_semaphore           = pl_wait_on_semaphore,
        .try_wait_on_semaphore       = pl_try_wait_on_semaphore,
        .release_semaphore           = pl_release_semaphore,
        .allocate_thread_local_key   = pl_allocate_thread_local_key,
        .allocate_thread_local_data  = pl_allocate_thread_local_data,
        .free_thread_local_key       = pl_free_thread_local_key, 
        .get_thread_local_data       = pl_get_thread_local_data, 
        .free_thread_local_data      = pl_free_thread_local_data, 
        .create_critical_section     = pl_create_critical_section,
        .destroy_critical_section    = pl_destroy_critical_section,
        .enter_critical_section      = pl_enter_critical_section,
        .leave_critical_section      = pl_leave_critical_section,
        .create_condition_variable   = pl_create_condition_variable,
        .destroy_condition_variable  = pl_destroy_condition_variable,
        .wake_condition_variable     = pl_wake_condition_variable,
        .wake_all_condition_variable = pl_wake_all_condition_variable,
        .sleep_condition_variable    = pl_sleep_condition_variable,
        .create_barrier              = pl_create_barrier,
        .destroy_barrier             = pl_destroy_barrier,
        .wait_on_barrier             = pl_wait_on_barrier
    };

    static const plAtomicsI tAtomicsApi = {
        .create_atomic_counter   = pl_create_atomic_counter,
        .destroy_atomic_counter  = pl_destroy_atomic_counter,
        .atomic_store            = pl_atomic_store,
        .atomic_load             = pl_atomic_load,
        .atomic_compare_exchange = pl_atomic_compare_exchange,
        .atomic_increment        = pl_atomic_increment,
        .atomic_decrement        = pl_atomic_decrement
    };

    static const plVirtualMemoryI tVirtualMemoryApi = {
        .get_page_size = pl_get_page_size,
        .alloc         = pl_virtual_alloc,
        .reserve       = pl_virtual_reserve,
        .commit        = pl_virtual_commit,
        .uncommit      = pl_virtual_uncommit,
        .free          = pl_virtual_free,
    };

    // add os specific apis
    #ifndef PL_HEADLESS_APP
    gptApiRegistry->add(PL_API_WINDOW, &tWindowApi);
    #endif
    gptApiRegistry->add(PL_API_LIBRARY, &tLibraryApi);
    gptApiRegistry->add(PL_API_FILE, &tFileApi);
    gptApiRegistry->add(PL_API_UDP, &tUdpApi);
    gptApiRegistry->add(PL_API_THREADS, &tThreadApi);
    gptApiRegistry->add(PL_API_ATOMICS, &tAtomicsApi);
    gptApiRegistry->add(PL_API_VIRTUAL_MEMORY, &tVirtualMemoryApi);

    gptDataRegistry      = gptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptExtensionRegistry = gptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptLibraryApi        = gptApiRegistry->first(PL_API_LIBRARY);
    gptIOI               = gptApiRegistry->first(PL_API_IO);
    gptMemory            = gptApiRegistry->first(PL_API_MEMORY);

    // setup & retrieve io context 
    gptIOCtx = gptIOI->get_io();

    // command line args
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    #ifndef PL_HEADLESS_APP
    // create view controller
    gKeyEventResponder = [[plKeyEventResponder alloc] initWithFrame:NSZeroRect];

    // set clipboard functions (may need to move this to OS api)
    gptIOCtx->set_clipboard_text_fn = pl_set_clipboard_text;
    gptIOCtx->get_clipboard_text_fn = pl_get_clipboard_text;
    #endif

    #ifndef PL_HEADLESS_APP

    gInputContext = [[NSTextInputContext alloc] initWithClient:gKeyEventResponder];

    // create app delegate
    gtAppDelegate= [[plNSAppDelegate alloc] init];
    NSApplication.sharedApplication.delegate = gtAppDelegate;

    // Load cursors. Some of them are undocumented.
    aptMouseCursors[PL_MOUSE_CURSOR_ARROW] = [NSCursor arrowCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_TEXT_INPUT] = [NSCursor IBeamCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_ALL] = [NSCursor closedHandCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_HAND] = [NSCursor pointingHandCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_NOT_ALLOWED] = [NSCursor operationNotAllowedCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NS] = [NSCursor respondsToSelector:@selector(_windowResizeNorthSouthCursor)] ? [NSCursor _windowResizeNorthSouthCursor] : [NSCursor resizeUpDownCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_EW] = [NSCursor respondsToSelector:@selector(_windowResizeEastWestCursor)] ? [NSCursor _windowResizeEastWestCursor] : [NSCursor resizeLeftRightCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NESW] = [NSCursor respondsToSelector:@selector(_windowResizeNorthEastSouthWestCursor)] ? [NSCursor _windowResizeNorthEastSouthWestCursor] : [NSCursor closedHandCursor];
    aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NWSE] = [NSCursor respondsToSelector:@selector(_windowResizeNorthWestSouthEastCursor)] ? [NSCursor _windowResizeNorthWestSouthEastCursor] : [NSCursor closedHandCursor];

    #endif

    // load library
    static char acLibraryName[256] = {0};
    static char acTransitionalName[256] = {0};
    pl_sprintf(acLibraryName, "%s.dylib", pcAppName);
    pl_sprintf(acTransitionalName, "%s_", pcAppName);
    if(gptLibraryApi->load(acLibraryName, acTransitionalName, "lock.tmp", &gptAppLibrary))
    {
        pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(()) *)(void*))                     gptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(()) *)(void*))                     gptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(()) *)(void*))                     gptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))     gptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
        
        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        
        gpUserData = pl_app_load(gptApiRegistry, NULL);
    }

    #ifdef PL_HEADLESS_APP

    // main loop
    while (gptIOCtx->bRunning)
    {
        pl_sleep((uint32_t)(1000.0f / gptIOCtx->fHeadlessUpdateRate));

        // reload library
        if(gptLibraryApi->has_changed(gptAppLibrary))
        {
            gptLibraryApi->reload(gptAppLibrary);
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            gptExtensionRegistry->reload();
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }

        if(gtTime == 0.0)
            gtTime = pl__get_absolute_time();

        double dCurrentTime = pl__get_absolute_time();
        gptIOCtx->fDeltaTime = (float)(dCurrentTime - gtTime);
        gtTime = dCurrentTime;

        gptDataRegistry->garbage_collect();
        pl_app_update(gpUserData);
        gptExtensionRegistry->reload();
    }
    #else
    // run app
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] plNSView
//-----------------------------------------------------------------------------

#ifndef PL_HEADLESS_APP
@implementation plNSView
{
    CVDisplayLinkRef _displayLink;
    dispatch_source_t _displaySource;
}

- (instancetype) initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if(self)
    {
        [self initCommon];
    }
    [self addSubview:gKeyEventResponder];

    pl__add_osx_tracking_area(self);
    return self;
}

- (void)dealloc
{
    [self stopRenderLoop];
    [gtAppDelegate release];
    
    [_delegate shutdown];
    [super dealloc];
}

- (void)resizeDrawable:(CGFloat)scaleFactor
{
    CGSize newSize = self.bounds.size;

    gptIOCtx->afMainFramebufferScale[0] = scaleFactor;
    gptIOCtx->afMainFramebufferScale[1] = scaleFactor;

    if(newSize.width <= 0 || newSize.width <= 0)
    {
        return;
    }

    if(newSize.width == _metalLayer.drawableSize.width && newSize.height == _metalLayer.drawableSize.height)
    {
        return;
    }

    _metalLayer.drawableSize = newSize;

    [(id)_delegate drawableResize:newSize];
    
}

- (void)render
{
    [(id)_delegate renderToMetalLayer:_metalLayer];
}

- (void)initCommon
{
    self.wantsLayer = YES;
    // self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawBeforeViewResize;
    _metalLayer = (CAMetalLayer*) self.layer;
    self.layer.delegate = self;
}

- (CALayer *)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self setupCVDisplayLinkForScreen:self.window.screen];
    gptIOCtx->afMainFramebufferScale[0] = self.window.screen.backingScaleFactor;
    gptIOCtx->afMainFramebufferScale[1] = self.window.screen.backingScaleFactor;
}

- (BOOL)setupCVDisplayLinkForScreen:(NSScreen*)screen
{
    // The CVDisplayLink callback, DispatchRenderLoop, never executes
    // on the main thread. To execute rendering on the main thread, create
    // a dispatch source using the main queue (the main thread).
    // DispatchRenderLoop merges this dispatch source in each call
    // to execute rendering on the main thread.
    _displaySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, dispatch_get_main_queue());
    plNSView* weakSelf = self;
    dispatch_source_set_event_handler(_displaySource, ^(){
        @autoreleasepool
        {
            [weakSelf render];
        }
    });
    dispatch_resume(_displaySource);

    CVReturn cvReturn;

    // Create a display link capable of being used with all active displays
    cvReturn = CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);

    if(cvReturn != kCVReturnSuccess)
    {
        return NO;
    }

    // Set DispatchRenderLoop as the callback function and
    // supply _displaySource as the argument to the callback.
    cvReturn = CVDisplayLinkSetOutputCallback(_displayLink, &DispatchRenderLoop, (__bridge void*)_displaySource);

    if(cvReturn != kCVReturnSuccess)
    {
        return NO;
    }

    // Associate the display link with the display on which the
    // view resides
    CGDirectDisplayID viewDisplayID =
        (CGDirectDisplayID) [self.window.screen.deviceDescription[@"NSScreenNumber"] unsignedIntegerValue];

    cvReturn = CVDisplayLinkSetCurrentCGDisplay(_displayLink, viewDisplayID);

    if(cvReturn != kCVReturnSuccess)
    {
        return NO;
    }

    CVDisplayLinkStart(_displayLink);

    NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];

    // Register to be notified when the window closes so that you
    // can stop the display link
    [notificationCenter addObserver:self
                           selector:@selector(windowWillClose:)
                               name:NSWindowWillCloseNotification
                             object:self.window];

    return YES;
}

- (void)windowWillClose:(NSNotification*)notification
{
    // Stop the display link when the window is closing since there
    // is no point in drawing something that can't be seen
    if(notification.object == self.window)
    {
        CVDisplayLinkStop(_displayLink);
        dispatch_source_cancel(_displaySource);
    }

    pl_app_shutdown(gpUserData);
    gptExtensionRegistry->unload_all();

    pl_sb_free(gsbtWindows);

    gptMemory->check_for_leaks();
}

// This is the renderer output callback function
CVReturn
DispatchRenderLoop(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{

    // 'DispatchRenderLoop' is always called on a secondary thread.  Merge the dispatch source
    // setup for the main queue so that rendering occurs on the main thread
    dispatch_source_t source = (__bridge dispatch_source_t)displayLinkContext;
    dispatch_source_merge_data(source, 1);
    return kCVReturnSuccess;
}

- (void)stopRenderLoop
{
    if(_displayLink)
    {
        // Stop the display link BEFORE releasing anything in the view otherwise the display link
        // thread may call into the view and crash when it encounters something that no longer
        // exists
        CVDisplayLinkStop(_displayLink);
        CVDisplayLinkRelease(_displayLink);
        dispatch_source_cancel(_displaySource);
    }
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    gptIOCtx->afMainFramebufferScale[0] = self.window.screen.backingScaleFactor;
    gptIOCtx->afMainFramebufferScale[1] = self.window.screen.backingScaleFactor;
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setBoundsSize:(NSSize)size
{
    [super setBoundsSize:size];
    printf("set bounds size\n");
}

@end

//-----------------------------------------------------------------------------
// [SECTION] plNSViewController
//-----------------------------------------------------------------------------

@implementation plNSViewController

- (void)loadView
{

}

- (void)drawableResize:(CGSize)size
{
    gptIOCtx->afMainViewportSize[0] = size.width;
    gptIOCtx->afMainViewportSize[1] = size.height;
    if(gpUserData)
    {
        pl_app_resize(gpUserData);
    }
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)layer
{
    @autoreleasepool
    {
        // gAppData.graphics.metalLayer = layer;
        gptIOCtx->pBackendPlatformData = layer;

        gptIOCtx->afMainFramebufferScale[0] = self.view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
        gptIOCtx->afMainFramebufferScale[1] = gptIOCtx->afMainFramebufferScale[0];

        // not osx
        // CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;

        // updating mouse cursor
        if(gptIOCtx->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && gptIOCtx->tNextCursor == PL_MOUSE_CURSOR_ARROW)
            gptIOCtx->bCursorChanged = true;

        if(gptIOCtx->bCursorChanged && gptIOCtx->tNextCursor != gptIOCtx->tCurrentCursor)
        {
            gptIOCtx->tCurrentCursor = gptIOCtx->tNextCursor;
            NSCursor* ptMacCursor = aptMouseCursors[gptIOCtx->tCurrentCursor] ?: aptMouseCursors[PL_MOUSE_CURSOR_ARROW];
            [ptMacCursor set];
        }
        gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
        gptIOCtx->bCursorChanged = false;

        // reload library
        if(gptLibraryApi->has_changed(gptAppLibrary))
        {
            gptLibraryApi->reload(gptAppLibrary);
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*)) gptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            gptExtensionRegistry->reload();
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }


        if(self.view)
        {
            const float fDpi = (float)[self.view.window backingScaleFactor];
            gptIOCtx->afMainViewportSize[0] = (float)self.view.bounds.size.width;
            gptIOCtx->afMainViewportSize[1] = (float)self.view.bounds.size.height;
            gptIOCtx->afMainFramebufferScale[0] = fDpi;
            gptIOCtx->afMainFramebufferScale[1] = fDpi;
        }

        if(gtTime == 0.0)
            gtTime = pl__get_absolute_time();

        double dCurrentTime = pl__get_absolute_time();
        gptIOCtx->fDeltaTime = (float)(dCurrentTime - gtTime);
        gtTime = dCurrentTime;

        gptDataRegistry->garbage_collect();
        pl_app_update(gpUserData);
        gptExtensionRegistry->reload();

        if(gptIOCtx->bRunning == false)
        {
            [NSApp terminate:nil];
        }
    }
}

- (void)shutdown
{
    pl_app_shutdown(gpUserData);
    if(gMonitor != NULL)
    {
        [NSEvent removeMonitor:gMonitor];
        gMonitor = NULL;
    }

    gptMemory->check_for_leaks();
}

@end

@implementation plKeyEventResponder
{
    float _posX;
    float _posY;
    NSRect _imeRect;
}

#pragma mark - Public

- (void)setImePosX:(float)posX imePosY:(float)posY
{
    _posX = posX;
    _posY = posY;
}

- (void)updateImePosWithView:(NSView *)view
{
    NSWindow *window = view.window;
    if (!window)
        return;
    NSRect contentRect = [window contentRectForFrameRect:window.frame];
    NSRect rect = NSMakeRect(_posX, contentRect.size.height - _posY, 0, 0);
    _imeRect = [window convertRectToScreen:rect];
}

- (void)viewDidMoveToWindow
{
    // Eensure self is a first responder to receive the input events.
    [self.window makeFirstResponder:self];
}

- (void)keyDown:(NSEvent*)event
{
    if (!pl__handle_osx_event(event, self))
        [super keyDown:event];

    // call to the macOS input manager system.
    [self interpretKeyEvents:@[event]];
}

- (void)keyUp:(NSEvent*)event
{
    if (!pl__handle_osx_event(event, self))
        [super keyUp:event];
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
    NSString* characters;
    if ([aString isKindOfClass:[NSAttributedString class]])
        characters = [aString string];
    else
        characters = (NSString*)aString;

    gptIOI->add_text_events_utf8(characters.UTF8String);
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)doCommandBySelector:(SEL)myselector
{
}

- (nullable NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return _imeRect;
}

- (BOOL)hasMarkedText
{
    return NO;
}

- (NSRange)markedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(nonnull id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
}

- (void)unmarkText
{
}

- (nonnull NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
    return @[];
}

@end

bool
pl__handle_osx_event(NSEvent* event, NSView* view)
{
    if (event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown)
    {
        int button = (int)[event buttonNumber];
        if (button >= 0 && button < PL_MOUSE_BUTTON_COUNT)
            gptIOI->add_mouse_button_event(button, true);
        return true;
    }

    if (event.type == NSEventTypeLeftMouseUp || event.type == NSEventTypeRightMouseUp || event.type == NSEventTypeOtherMouseUp)
    {
        int button = (int)[event buttonNumber];
        if (button >= 0 && button < PL_MOUSE_BUTTON_COUNT)
            gptIOI->add_mouse_button_event(button, false);
        return true;
    }

    if (event.type == NSEventTypeMouseMoved || event.type == NSEventTypeLeftMouseDragged || event.type == NSEventTypeRightMouseDragged || event.type == NSEventTypeOtherMouseDragged)
    {
        NSPoint mousePoint = event.locationInWindow;
        mousePoint = [view convertPoint:mousePoint fromView:nil];
        if ([view isFlipped])
            mousePoint = NSMakePoint(mousePoint.x, mousePoint.y);
        else
            mousePoint = NSMakePoint(mousePoint.x, view.bounds.size.height - mousePoint.y);
        gptIOI->add_mouse_pos_event((float)mousePoint.x, (float)mousePoint.y);
        return true;
    }

    if (event.type == NSEventTypeScrollWheel)
    {
        // Ignore canceled events.
        //
        // From macOS 12.1, scrolling with two fingers and then decelerating
        // by tapping two fingers results in two events appearing:
        //
        // 1. A scroll wheel NSEvent, with a phase == NSEventPhaseMayBegin, when the user taps
        // two fingers to decelerate or stop the scroll events.
        //
        // 2. A scroll wheel NSEvent, with a phase == NSEventPhaseCancelled, when the user releases the
        // two-finger tap. It is this event that sometimes contains large values for scrollingDeltaX and
        // scrollingDeltaY. When these are added to the current x and y positions of the scrolling view,
        // it appears to jump up or down. It can be observed in Preview, various JetBrains IDEs and here.
        if (event.phase == NSEventPhaseCancelled)
            return false;

        double wheel_dx = 0.0;
        double wheel_dy = 0.0;

        #if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
        if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6)
        {
            wheel_dx = [event scrollingDeltaX];
            wheel_dy = [event scrollingDeltaY];
            if ([event hasPreciseScrollingDeltas])
            {
                wheel_dx *= 0.1;
                wheel_dy *= 0.1;
            }
        }
        else
        #endif // MAC_OS_X_VERSION_MAX_ALLOWED
        {
            wheel_dx = [event deltaX];
            wheel_dy = [event deltaY];
        }
        if (wheel_dx != 0.0 || wheel_dy != 0.0)
            gptIOI->add_mouse_wheel_event((float)wheel_dx * 0.1f, (float)wheel_dy * 0.1f);

        return true;
    }

    if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)
    {
        if ([event isARepeat])
            return true;

        int key_code = (int)[event keyCode];
        gptIOI->add_key_event(pl__osx_key_to_pl_key(key_code), event.type == NSEventTypeKeyDown);
        return true;
    }

    if (event.type == NSEventTypeFlagsChanged)
    {
        unsigned short key_code = [event keyCode];
        NSEventModifierFlags modifier_flags = [event modifierFlags];

        gptIOI->add_key_event(PL_KEY_MOD_SHIFT, (modifier_flags & NSEventModifierFlagShift)   != 0);
        gptIOI->add_key_event(PL_KEY_MOD_CTRL,  (modifier_flags & NSEventModifierFlagControl) != 0);
        gptIOI->add_key_event(PL_KEY_MOD_ALT,   (modifier_flags & NSEventModifierFlagOption)  != 0);
        gptIOI->add_key_event(PL_KEY_MOD_SUPER, (modifier_flags & NSEventModifierFlagCommand) != 0);

        plKey key = pl__osx_key_to_pl_key(key_code);
        if (key != PL_KEY_NONE)
        {
            // macOS does not generate down/up event for modifiers. We're trying
            // to use hardware dependent masks to extract that information.
            NSEventModifierFlags mask = 0;
            switch (key)
            {
                case PL_KEY_LEFT_CTRL:   mask = 0x0001; break;
                case PL_KEY_RIGHT_CTRL:  mask = 0x2000; break;
                case PL_KEY_LEFT_SHIFT:  mask = 0x0002; break;
                case PL_KEY_RIGHT_SHIFT: mask = 0x0004; break;
                case PL_KEY_LEFT_SUPER:  mask = 0x0008; break;
                case PL_KEY_RIGHT_SUPER: mask = 0x0010; break;
                case PL_KEY_LEFT_ALT:    mask = 0x0020; break;
                case PL_KEY_RIGHT_ALT:   mask = 0x0040; break;
                default: return true;
            }

            NSEventModifierFlags modifier_flags = [event modifierFlags];
            gptIOI->add_key_event(key, (modifier_flags & mask) != 0);
            // io.SetKeyEventNativeData(key, key_code, -1); // To support legacy indexing (<1.87 user code)
        }

        return true;
    }

    return false;
}

void 
pl__add_osx_tracking_area(NSView* _Nonnull view)
{
    if(gMonitor) return;
    NSEventMask eventMask = 0;
    eventMask |= NSEventMaskMouseMoved | NSEventMaskScrollWheel;
    eventMask |= NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
    eventMask |= NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskRightMouseDragged;
    eventMask |= NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp | NSEventMaskOtherMouseDragged;
    eventMask |= NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged;
    gMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:eventMask
                                                        handler:^NSEvent* _Nullable(NSEvent* event)
    {
        pl__handle_osx_event(event, view);
        return event;
    }]; 
}

#endif

//-----------------------------------------------------------------------------
// [SECTION] file api
//-----------------------------------------------------------------------------

void
pl_binary_read_file(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{
    PL_ASSERT(pszSizeIn);

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
    fseek(ptDataFile, 0, SEEK_SET);

    if(pcBuffer == NULL)
    {
        *pszSizeIn = uSize;
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
pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer)
{
    FILE* ptDataFile = fopen(pcFile, "wb");
    if (ptDataFile)
    {
        fwrite(pcBuffer, 1, szSize, ptDataFile);
        fclose(ptDataFile);
    }
}

void
pl_copy_file(const char* source, const char* destination)
{
    copyfile_state_t s;
    s = copyfile_state_alloc();
    copyfile(source, destination, s, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(s);
}

void
pl_file_delete(const char* pcFile)
{
    remove(pcFile);
}

bool
pl_file_exists(const char* pcFile)
{
    FILE* ptDataFile = fopen(pcFile, "r");
    
    if(ptDataFile)
    {
        fclose(ptDataFile);
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] udp api
//-----------------------------------------------------------------------------

void
pl_create_udp_socket(plSocket** pptSocketOut, bool bNonBlocking)
{
    *pptSocketOut = PL_ALLOC(sizeof(plSocket));

    // create socket
    if(((*pptSocketOut)->iSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        printf("Could not create socket\n");
        PL_ASSERT(false && "Could not create socket");
    }

    // enable non-blocking
    if(bNonBlocking)
    {
        int iFlags = fcntl((*pptSocketOut)->iSocket, F_GETFL);
        fcntl((*pptSocketOut)->iSocket, F_SETFL, iFlags | O_NONBLOCK);
    }
}

void
pl_bind_udp_socket(plSocket* ptSocket, int iPort)
{
    ptSocket->iPort = iPort;
    
    // prepare sockaddr_in struct
    struct sockaddr_in tServer = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)iPort),
        .sin_addr.s_addr = INADDR_ANY
    };

    // bind socket
    if(bind(ptSocket->iSocket, (struct sockaddr* )&tServer, sizeof(tServer)) < 0)
    {
        printf("Bind socket failed with error code : %d\n", errno);
        PL_ASSERT(false && "Socket error");
    }
}

bool
pl_send_udp_data(plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize)
{
    struct sockaddr_in tDestSocket = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)iDestPort),
        .sin_addr.s_addr = inet_addr(pcDestIP)
    };
    static const size_t szLen = sizeof(tDestSocket);

    // send
    if(sendto(ptFromSocket->iSocket, (const char*)pData, (int)szSize, 0, (struct sockaddr*)&tDestSocket, (int)szLen) < 0)
    {
        printf("sendto() failed with error code : %d\n", errno);
        PL_ASSERT(false && "Socket error");
        return false;
    }

    return true;
}

bool
pl_get_udp_data(plSocket* ptSocket, void* pData, size_t szSize)
{
    struct sockaddr_in tSiOther = {0};
    static socklen_t iSLen = (int)sizeof(tSiOther);
    memset(pData, 0, szSize);
    int iRecvLen = recvfrom(ptSocket->iSocket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tSiOther, &iSLen);

    if(iRecvLen < 0)
    {
        if(errno != EWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", errno);
            PL_ASSERT(false && "Socket error");
            return false;
        }
    }
    return iRecvLen > 0;
}

//-----------------------------------------------------------------------------
// [SECTION] library api
//-----------------------------------------------------------------------------

bool
pl_has_library_changed(plSharedLibrary* library)
{
    struct timespec newWriteTime = pl__get_last_write_time(library->acPath);
    return newWriteTime.tv_sec != library->lastWriteTime.tv_sec;
}

bool
pl_load_library(const char* name, const char* transitionalName, const char* lockFile, plSharedLibrary** pptLibraryOut)
{

    if(*pptLibraryOut == NULL)
    {
        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));
        (*pptLibraryOut)->bValid = false;
    }
    plSharedLibrary* library = *pptLibraryOut;
    
    if(library->acPath[0] == 0)             strncpy(library->acPath, name, PL_MAX_NAME_LENGTH);
    if(library->acTransitionalName[0] == 0) strncpy(library->acTransitionalName, transitionalName, PL_MAX_NAME_LENGTH);
    if(library->acLockFile[0] == 0)         strncpy(library->acLockFile, lockFile, PL_MAX_NAME_LENGTH);
    library->bValid = false;

    struct stat attr2;
    if(stat(library->acLockFile, &attr2) == -1)  // lock file gone
    {
        char temporaryName[2024] = {0};
        library->lastWriteTime = pl__get_last_write_time(library->acPath);
        
        pl_sprintf(temporaryName, "%s%u%s", library->acTransitionalName, library->uTempIndex, ".dylib");
        if(++library->uTempIndex >= 1024)
        {
            library->uTempIndex = 0;
        }
        pl_copy_file(library->acPath, temporaryName);

        library->handle = NULL;
        library->handle = dlopen(temporaryName, RTLD_NOW);
        if(library->handle)
            library->bValid = true;
        else
        {
            printf("\n\n%s\n\n", dlerror());
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
        if(pl_load_library(library->acPath, library->acTransitionalName, library->acLockFile, &library))
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
        loadedFunction = dlsym(library->handle, name);
    }
    return loadedFunction;
}

//-----------------------------------------------------------------------------
// [SECTION] thread api
//-----------------------------------------------------------------------------

void
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
}

void
pl_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** pptThreadOut)
{
    *pptThreadOut = PL_ALLOC(sizeof(plThread));
    if(pthread_create(&(*pptThreadOut)->tHandle, NULL, ptProcedure, pData))
    {
        PL_ASSERT(false);
    }
}

void
pl_destroy_thread(plThread** ppThread)
{
    pl_join_thread(*ppThread);
    PL_FREE(*ppThread);
    *ppThread = NULL;
}

void
pl_join_thread(plThread* ptThread)
{
    pthread_join(ptThread->tHandle, NULL);
}

void
pl_yield_thread(void)
{
    sched_yield();
}

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

void
pl_create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    *pptCriticalSectionOut = PL_ALLOC(sizeof(plCriticalSection));
    if(pthread_mutex_init(&(*pptCriticalSectionOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
    }
}

void
pl_destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    pthread_mutex_destroy(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    *pptCriticalSection = NULL;
}

void
pl_enter_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_lock(&ptCriticalSection->tHandle);
}

void
pl_leave_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_unlock(&ptCriticalSection->tHandle);
}

uint32_t
pl_get_hardware_thread_count(void)
{

    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)numCPU;
}

void
pl_create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    *pptSemaphoreOut = PL_ALLOC(sizeof(plSemaphore));
    memset((*pptSemaphoreOut), 0, sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = dispatch_semaphore_create(uIntialCount);
}

void
pl_destroy_semaphore(plSemaphore** pptSemaphore)
{
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_FOREVER);
}

bool
pl_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    return dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_NOW) == 0;
}

void
pl_release_semaphore(plSemaphore* ptSemaphore)
{
    dispatch_semaphore_signal(ptSemaphore->tHandle);
}

void
pl_allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    *pptKeyOut = PL_ALLOC(sizeof(plThreadKey));
    int iStatus = pthread_key_create(&(*pptKeyOut)->tKey, NULL);
    if(iStatus != 0)
    {
        printf("pthread_key_create failed, errno=%d", errno);
        PL_ASSERT(false);
    }
}

void
pl_free_thread_local_key(plThreadKey** pptKey)
{
    pthread_key_delete((*pptKey)->tKey);
    PL_FREE((*pptKey));
    *pptKey = NULL;
}

void*
pl_allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    void* pData = PL_ALLOC(szSize);
    memset(pData, 0, szSize);
    pthread_setspecific(ptKey->tKey, pData);
    return pData;
}

void*
pl_get_thread_local_data(plThreadKey* ptKey)
{
    void* pData = pthread_getspecific(ptKey->tKey);
    return pData;
}

void
pl_free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    PL_FREE(pData);
}

void
pl_create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    *pptBarrierOut = PL_ALLOC(sizeof(plBarrier));
    pthread_barrier_init(&(*pptBarrierOut)->tHandle, NULL, uThreadCount);
}

void
pl_destroy_barrier(plBarrier** pptBarrier)
{
    pthread_barrier_destroy(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
}

void
pl_wait_on_barrier(plBarrier* ptBarrier)
{
    pthread_barrier_wait(&ptBarrier->tHandle);
}

void
pl_create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    *pptConditionVariableOut = PL_ALLOC(sizeof(plConditionVariable));
    pthread_cond_init(&(*pptConditionVariableOut)->tHandle, NULL);
}

void               
pl_destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    pthread_cond_destroy(&(*pptConditionVariable)->tHandle);
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
}

void               
pl_wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_signal(&ptConditionVariable->tHandle);
}

void               
pl_wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_broadcast(&ptConditionVariable->tHandle);
}

void               
pl_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    pthread_cond_wait(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle);
}

int
pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count)
{
    if(count == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if(pthread_mutex_init(&barrier->mutex, 0) < 0)
    {
        return -1;
    }
    if(pthread_cond_init(&barrier->cond, 0) < 0)
    {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    barrier->tripCount = count;
    barrier->count = 0;

    return 0;
}

int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    ++(barrier->count);
    if(barrier->count >= barrier->tripCount)
    {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    }
    else
    {
        pthread_cond_wait(&barrier->cond, &(barrier->mutex));
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] atomic api
//-----------------------------------------------------------------------------

void
pl_create_atomic_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = malloc(sizeof(plAtomicCounter));
    atomic_init(&(*ptCounter)->ilValue, ilValue);
}

void
pl_destroy_atomic_counter(plAtomicCounter** ptCounter)
{
    free((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl_atomic_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    atomic_store(&ptCounter->ilValue, ilValue);
}

int64_t
pl_atomic_load(plAtomicCounter* ptCounter)
{
    return atomic_load(&ptCounter->ilValue);
}

bool
pl_atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return atomic_compare_exchange_strong(&ptCounter->ilValue, &ilExpectedValue, ilDesiredValue);
}

void
pl_atomic_increment(plAtomicCounter* ptCounter)
{
    atomic_fetch_add(&ptCounter->ilValue, 1);
}

void
pl_atomic_decrement(plAtomicCounter* ptCounter)
{
    atomic_fetch_sub(&ptCounter->ilValue, 1);
}

//-----------------------------------------------------------------------------
// [SECTION] virtual memory api
//-----------------------------------------------------------------------------

size_t
pl_get_page_size(void)
{
    return (size_t)getpagesize();
}

void*
pl_virtual_alloc(void* pAddress, size_t szSize)
{
    void* pResult = mmap(pAddress, szSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pResult;
}

void*
pl_virtual_reserve(void* pAddress, size_t szSize)
{
    void* pResult = mmap(pAddress, szSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pResult;
}

void*
pl_virtual_commit(void* pAddress, size_t szSize)
{
    mprotect(pAddress, szSize, PROT_READ | PROT_WRITE);
    return pAddress;
}

void
pl_virtual_free(void* pAddress, size_t szSize)
{
    munmap(pAddress, szSize);
}

void
pl_virtual_uncommit(void* pAddress, size_t szSize)
{
    mprotect(pAddress, szSize, PROT_NONE);
}

//-----------------------------------------------------------------------------
// [SECTION] window api
//-----------------------------------------------------------------------------

#ifndef PL_HEADLESS_APP

plWindow*
pl_create_window(const plWindowDesc* ptDesc)
{
    plWindow* ptWindow = malloc(sizeof(plWindow));
    plWindowData* ptData = malloc(sizeof(plWindowData));
    ptWindow->tDesc = *ptDesc;
    ptWindow->_pPlatformData = ptData;

    // create view
    ptData->ptViewController = [[plNSViewController alloc] init];

   id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    NSRect frame = NSMakeRect(0, 0, ptDesc->uWidth, ptDesc->uHeight);
    ptData->ptViewController.view = [[plNSView alloc] initWithFrame:frame];

    plNSView *view = (plNSView *)ptData->ptViewController.view;
    view.metalLayer.device = device;
    view.delegate = ptData->ptViewController;
    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    gptIOCtx->pBackendPlatformData = device;
    ptData->ptLayer = view.metalLayer;

    // create window
    ptData->ptWindow = [NSWindow windowWithContentViewController:ptData->ptViewController];
    [ptData->ptWindow orderFront:nil];
    [ptData->ptWindow center];
    [ptData->ptWindow becomeKeyWindow];

    NSString* tWindowTitle = [NSString stringWithUTF8String:ptDesc->pcName];
    [ptData->ptWindow setTitle:tWindowTitle];


    NSPoint tOrigin = NSMakePoint(ptDesc->iXPos, ptData->ptWindow.screen.frame.size.height - ptDesc->iYPos);
    [ptData->ptWindow setFrameTopLeftPoint:tOrigin];

    
    ptData->ptWindow.delegate = gtAppDelegate;

    gptIOCtx->afMainViewportSize[0] = ptDesc->uWidth;
    gptIOCtx->afMainViewportSize[1] = ptDesc->uHeight;

    pl_sb_push(gsbtWindows, ptWindow);

    return ptWindow;
}

void
pl_destroy_window(plWindow* ptWindow)
{
    plWindowData* ptData = ptWindow->_pPlatformData;
    [ptData->ptWindow release];
    free(ptData);
    free(ptWindow);
}

plKey
pl__osx_key_to_pl_key(int iKey)
{
    switch (iKey)
    {
        case kVK_ANSI_A:              return PL_KEY_A;
        case kVK_ANSI_S:              return PL_KEY_S;
        case kVK_ANSI_D:              return PL_KEY_D;
        case kVK_ANSI_F:              return PL_KEY_F;
        case kVK_ANSI_H:              return PL_KEY_H;
        case kVK_ANSI_G:              return PL_KEY_G;
        case kVK_ANSI_Z:              return PL_KEY_Z;
        case kVK_ANSI_X:              return PL_KEY_X;
        case kVK_ANSI_C:              return PL_KEY_C;
        case kVK_ANSI_V:              return PL_KEY_V;
        case kVK_ANSI_B:              return PL_KEY_B;
        case kVK_ANSI_Q:              return PL_KEY_Q;
        case kVK_ANSI_W:              return PL_KEY_W;
        case kVK_ANSI_E:              return PL_KEY_E;
        case kVK_ANSI_R:              return PL_KEY_R;
        case kVK_ANSI_Y:              return PL_KEY_Y;
        case kVK_ANSI_T:              return PL_KEY_T;
        case kVK_ANSI_1:              return PL_KEY_1;
        case kVK_ANSI_2:              return PL_KEY_2;
        case kVK_ANSI_3:              return PL_KEY_3;
        case kVK_ANSI_4:              return PL_KEY_4;
        case kVK_ANSI_6:              return PL_KEY_6;
        case kVK_ANSI_5:              return PL_KEY_5;
        case kVK_ANSI_Equal:          return PL_KEY_EQUAL;
        case kVK_ANSI_9:              return PL_KEY_9;
        case kVK_ANSI_7:              return PL_KEY_7;
        case kVK_ANSI_Minus:          return PL_KEY_MINUS;
        case kVK_ANSI_8:              return PL_KEY_8;
        case kVK_ANSI_0:              return PL_KEY_0;
        case kVK_ANSI_RightBracket:   return PL_KEY_RIGHT_BRACKET;
        case kVK_ANSI_O:              return PL_KEY_O;
        case kVK_ANSI_U:              return PL_KEY_U;
        case kVK_ANSI_LeftBracket:    return PL_KEY_LEFT_BRACKET;
        case kVK_ANSI_I:              return PL_KEY_I;
        case kVK_ANSI_P:              return PL_KEY_P;
        case kVK_ANSI_L:              return PL_KEY_L;
        case kVK_ANSI_J:              return PL_KEY_J;
        case kVK_ANSI_Quote:          return PL_KEY_APOSTROPHE;
        case kVK_ANSI_K:              return PL_KEY_K;
        case kVK_ANSI_Semicolon:      return PL_KEY_SEMICOLON;
        case kVK_ANSI_Backslash:      return PL_KEY_BACKSLASH;
        case kVK_ANSI_Comma:          return PL_KEY_COMMA;
        case kVK_ANSI_Slash:          return PL_KEY_SLASH;
        case kVK_ANSI_N:              return PL_KEY_N;
        case kVK_ANSI_M:              return PL_KEY_M;
        case kVK_ANSI_Period:         return PL_KEY_PERIOD;
        case kVK_ANSI_Grave:          return PL_KEY_GRAVE_ACCENT;
        case kVK_ANSI_KeypadDecimal:  return PL_KEY_KEYPAD_DECIMAL;
        case kVK_ANSI_KeypadMultiply: return PL_KEY_KEYPAD_MULTIPLY;
        case kVK_ANSI_KeypadPlus:     return PL_KEY_KEYPAD_ADD;
        case kVK_ANSI_KeypadClear:    return PL_KEY_NUM_LOCK;
        case kVK_ANSI_KeypadDivide:   return PL_KEY_KEYPAD_DIVIDE;
        case kVK_ANSI_KeypadEnter:    return PL_KEY_KEYPAD_ENTER;
        case kVK_ANSI_KeypadMinus:    return PL_KEY_KEYPAD_SUBTRACT;
        case kVK_ANSI_KeypadEquals:   return PL_KEY_KEYPAD_EQUAL;
        case kVK_ANSI_Keypad0:        return PL_KEY_KEYPAD_0;
        case kVK_ANSI_Keypad1:        return PL_KEY_KEYPAD_1;
        case kVK_ANSI_Keypad2:        return PL_KEY_KEYPAD_2;
        case kVK_ANSI_Keypad3:        return PL_KEY_KEYPAD_3;
        case kVK_ANSI_Keypad4:        return PL_KEY_KEYPAD_4;
        case kVK_ANSI_Keypad5:        return PL_KEY_KEYPAD_5;
        case kVK_ANSI_Keypad6:        return PL_KEY_KEYPAD_6;
        case kVK_ANSI_Keypad7:        return PL_KEY_KEYPAD_7;
        case kVK_ANSI_Keypad8:        return PL_KEY_KEYPAD_8;
        case kVK_ANSI_Keypad9:        return PL_KEY_KEYPAD_9;
        case kVK_Return:              return PL_KEY_ENTER;
        case kVK_Tab:                 return PL_KEY_TAB;
        case kVK_Space:               return PL_KEY_SPACE;
        case kVK_Delete:              return PL_KEY_BACKSPACE;
        case kVK_Escape:              return PL_KEY_ESCAPE;
        case kVK_CapsLock:            return PL_KEY_CAPS_LOCK;
        case kVK_Control:             return PL_KEY_LEFT_CTRL;
        case kVK_Shift:               return PL_KEY_LEFT_SHIFT;
        case kVK_Option:              return PL_KEY_LEFT_ALT;
        case kVK_Command:             return PL_KEY_LEFT_SUPER;
        case kVK_RightControl:        return PL_KEY_RIGHT_CTRL;
        case kVK_RightShift:          return PL_KEY_RIGHT_SHIFT;
        case kVK_RightOption:         return PL_KEY_RIGHT_ALT;
        case kVK_RightCommand:        return PL_KEY_RIGHT_SUPER;
        case kVK_F5:                  return PL_KEY_F5;
        case kVK_F6:                  return PL_KEY_F6;
        case kVK_F7:                  return PL_KEY_F7;
        case kVK_F3:                  return PL_KEY_F3;
        case kVK_F8:                  return PL_KEY_F8;
        case kVK_F9:                  return PL_KEY_F9;
        case kVK_F11:                 return PL_KEY_F11;
        case kVK_F13:                 return PL_KEY_PRINT_SCREEN;
        case kVK_F10:                 return PL_KEY_F10;
        case 0x6E:                    return PL_KEY_MENU;
        case kVK_F12:                 return PL_KEY_F12;
        case kVK_Help:                return PL_KEY_INSERT;
        case kVK_Home:                return PL_KEY_HOME;
        case kVK_PageUp:              return PL_KEY_PAGE_UP;
        case kVK_ForwardDelete:       return PL_KEY_DELETE;
        case kVK_F4:                  return PL_KEY_F4;
        case kVK_End:                 return PL_KEY_END;
        case kVK_F2:                  return PL_KEY_F2;
        case kVK_PageDown:            return PL_KEY_PAGE_DOWN;
        case kVK_F1:                  return PL_KEY_F1;
        case kVK_LeftArrow:           return PL_KEY_LEFT_ARROW;
        case kVK_RightArrow:          return PL_KEY_RIGHT_ARROW;
        case kVK_DownArrow:           return PL_KEY_DOWN_ARROW;
        case kVK_UpArrow:             return PL_KEY_UP_ARROW;
        default:                      return PL_KEY_NONE;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] clipboard
//-----------------------------------------------------------------------------

const char*
pl_get_clipboard_text(void* user_data_ctx)
{
    pl_sb_reset(gptIOCtx->sbcClipboardData);

    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSString* available = [pasteboard availableTypeFromArray: [NSArray arrayWithObject:NSPasteboardTypeString]];
    if (![available isEqualToString:NSPasteboardTypeString])
        return NULL;

    NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
    if (string == nil)
        return NULL;

    const char* string_c = (const char*)[string UTF8String];
    size_t string_len = strlen(string_c);
    pl_sb_resize(gptIOCtx->sbcClipboardData, (int)string_len + 1);
    strcpy(gptIOCtx->sbcClipboardData, string_c);
    return gptIOCtx->sbcClipboardData;
}

void
pl_set_clipboard_text(void* pUnused, const char* text)
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
    [pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

#endif

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_exe.c"