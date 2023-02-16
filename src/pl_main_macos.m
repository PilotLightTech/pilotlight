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

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <time.h>
#include "pl_os.h"
#include "pl_io.h"
#include "pl_macos.h"

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

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plKey                 pl__osx_key_to_pl_key(int iKey);
static void                  pl__add_osx_tracking_area(NSView* _Nonnull view);
static bool                  pl__handle_osx_event(NSEvent* event, NSView* view);
static inline CFTimeInterval pl__get_absolute_time(void) { return (CFTimeInterval)((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9); }

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static NSWindow*            gWindow = NULL;
static NSViewController*    gViewController = NULL;
static plSharedLibrary      gAppLibrary = {0};
static void*                gUserData = NULL;
static bool                 gRunning = true;
plIOContext                 gtIOContext = {0};
static plKeyEventResponder* gKeyEventResponder = NULL;
static NSTextInputContext*  gInputContext = NULL;
static id                   gMonitor;

typedef struct _plAppData plAppData;
static void* (*pl_app_load)    (plIOContext* ptIOCtx, plAppData* ptAppData);
static void  (*pl_app_shutdown)(plAppData* ptAppData);
static void  (*pl_app_resize)  (plAppData* ptAppData);
static void  (*pl_app_update)  (plAppData* ptAppData);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{
    // setup io context
    pl_initialize_io_context(&gtIOContext);
    plIOContext* ptIOCtx = pl_get_io_context();

    // create view controller
    gViewController = [[plNSViewController alloc] init];

    // create window
    gWindow = [NSWindow windowWithContentViewController:gViewController];
    [gWindow orderFront:nil];
    [gWindow center];
    [gWindow becomeKeyWindow];

    gKeyEventResponder = [[plKeyEventResponder alloc] initWithFrame:NSZeroRect];
    gInputContext = [[NSTextInputContext alloc] initWithClient:gKeyEventResponder];

    // create app delegate
    id appDelegate = [[plNSAppDelegate alloc] init];
    gWindow.delegate = appDelegate;
    NSApplication.sharedApplication.delegate = appDelegate;

    pl_init_macos();

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
    [gWindow.delegate release];
    [gWindow release];
    [_delegate shutdown];
    [super dealloc];
}

- (void)resizeDrawable:(CGFloat)scaleFactor
{
    CGSize newSize = self.bounds.size;
    newSize.width *= scaleFactor;
    newSize.height *= scaleFactor;

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
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
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
    [self resizeDrawable:self.window.screen.backingScaleFactor];
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
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setBoundsSize:(NSSize)size
{
    [super setBoundsSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

@end

//-----------------------------------------------------------------------------
// [SECTION] plNSViewController
//-----------------------------------------------------------------------------

@implementation plNSViewController

- (void)loadView
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    NSRect frame = NSMakeRect(0, 0, 500, 500);
    self.view = [[plNSView alloc] initWithFrame:frame];

    plNSView *view = (plNSView *)self.view;
    view.metalLayer.device = device;    
    view.delegate = self;
    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    gtIOContext.pBackendPlatformData = device;

    #ifdef PL_VULKAN_BACKEND
        plIOContext* ptIOCtx = pl_get_io_context();
        ptIOCtx->pBackendPlatformData = view.metalLayer;
    #endif
    gtIOContext.afMainViewportSize[0] = 500;
    gtIOContext.afMainViewportSize[1] = 500;

    // load library
    if(pl_load_library(&gAppLibrary, "app.so", "app_", "lock.tmp"))
    {
        pl_app_load     = (void* (__attribute__(())  *)(plIOContext*, plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_update");
        gUserData = pl_app_load(&gtIOContext, NULL);
    }
}

- (void)drawableResize:(CGSize)size
{
    gtIOContext.afMainViewportSize[0] = size.width;
    gtIOContext.afMainViewportSize[1] = size.height;
    pl_app_resize(gUserData);
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)layer
{
    // gAppData.graphics.metalLayer = layer;
    gtIOContext.pBackendRendererData = layer;

    gtIOContext.afMainFramebufferScale[0] = self.view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    gtIOContext.afMainFramebufferScale[1] = gtIOContext.afMainFramebufferScale[0];

    // not osx
    // CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;

    pl_update_mouse_cursor_macos();

    // reload library
    if(pl_has_library_changed(&gAppLibrary))
    {
        pl_reload_library(&gAppLibrary);
        pl_app_load     = (void* (__attribute__(())  *)(plIOContext*, plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_update");
        gUserData = pl_app_load(&gtIOContext, gUserData);
    }

    pl_new_frame_macos();
    pl_app_update(gUserData);
}

- (void)shutdown
{
    pl_app_shutdown(gUserData);
    if(gMonitor != NULL)
    {
        [NSEvent removeMonitor:gMonitor];
        gMonitor = NULL;
    }
}

@end

@implementation plKeyEventResponder
{
    float _posX;
    float _posY;
    NSRect _imeRect;
}
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
    return pl_macos_procedure(event, view);
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