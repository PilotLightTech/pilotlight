/*
   pl_main_macos.c
     * MacOS platform backend
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
// [SECTION] library ext
// [SECTION] window ext
// [SECTION] clipboard
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
#include "pl_ds.h"
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
#include <unistd.h> // close
#include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
#include <pthread.h>

// embedded extensions
#include "pl_window_ext.h"
#include "pl_library_ext.h"

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

static inline CFTimeInterval pl__get_absolute_time(void) { return (CFTimeInterval)((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9); }

//-----------------------------------------------------------------------------
// [SECTION] structs & interfaces
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
    bool            bValid;
    uint32_t        uTempIndex;
    char            acPath[PL_MAX_PATH_LENGTH];
    char            acTransitionalName[PL_MAX_PATH_LENGTH];
    char            acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc   tDesc;
    void*           handle;
    struct timespec lastWriteTime;
} plSharedLibrary;

typedef struct _plWindowData
{
    NSWindow*           ptWindow;
    plNSViewController* ptViewController;
    CAMetalLayer*       ptLayer;
} plWindowData;

typedef struct _plRuntimeMutex
{
    pthread_mutex_t tHandle;
} plRuntimeMutex;

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
 
//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// MacOS stuff
CFTimeInterval gtTime;
id gtAppDelegate;
const plLibraryI* gptLibraryApi = NULL;

plKeyEventResponder* gKeyEventResponder = NULL;
NSTextInputContext*  gInputContext = NULL;
id                   gMonitor;
NSCursor* aptMouseCursors[PL_MOUSE_CURSOR_COUNT];

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
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            return 0;
        }
        else if(strcmp(argv[i], "--extensions") == 0)
        {
            plVersion tWindowExtVersion = plWindowI_version;
            plVersion tLibraryVersion = plLibraryI_version;
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            printf("Embedded Extensions:\n");
            printf("   pl_window_ext:  %u.%u.%u\n", tWindowExtVersion.uMajor, tWindowExtVersion.uMinor, tWindowExtVersion.uMinor);
            printf("   pl_library_ext: %u.%u.%u\n", tLibraryVersion.uMajor, tLibraryVersion.uMinor, tLibraryVersion.uMinor);
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
            printf("--extensions    %s\n", "Displays embedded extensions.");
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
    pl__load_core_apis();
    pl__load_ext_apis();

    // setup & retrieve io context 
    gptIOCtx = gptIOI->get_io();

    // command line args
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    // create view controller
    gKeyEventResponder = [[plKeyEventResponder alloc] initWithFrame:NSZeroRect];

    // set clipboard functions (may need to move this to OS api)
    gptIOCtx->set_clipboard_text_fn = pl_set_clipboard_text;
    gptIOCtx->get_clipboard_text_fn = pl_get_clipboard_text;

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
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    gptLibraryApi = ptLibraryApi;
    const plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName
    };
    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
        
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
    else
        return 2;

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

    // gptIOCtx->tMainFramebufferScale.x = scaleFactor;
    // gptIOCtx->tMainFramebufferScale.y = scaleFactor;

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
    // gptIOCtx->tMainFramebufferScale.x = self.window.screen.backingScaleFactor;
    // gptIOCtx->tMainFramebufferScale.y = self.window.screen.backingScaleFactor;
    if(gpUserData)
        pl_app_resize(gpUserData);
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
    pl__unload_all_extensions();
    pl__unload_core_apis();

    pl_sb_free(gsbtWindows);

    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    pl__check_for_leaks();
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

    // if(gpUserData)
    //     pl_app_resize(gpUserData);
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
    // gptIOCtx->tMainFramebufferScale.x = self.view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    // gptIOCtx->tMainFramebufferScale.y = gptIOCtx->tMainFramebufferScale.x;
    // gptIOCtx->tMainViewportSize.x = size.width;
    // gptIOCtx->tMainViewportSize.y = size.height;
    // if(gpUserData)
    // {
    //     pl_app_resize(gpUserData);
    // }
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)layer
{
    @autoreleasepool
    {
        // gAppData.graphics.metalLayer = layer;
        gptIOCtx->pBackendPlatformData = layer;

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
            pl__handle_extension_reloads();
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }

        layer.contentsScale = self.view.window.screen.backingScaleFactor;

        // frame buffer size
        const NSRect contentRect = [self.view frame];
        const NSRect fbRect = [self.view convertRectToBacking:contentRect];
        layer.drawableSize = CGSizeMake(fbRect.size.width, fbRect.size.height);
        
        

        // window size
        const NSRect contentRect2 = [self.view frame];
        float fCurrentWidth = (float)contentRect2.size.width;
        float fCurrentHeight = (float)contentRect2.size.height;

        float fCurrentScaleX = (float)self.view.window.screen.backingScaleFactor;
        float fCurrentScaleY = (float)self.view.window.screen.backingScaleFactor;


        // float fCurrentScaleX = 1.0f;
        // float fCurrentScaleY = 1.0f;

        bool bResize = false;

        if(fCurrentWidth != gptIOCtx->tMainViewportSize.x || fCurrentHeight != gptIOCtx->tMainViewportSize.y)
            bResize = true;
        else if(fCurrentScaleX != gptIOCtx->tMainFramebufferScale.x || fCurrentScaleY != gptIOCtx->tMainFramebufferScale.y )
            bResize = true;


        if(bResize)
        {
            gptIOCtx->tMainViewportSize.x = fCurrentWidth;
            gptIOCtx->tMainViewportSize.y = fCurrentHeight;
            gptIOCtx->tMainFramebufferScale.x = fCurrentScaleX;
            gptIOCtx->tMainFramebufferScale.y = fCurrentScaleY;
            pl_app_resize(gpUserData);
            return;
        }

        if(gtTime == 0.0)
            gtTime = pl__get_absolute_time();

        double dCurrentTime = pl__get_absolute_time();
        gptIOCtx->fDeltaTime = (float)(dCurrentTime - gtTime);
        gtTime = dCurrentTime;

        pl__garbage_collect_data_reg();
        pl_app_update(gpUserData);
        pl__handle_extension_reloads();

        if(gbApisDirty)
            pl__check_apis();

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

    pl__check_for_leaks();
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

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

void
pl_copy_file(const char* source, const char* destination)
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
        return newWriteTime.tv_sec != library->lastWriteTime.tv_sec;
    }
    return false;
}

plLibraryResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{

    plSharedLibrary* ptLibrary = NULL;

    if(*pptLibraryOut == NULL)
    {
        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));

        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;

        pl_sprintf(ptLibrary->acPath, "%s.dylib", tDesc.pcName);

        if(tDesc.pcTransitionalName)
            strncpy(ptLibrary->acTransitionalName, tDesc.pcTransitionalName, PL_MAX_PATH_LENGTH);
        else
        {
            pl_sprintf(ptLibrary->acTransitionalName, "%s_", tDesc.pcName);
        }

        if(tDesc.pcLockFile)
            strncpy(ptLibrary->acLockFile, tDesc.pcLockFile, PL_MAX_PATH_LENGTH);
        else
            strncpy(ptLibrary->acLockFile, "lock.tmp", PL_MAX_PATH_LENGTH);
    }
    else
        ptLibrary = *pptLibraryOut;

    ptLibrary->bValid = false;

    struct stat attr2;
    if(stat(ptLibrary->acLockFile, &attr2) == -1)  // lock file gone
    {
        char temporaryName[2024] = {0};
        ptLibrary->lastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        pl_sprintf(temporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dylib");
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl_copy_file(ptLibrary->acPath, temporaryName);

        ptLibrary->handle = NULL;
        ptLibrary->handle = dlopen(temporaryName, RTLD_NOW);
        if(ptLibrary->handle)
            ptLibrary->bValid = true;
        else
        {
            printf("\n\n%s\n\n", dlerror());
        }
    }

    if(ptLibrary->bValid)
        return PL_LIBRARY_RESULT_SUCCESS;
    return PL_LIBRARY_RESULT_FAIL;
}

void
pl_reload_library(plSharedLibrary* library)
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
// [SECTION] window ext
//-----------------------------------------------------------------------------

plWindowResult
pl_create_window(plWindowDesc tDesc, plWindow** pptWindowOut)
{

    plWindowData* ptData = malloc(sizeof(plWindowData));

    // create view
    ptData->ptViewController = [[plNSViewController alloc] init];

   id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    NSRect frame = NSMakeRect(0, 0, tDesc.uWidth, tDesc.uHeight);
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

    NSString* tWindowTitle = [NSString stringWithUTF8String:tDesc.pcTitle];
    [ptData->ptWindow setTitle:tWindowTitle];


    NSPoint tOrigin = NSMakePoint(tDesc.iXPos, ptData->ptWindow.screen.frame.size.height - tDesc.iYPos);
    [ptData->ptWindow setFrameTopLeftPoint:tOrigin];

    
    ptData->ptWindow.delegate = gtAppDelegate;

    // window size
    const NSRect contentRect2 = [view frame];
    gptIOCtx->tMainViewportSize.x = (float)contentRect2.size.width;
    gptIOCtx->tMainViewportSize.y = (float)contentRect2.size.height;

    plWindow* ptWindow = malloc(sizeof(plWindow));
    
    ptWindow->tDesc = tDesc;
    ptWindow->_pPlatformData = ptData;
    pl_sb_push(gsbtWindows, ptWindow);
    *pptWindowOut = ptWindow;

    return PL_WINDOW_RESULT_SUCCESS;
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
