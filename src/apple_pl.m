/*
   apple_pl.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] entry point
// [SECTION] plNSView
// [SECTION] plNSViewController
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include "metal_pl.h"
#include "pl_os.h"

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

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static NSWindow*         gWindow = NULL;
static NSViewController* gViewController = NULL;
static plSharedLibrary   gSharedLibrary = {0};
static void*             gUserData = NULL;
static plAppData         gAppData = { .running = true, .clientWidth = 500, .clientHeight = 500};

typedef struct plUserData_t plUserData;
static void* (*pl_app_load)(plAppData* appData, plUserData* userData);
static void  (*pl_app_setup)(plAppData* appData, plUserData* userData);
static void  (*pl_app_shutdown)(plAppData* appData, plUserData* userData);
static void  (*pl_app_resize)(plAppData* appData, plUserData* userData);
static void  (*pl_app_render)(plAppData* appData, plUserData* userData);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{
    // load library
    if(pl_load_library(&gSharedLibrary, "app.so", "app_", "lock.tmp"))
    {
        pl_app_load = (void* (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_load");
        pl_app_setup = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_setup");
        pl_app_shutdown = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_shutdown");
        pl_app_resize = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_resize");
        pl_app_render = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_render");
        gUserData = pl_app_load(&gAppData, NULL);
    }

    // create view controller
    gViewController = [[plNSViewController alloc] init];

    // create window
    gWindow = [NSWindow windowWithContentViewController:gViewController];
    [gWindow orderFront:nil];
    [gWindow center];
    [gWindow becomeKeyWindow];

    // create app delegate
    id appDelegate = [[plNSAppDelegate alloc] init];
    gWindow.delegate = appDelegate;
    NSApplication.sharedApplication.delegate = appDelegate;

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

    pl_app_shutdown(&gAppData, gUserData);
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
    gAppData.device.device = MTLCreateSystemDefaultDevice();

    NSRect frame = NSMakeRect(0, 0, 500, 500);
    self.view = [[plNSView alloc] initWithFrame:frame];

    plNSView *view = (plNSView *)self.view;
    view.metalLayer.device = gAppData.device.device;    
    view.delegate = self;
    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    pl_app_setup(&gAppData, gUserData);
}

- (void)drawableResize:(CGSize)size
{
    gAppData.clientWidth = size.width;
    gAppData.clientHeight = size.height;
    pl_app_resize(&gAppData, gUserData);
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)layer
{
    gAppData.graphics.metalLayer = layer;

    // reload library
    if(pl_has_library_changed(&gSharedLibrary))
    {
        pl_reload_library(&gSharedLibrary);
        pl_app_load = (void* (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_load");
        pl_app_setup = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_setup");
        pl_app_shutdown = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_shutdown");
        pl_app_resize = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_resize");
        pl_app_render = (void (__attribute__(()) *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_render");
        gUserData = pl_app_load(&gAppData, gUserData);
    }
    pl_app_render(&gAppData, gUserData);
}

- (void)shutdown
{
    pl_app_shutdown(&gAppData, gUserData);
}

@end

// #include "apple_pl_os.m"