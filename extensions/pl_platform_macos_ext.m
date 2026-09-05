/*
   pl_platform_macos_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// #include <math.h>
#import <Cocoa/Cocoa.h> // dispatch semaphore stuff
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"

// extensions
#include "pl_platform_ext.h"

// macos stuff
#import <time.h>
#include <sys/stat.h> // timespec
#include <copyfile.h> // copyfile
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h> // close, rmdir
#include <dirent.h> // directory operations
#include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
#include <sys/mman.h>

plThread** gsbtThreads = NULL;

static const plMemoryI*  gptMemory = NULL;
static const plIOI*      gptIOI = NULL;

static plIO* gptIO = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#ifndef PL_DS_ALLOC
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"

typedef struct _plPlatformExtData
{
    NSNumber* tScreen;
    id<MTLDevice> tDevice;
    CAMetalLayer* ptLayer;
    id gtAppDelegate;
    double dInitialTime;
} plPlatformExtData;

typedef struct _plWindowSurfaceImageCocoa
{
    CGColorSpaceRef colorSpace;
    CGDataProviderRef provider;
    CGImageRef image;
    uint32_t uWidth;
    uint32_t uHeight;
    void*    pPixels;
    uint32_t uPixelCapacity;
} plWindowSurfaceImageCocoa;

typedef struct _plWindowSurface
{
    plWindow* ptWindow;
    uint32_t  uCurrentImage;
    uint32_t  uImageCount;
    plWindowSurfaceImageCocoa* atImages;
} plWindowSurface;

static plPlatformExtData* gptPlatformExtCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] timer api
//-----------------------------------------------------------------------------

double
pl_timer_get_time(void)
{
    double dNewTime = (CFTimeInterval)((double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9);
    return dNewTime - gptPlatformExtCtx->dInitialTime;
}

//-----------------------------------------------------------------------------
// [SECTION] window ext
//-----------------------------------------------------------------------------

@class plNSView;
@class plNSCpuView;
@class plNSViewController;
@class plKeyEventResponder;

typedef struct _plWindowData plWindowData;

static plWindowData* pl__find_window_data(NSWindow*);
static void          pl__mark_window_resize(plWindowData*);
static bool          pl__application_process_window_events(void);
static void          pl__install_osx_event_monitor(void);
static void          pl__remove_osx_event_monitor(void);
static void          pl__update_mouse_cursor(void);

plKey pl__osx_key_to_pl_key(int iKey);
bool  pl__handle_osx_event(NSEvent* event, NSView* view);

// clipboard
const char* pl_get_clipboard_text(void* user_data_ctx);
void        pl_set_clipboard_text(void* pUnused, const char* text);

typedef enum _plMacWindowEventType
{
    PL_MAC_WINDOW_EVENT_RESIZED,
    PL_MAC_WINDOW_EVENT_CLOSE_REQUESTED
} plMacWindowEventType;

typedef struct _plMacWindowEvent
{
    plMacWindowEventType tType;
    plWindow*            ptWindow;
    uint32_t             uDrawableWidth;
    uint32_t             uDrawableHeight;
    float                fViewportWidth;
    float                fViewportHeight;
    float                fScale;
} plMacWindowEvent;

struct _plWindowData
{
    plWindow*             ptPublicWindow;
    NSWindow*             ptNativeWindow;
    plNSViewController*   ptViewController;
    plNSView*             ptView;
    plNSCpuView*          ptCpuView;
    CAMetalLayer*         ptLayer;
    id<MTLDevice>         tDevice;
    plKeyEventResponder*  ptKeyResponder;
    NSTextInputContext*   ptInputContext;
    CGSize                tPendingDrawableSize;
    CGFloat               fPendingScale;
    bool                  bResizePending;
    bool                  bClosePending;
};

@interface plNSAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@interface plNSViewController : NSViewController
@end

@interface plNSView : NSView <CALayerDelegate>
@property(nonatomic, assign) plWindowData* windowData;
@property(nonatomic, nonnull, readonly) CAMetalLayer* metalLayer;
- (void)initCommon;
- (void)markDrawableResize;
- (CGSize)backingDrawableSize;
@end

@interface plNSCpuView : NSView
{
@public
    plWindowSurface*  ptSurface;
}
@end

@interface plKeyEventResponder : NSView <NSTextInputClient>
@end

// Undocumented methods for creating cursors. (from Dear ImGui)
@interface NSCursor()
+ (id)_windowResizeNorthWestSouthEastCursor;
+ (id)_windowResizeNorthEastSouthWestCursor;
+ (id)_windowResizeNorthSouthCursor;
+ (id)_windowResizeEastWestCursor;
@end

id                gMonitor = nil;
NSCursor*         aptMouseCursors[PL_MOUSE_CURSOR_COUNT];
static plMacWindowEvent*      gsbtWindowEvents = NULL;

// typedef struct _plPlatformExtData
// {
    plWindow* gptMainWindow;
    plWindow** gsbtWindows;
// } plPlatformExtData;

@implementation plNSAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)application
{
    (void)application;
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
    (void)notification;
    gptIO->bRunning = false;
}

- (void)windowWillClose:(NSNotification*)notification
{
    plWindowData* ptData = pl__find_window_data((NSWindow*)notification.object);
    if(!ptData)
        return;

    ptData->bClosePending = true;

    if(ptData->ptPublicWindow == gptMainWindow)
        gptIO->bRunning = false;
}

@end

@implementation plNSCpuView

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    // if(ptSurface->atImages[ptSurface->uCurrentImage].)
    //     return;

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextDrawImage(context, NSRectToCGRect(self.bounds), ptSurface->atImages[ptSurface->uCurrentImage].image);

}

@end

@implementation plNSView

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if(self)
        [self initCommon];
    return self;
}

- (void)dealloc
{
    self.layer.delegate = nil;
    _windowData = NULL;
    [super dealloc];
}

- (void)initCommon
{
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawBeforeViewResize;
    _metalLayer = (CAMetalLayer*)self.layer;
    self.layer.delegate = self;
}

- (CALayer*)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (CGSize)backingDrawableSize
{
    NSRect tBackingBounds = [self convertRectToBacking:self.bounds];
    CGFloat fWidth = floor(tBackingBounds.size.width);
    CGFloat fHeight = floor(tBackingBounds.size.height);

    if(fWidth < 1.0)
        fWidth = 1.0;
    if(fHeight < 1.0)
        fHeight = 1.0;

    return CGSizeMake(fWidth, fHeight);
}

- (void)markDrawableResize
{
    if(!_windowData || !self.window)
        return;

    _windowData->tPendingDrawableSize = [self backingDrawableSize];
    _windowData->fPendingScale = self.window.screen ? self.window.screen.backingScaleFactor : NSScreen.mainScreen.backingScaleFactor;
    _windowData->bResizePending = true;
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self markDrawableResize];
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self markDrawableResize];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self markDrawableResize];
}

- (void)setBoundsSize:(NSSize)size
{
    [super setBoundsSize:size];
    [self markDrawableResize];
}

@end

@implementation plNSViewController

- (void)loadView
{
    // The window backend assigns the view explicitly in pl_create_window().
}

@end

@implementation plKeyEventResponder
{
    float  _posX;
    float  _posY;
    NSRect _imeRect;
}

- (void)setImePosX:(float)posX imePosY:(float)posY
{
    _posX = posX;
    _posY = posY;
}

- (void)updateImePosWithView:(NSView*)view
{
    NSWindow* ptWindow = view.window;
    if(!ptWindow)
        return;

    NSRect tContentRect = [ptWindow contentRectForFrameRect:ptWindow.frame];
    NSRect tRect = NSMakeRect(_posX, tContentRect.size.height - _posY, 0, 0);
    _imeRect = [ptWindow convertRectToScreen:tRect];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self.window makeFirstResponder:self];
}

- (void)keyDown:(NSEvent*)event
{
    if(!pl__handle_osx_event(event, self))
        [super keyDown:event];
    [self interpretKeyEvents:@[event]];
}

- (void)keyUp:(NSEvent*)event
{
    if(!pl__handle_osx_event(event, self))
        [super keyUp:event];
}

- (void)flagsChanged:(NSEvent*)event
{
    if(!pl__handle_osx_event(event, self))
        [super flagsChanged:event];
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
    (void)replacementRange;
    NSString* ptCharacters = [aString isKindOfClass:[NSAttributedString class]] ? [aString string] : (NSString*)aString;
    gptIOI->add_text_events_utf8(ptCharacters.UTF8String);
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)doCommandBySelector:(SEL)selector
{
    (void)selector;
}

- (nullable NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    (void)range;
    (void)actualRange;
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    (void)point;
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    (void)range;
    (void)actualRange;
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
    (void)string;
    (void)selectedRange;
    (void)replacementRange;
}

- (void)unmarkText
{
}

- (nonnull NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
    return @[];
}

@end

void*
pl_platform_setup(void)
{
    return gptPlatformExtCtx;
}

void
pl_platform_new_frame(void* pPlatformData)
{
    pl__update_mouse_cursor();

    pl_sb_reset(gsbtWindowEvents);

    // Preserve the existing platform-data convention without making the view
    // controller responsible for the application frame.
    if(gptMainWindow)
    {
        plWindowData* ptData = (plWindowData*)gptMainWindow->_pBackendData;
        if(ptData)
        {
            gptPlatformExtCtx->ptLayer = ptData->ptLayer;
        }
        NSScreen* ptScreen = ptData && ptData->ptNativeWindow.screen ? ptData->ptNativeWindow.screen : NSScreen.mainScreen;
        NSNumber* ptScreenNumber = ptScreen.deviceDescription[@"NSScreenNumber"];
        gptPlatformExtCtx->tScreen = ptScreenNumber;

    }

    for(uint32_t i = 0; i < pl_sb_size(gsbtWindows); i++)
    {
        plWindow* ptWindow = gsbtWindows[i];
        plWindowData* ptData = ptWindow ? (plWindowData*)ptWindow->_pBackendData : NULL;
        if(!ptData)
            continue;

        if(ptData->bClosePending)
        {
            const plMacWindowEvent tEvent = {
                .tType = PL_MAC_WINDOW_EVENT_CLOSE_REQUESTED,
                .ptWindow = ptWindow
            };
            pl_sb_push(gsbtWindowEvents, tEvent);
            ptData->bClosePending = false;
        }

        const CGSize tCurrentDrawableSize = [ptData->ptView backingDrawableSize];
        if(tCurrentDrawableSize.width != ptData->ptLayer.drawableSize.width ||
           tCurrentDrawableSize.height != ptData->ptLayer.drawableSize.height)
        {
            ptData->tPendingDrawableSize = tCurrentDrawableSize;
            NSScreen* ptScreen = ptData->ptNativeWindow.screen ?: NSScreen.mainScreen;
            ptData->fPendingScale = ptScreen ? ptScreen.backingScaleFactor : 1.0;
            ptData->bResizePending = true;
        }

        if(!ptData->bResizePending)
            continue;

        ptData->ptLayer.contentsScale = ptData->fPendingScale;
        ptData->ptLayer.drawableSize = ptData->tPendingDrawableSize;
        ptData->bResizePending = false;

        const plMacWindowEvent tEvent = {
            .tType = PL_MAC_WINDOW_EVENT_RESIZED,
            .ptWindow = ptWindow,
            .uDrawableWidth = (uint32_t)ptData->tPendingDrawableSize.width,
            .uDrawableHeight = (uint32_t)ptData->tPendingDrawableSize.height,
            .fViewportWidth = (float)ptData->ptView.bounds.size.width,
            .fViewportHeight = (float)ptData->ptView.bounds.size.height,
            .fScale = (float)ptData->fPendingScale
        };
        pl_sb_push(gsbtWindowEvents, tEvent);
    }

    for(uint32_t i = 0; i < pl_sb_size(gsbtWindowEvents); i++)
    {
        const plMacWindowEvent* ptEvent = &gsbtWindowEvents[i];

        switch(ptEvent->tType)
        {
            case PL_MAC_WINDOW_EVENT_RESIZED:
            {
                if(ptEvent->ptWindow == gptMainWindow)
                {
                    gptIO->tMainFramebufferScale.x = ptEvent->fScale;
                    gptIO->tMainFramebufferScale.y = ptEvent->fScale;
                    gptIO->tMainViewportSize.x = ptEvent->fViewportWidth;
                    gptIO->tMainViewportSize.y = ptEvent->fViewportHeight;
                    gptIO->bViewportSizeChanged = true;
                }

                // if(gptIO->pl_app_resize && gptIO->_bFirstLoadComplete)
                //    gptIO->pl_app_resize(gptMainWindow, gptIO->pAppUserData);
                // gptIO->bViewportSizeChanged = true;
                break;
            }

            case PL_MAC_WINDOW_EVENT_CLOSE_REQUESTED:
            {
                if(ptEvent->ptWindow == gptMainWindow)
                    gptIO->bRunning = false;
                break;
            }
        }
    }
    pl_sb_reset(gsbtWindowEvents);

    if(gptIO->bViewportSizeChanged && gptIO->pl_app_resize && gptIO->_bFirstLoadComplete)
    {
        gptIO->pl_app_resize(gptMainWindow, gptIO->pAppUserData);
    }
}

void
pl_platform_cleanup(void* ptPlatformData)
{
    pl__remove_osx_event_monitor();

    while(pl_sb_size(gsbtWindows) > 0)
        pl_window_destroy(pl_sb_last(gsbtWindows));

    pl_sb_free(gsbtWindows);
    pl_sb_free(gsbtWindowEvents);
    gptMainWindow = NULL;
}

bool
pl__handle_osx_event(NSEvent* event, NSView* view)
{
    if(!gptIOI || gptIOI->add_mouse_pos_event == NULL)
        return true;

    if(event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown)
    {
        const int iButton = (int)event.buttonNumber;
        if(iButton >= 0 && iButton < PL_MOUSE_BUTTON_COUNT)
            gptIOI->add_mouse_button_event(iButton, true);
        return true;
    }

    if(event.type == NSEventTypeLeftMouseUp || event.type == NSEventTypeRightMouseUp || event.type == NSEventTypeOtherMouseUp)
    {
        const int iButton = (int)event.buttonNumber;
        if(iButton >= 0 && iButton < PL_MOUSE_BUTTON_COUNT)
            gptIOI->add_mouse_button_event(iButton, false);
        return true;
    }

    if(event.type == NSEventTypeMouseMoved || event.type == NSEventTypeLeftMouseDragged || event.type == NSEventTypeRightMouseDragged || event.type == NSEventTypeOtherMouseDragged)
    {
        NSPoint tMousePoint = event.locationInWindow;
        tMousePoint = [view convertPoint:tMousePoint fromView:nil];
        if(!view.isFlipped)
            tMousePoint.y = view.bounds.size.height - tMousePoint.y;
        gptIOI->add_mouse_pos_event((float)tMousePoint.x, (float)tMousePoint.y);
        return true;
    }

    if(event.type == NSEventTypeScrollWheel)
    {
        if(event.phase == NSEventPhaseCancelled)
            return false;

        double dWheelX = 0.0;
        double dWheelY = 0.0;

        #if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
        if(floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6)
        {
            dWheelX = event.scrollingDeltaX;
            dWheelY = event.scrollingDeltaY;
            if(event.hasPreciseScrollingDeltas)
            {
                dWheelX *= 0.1;
                dWheelY *= 0.1;
            }
        }
        else
        #endif
        {
            dWheelX = event.deltaX;
            dWheelY = event.deltaY;
        }

        if(dWheelX != 0.0 || dWheelY != 0.0)
            gptIOI->add_mouse_wheel_event((float)dWheelX * 0.1f, (float)dWheelY * 0.1f);
        return true;
    }

    if(event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)
    {
        if(event.isARepeat)
            return true;

        const int iKeyCode = (int)event.keyCode;
        gptIOI->add_key_event(pl__osx_key_to_pl_key(iKeyCode), event.type == NSEventTypeKeyDown);
        return true;
    }

    if(event.type == NSEventTypeFlagsChanged)
    {
        const unsigned short uKeyCode = event.keyCode;
        const NSEventModifierFlags tModifierFlags = event.modifierFlags;

        gptIOI->add_key_event(PL_KEY_MOD_SHIFT, (tModifierFlags & NSEventModifierFlagShift) != 0);
        gptIOI->add_key_event(PL_KEY_MOD_CTRL,  (tModifierFlags & NSEventModifierFlagControl) != 0);
        gptIOI->add_key_event(PL_KEY_MOD_ALT,   (tModifierFlags & NSEventModifierFlagOption) != 0);
        gptIOI->add_key_event(PL_KEY_MOD_SUPER, (tModifierFlags & NSEventModifierFlagCommand) != 0);

        const plKey tKey = pl__osx_key_to_pl_key(uKeyCode);
        if(tKey != PL_KEY_NONE)
        {
            NSEventModifierFlags tMask = 0;
            switch(tKey)
            {
                case PL_KEY_LEFT_CTRL:   tMask = 0x0001; break;
                case PL_KEY_RIGHT_CTRL:  tMask = 0x2000; break;
                case PL_KEY_LEFT_SHIFT:  tMask = 0x0002; break;
                case PL_KEY_RIGHT_SHIFT: tMask = 0x0004; break;
                case PL_KEY_LEFT_SUPER:  tMask = 0x0008; break;
                case PL_KEY_RIGHT_SUPER: tMask = 0x0010; break;
                case PL_KEY_LEFT_ALT:    tMask = 0x0020; break;
                case PL_KEY_RIGHT_ALT:   tMask = 0x0040; break;
                default: return true;
            }
            gptIOI->add_key_event(tKey, (tModifierFlags & tMask) != 0);
        }
        return true;
    }

    return false;
}

static void
pl__install_osx_event_monitor(void)
{
    if(gMonitor)
        return;

    NSEventMask tEventMask = 0;
    tEventMask |= NSEventMaskMouseMoved | NSEventMaskScrollWheel;
    tEventMask |= NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged;
    tEventMask |= NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskRightMouseDragged;
    tEventMask |= NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp | NSEventMaskOtherMouseDragged;

    gMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:tEventMask
                                                     handler:^NSEvent* _Nullable(NSEvent* event)
    {
        NSWindow* ptNativeWindow = event.window ?: NSApp.keyWindow;
        plWindowData* ptData = pl__find_window_data(ptNativeWindow);
        if(ptData && ptData->ptView)
            pl__handle_osx_event(event, ptData->ptView);
        return event;
    }];
}

static void
pl__remove_osx_event_monitor(void)
{
    if(gMonitor)
    {
        [NSEvent removeMonitor:gMonitor];
        gMonitor = nil;
    }
}

static void
pl__update_mouse_cursor(void)
{
    // if(gptIO->tCurrentCursor == gptIO->tNextCursor)
    //     return;

    plMouseCursor tCursor = gptIO->tNextCursor;
    if(tCursor < 0 || tCursor >= PL_MOUSE_CURSOR_COUNT)
        tCursor = PL_MOUSE_CURSOR_ARROW;

    gptIO->tCurrentCursor = tCursor;
    NSCursor* ptCursor = aptMouseCursors[tCursor] ?: aptMouseCursors[PL_MOUSE_CURSOR_ARROW];
    [ptCursor set];
    gptIO->tNextCursor = PL_MOUSE_CURSOR_ARROW;
}

static plWindowData*
pl__find_window_data(NSWindow* ptNativeWindow)
{
    if(!ptNativeWindow)
        return NULL;

    for(uint32_t i = 0; i < pl_sb_size(gsbtWindows); i++)
    {
        plWindow* ptWindow = gsbtWindows[i];
        plWindowData* ptData = ptWindow ? (plWindowData*)ptWindow->_pBackendData : NULL;
        if(ptData && ptData->ptNativeWindow == ptNativeWindow)
            return ptData;
    }
    return NULL;
}

static void
pl__mark_window_resize(plWindowData* ptData)
{
    if(!ptData || !ptData->ptView || !ptData->ptNativeWindow)
        return;

    ptData->tPendingDrawableSize = [ptData->ptView backingDrawableSize];
    NSScreen* ptScreen = ptData->ptNativeWindow.screen ?: NSScreen.mainScreen;
    ptData->fPendingScale = ptScreen ? ptScreen.backingScaleFactor : 1.0;
    ptData->bResizePending = true;
}

plWindowResult
pl_window_create(plWindowDesc tDesc, plWindow** pptWindowOut)
{
    if(!pptWindowOut)
        return PL_WINDOW_RESULT_FAIL;

    *pptWindowOut = NULL;

    plWindow* ptWindow = (plWindow*)calloc(1, sizeof(plWindow));
    plWindowData* ptData = (plWindowData*)calloc(1, sizeof(plWindowData));
    if(!ptWindow || !ptData)
    {
        free(ptData);
        free(ptWindow);
        return PL_WINDOW_RESULT_FAIL;
    }

    ptWindow->_pBackendData = ptData;
    ptData->ptPublicWindow = ptWindow;

    ptData->ptViewController = [[plNSViewController alloc] init];
    ptData->ptKeyResponder = [[plKeyEventResponder alloc] initWithFrame:NSZeroRect];
    ptData->ptInputContext = [[NSTextInputContext alloc] initWithClient:ptData->ptKeyResponder];

    const NSRect tFrame = NSMakeRect(0, 0, tDesc.uWidth, tDesc.uHeight);
    if(tDesc.tMode == PL_WINDOW_PRESENTATION_MODE_GRAPHICS_API)
    {
        plNSView* ptView = [[plNSView alloc] initWithFrame:tFrame];
        ptView.windowData = ptData;
        ptData->ptViewController.view = ptView;
        ptData->ptView = ptView;
        [ptView release]; // retained by the view controller

        [ptData->ptView addSubview:ptData->ptKeyResponder];

        ptData->ptLayer = ptData->ptView.metalLayer;
        ptData->ptLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    }
    else if(tDesc.tMode == PL_WINDOW_PRESENTATION_MODE_SOFTWARE)
    {
        plNSCpuView* view = [[plNSCpuView alloc] initWithFrame:tFrame];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        ptData->ptCpuView = view;
        ptData->ptViewController.view = view;
        [view release]; // retained by the view controller

        [ptData->ptCpuView addSubview:ptData->ptKeyResponder];
    }

    ptData->ptNativeWindow = [[NSWindow windowWithContentViewController:ptData->ptViewController] retain];
    if(!ptData->ptNativeWindow)
    {
        [ptData->ptInputContext release];
        [ptData->ptKeyResponder release];
        [ptData->ptViewController release];
        // [ptData->tDevice release];
        free(ptData);
        free(ptWindow);
        return PL_WINDOW_RESULT_FAIL;
    }

    ptData->ptNativeWindow.delegate = gptPlatformExtCtx->gtAppDelegate;

    NSString* ptWindowTitle = [NSString stringWithUTF8String:tDesc.pcTitle ? tDesc.pcTitle : "Pilot Light"];
    ptData->ptNativeWindow.title = ptWindowTitle;

    NSScreen* ptScreen = ptData->ptNativeWindow.screen ?: NSScreen.mainScreen;
    const CGFloat fScreenHeight = ptScreen ? ptScreen.frame.size.height : 0.0;
    const NSPoint tOrigin = NSMakePoint(tDesc.iXPos, fScreenHeight - tDesc.iYPos);
    [ptData->ptNativeWindow setFrameTopLeftPoint:tOrigin];

    [ptData->ptNativeWindow center];
    [ptData->ptNativeWindow orderFront:nil];
    [ptData->ptNativeWindow makeKeyWindow];

    if(gptMainWindow == NULL)
        gptMainWindow = ptWindow;

    pl_sb_push(gsbtWindows, ptWindow);
    *pptWindowOut = ptWindow;

    if(ptWindow == gptMainWindow)
    {
        gptIO->tMainViewportSize.x = (float)ptData->ptView.bounds.size.width;
        gptIO->tMainViewportSize.y = (float)ptData->ptView.bounds.size.height;
    }

    pl__mark_window_resize(ptData);
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy(plWindow* ptWindow)
{
    if(!ptWindow)
        return;

    plWindowData* ptData = (plWindowData*)ptWindow->_pBackendData;

    for(uint32_t i = 0; i < pl_sb_size(gsbtWindows); i++)
    {
        if(gsbtWindows[i] == ptWindow)
        {
            pl_sb_del(gsbtWindows, i);
            break;
        }
    }

    if(gptMainWindow == ptWindow)
        gptMainWindow = pl_sb_size(gsbtWindows) > 0 ? gsbtWindows[0] : NULL;

    if(ptData)
    {
        if(ptData->ptView)
            ptData->ptView.windowData = NULL;

        if(ptData->ptNativeWindow)
        {
            ptData->ptNativeWindow.delegate = nil;
            [ptData->ptNativeWindow orderOut:nil];
        }

        [ptData->ptKeyResponder removeFromSuperview];
        [ptData->ptInputContext release];
        [ptData->ptKeyResponder release];
        [ptData->ptNativeWindow release];
        [ptData->ptViewController release];
        [ptData->tDevice release];
        free(ptData);
    }

    ptWindow->_pBackendData = NULL;
    free(ptWindow);
}

void
pl_window_show(plWindow* ptWindow)
{
    if(!ptWindow)
        return;
    plWindowData* ptData = (plWindowData*)ptWindow->_pBackendData;
    [ptData->ptNativeWindow orderFront:nil];
}

plWindowResult
pl_window_create_surface(plWindow* ptWindow, const plWindowSurfaceDesc* ptDesc, plWindowSurface** ptSurfaceOut)
{
    plWindowSurface* ptSurface = PL_ALLOC(sizeof(plWindowSurface));
    
    ptSurface->ptWindow = ptWindow;
    ptSurface->atImages = PL_ALLOC(sizeof(plWindowSurfaceImageCocoa) * ptDesc->uImageCount);
    memset(ptSurface->atImages, 0, sizeof(plWindowSurfaceImageCocoa) * ptDesc->uImageCount);
    ptSurface->uImageCount = ptDesc->uImageCount;

    *ptSurfaceOut = ptSurface;
    plWindowData* ptData = ptWindow->_pBackendData;
    ptData->ptCpuView->ptSurface = ptSurface;
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy_surface(plWindowSurface** ptSurfaceIn)
{
    plWindowSurface* ptSurface = *ptSurfaceIn;
    plWindow* ptWindow = ptSurface->ptWindow;
    for(uint32_t i = 0; i < ptSurface->uImageCount; i++)
    {
        // ptSurface->atImages[i].image->data = NULL;
        // XDestroyImage(ptSurface->atImages[i].image);
        PL_FREE(ptSurface->atImages[i].pPixels);
        
    }
    PL_FREE(ptSurface->atImages);
    memset(ptSurface, 0, sizeof(plWindowSurface));
    PL_FREE(*ptSurfaceIn);
    *ptSurfaceIn = NULL;
}

bool
pl_window_acquire_surface_image(plWindowSurface* ptSurface, plWindowSurfaceImage* ptImageOut)
{
    plWindow* ptWindow = ptSurface->ptWindow;
    plWindowData* ptData = ptWindow->_pBackendData;

    plWindowSurfaceImageCocoa* ptImageCocoa = &ptSurface->atImages[ptSurface->uCurrentImage];


    NSRect tBackingBounds = [ptData->ptCpuView convertRectToBacking:[ptData->ptCpuView bounds]];

    uint32_t uWidth  = (uint32_t)tBackingBounds.size.width;
    uint32_t uHeight = (uint32_t)tBackingBounds.size.height;

    uint32_t uPixelsNeeded = uWidth * uHeight;
    bool bResizeNeeded = uPixelsNeeded > ptImageCocoa->uPixelCapacity;

    if(bResizeNeeded)
    {
        if(ptImageCocoa->pPixels)
        {
            PL_FREE(ptImageCocoa->pPixels);
            CGImageRelease(ptImageCocoa->image);
            CGDataProviderRelease(ptImageCocoa->provider);
            CGColorSpaceRelease(ptImageCocoa->colorSpace);
        }
        ptImageCocoa->pPixels = PL_ALLOC(uPixelsNeeded * sizeof(uint32_t));
        memset(ptImageCocoa->pPixels, 0, uPixelsNeeded * sizeof(uint32_t));
        ptImageCocoa->uPixelCapacity = uPixelsNeeded;

        ptImageCocoa->colorSpace = CGColorSpaceCreateDeviceRGB();
        ptImageCocoa->provider = CGDataProviderCreateWithData(NULL, ptImageCocoa->pPixels, uHeight * uWidth * sizeof(uint32_t), NULL);

        ptImageCocoa->image = CGImageCreate(
                (size_t)uWidth,
                (size_t)uHeight,
                8,
                32,
                (size_t)uWidth * sizeof(uint32_t),
                ptImageCocoa->colorSpace,
                kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                ptImageCocoa->provider,
                NULL,
                false,
                kCGRenderingIntentDefault);
    }

    ptImageOut->pPixels = ptImageCocoa->pPixels;
    ptImageOut->uWidth = uWidth;
    ptImageOut->uHeight = uHeight;
    ptImageOut->uRowPitch = ptImageOut->uWidth * sizeof(uint32_t);
    ptImageOut->uImageIndex = ptSurface->uCurrentImage;
    ptImageOut->tFormat = PL_WINDOW_SURFACE_FORMAT_B8G8R8A8_UNORM;

    ptImageCocoa->uWidth = ptImageOut->uWidth;
    ptImageCocoa->uHeight = ptImageOut->uHeight;

    return true;
}

void
pl_window_present_surface_image(plWindowSurface* ptSurface, uint32_t uImageIndex)
{
    plWindow* ptWindow = ptSurface->ptWindow;
    plWindowData* ptData = ptWindow->_pBackendData;
    [ptData->ptCpuView setNeedsDisplay:YES];

    ptSurface->uCurrentImage++;
    ptSurface->uCurrentImage %= ptSurface->uImageCount;
}

bool
pl_window_set_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, const plWindowAttributeValue* ptValue)
{
    (void)ptWindow;
    (void)tAttribute;
    (void)ptValue;
    return false;
}

bool
pl_window_get_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, plWindowAttributeValue* ptValue)
{
    (void)ptWindow;
    (void)tAttribute;
    (void)ptValue;
    return false;
}

bool
pl_window_set_cursor_mode(plWindow* ptWindow, plCursorMode tMode)
{
    (void)ptWindow;
    return tMode == PL_CURSOR_MODE_NORMAL;
}

plCursorMode
pl_window_get_cursor_mode(plWindow* ptWindow)
{
    (void)ptWindow;
    return PL_CURSOR_MODE_NORMAL;
}

bool
pl_window_set_raw_mouse_input(plWindow* ptWindow, bool bValue)
{
    (void)ptWindow;
    return !bValue;
}

bool
pl_window_set_fullscreen(plWindow* ptWindow, const plFullScreenDesc* ptDesc)
{
    (void)ptWindow;
    return ptDesc && ptDesc->tMode == PL_FULLSCREEN_MODE_NONE;
}

const plWindowCapabilities*
pl_window_get_capabilities(void)
{
    static plWindowCapabilities tCapabilities = {0};

    static const plWindowAttribute atSupportedAttributes[] = {
        -1
    };

    static const plCursorMode atSupportedCursorModes[] = {
        PL_CURSOR_MODE_NORMAL
    };

    static const plFullScreenMode atSupportedScreenModes[] = {
        PL_FULLSCREEN_MODE_NONE,
        PL_FULLSCREEN_MODE_EXCLUSIVE
    };

    tCapabilities.uCursorModeCount = 1;
    tCapabilities.uAttributeCount = 0;
    tCapabilities.uFullScreenModeCount = 2;
    tCapabilities.atCursorModes = atSupportedCursorModes;
    tCapabilities.atFullScreenModes = atSupportedScreenModes;
    tCapabilities.atWindowAttributes = atSupportedAttributes;
    tCapabilities.tFlags = PL_WINDOW_CAPABILITY_FLAGS_NONE;

    return &tCapabilities;
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

void
pl_window_set_callback(plWindow* ptWindow, plWindowEventCallback tCallback, void* pUserData)
{
    // TODO: implement
}

plWindowEventCallback
pl_window_get_callback(plWindow* ptWindow)
{
    plWindowEventCallback tCallback = PL_ZERO_INIT;
    return tCallback;
}

//-----------------------------------------------------------------------------
// [SECTION] clipboard
//-----------------------------------------------------------------------------

const char*
pl_get_clipboard_text(void* user_data_ctx)
{
    pl_sb_reset(gptIO->sbcClipboardData);

    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSString* available = [pasteboard availableTypeFromArray: [NSArray arrayWithObject:NSPasteboardTypeString]];
    if (![available isEqualToString:NSPasteboardTypeString])
        return NULL;

    NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
    if (string == nil)
        return NULL;

    const char* string_c = (const char*)[string UTF8String];
    size_t string_len = strlen(string_c);
    pl_sb_resize(gptIO->sbcClipboardData, (int)string_len + 1);
    strcpy(gptIO->sbcClipboardData, string_c);
    return gptIO->sbcClipboardData;
}

void
pl_set_clipboard_text(void* pUnused, const char* text)
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
    [pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

//-----------------------------------------------------------------------------
// [SECTION] file ext
//-----------------------------------------------------------------------------

plFileResult
pl_file_binary_read(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{
    if(pszSizeIn == NULL)
        return PL_FILE_RESULT_FAIL;

    FILE* ptDataFile = fopen(pcFile, "rb");
    size_t uSize = 0u;

    if (ptDataFile == NULL)
    {
        *pszSizeIn = 0u;
        return PL_FILE_RESULT_FAIL;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    
    if(pcBuffer == NULL)
    {
        *pszSizeIn = uSize;
        fclose(ptDataFile);
        return PL_FILE_RESULT_SUCCESS;
    }
    fseek(ptDataFile, 0, SEEK_SET);

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile))
            perror("Error reading test.bin");
        return PL_FILE_RESULT_FAIL;
    }

    fclose(ptDataFile);
    return PL_FILE_RESULT_SUCCESS;
}

plFileResult
pl_file_binary_write(const char* pcFile, size_t szSize, uint8_t* pcBuffer)
{
    FILE* ptDataFile = fopen(pcFile, "wb");
    if (ptDataFile)
    {
        fwrite(pcBuffer, 1, szSize, ptDataFile);
        fclose(ptDataFile);
        return PL_FILE_RESULT_SUCCESS;
    }
    return PL_FILE_RESULT_FAIL;
}

plFileResult
pl_file_copy(const char* source, const char* destination)
{
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(source, destination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
    return PL_FILE_RESULT_SUCCESS;
}

plFileResult
pl_file_remove(const char* pcFile)
{
    int iResult = remove(pcFile);
    if(iResult)
        return PL_FILE_RESULT_FAIL;
    return PL_FILE_RESULT_SUCCESS;
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


bool
pl_file_directory_exists(const char* pcPath)
{
    struct stat st = {0};

    if (stat(pcPath, &st) == -1)
        return false;
    return true;
}

plFileResult
pl_file_create_directory(const char* pcPath)
{
    struct stat st = {0};

    if (stat(pcPath, &st) == -1)
    {
        mkdir(pcPath, 0700);
        return PL_FILE_RESULT_SUCCESS;
    }
    return PL_FILE_DIRECTORY_ALREADY_EXIST;
}

plFileResult
pl_file_remove_directory(const char* pcPath)
{
    if(pl_file_directory_exists(pcPath))
    {
        rmdir(pcPath);
        return PL_FILE_RESULT_SUCCESS;
    }
    return PL_FILE_RESULT_FAIL;
}

void
pl_file_cleanup_directory_info(plDirectoryInfo* ptInfoOut)
{
    pl_sb_free(ptInfoOut->sbtEntries);
    ptInfoOut->uEntryCount = 0;
}

plFileResult
pl_file_get_directory_info(const char* pcPath, plDirectoryInfo* ptInfoOut)
{
    DIR* ptDirectoryPath = opendir(pcPath);
    struct dirent* ptEntry = NULL;

    if (ptDirectoryPath == NULL)
    {
        perror("Error opening directory");
        return PL_FILE_RESULT_FAIL;
    }

    // Read directory entries
    while ((ptEntry = readdir(ptDirectoryPath)) != NULL)
    {
        // Skip "." and ".." entries (current and parent directory)
        if (strcmp(ptEntry->d_name, ".") == 0 || strcmp(ptEntry->d_name, "..") == 0)
        {
            continue;
        }

        pl_sb_add(ptInfoOut->sbtEntries);
        plDirectoryEntry* ptNewEntry = &pl_sb_top(ptInfoOut->sbtEntries);

        switch(ptEntry->d_type)
        {
            case DT_REG: ptInfoOut->uFileCount++; ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_FILE; break;
            case DT_DIR: ptInfoOut->uDirectoryCount++; ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_DIRECTORY; break;
            case DT_LNK: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_LINK; break;
            case DT_FIFO: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_PIPE; break;
            case DT_SOCK: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_SOCKET; break;
            case DT_BLK: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_BLOCK_DEVICE; break;
            case DT_CHR: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_CHARACTER_DEVICE; break;
            case DT_UNKNOWN: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_UNKNOWN; break;
            default:
                PL_ASSERT(false && "unknown dirent file type");
                break;
        }

        strncpy(ptNewEntry->acName, ptEntry->d_name, PL_MAX_PATH_LENGTH);
    }

    if (closedir(ptDirectoryPath) == -1)
    {
        perror("Error closing directory");
        return PL_FILE_RESULT_FAIL;
    }

    ptInfoOut->uEntryCount = pl_sb_size(ptInfoOut->sbtEntries);
    return PL_FILE_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] atomics ext
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounter
{
    atomic_int_fast64_t ilValue;
} plAtomicCounter;

plAtomicsResult
pl_atomics_create_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = PL_ALLOC(sizeof(plAtomicCounter));
    atomic_init(&(*ptCounter)->ilValue, ilValue);
    return PL_ATOMICS_RESULT_SUCCESS;
}

void
pl_atomics_destroy_counter(plAtomicCounter** ptCounter)
{
    PL_FREE((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl_atomics_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    atomic_store(&ptCounter->ilValue, ilValue);
}

int64_t
pl_atomics_load(plAtomicCounter* ptCounter)
{
    return atomic_load(&ptCounter->ilValue);
}

bool
pl_atomics_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return atomic_compare_exchange_strong(&ptCounter->ilValue, &ilExpectedValue, ilDesiredValue);
}

int64_t
pl_atomics_increment(plAtomicCounter* ptCounter)
{
    return atomic_fetch_add(&ptCounter->ilValue, 1);
}

int64_t
pl_atomics_decrement(plAtomicCounter* ptCounter)
{
    return atomic_fetch_sub(&ptCounter->ilValue, 1);
}

//-----------------------------------------------------------------------------
// [SECTION] network ext
//-----------------------------------------------------------------------------

typedef struct _plNetworkAddress
{
    struct addrinfo* tInfo;
} plNetworkAddress;

#define SOCKET int
typedef struct _plSocket
{
    SOCKET        tSocket;
    bool          bInitialized;
    plSocketFlags tFlags;
} plSocket;

bool
pl_network_initialize(void)
{
    return true;
}

void
pl_network_cleanup(void)
{
}

plNetworkResult
pl_network_create_address(const char* pcAddress, const char* pcService, plNetworkAddressFlags tFlags, plNetworkAddress** pptAddress)
{
    
    struct addrinfo tHints;
    memset(&tHints, 0, sizeof(tHints));
    tHints.ai_socktype = SOCK_DGRAM;

    if(tFlags & PL_NETWORK_ADDRESS_FLAGS_TCP)
        tHints.ai_socktype = SOCK_STREAM;

    if(pcAddress == NULL)
        tHints.ai_flags = AI_PASSIVE;

    if(tFlags & PL_NETWORK_ADDRESS_FLAGS_IPV4)
        tHints.ai_family = AF_INET;
    else if(tFlags & PL_NETWORK_ADDRESS_FLAGS_IPV6)
        tHints.ai_family = AF_INET6;

    struct addrinfo* tInfo = NULL;
    if(getaddrinfo(pcAddress, pcService, &tHints, &tInfo))
    {
        printf("Could not create address : %d\n", errno);
        return PL_NETWORK_RESULT_FAIL;
    }

    *pptAddress = PL_ALLOC(sizeof(plNetworkAddress));
    (*pptAddress)->tInfo = tInfo;
    return PL_NETWORK_RESULT_SUCCESS;
}

void
pl_network_destroy_address(plNetworkAddress** pptAddress)
{
    plNetworkAddress* ptAddress = *pptAddress;
    if(ptAddress == NULL)
        return;

    freeaddrinfo(ptAddress->tInfo);
    PL_FREE(ptAddress);
    *pptAddress = NULL;
}

void
pl_network_create_socket(plSocketFlags tFlags, plSocket** pptSocketOut)
{
    *pptSocketOut = PL_ALLOC(sizeof(plSocket));
    plSocket* ptSocket = *pptSocketOut;
    ptSocket->bInitialized = false;
    ptSocket->tFlags = tFlags;
}

void
pl_network_destroy_socket(plSocket** pptSocket)
{
    plSocket* ptSocket = *pptSocket;

    if(ptSocket == NULL)
        return;

    close(ptSocket->tSocket);

    PL_FREE(ptSocket);
    *pptSocket = NULL;
}

plNetworkResult
pl_network_send_socket_data_to(plSocket* ptFromSocket, plNetworkAddress* ptAddress, const void* pData, size_t szSize, size_t* pszSentSize)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptFromSocket->tSocket < 0) // invalid socket
        {
            printf("Could not create socket : %d\n", errno);
            return 0;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            int iFlags = fcntl(ptFromSocket->tSocket, F_GETFL);
            fcntl(ptFromSocket->tSocket, F_SETFL, iFlags | O_NONBLOCK);
        }

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = sendto(ptFromSocket->tSocket, (const char*)pData, (int)szSize, 0, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);

    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_bind_socket(plSocket* ptSocket, plNetworkAddress* ptAddress)
{
    if(!ptSocket->bInitialized)
    {
        
        ptSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptSocket->tSocket < 0)
        {
            printf("Could not create socket : %d\n", errno);
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            int iFlags = fcntl(ptSocket->tSocket, F_GETFL);
            fcntl(ptSocket->tSocket, F_SETFL, iFlags | O_NONBLOCK);
        }

        ptSocket->bInitialized = true;
    }

    // bind socket
    if(bind(ptSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen))
    {
        printf("Bind socket failed with error code : %d\n", errno);
        return PL_NETWORK_RESULT_FAIL;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_get_socket_data_from(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize, plSocketReceiverInfo* ptReceiverInfo)
{
    struct sockaddr_storage tClientAddress = {0};
    socklen_t tClientLen = sizeof(tClientAddress);

    int iRecvLen = recvfrom(ptSocket->tSocket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tClientAddress, &tClientLen);
   

    if(iRecvLen == -1)
    {
        if(errno != EWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", errno);
            return PL_NETWORK_RESULT_FAIL;
        }
    }

    if(iRecvLen > 0)
    {
        if(ptReceiverInfo)
        {
            getnameinfo((struct sockaddr*)&tClientAddress, tClientLen,
                ptReceiverInfo->acAddressBuffer, 100,
                ptReceiverInfo->acServiceBuffer, 100,
                NI_NUMERICHOST | NI_NUMERICSERV);
        }
        if(pszRecievedSize)
            *pszRecievedSize = (size_t)iRecvLen;
    }

    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_connect_socket(plSocket* ptFromSocket, plNetworkAddress* ptAddress)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptFromSocket->tSocket < 0)
        {
            printf("Could not create socket : %d\n", errno);
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            int iFlags = fcntl(ptFromSocket->tSocket, F_GETFL);
            fcntl(ptFromSocket->tSocket, F_SETFL, iFlags | O_NONBLOCK);
        }

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = connect(ptFromSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);
    if(iResult)
    {
        printf("connect() failed with error code : %d\n", errno);
        return PL_NETWORK_RESULT_FAIL;
    }

    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_get_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize)
{
    int iBytesReceived = recv(ptSocket->tSocket, (char*)pData, (int)szSize, 0);
    if(iBytesReceived < 1)
    {
        return PL_NETWORK_RESULT_FAIL; // connection closed by peer
    }
    if(pszRecievedSize)
        *pszRecievedSize = (size_t)iBytesReceived;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_select_sockets(plSocket** ptSockets, bool* abSelectedSockets, uint32_t uSocketCount, uint32_t uTimeOutMilliSec)
{
    SOCKET tMaxSocket = 0;
    fd_set tReads;
    FD_ZERO(&tReads);
    for(uint32_t i = 0; i < uSocketCount; i++)
    {
        FD_SET(ptSockets[i]->tSocket, &tReads);
        if(ptSockets[i]->tSocket > tMaxSocket)
            tMaxSocket = ptSockets[i]->tSocket;
    }

    struct timeval tTimeout = {0};
    tTimeout.tv_sec = 0;
    tTimeout.tv_usec = (int)uTimeOutMilliSec * 1000;

    if(select(tMaxSocket + 1, &tReads, NULL, NULL, &tTimeout) < 0)
    {
        printf("select socket failed with error code : %d\n", errno);
        return PL_NETWORK_RESULT_FAIL;
    }

    for(uint32_t i = 0; i < uSocketCount; i++)
    {
        if(FD_ISSET(ptSockets[i]->tSocket, &tReads))
            abSelectedSockets[i] = true;
        else
            abSelectedSockets[i] = false;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_accept_socket(plSocket* ptSocket, plSocket** pptSocketOut)
{
    *pptSocketOut = NULL; 
    struct sockaddr_storage tClientAddress = {0};
    socklen_t tClientLen = sizeof(tClientAddress);
    SOCKET tSocketClient = accept(ptSocket->tSocket, (struct sockaddr*)&tClientAddress, &tClientLen);

    if(tSocketClient < 1)
        return PL_NETWORK_RESULT_FAIL;

    *pptSocketOut = PL_ALLOC(sizeof(plSocket));
    plSocket* ptNewSocket = *pptSocketOut;
    ptNewSocket->bInitialized = true;
    ptNewSocket->tFlags = ptSocket->tFlags;
    ptNewSocket->tSocket = tSocketClient;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_listen_socket(plSocket* ptSocket)
{
    if(listen(ptSocket->tSocket, 10) < 0)
    {
        return PL_NETWORK_RESULT_FAIL;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_send_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszSentSize)
{
    int iResult = send(ptSocket->tSocket, (char*)pData, (int)szSize, 0);
    if(iResult == -1)
        return PL_NETWORK_RESULT_FAIL;
    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] thread ext
//-----------------------------------------------------------------------------

typedef int pthread_barrierattr_t;
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;

typedef struct _plThread
{
    pthread_t tHandle;
    uint64_t  uID;
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

uint64_t
pl_threads_get_thread_id(plThread* ptThread)
{
    return ptThread->uID;
}

uint64_t
pl_threads_get_current_thread_id(void)
{
    pthread_t tId = pthread_self();

    const uint32_t uThreadCount = pl_sb_size(gsbtThreads);
    for(uint32_t i = 0; i < uThreadCount; i++)
    {
        if(pthread_equal(tId, gsbtThreads[i]->tHandle))
        {
            return gsbtThreads[i]->uID;
        }
    }

    return UINT64_MAX;
}

void
pl_threads_sleep_thread(uint32_t millisec)
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

plThreadResult
pl_threads_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** pptThreadOut)
{
    *pptThreadOut = PL_ALLOC(sizeof(plThread));
    if(pthread_create(&(*pptThreadOut)->tHandle, NULL, ptProcedure, pData))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    static uint32_t uNextThreadId = 1;
    uNextThreadId++;
    (*pptThreadOut)->uID = uNextThreadId;
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_join_thread(plThread* ptThread)
{
    pthread_join(ptThread->tHandle, NULL);
}

void
pl_threads_destroy_thread(plThread** ppThread)
{
    pl_threads_join_thread(*ppThread);

    const uint32_t uThreadCount = pl_sb_size(gsbtThreads);
    for(uint32_t i = 0; i < uThreadCount; i++)
    {
        if(gsbtThreads[i] == (*ppThread))
        {
            pl_sb_del_swap(gsbtThreads, i);
            break;
        }
    }

    PL_FREE(*ppThread);
    *ppThread = NULL;
}

void
pl_threads_yield_thread(void)
{
    sched_yield();
}

plThreadResult
pl_threads_create_mutex(plMutex** pptMutexOut)
{
    *pptMutexOut = malloc(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_lock_mutex(plMutex* ptMutex)
{
    pthread_mutex_lock(&ptMutex->tHandle);
}

void
pl_threads_unlock_mutex(plMutex* ptMutex)
{
    pthread_mutex_unlock(&ptMutex->tHandle);
}

void
pl_threads_destroy_mutex(plMutex** pptMutex)
{
    pthread_mutex_destroy(&(*pptMutex)->tHandle);
    free(*pptMutex);
    *pptMutex = NULL;
}

plThreadResult
pl_threads_create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    *pptCriticalSectionOut = PL_ALLOC(sizeof(plCriticalSection));
    if(pthread_mutex_init(&(*pptCriticalSectionOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    pthread_mutex_destroy(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    *pptCriticalSection = NULL;
}

void
pl_threads_enter_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_lock(&ptCriticalSection->tHandle);
}

void
pl_threads_leave_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_unlock(&ptCriticalSection->tHandle);
}

uint32_t
pl_threads_get_hardware_thread_count(void)
{

    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)numCPU;
}

plThreadResult
pl_threads_create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    *pptSemaphoreOut = PL_ALLOC(sizeof(plSemaphore));
    memset((*pptSemaphoreOut), 0, sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = dispatch_semaphore_create(uIntialCount);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_semaphore(plSemaphore** pptSemaphore)
{
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl_threads_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_FOREVER);
}

bool
pl_threads_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    return dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_NOW) == 0;
}

void
pl_threads_release_semaphore(plSemaphore* ptSemaphore)
{
    dispatch_semaphore_signal(ptSemaphore->tHandle);
}

plThreadResult
pl_threads_allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    *pptKeyOut = PL_ALLOC(sizeof(plThreadKey));
    int iStatus = pthread_key_create(&(*pptKeyOut)->tKey, NULL);
    if(iStatus != 0)
    {
        printf("pthread_key_create failed, errno=%d", errno);
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_free_thread_local_key(plThreadKey** pptKey)
{
    pthread_key_delete((*pptKey)->tKey);
    PL_FREE((*pptKey));
    *pptKey = NULL;
}

void*
pl_threads_allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    void* pData = PL_ALLOC(szSize);
    memset(pData, 0, szSize);
    pthread_setspecific(ptKey->tKey, pData);
    return pData;
}

void*
pl_threads_get_thread_local_data(plThreadKey* ptKey)
{
    void* pData = pthread_getspecific(ptKey->tKey);
    return pData;
}

void
pl_threads_free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    PL_FREE(pData);
}

plThreadResult
pl_threads_create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    *pptBarrierOut = PL_ALLOC(sizeof(plBarrier));
    memset((*pptBarrierOut), 0, sizeof(plBarrier));
    pthread_barrier_init(&(*pptBarrierOut)->tHandle, NULL, uThreadCount);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_barrier(plBarrier** pptBarrier)
{
    pthread_barrier_destroy(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
}

void
pl_threads_wait_on_barrier(plBarrier* ptBarrier)
{
    pthread_barrier_wait(&ptBarrier->tHandle);
}

plThreadResult
pl_threads_create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    *pptConditionVariableOut = PL_ALLOC(sizeof(plConditionVariable));
    memset((*pptConditionVariableOut), 0, sizeof(plConditionVariable));
    pthread_cond_init(&(*pptConditionVariableOut)->tHandle, NULL);
    return PL_THREAD_RESULT_SUCCESS;
}

void               
pl_threads_destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    pthread_cond_destroy(&(*pptConditionVariable)->tHandle);
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
}

void               
pl_threads_wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_signal(&ptConditionVariable->tHandle);
}

void               
pl_threads_wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    if(ptConditionVariable)
        pthread_cond_broadcast(&ptConditionVariable->tHandle);
}

void               
pl_threads_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    pthread_cond_wait(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle);
}

int
pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count)
{
    if(count == 0)
    {
        errno = EINVAL;
        return PL_THREAD_RESULT_FAIL;
    }
    if(pthread_mutex_init(&barrier->mutex, 0) < 0)
    {
        return PL_THREAD_RESULT_FAIL;
    }
    if(pthread_cond_init(&barrier->cond, 0) < 0)
    {
        pthread_mutex_destroy(&barrier->mutex);
        return PL_THREAD_RESULT_FAIL;
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
// [SECTION] virtual memory ext
//-----------------------------------------------------------------------------

size_t
pl_virtual_memory_get_page_size(void)
{
    return (size_t)getpagesize();
}

void*
pl_virtual_memory_alloc(size_t szSize)
{
    void* pResult = mmap(NULL, szSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pResult == MAP_FAILED ? NULL : pResult;
}

void*
pl_virtual_memory_reserve(size_t szSize)
{
    void* pResult = mmap(NULL, szSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pResult == MAP_FAILED ? NULL : pResult;
}

void*
pl_virtual_memory_commit(void* pAddress, size_t szSize)
{
    if(mprotect(pAddress, szSize, PROT_READ | PROT_WRITE) != 0)
    {
        PL_ASSERT(false);
        return NULL;
    }

    return pAddress;
}

void
pl_virtual_memory_free(void* pAddress, size_t szSize)
{
    if(munmap(pAddress, szSize) != 0)
    {
        PL_ASSERT(false);
    }
}

void
pl_virtual_memory_decommit(void* pAddress, size_t szSize)
{
    int iResult = madvise(pAddress, szSize, MADV_FREE);
    PL_ASSERT(iResult == 0);

    iResult = mprotect(pAddress, szSize, PROT_NONE);
    PL_ASSERT(iResult == 0);
}

#include "pl_platform_ext.c"