/*
   apple_pl.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] internal api
// [SECTION] globals
// [SECTION] entry point
// [SECTION] plNSView
// [SECTION] plNSViewController
// [SECTION] internal implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h" // data registry, api registry, extension registry
#include "pl_os.h"
#include "pl_ds.h"      // hashmap

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <time.h>
#include <stdlib.h>   // malloc
#include <string.h>   // strncpy
#include <sys/stat.h> // timespec
#include <stdio.h>    // file api
#include <copyfile.h> // copyfile
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <sys/socket.h>   // sockets
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

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

typedef struct _plWindowData
{
    NSWindow*           ptWindow;
    plNSViewController* ptViewController;
    CAMetalLayer*       ptLayer;
} plWindowData;

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

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plKey pl__osx_key_to_pl_key(int iKey);

// clip board
static const char* pl__get_clipboard_text(void* user_data_ctx);
static void        pl__set_clipboard_text(void* pUnused, const char* text);

static struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}

static inline CFTimeInterval pl__get_absolute_time(void) { return (CFTimeInterval)((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9); }

// Undocumented methods for creating cursors. (from Dear ImGui)
@interface NSCursor()
+ (id)_windowResizeNorthWestSouthEastCursor;
+ (id)_windowResizeNorthEastSouthWestCursor;
+ (id)_windowResizeNorthSouthCursor;
+ (id)_windowResizeEastWestCursor;
@end

static plKey pl__osx_key_to_pl_key(int iKey);
static void  pl__add_osx_tracking_area(NSView* _Nonnull view);
static bool  pl__handle_osx_event(NSEvent* event, NSView* view);

// window api
plWindow* pl__create_window(const plWindowDesc* ptDesc);
void      pl__destroy_window(plWindow* ptWindow);

// os services
void  pl__read_file            (const char* pcFile, uint32_t* puSize, uint8_t* pcBuffer, const char* pcMode);
void  pl__copy_file            (const char* pcSource, const char* pcDestination);
void  pl__create_udp_socket    (plSocket** pptSocketOut, bool bNonBlocking);
void  pl__bind_udp_socket      (plSocket* ptSocket, int iPort);
bool  pl__send_udp_data        (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
bool  pl__get_udp_data         (plSocket* ptSocket, void* pData, size_t szSize);
bool  pl__has_library_changed  (plSharedLibrary* ptLibrary);
bool  pl__load_library         (const char* pcName, const char* pcTransitionalName, const char* pcLockFile, plSharedLibrary** pptLibraryOut);
void  pl__reload_library       (plSharedLibrary* ptLibrary);
void* pl__load_library_function(plSharedLibrary* ptLibrary, const char* pcName);

// thread api
void     pl__sleep(uint32_t millisec);
uint32_t pl__get_hardware_thread_count(void);
void     pl__create_thread(plThreadProcedure ptProcedure, void* pData, plThread** ppThreadOut);
void     pl__join_thread(plThread* ptThread);
void     pl__yield_thread(void);
void     pl__create_mutex(plMutex** ppMutexOut);
void     pl__lock_mutex(plMutex* ptMutex);
void     pl__unlock_mutex(plMutex* ptMutex);
void     pl__destroy_mutex(plMutex** pptMutex);
void     pl__create_critical_section(plCriticalSection** pptCriticalSectionOut);
void     pl__destroy_critical_section(plCriticalSection** pptCriticalSection);
void     pl__enter_critical_section  (plCriticalSection* ptCriticalSection);
void     pl__leave_critical_section  (plCriticalSection* ptCriticalSection);
void     pl__create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
void     pl__wait_on_semaphore(plSemaphore* ptSemaphore);
bool     pl__try_wait_on_semaphore(plSemaphore* ptSemaphore);
void     pl__release_semaphore(plSemaphore* ptSemaphore);
void     pl__destroy_semaphore(plSemaphore** pptSemaphore);
void     pl__allocate_thread_local_key(plThreadKey** pptKeyOut);
void     pl__free_thread_local_key(plThreadKey** ppuIndex);
void*    pl__allocate_thread_local_data(plThreadKey* ptKey, size_t szSize);
void*    pl__get_thread_local_data(plThreadKey* ptKey);
void     pl__free_thread_local_data(plThreadKey* ptKey, void* pData);
void     pl__create_condition_variable(plConditionVariable** pptConditionVariableOut);
void     pl__destroy_condition_variable(plConditionVariable** pptConditionVariable);
void     pl__wake_condition_variable(plConditionVariable* ptConditionVariable);
void     pl__wake_all_condition_variable(plConditionVariable* ptConditionVariable);
void     pl__sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection);
void     pl__create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut);
void     pl__destroy_barrier(plBarrier** pptBarrier);
void     pl__wait_on_barrier(plBarrier* ptBarrier);

// atomics
void    pl__create_atomic_counter  (int64_t ilValue, plAtomicCounter** ptCounter);
void    pl__destroy_atomic_counter (plAtomicCounter** ptCounter);
void    pl__atomic_store           (plAtomicCounter* ptCounter, int64_t ilValue);
int64_t pl__atomic_load            (plAtomicCounter* ptCounter);
bool    pl__atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue);
void    pl__atomic_increment       (plAtomicCounter* ptCounter);
void    pl__atomic_decrement       (plAtomicCounter* ptCounter);

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// apis
static const plDataRegistryI*      gptDataRegistry = NULL;
static const plApiRegistryI*       gptApiRegistry = NULL;
static const plExtensionRegistryI* gptExtensionRegistry = NULL;
static const plIOI*                gptIOI               = NULL;

// OS apis
static const plLibraryI* gptLibraryApi = NULL;

static plSharedLibrary*     gptAppLibrary = NULL;
static void*                gUserData = NULL;
static plKeyEventResponder* gKeyEventResponder = NULL;
static NSTextInputContext*  gInputContext = NULL;
static id                   gMonitor;
static CFTimeInterval tTime;
static NSCursor*      aptMouseCursors[PL_MOUSE_CURSOR_COUNT];

// ui
plIO*        gptIOCtx = NULL;

// memory tracking
static plMemoryContext gtMemoryContext = {0};
static plHashMap gtMemoryHashMap = {0};

// windows
plWindow** gsbtWindows = NULL;

// app config
id gtAppDelegate;

// app function pointers
static void* (*pl_app_load)    (const plApiRegistryI* ptApiRegistry, void* ptAppData);
static void  (*pl_app_shutdown)(void* ptAppData);
static void  (*pl_app_resize)  (void* ptAppData);
static void  (*pl_app_update)  (void* ptAppData);
static bool  (*pl_app_info)    (const plApiRegistryI*);

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
            printf("Version: %s\n", PILOTLIGHT_VERSION);
            return 0;
        }
        else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("\nPilot Light - light weight game engine\n");
            printf("Version: %s\n\n", PILOTLIGHT_VERSION);
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

    // load apis
    gtMemoryContext.ptHashMap = &gtMemoryHashMap;
    gptApiRegistry = pl_load_core_apis();

    static const plWindowI tWindowApi = {
        .create_window  = pl__create_window,
        .destroy_window = pl__destroy_window
    };

    static const plLibraryI tApi3 = {
        .has_changed   = pl__has_library_changed,
        .load          = pl__load_library,
        .load_function = pl__load_library_function,
        .reload        = pl__reload_library
    };

    static const plFileI tApi4 = {
        .copy = pl__copy_file,
        .read = pl__read_file
    };
    
    static const plUdpI tApi5 = {
        .create_socket = pl__create_udp_socket,
        .bind_socket   = pl__bind_udp_socket,  
        .get_data      = pl__get_udp_data,
        .send_data     = pl__send_udp_data
    };

    static const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl__get_hardware_thread_count,
        .create_thread               = pl__create_thread,
        .join_thread                 = pl__join_thread,
        .yield_thread                = pl__yield_thread,
        .sleep_thread                = pl__sleep,
        .create_mutex                = pl__create_mutex,
        .destroy_mutex               = pl__destroy_mutex,
        .lock_mutex                  = pl__lock_mutex,
        .unlock_mutex                = pl__unlock_mutex,
        .create_semaphore            = pl__create_semaphore,
        .destroy_semaphore           = pl__destroy_semaphore,
        .wait_on_semaphore           = pl__wait_on_semaphore,
        .try_wait_on_semaphore       = pl__try_wait_on_semaphore,
        .release_semaphore           = pl__release_semaphore,
        .allocate_thread_local_key   = pl__allocate_thread_local_key,
        .allocate_thread_local_data  = pl__allocate_thread_local_data,
        .free_thread_local_key       = pl__free_thread_local_key, 
        .get_thread_local_data       = pl__get_thread_local_data, 
        .free_thread_local_data      = pl__free_thread_local_data, 
        .create_critical_section     = pl__create_critical_section,
        .destroy_critical_section    = pl__destroy_critical_section,
        .enter_critical_section      = pl__enter_critical_section,
        .leave_critical_section      = pl__leave_critical_section,
        .create_condition_variable   = pl__create_condition_variable,
        .destroy_condition_variable  = pl__destroy_condition_variable,
        .wake_condition_variable     = pl__wake_condition_variable,
        .wake_all_condition_variable = pl__wake_all_condition_variable,
        .sleep_condition_variable    = pl__sleep_condition_variable,
        .create_barrier              = pl__create_barrier,
        .destroy_barrier             = pl__destroy_barrier,
        .wait_on_barrier             = pl__wait_on_barrier
    };

    static const plAtomicsI tAtomicsApi = {
        .create_atomic_counter   = pl__create_atomic_counter,
        .destroy_atomic_counter  = pl__destroy_atomic_counter,
        .atomic_store            = pl__atomic_store,
        .atomic_load             = pl__atomic_load,
        .atomic_compare_exchange = pl__atomic_compare_exchange,
        .atomic_increment        = pl__atomic_increment,
        .atomic_decrement        = pl__atomic_decrement
    };

    gptApiRegistry->add(PL_API_WINDOW, &tWindowApi);
    gptApiRegistry->add(PL_API_LIBRARY, &tApi3);
    gptApiRegistry->add(PL_API_FILE, &tApi4);
    gptApiRegistry->add(PL_API_UDP, &tApi5);
    gptApiRegistry->add(PL_API_THREADS, &tThreadApi);
    gptApiRegistry->add(PL_API_ATOMICS, &tAtomicsApi);

    gptDataRegistry      = gptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptExtensionRegistry = gptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptLibraryApi        = gptApiRegistry->first(PL_API_LIBRARY);
    gptIOI               = gptApiRegistry->first(PL_API_IO);

    // setup & retrieve io context 
    gptIOCtx = gptIOI->get_io();
    gtMemoryContext.plThreadsI = &tThreadApi;
    gptDataRegistry->set_data(PL_CONTEXT_MEMORY, &gtMemoryContext);

    // create view controller
    gKeyEventResponder = [[plKeyEventResponder alloc] initWithFrame:NSZeroRect];

    // set clipboard functions (may need to move this to OS api)
    gptIOCtx->set_clipboard_text_fn = pl__set_clipboard_text;
    gptIOCtx->get_clipboard_text_fn = pl__get_clipboard_text;

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
        
        gUserData = pl_app_load(gptApiRegistry, NULL);
    }

    // run app
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}

//-----------------------------------------------------------------------------
// [SECTION] plNSView
//-----------------------------------------------------------------------------

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

    pl_app_shutdown(gUserData);
    gptExtensionRegistry->unload_all();

    uint32_t uMemoryLeakCount = 0;
    for(uint32_t i = 0; i < pl_sb_size(gtMemoryContext.sbtAllocations); i++)
    {
        if(gtMemoryContext.sbtAllocations[i].pAddress != NULL)
        {
            printf("Unfreed memory from line %i in file '%s'.\n", gtMemoryContext.sbtAllocations[i].iLine, gtMemoryContext.sbtAllocations[i].pcFile);
            uMemoryLeakCount++;
        }
    }
        
    assert(uMemoryLeakCount == gtMemoryContext.szActiveAllocations);
    if(uMemoryLeakCount > 0)
        printf("%u unfreed allocations.\n", uMemoryLeakCount);
}

// This is the renderer output callback function
static CVReturn
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
    if(gUserData)
    {
        pl_app_resize(gUserData);
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
            gUserData = pl_app_load(gptApiRegistry, gUserData);
        }


        if(self.view)
        {
            const float fDpi = (float)[self.view.window backingScaleFactor];
            gptIOCtx->afMainViewportSize[0] = (float)self.view.bounds.size.width;
            gptIOCtx->afMainViewportSize[1] = (float)self.view.bounds.size.height;
            gptIOCtx->afMainFramebufferScale[0] = fDpi;
            gptIOCtx->afMainFramebufferScale[1] = fDpi;
        }

        if(tTime == 0.0)
            tTime = pl__get_absolute_time();

        double dCurrentTime = pl__get_absolute_time();
        gptIOCtx->fDeltaTime = (float)(dCurrentTime - tTime);
        tTime = dCurrentTime;

        gptDataRegistry->garbage_collect();
        pl_app_update(gUserData);
        gptExtensionRegistry->reload();

        if(gptIOCtx->bRunning == false)
        {
            [NSApp terminate:nil];
        }
    }
}

- (void)shutdown
{
    pl_app_shutdown(gUserData);
    if(gMonitor != NULL)
    {
        [NSEvent removeMonitor:gMonitor];
        gMonitor = NULL;
    }

    for(uint32_t i = 0; i < pl_sb_size(gtMemoryContext.sbtAllocations); i++)
        printf("Unfreed memory from line %i in file '%s'.\n", gtMemoryContext.sbtAllocations[i].iLine, gtMemoryContext.sbtAllocations[i].pcFile);

    if(pl_sb_size(gtMemoryContext.sbtAllocations) > 0)
        printf("%u unfreed allocations.\n", pl_sb_size(gtMemoryContext.sbtAllocations));
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

static bool
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

static void 
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

void
pl__read_file(const char* file, uint32_t* sizeIn, uint8_t* buffer, const char* mode)
{
    PL_ASSERT(sizeIn);

    FILE* dataFile = fopen(file, mode);
    uint32_t size = 0u;

    if (dataFile == NULL)
    {
        *sizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(dataFile, 0, SEEK_END);
    size = ftell(dataFile);
    // fseek(dataFile, 0, SEEK_SET);
    rewind(dataFile);

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
pl__copy_file(const char* source, const char* destination)
{
    copyfile_state_t s;
    s = copyfile_state_alloc();
    copyfile(source, destination, s, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(s);
}

void
pl__create_udp_socket(plSocket** pptSocketOut, bool bNonBlocking)
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
pl__bind_udp_socket(plSocket* ptSocket, int iPort)
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
pl__send_udp_data(plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize)
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
pl__get_udp_data(plSocket* ptSocket, void* pData, size_t szSize)
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

bool
pl__has_library_changed(plSharedLibrary* library)
{
    struct timespec newWriteTime = pl__get_last_write_time(library->acPath);
    return newWriteTime.tv_sec != library->lastWriteTime.tv_sec;
}

bool
pl__load_library(const char* name, const char* transitionalName, const char* lockFile, plSharedLibrary** pptLibraryOut)
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
        pl__copy_file(library->acPath, temporaryName);

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
pl__reload_library(plSharedLibrary* library)
{
    library->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl__load_library(library->acPath, library->acTransitionalName, library->acLockFile, &library))
            break;
        pl__sleep(100);
    }
}

void*
pl__load_library_function(plSharedLibrary* library, const char* name)
{
    PL_ASSERT(library->bValid && "Library not valid");
    void* loadedFunction = NULL;
    if(library->bValid)
    {
        loadedFunction = dlsym(library->handle, name);
    }
    return loadedFunction;
}

void
pl__sleep(uint32_t millisec)
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
pl__create_thread(plThreadProcedure ptProcedure, void* pData, plThread** pptThreadOut)
{
    *pptThreadOut = PL_ALLOC(sizeof(plThread));
    if(pthread_create(&(*pptThreadOut)->tHandle, NULL, ptProcedure, pData))
    {
        PL_ASSERT(false);
    }
}

void
pl__join_thread(plThread* ptThread)
{
    pthread_join(ptThread->tHandle, NULL);
}

void
pl__yield_thread(void)
{
    sched_yield();
}

void
pl__create_mutex(plMutex** pptMutexOut)
{
    *pptMutexOut = PL_ALLOC(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
    }
}

void
pl__lock_mutex(plMutex* ptMutex)
{
    pthread_mutex_lock(&ptMutex->tHandle);
}

void
pl__unlock_mutex(plMutex* ptMutex)
{
    pthread_mutex_unlock(&ptMutex->tHandle);
}

void
pl__destroy_mutex(plMutex** pptMutex)
{
    pthread_mutex_destroy(&(*pptMutex)->tHandle);
    PL_FREE((*pptMutex));
    *pptMutex = NULL;
}

void
pl__create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    *pptCriticalSectionOut = PL_ALLOC(sizeof(plCriticalSection));
    if(pthread_mutex_init(&(*pptCriticalSectionOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
    }
}

void
pl__destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    pthread_mutex_destroy(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    *pptCriticalSection = NULL;
}

void
pl__enter_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_lock(&ptCriticalSection->tHandle);
}

void
pl__leave_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_unlock(&ptCriticalSection->tHandle);
}

uint32_t
pl__get_hardware_thread_count(void)
{

    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)numCPU;
}

void
pl__create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    *pptSemaphoreOut = PL_ALLOC(sizeof(plSemaphore));
    memset((*pptSemaphoreOut), 0, sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = dispatch_semaphore_create(uIntialCount);
}

void
pl__destroy_semaphore(plSemaphore** pptSemaphore)
{
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl__wait_on_semaphore(plSemaphore* ptSemaphore)
{
    dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_FOREVER);
}

bool
pl__try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    return dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_NOW) == 0;
}

void
pl__release_semaphore(plSemaphore* ptSemaphore)
{
    dispatch_semaphore_signal(ptSemaphore->tHandle);
}


void
pl__allocate_thread_local_key(plThreadKey** pptKeyOut)
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
pl__free_thread_local_key(plThreadKey** pptKey)
{
    pthread_key_delete((*pptKey)->tKey);
    PL_FREE((*pptKey));
    *pptKey = NULL;
}

void*
pl__allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    void* pData = PL_ALLOC(szSize);
    memset(pData, 0, szSize);
    pthread_setspecific(ptKey->tKey, pData);
    return pData;
}

void*
pl__get_thread_local_data(plThreadKey* ptKey)
{
    void* pData = pthread_getspecific(ptKey->tKey);
    return pData;
}

void
pl__free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    PL_FREE(pData);
}

void
pl__create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    *pptBarrierOut = PL_ALLOC(sizeof(plBarrier));
    pthread_barrier_init(&(*pptBarrierOut)->tHandle, NULL, uThreadCount);
}

void
pl__destroy_barrier(plBarrier** pptBarrier)
{
    pthread_barrier_destroy(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
}

void
pl__wait_on_barrier(plBarrier* ptBarrier)
{
    pthread_barrier_wait(&ptBarrier->tHandle);
}

void
pl__create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    *pptConditionVariableOut = PL_ALLOC(sizeof(plConditionVariable));
    pthread_cond_init(&(*pptConditionVariableOut)->tHandle, NULL);
}

void               
pl__destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    pthread_cond_destroy(&(*pptConditionVariable)->tHandle);
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
}

void               
pl__wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_signal(&ptConditionVariable->tHandle);
}

void               
pl__wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_broadcast(&ptConditionVariable->tHandle);
}

void               
pl__sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    pthread_cond_wait(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle);
}

void
pl__create_atomic_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = malloc(sizeof(plAtomicCounter));
    atomic_init(&(*ptCounter)->ilValue, ilValue);
}

void
pl__destroy_atomic_counter(plAtomicCounter** ptCounter)
{
    free((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl__atomic_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    atomic_store(&ptCounter->ilValue, ilValue);
}

int64_t
pl__atomic_load(plAtomicCounter* ptCounter)
{
    return atomic_load(&ptCounter->ilValue);
}

bool
pl__atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return atomic_compare_exchange_strong(&ptCounter->ilValue, &ilExpectedValue, ilDesiredValue);
}

void
pl__atomic_increment(plAtomicCounter* ptCounter)
{
    atomic_fetch_add(&ptCounter->ilValue, 1);
}

void
pl__atomic_decrement(plAtomicCounter* ptCounter)
{
    atomic_fetch_sub(&ptCounter->ilValue, 1);
}

plWindow*
pl__create_window(const plWindowDesc* ptDesc)
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
pl__destroy_window(plWindow* ptWindow)
{
    plWindowData* ptData = ptWindow->_pPlatformData;
    [ptData->ptWindow release];
    free(ptData);
    free(ptWindow);
}

const char*
pl__get_clipboard_text(void* user_data_ctx)
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

void
pl__set_clipboard_text(void* pUnused, const char* text)
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
    [pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
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

#include "pilotlight_exe.c"