/*
   pl_platform_x11_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] timer api
// [SECTION] window api
// [SECTION] file api
// [SECTION] atomics api
// [SECTION] network api
// [SECTION] thread api
// [SECTION] virtual memory api
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"

// extensions
#include "pl_platform_ext.h"

// linux stuff
#include <time.h>     // clock_gettime, clock_getres
#include <sys/stat.h> // stat, timespec
#include <sys/types.h>
#include <fcntl.h>    // O_RDONLY, O_WRONLY ,O_CREAT
#include <pthread.h>
#include <unistd.h> // rmdir
#include <sys/sendfile.h> // sendfile
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h> // virtual memory
#include <dirent.h> // directory operations
#include <xcb/xfixes.h> //xcb_xfixes_query_version, apt install libxcb-xfixes0-dev
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h> // apt install libxcb-cursor-dev, libxcb-cursor0
#include <xcb/xcb_keysyms.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h> // XDestroyImage
#include <poll.h>

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

plThread** gsbtThreads;

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
    plWindow* ptMainWindow;
    plWindow** sbtWindows;

    // x11 & xcb stuff
    Display*              ptDisplay;
    xcb_connection_t*     ptConnection;
    xcb_key_symbols_t*    ptKeySyms;
    xcb_screen_t*         ptScreen;
    int                   screen;
    xcb_cursor_context_t* ptCursorContext;
    xcb_atom_t            tWmProtocols;
    xcb_atom_t            tWmDeleteWin;

    // clipboard stuff
    xcb_atom_t tClipboard;
    xcb_atom_t tClipboardProperty;
    xcb_atom_t tTargets;
    xcb_atom_t tUtf8String;
    xcb_atom_t tText;
    xcb_atom_t tIncr;

    char* sbcOwnedClipboardData;

    // timer stuff
    double dFrequency;
    double dStartTime;
} plPlatformExtData;

typedef struct _plWindowSurfaceImageX11
{
    XImage* image;
    uint32_t uWidth;
    uint32_t uHeight;
    void*    pPixels;
    uint32_t uPixelCapacity;
} plWindowSurfaceImageX11;

typedef struct _plWindowSurface
{
    plWindow* ptWindow;
    uint32_t  uCurrentImage;
    uint32_t  uImageCount;
    plWindowSurfaceImageX11* atImages;
    Visual* visual;
    int depth;
} plWindowSurface;

static plPlatformExtData* gptPlatformExtCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] timer api
//-----------------------------------------------------------------------------

double
pl_timer_get_time(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        PL_ASSERT(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return ((double)nsec_count / gptPlatformExtCtx->dFrequency) - gptPlatformExtCtx->dStartTime;
}

double
pl_timer_get_raw_time(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        PL_ASSERT(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / gptPlatformExtCtx->dFrequency;
}

//-----------------------------------------------------------------------------
// [SECTION] window api
//-----------------------------------------------------------------------------

typedef struct _plWindowData
{
    uint32_t          header;
    xcb_connection_t* ptConnection;
    xcb_window_t      tWindow;
    GC                gc;
} plWindowData;

void pl__linux_procedure(xcb_generic_event_t* event);
void pl__update_mouse_cursor(void);
static void pl__linux_handle_selection_request(const xcb_selection_request_event_t* ptRequest);

void*
pl_platform_setup(void)
{
    return NULL;
}

void
pl_platform_new_frame(void* pPlatformData)
{
    
    // Poll for events until null is returned.
    if(gptPlatformExtCtx->ptConnection)
    {
        xcb_generic_event_t* event;
        while (event = xcb_poll_for_event(gptPlatformExtCtx->ptConnection)) 
            pl__linux_procedure(event);

        pl__update_mouse_cursor();
    }

    if(gptIO->bViewportSizeChanged && gptIO->pl_app_resize && gptIO->_bFirstLoadComplete)
    {
        gptIO->pl_app_resize(gptPlatformExtCtx->ptMainWindow, gptIO->pAppUserData);
    }
}

void
pl_platform_cleanup(void* ptPlatformData)
{
    pl_sb_free(gptPlatformExtCtx->sbtWindows);
    pl_sb_free(gptPlatformExtCtx->sbcOwnedClipboardData);
    pl_sb_free(gsbtThreads);

    // platform cleanup
    if(gptPlatformExtCtx->ptCursorContext)
        xcb_cursor_context_free(gptPlatformExtCtx->ptCursorContext);
    if(gptPlatformExtCtx->ptKeySyms)
        xcb_key_symbols_free(gptPlatformExtCtx->ptKeySyms);
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

plWindowResult
pl_window_create(plWindowDesc tDesc, plWindow** pptWindowOut)
{
    plWindowData* ptData = PL_ALLOC(sizeof(plWindowData));

    ptData->header = 0; //-V522
    ptData->tWindow = xcb_generate_id(gptPlatformExtCtx->ptConnection); //-V522
    ptData->ptConnection = gptPlatformExtCtx->ptConnection; //-V522
    ptData->gc = XCreateGC(gptPlatformExtCtx->ptDisplay, ptData->tWindow, 0, NULL);

    // register event types.
    // XCB_CW_BACK_PIXEL = filling then window bg with a single colour
    // XCB_CW_EVENT_MASK is required.
    unsigned int event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // listen for keyboard and mouse buttons
    unsigned int  event_values = 
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // values to be sent over XCB (bg colour, events)
    unsigned int  value_list[] = {gptPlatformExtCtx->ptScreen->black_pixel, event_values};

    // Create the window
    xcb_create_window(
        gptPlatformExtCtx->ptConnection,
        XCB_COPY_FROM_PARENT,  // depth
        ptData->tWindow,
        gptPlatformExtCtx->ptScreen->root, // parent
        tDesc.iXPos,
        tDesc.iYPos,
        tDesc.uWidth,
        tDesc.uHeight,
        0, // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
        gptPlatformExtCtx->ptScreen->root_visual,
        event_mask,
        value_list);

    // Change the title
    xcb_change_property(
        gptPlatformExtCtx->ptConnection,
        XCB_PROP_MODE_REPLACE,
        ptData->tWindow,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,  // data should be viewed 8 bits at a time
        strlen(tDesc.pcTitle),
        tDesc.pcTitle);

    // Tell the server to notify when the window manager
    // attempts to destroy the window.
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        gptPlatformExtCtx->ptConnection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        gptPlatformExtCtx->ptConnection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        gptPlatformExtCtx->ptConnection,
        wm_delete_cookie,
        NULL);
    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        gptPlatformExtCtx->ptConnection,
        wm_protocols_cookie,
        NULL);
    gptPlatformExtCtx->tWmDeleteWin = wm_delete_reply->atom;
    gptPlatformExtCtx->tWmProtocols = wm_protocols_reply->atom;

    xcb_change_property(
        gptPlatformExtCtx->ptConnection,
        XCB_PROP_MODE_REPLACE,
        ptData->tWindow,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(gptPlatformExtCtx->ptConnection, ptData->tWindow);

    int stream_result = xcb_flush(gptPlatformExtCtx->ptConnection);

    plWindow* ptWindow = PL_ALLOC(sizeof(plWindow));
    ptWindow->_pBackendData = ptData; //-V522
    pl_sb_push(gptPlatformExtCtx->sbtWindows, ptWindow);
    *pptWindowOut = ptWindow;

    if(gptPlatformExtCtx->ptMainWindow == NULL)
        gptPlatformExtCtx->ptMainWindow = ptWindow;
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy(plWindow* ptWindow)
{
    plWindowData* ptData = ptWindow->_pBackendData;
    xcb_destroy_window(gptPlatformExtCtx->ptConnection, ptData->tWindow);
    PL_FREE(ptData);
    PL_FREE(ptWindow);
}

void
pl_window_show(plWindow* ptWindow)
{
    
}

plWindowResult
pl_window_create_surface(plWindow* ptWindow, const plWindowSurfaceDesc* ptDesc, plWindowSurface** ptSurfaceOut)
{
    plWindowSurface* ptSurface = PL_ALLOC(sizeof(plWindowSurface));
    
    ptSurface->ptWindow = ptWindow;
    ptSurface->atImages = PL_ALLOC(sizeof(plWindowSurfaceImageX11) * ptDesc->uImageCount);
    memset(ptSurface->atImages, 0, sizeof(plWindowSurfaceImageX11) * ptDesc->uImageCount);
    ptSurface->uImageCount = ptDesc->uImageCount;
    ptSurface->visual = DefaultVisual(gptPlatformExtCtx->ptDisplay, gptPlatformExtCtx->screen);
    ptSurface->depth = DefaultDepth(gptPlatformExtCtx->ptDisplay, gptPlatformExtCtx->screen);

    *ptSurfaceOut = ptSurface;
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy_surface(plWindowSurface** ptSurfaceIn)
{
    plWindowSurface* ptSurface = *ptSurfaceIn;
    plWindow* ptWindow = ptSurface->ptWindow;
    for(uint32_t i = 0; i < ptSurface->uImageCount; i++)
    {
        ptSurface->atImages[i].image->data = NULL;
        XDestroyImage(ptSurface->atImages[i].image);
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

    plWindowSurfaceImageX11* ptImageX11 = &ptSurface->atImages[ptSurface->uCurrentImage];

    XWindowAttributes tAttributes = {0};

    uint32_t uWidth  = 0;
    uint32_t uHeight = 0;

    if(XGetWindowAttributes(gptPlatformExtCtx->ptDisplay, ptData->tWindow, &tAttributes))
    {
        uWidth  = (uint32_t)tAttributes.width;
        uHeight = (uint32_t)tAttributes.height;
    }

    uint32_t uPixelsNeeded = uWidth * uHeight;
    bool bResizeNeeded = uPixelsNeeded > ptImageX11->uPixelCapacity;

    if(bResizeNeeded)
    {
        if(ptImageX11->pPixels)
        {
            ptImageX11->image->data = NULL;
            XDestroyImage(ptImageX11->image);
            PL_FREE(ptImageX11->pPixels);
        }
        ptImageX11->pPixels = PL_ALLOC(uPixelsNeeded * sizeof(uint32_t));
        memset(ptImageX11->pPixels, 0, uPixelsNeeded * sizeof(uint32_t));
        ptImageX11->uPixelCapacity = uPixelsNeeded;

        ptImageX11->image = XCreateImage(
            gptPlatformExtCtx->ptDisplay,
            ptSurface->visual,
            (unsigned int)ptSurface->depth,
            ZPixmap,
            0,
            (char*)ptImageX11->pPixels,
            (unsigned int)uWidth,
            (unsigned int)uHeight,
            32,
            uWidth * sizeof(uint32_t));
    }

    ptImageOut->pPixels = ptImageX11->pPixels;
    ptImageOut->uWidth = uWidth;
    ptImageOut->uHeight = uHeight;
    ptImageOut->uRowPitch = ptImageOut->uWidth * sizeof(uint32_t);
    ptImageOut->uImageIndex = ptSurface->uCurrentImage;
    ptImageOut->tFormat = PL_WINDOW_SURFACE_FORMAT_B8G8R8A8_UNORM;

    ptImageX11->uWidth = ptImageOut->uWidth;
    ptImageX11->uHeight = ptImageOut->uHeight;

    return true;
}

void
pl_window_present_surface_image(plWindowSurface* ptSurface, uint32_t uImageIndex)
{
    plWindow* ptWindow = ptSurface->ptWindow;
    plWindowData* ptData = ptWindow->_pBackendData;
    plWindowSurfaceImageX11* ptImageX11 = &ptSurface->atImages[ptSurface->uCurrentImage];

    XPutImage(
        gptPlatformExtCtx->ptDisplay,
        ptData->tWindow,
        ptData->gc,
        ptImageX11->image,
        0,
        0,
        0,
        0,
        (unsigned int)ptImageX11->uWidth,
        (unsigned int)ptImageX11->uHeight);
    ptSurface->uCurrentImage++;
    ptSurface->uCurrentImage %= ptSurface->uImageCount;
}

bool 
pl_window_set_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, const plWindowAttributeValue* ptValue)
{
    return false;
}

bool
pl_window_get_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, plWindowAttributeValue* ptValue)
{
    return false;
}

bool
pl_window_set_cursor_mode(plWindow* ptWindow, plCursorMode tMode)
{
    return tMode == PL_CURSOR_MODE_NORMAL;
}

plCursorMode
pl_window_get_cursor_mode(plWindow* ptWindow)
{
    return PL_CURSOR_MODE_NORMAL;
}

bool
pl_window_set_raw_mouse_input(plWindow* ptWindow, bool bValue)
{
    return !bValue;
}

bool
pl_window_set_fullscreen(plWindow* ptWindow, const plFullScreenDesc* tDesc)
{
    return tDesc->tMode == PL_FULLSCREEN_MODE_NONE;
}

const plWindowCapabilities*
pl_window_get_capabilities(void)
{
    static plWindowCapabilities tCapabilities = {};

    tCapabilities.uCursorModeCount = 1;
    tCapabilities.uAttributeCount = 1;
    tCapabilities.uFullScreenModeCount = 2;

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

    tCapabilities.atCursorModes = atSupportedCursorModes;
    tCapabilities.atFullScreenModes = atSupportedScreenModes;
    tCapabilities.atWindowAttributes = atSupportedAttributes;
    tCapabilities.tFlags = PL_WINDOW_CAPABILITY_FLAGS_NONE;

    return &tCapabilities;
}

void
pl__update_mouse_cursor(void)
{
    const plMouseCursor tRequestedCursor = gptIO->tNextCursor;

    if(tRequestedCursor != gptIO->tCurrentCursor)
    {
        const char* pcX11Cursor = "left_ptr";

        switch(tRequestedCursor)
        {
            case PL_MOUSE_CURSOR_ARROW:       pcX11Cursor = "left_ptr";             break;
            case PL_MOUSE_CURSOR_TEXT_INPUT:  pcX11Cursor = "xterm";                break;
            case PL_MOUSE_CURSOR_RESIZE_ALL:  pcX11Cursor = "fleur";                break;
            case PL_MOUSE_CURSOR_RESIZE_EW:   pcX11Cursor = "sb_h_double_arrow";    break;
            case PL_MOUSE_CURSOR_RESIZE_NS:   pcX11Cursor = "sb_v_double_arrow";    break;
            case PL_MOUSE_CURSOR_RESIZE_NESW: pcX11Cursor = "bottom_left_corner";   break;
            case PL_MOUSE_CURSOR_RESIZE_NWSE: pcX11Cursor = "bottom_right_corner";  break;
            case PL_MOUSE_CURSOR_HAND:        pcX11Cursor = "hand1";                break;
            case PL_MOUSE_CURSOR_NOT_ALLOWED: pcX11Cursor = "circle";               break;
            default:                          pcX11Cursor = "left_ptr";              break;
        }

        xcb_cursor_t tCursor = xcb_cursor_load_cursor(gptPlatformExtCtx->ptCursorContext, pcX11Cursor);

        if(tCursor != XCB_NONE)
        {
            const uint32_t uValue = tCursor;

            // Apply to every engine window, not only gsbtWindows[0].
            const uint32_t uWindowCount = pl_sb_size(gptPlatformExtCtx->sbtWindows);
            for(uint32_t i = 0; i < uWindowCount; i++)
            {
                plWindowData* ptData = gptPlatformExtCtx->sbtWindows[i]->_pBackendData;

                xcb_change_window_attributes(gptPlatformExtCtx->ptConnection, ptData->tWindow, XCB_CW_CURSOR, &uValue);
            }

            gptIO->tCurrentCursor = tRequestedCursor;

            // Safe after the change-window-attributes requests have been queued.
            xcb_free_cursor(gptPlatformExtCtx->ptConnection, tCursor);
            xcb_flush(gptPlatformExtCtx->ptConnection);
        }
    }

    // Default request for the next application frame.
    gptIO->tNextCursor = PL_MOUSE_CURSOR_ARROW;
}

plKey
pl__xcb_key_to_pl_key(uint32_t x_keycode)
{
    switch (x_keycode) 
    {
        case XKB_KEY_BackSpace:   return PL_KEY_BACKSPACE;
        case XKB_KEY_Return:      return PL_KEY_ENTER;
        case XKB_KEY_Tab:         return PL_KEY_TAB;
        case XKB_KEY_Pause:       return PL_KEY_PAUSE;
        case XKB_KEY_Caps_Lock:   return PL_KEY_CAPS_LOCK;
        case XKB_KEY_Escape:      return PL_KEY_ESCAPE;
        case XKB_KEY_space:       return PL_KEY_SPACE;
        case XKB_KEY_Prior:       return PL_KEY_PAGE_UP;
        case XKB_KEY_Next:        return PL_KEY_PAGE_DOWN;
        case XKB_KEY_End:         return PL_KEY_END;
        case XKB_KEY_Home:        return PL_KEY_HOME;
        case XKB_KEY_Left:        return PL_KEY_LEFT_ARROW;
        case XKB_KEY_Up:          return PL_KEY_UP_ARROW;
        case XKB_KEY_Right:       return PL_KEY_RIGHT_ARROW;
        case XKB_KEY_Down:        return PL_KEY_DOWN_ARROW;
        case XKB_KEY_Print:       return PL_KEY_PRINT_SCREEN;
        case XKB_KEY_Insert:      return PL_KEY_INSERT;
        case XKB_KEY_Delete:      return PL_KEY_DELETE;
        case XKB_KEY_Help:        return PL_KEY_MENU;
        case XKB_KEY_Meta_L:      return PL_KEY_LEFT_SUPER;
        case XKB_KEY_Meta_R:      return PL_KEY_RIGHT_SUPER;
        case XKB_KEY_KP_0:        return PL_KEY_KEYPAD_0;
        case XKB_KEY_KP_1:        return PL_KEY_KEYPAD_1;
        case XKB_KEY_KP_2:        return PL_KEY_KEYPAD_2;
        case XKB_KEY_KP_3:        return PL_KEY_KEYPAD_3;
        case XKB_KEY_KP_4:        return PL_KEY_KEYPAD_4;
        case XKB_KEY_KP_5:        return PL_KEY_KEYPAD_5;
        case XKB_KEY_KP_6:        return PL_KEY_KEYPAD_6;
        case XKB_KEY_KP_7:        return PL_KEY_KEYPAD_7;
        case XKB_KEY_KP_8:        return PL_KEY_KEYPAD_8;
        case XKB_KEY_KP_9:        return PL_KEY_KEYPAD_9;
        case XKB_KEY_multiply:    return PL_KEY_KEYPAD_MULTIPLY;
        case XKB_KEY_KP_Add:      return PL_KEY_KEYPAD_ADD;   ;
        case XKB_KEY_KP_Subtract: return PL_KEY_KEYPAD_SUBTRACT;
        case XKB_KEY_KP_Decimal:  return PL_KEY_KEYPAD_DECIMAL;
        case XKB_KEY_KP_Divide:   return PL_KEY_KEYPAD_DIVIDE;
        case XKB_KEY_F1:          return PL_KEY_F1;
        case XKB_KEY_F2:          return PL_KEY_F2;
        case XKB_KEY_F3:          return PL_KEY_F3;
        case XKB_KEY_F4:          return PL_KEY_F4;
        case XKB_KEY_F5:          return PL_KEY_F5;
        case XKB_KEY_F6:          return PL_KEY_F6;
        case XKB_KEY_F7:          return PL_KEY_F7;
        case XKB_KEY_F8:          return PL_KEY_F8;
        case XKB_KEY_F9:          return PL_KEY_F9;
        case XKB_KEY_F10:         return PL_KEY_F10;
        case XKB_KEY_F11:         return PL_KEY_F11;
        case XKB_KEY_F12:         return PL_KEY_F12;
        case XKB_KEY_F13:         return PL_KEY_F13;
        case XKB_KEY_F14:         return PL_KEY_F14;
        case XKB_KEY_F15:         return PL_KEY_F15;
        case XKB_KEY_F16:         return PL_KEY_F16;
        case XKB_KEY_F17:         return PL_KEY_F17;
        case XKB_KEY_F18:         return PL_KEY_F18;
        case XKB_KEY_F19:         return PL_KEY_F19;
        case XKB_KEY_F20:         return PL_KEY_F20;
        case XKB_KEY_F21:         return PL_KEY_F21;
        case XKB_KEY_F22:         return PL_KEY_F22;
        case XKB_KEY_F23:         return PL_KEY_F23;
        case XKB_KEY_F24:         return PL_KEY_F24;
        case XKB_KEY_Num_Lock:    return PL_KEY_NUM_LOCK;
        case XKB_KEY_Scroll_Lock: return PL_KEY_SCROLL_LOCK;
        case XKB_KEY_KP_Equal:    return PL_KEY_KEYPAD_EQUAL;
        case XKB_KEY_Shift_L:     return PL_KEY_LEFT_SHIFT;
        case XKB_KEY_Shift_R:     return PL_KEY_RIGHT_SHIFT;
        case XKB_KEY_Control_L:   return PL_KEY_LEFT_CTRL;
        case XKB_KEY_Control_R:   return PL_KEY_RIGHT_CTRL;
        case XKB_KEY_Alt_L:       return PL_KEY_LEFT_ALT;
        case XKB_KEY_Alt_R:       return PL_KEY_RIGHT_ALT;
        case XKB_KEY_semicolon:   return PL_KEY_SEMICOLON;
        case XKB_KEY_plus:        return PL_KEY_KEYPAD_ADD;
        case XKB_KEY_comma:       return PL_KEY_COMMA;
        case XKB_KEY_minus:       return PL_KEY_MINUS;
        case XKB_KEY_period:      return PL_KEY_PERIOD;
        case XKB_KEY_slash:       return PL_KEY_SLASH;
        case XKB_KEY_grave:       return PL_KEY_GRAVE_ACCENT;
        case XKB_KEY_0:           return PL_KEY_0;
        case XKB_KEY_1:           return PL_KEY_1;
        case XKB_KEY_2:           return PL_KEY_2;
        case XKB_KEY_3:           return PL_KEY_3;
        case XKB_KEY_4:           return PL_KEY_4;
        case XKB_KEY_5:           return PL_KEY_5;
        case XKB_KEY_6:           return PL_KEY_6;
        case XKB_KEY_7:           return PL_KEY_7;
        case XKB_KEY_8:           return PL_KEY_8;
        case XKB_KEY_9:           return PL_KEY_9;
        case XKB_KEY_a:
        case XKB_KEY_A:           return PL_KEY_A;
        case XKB_KEY_b:
        case XKB_KEY_B:           return PL_KEY_B;
        case XKB_KEY_c:
        case XKB_KEY_C:           return PL_KEY_C;
        case XKB_KEY_d:
        case XKB_KEY_D:           return PL_KEY_D;
        case XKB_KEY_e:
        case XKB_KEY_E:           return PL_KEY_E;
        case XKB_KEY_f:
        case XKB_KEY_F:           return PL_KEY_F;
        case XKB_KEY_g:
        case XKB_KEY_G:           return PL_KEY_G;
        case XKB_KEY_h:
        case XKB_KEY_H:           return PL_KEY_H;
        case XKB_KEY_i:
        case XKB_KEY_I:           return PL_KEY_I;
        case XKB_KEY_j:
        case XKB_KEY_J:           return PL_KEY_J;
        case XKB_KEY_k:
        case XKB_KEY_K:           return PL_KEY_K;
        case XKB_KEY_l:
        case XKB_KEY_L:           return PL_KEY_L;
        case XKB_KEY_m:
        case XKB_KEY_M:           return PL_KEY_M;
        case XKB_KEY_n:
        case XKB_KEY_N:           return PL_KEY_N;
        case XKB_KEY_o:
        case XKB_KEY_O:           return PL_KEY_O;
        case XKB_KEY_p:
        case XKB_KEY_P:           return PL_KEY_P;
        case XKB_KEY_q:
        case XKB_KEY_Q:           return PL_KEY_Q;
        case XKB_KEY_r:
        case XKB_KEY_R:           return PL_KEY_R;
        case XKB_KEY_s:
        case XKB_KEY_S:           return PL_KEY_S;
        case XKB_KEY_t:
        case XKB_KEY_T:           return PL_KEY_T;
        case XKB_KEY_u:
        case XKB_KEY_U:           return PL_KEY_U;
        case XKB_KEY_v:
        case XKB_KEY_V:           return PL_KEY_V;
        case XKB_KEY_w:
        case XKB_KEY_W:           return PL_KEY_W;
        case XKB_KEY_x:
        case XKB_KEY_X:           return PL_KEY_X;
        case XKB_KEY_y:
        case XKB_KEY_Y:           return PL_KEY_Y;
        case XKB_KEY_z:
        case XKB_KEY_Z:           return PL_KEY_Z;
        default:
        return PL_KEY_NONE;
    }            
}

void
pl__linux_procedure(xcb_generic_event_t* event)
{
    xcb_client_message_event_t* cm;

    switch (event->response_type & ~0x80) 
    {

        case XCB_SELECTION_REQUEST:
        {
            const xcb_selection_request_event_t* ptRequest =
                (const xcb_selection_request_event_t*)event;

            pl__linux_handle_selection_request(ptRequest);
            break;
        }

        case XCB_SELECTION_CLEAR:
        {
            /*
                Another application has replaced us as clipboard owner. The retained
                buffer may be kept for reuse, but we no longer serve it.
            */
            break;
        }

        case XCB_SELECTION_NOTIFY:
        {
            /*
                Normally consumed synchronously by pl_get_clipboard_text(). A stale
                notification may reach the normal event loop, so safely ignore it.
            */
            break;
        }

        case XCB_CLIENT_MESSAGE: 
        {
            cm = (xcb_client_message_event_t*)event;

            // Window close
            if (cm->data.data32[0] == gptPlatformExtCtx->tWmDeleteWin) 
            {
                gptIO->bRunning  = false;
            }
            break;
        }

        case XCB_MOTION_NOTIFY: 
        {
            xcb_motion_notify_event_t* motion = (xcb_motion_notify_event_t*)event;
            gptIOI->add_mouse_pos_event((float)motion->event_x, (float)motion->event_y);
            break;
        }

        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: gptIOI->add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, true);   break;
                case XCB_BUTTON_INDEX_2: gptIOI->add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, true); break;
                case XCB_BUTTON_INDEX_3: gptIOI->add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, true);  break;
                case XCB_BUTTON_INDEX_4: gptIOI->add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: gptIOI->add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:
                    break;
            }
            break;
        }
        
        case XCB_BUTTON_RELEASE:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: gptIOI->add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, false);   break;
                case XCB_BUTTON_INDEX_2: gptIOI->add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, false); break;
                case XCB_BUTTON_INDEX_3: gptIOI->add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, false);  break;
                case XCB_BUTTON_INDEX_4: gptIOI->add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: gptIOI->add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:
                    break;
            }
            break;
        }

        case XCB_KEY_PRESS:
        {
            xcb_key_release_event_t *keyEvent = (xcb_key_release_event_t *)event;

            xcb_keycode_t code = keyEvent->detail;
            uint32_t uCol = gptIO->bKeyShift ? 1 : 0;
            KeySym key_sym = XkbKeycodeToKeysym(
                gptPlatformExtCtx->ptDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                uCol);
            xcb_keysym_t k = xcb_key_press_lookup_keysym(gptPlatformExtCtx->ptKeySyms, keyEvent, uCol);
            gptIOI->add_key_event(pl__xcb_key_to_pl_key(key_sym), true);
            if(k < 0xFF)
                gptIOI->add_text_event(k);
            else if (k >= 0x1000100 && k <= 0x110ffff) // utf range
                gptIOI->add_text_event_utf16(k);
            break;
        }
        case XCB_KEY_RELEASE:
        {
            const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
            xcb_keycode_t code = keyEvent->detail;
            KeySym key_sym = XkbKeycodeToKeysym(
                gptPlatformExtCtx->ptDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                0 /*code & ShiftMask ? 1 : 0*/);
            gptIOI->add_key_event(pl__xcb_key_to_pl_key(key_sym), false);
            break;
        }
        case XCB_CONFIGURE_NOTIFY: 
        {
            // Resizing - note that this is also triggered by moving the window, but should be
            // passed anyway since a change in the x/y could mean an upper-left resize.
            // The application layer can decide what to do with this.
            xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)event;

                // gsbtWindows[0]->tDesc.iXPos = configure_event->x;
                // gsbtWindows[0]->tDesc.iYPos = configure_event->y;

            // Fire the event. The application layer should pick this up, but not handle it
            // as it shouldn be visible to other parts of the application.
            if(configure_event->width != gptIO->tMainViewportSize.x || configure_event->height != gptIO->tMainViewportSize.y)
            {
                gptIO->tMainViewportSize.x = configure_event->width;
                gptIO->tMainViewportSize.y = configure_event->height;
                gptIO->bViewportSizeChanged = true;
                // gsbtWindows[0]->tDesc.uWidth = configure_event->width;
                // gsbtWindows[0]->tDesc.uHeight = configure_event->height;

            }
            break;
        } 
        default: break;
    }
    free(event);
}

static xcb_atom_t
pl__intern_atom(const char* pcName)
{
    const xcb_intern_atom_cookie_t tCookie =
        xcb_intern_atom(
            gptPlatformExtCtx->ptConnection,
            false,
            (uint16_t)strlen(pcName),
            pcName);

    xcb_intern_atom_reply_t* ptReply = xcb_intern_atom_reply(gptPlatformExtCtx->ptConnection, tCookie, NULL);

    if(ptReply == NULL)
        return XCB_NONE;

    const xcb_atom_t tAtom = ptReply->atom;
    free(ptReply);

    return tAtom;
}

static uint64_t
pl__linux_get_time_milliseconds(void)
{
    struct timespec tTime;
    clock_gettime(CLOCK_MONOTONIC, &tTime);

    return (uint64_t)tTime.tv_sec * 1000ull + (uint64_t)tTime.tv_nsec / 1000000ull;
}

static void
pl__linux_handle_selection_request(const xcb_selection_request_event_t* ptRequest)
{
    xcb_selection_notify_event_t tNotify = {0};

    tNotify.response_type = XCB_SELECTION_NOTIFY;
    tNotify.requestor     = ptRequest->requestor;
    tNotify.selection     = ptRequest->selection;
    tNotify.target        = ptRequest->target;
    tNotify.time          = ptRequest->time;
    tNotify.property      = XCB_NONE;

    /*
        Older clients may pass XCB_NONE as the destination property. In that
        case, the target atom is used as the property.
    */
    const xcb_atom_t tProperty =
        ptRequest->property != XCB_NONE ?
        ptRequest->property :
        ptRequest->target;

    if(ptRequest->selection == gptPlatformExtCtx->tClipboard)
    {
        if(ptRequest->target == gptPlatformExtCtx->tTargets)
        {
            const xcb_atom_t atSupportedTargets[] = {
                gptPlatformExtCtx->tTargets,
                gptPlatformExtCtx->tUtf8String,
                gptPlatformExtCtx->tText,
                XCB_ATOM_STRING
            };

            xcb_change_property(
                gptPlatformExtCtx->ptConnection,
                XCB_PROP_MODE_REPLACE,
                ptRequest->requestor,
                tProperty,
                XCB_ATOM_ATOM,
                32,
                PL_ARRAYSIZE(atSupportedTargets),
                atSupportedTargets);

            tNotify.property = tProperty;
        }
        else if(ptRequest->target == gptPlatformExtCtx->tUtf8String || ptRequest->target == gptPlatformExtCtx->tText || ptRequest->target == XCB_ATOM_STRING)
        {
            const uint32_t uSize = gptPlatformExtCtx->sbcOwnedClipboardData ? pl_sb_size(gptPlatformExtCtx->sbcOwnedClipboardData) - 1u : 0u;

            /*
                UTF8_STRING and TEXT are returned as UTF-8. STRING is included
                as a compatibility fallback, although strictly speaking it is
                intended for ISO-8859-1 text.
            */
            const xcb_atom_t tPropertyType = ptRequest->target == XCB_ATOM_STRING ? XCB_ATOM_STRING : gptPlatformExtCtx->tUtf8String;

            xcb_change_property(
                gptPlatformExtCtx->ptConnection,
                XCB_PROP_MODE_REPLACE,
                ptRequest->requestor,
                tProperty,
                tPropertyType,
                8,
                uSize,
                gptPlatformExtCtx->sbcOwnedClipboardData);

            tNotify.property = tProperty;
        }
    }

    xcb_send_event(
        gptPlatformExtCtx->ptConnection,
        false,
        ptRequest->requestor,
        XCB_EVENT_MASK_NO_EVENT,
        (const char*)&tNotify);

    xcb_flush(gptPlatformExtCtx->ptConnection);
}

static bool
pl__linux_read_clipboard_property(xcb_atom_t tProperty)
{
    plIO* ptIO = gptIOI->get_io();

    const xcb_get_property_cookie_t tCookie =
        xcb_get_property(
            gptPlatformExtCtx->ptConnection,
            true, /* delete property after reading */
            ((plWindowData*)gptPlatformExtCtx->ptMainWindow->_pBackendData)->tWindow,
            tProperty,
            XCB_GET_PROPERTY_TYPE_ANY,
            0,
            UINT32_MAX);

    xcb_get_property_reply_t* ptReply = xcb_get_property_reply(gptPlatformExtCtx->ptConnection, tCookie, NULL);

    if(ptReply == NULL)
        return false;

    /*
        INCR is the X11 mechanism for transferring very large selections.
        This implementation intentionally handles normal, non-INCR text only.
    */
    if(ptReply->type == gptPlatformExtCtx->tIncr || ptReply->format != 8)
    {
        free(ptReply);
        return false;
    }

    const int iLength = xcb_get_property_value_length(ptReply);
    const void* pValue = xcb_get_property_value(ptReply);

    pl_sb_resize(ptIO->sbcClipboardData, (uint32_t)iLength + 1u);

    if(iLength > 0)
        memcpy(ptIO->sbcClipboardData, pValue, (size_t)iLength);

    ptIO->sbcClipboardData[iLength] = '\0';

    free(ptReply);
    return true;
}

static bool
pl__linux_request_clipboard_target(xcb_atom_t tTarget)
{
    if(gptPlatformExtCtx->ptMainWindow == NULL)
        return false;

    plWindowData* ptWindowData = gptPlatformExtCtx->ptMainWindow->_pBackendData;
    const xcb_window_t tWindow = ptWindowData->tWindow;

    xcb_delete_property(
        gptPlatformExtCtx->ptConnection,
        tWindow,
        gptPlatformExtCtx->tClipboardProperty);

    xcb_convert_selection(
        gptPlatformExtCtx->ptConnection,
        tWindow,
        gptPlatformExtCtx->tClipboard,
        tTarget,
        gptPlatformExtCtx->tClipboardProperty,
        XCB_CURRENT_TIME);

    xcb_flush(gptPlatformExtCtx->ptConnection);

    const uint64_t ulStartTime = pl__linux_get_time_milliseconds();
    const uint64_t ulTimeout   = 1000ull;

    for(;;)
    {
        xcb_generic_event_t* ptEvent = NULL;

        while((ptEvent = xcb_poll_for_event(gptPlatformExtCtx->ptConnection)) != NULL)
        {
            const uint8_t uEventType =
                ptEvent->response_type & ~0x80;

            if(uEventType == XCB_SELECTION_NOTIFY)
            {
                const xcb_selection_notify_event_t* ptNotify =
                    (const xcb_selection_notify_event_t*)ptEvent;

                if(ptNotify->requestor == tWindow &&
                   ptNotify->selection == gptPlatformExtCtx->tClipboard &&
                   ptNotify->target == tTarget)
                {
                    bool bResult = false;

                    if(ptNotify->property != XCB_NONE)
                    {
                        bResult = pl__linux_read_clipboard_property(
                            ptNotify->property);
                    }

                    free(ptEvent);
                    return bResult;
                }
            }

            /*
                Continue processing ordinary application events while waiting
                for the selection owner.
            */
            pl__linux_procedure(ptEvent);
        }

        const uint64_t ulCurrentTime =
            pl__linux_get_time_milliseconds();

        if(ulCurrentTime - ulStartTime >= ulTimeout)
            return false;

        const int iRemainingTime =
            (int)(ulTimeout - (ulCurrentTime - ulStartTime));

        struct pollfd tPollDescriptor = {
            .fd      = xcb_get_file_descriptor(gptPlatformExtCtx->ptConnection),
            .events  = POLLIN,
            .revents = 0
        };

        const int iPollResult =
            poll(&tPollDescriptor, 1, iRemainingTime);

        if(iPollResult <= 0)
            return false;
    }
}

const char*
pl_get_clipboard_text(void* pUnused)
{
    (void)pUnused;

    plIO* ptIO = gptIOI->get_io();
    pl_sb_reset(ptIO->sbcClipboardData);

    if(gptPlatformExtCtx->ptMainWindow == NULL)
        return NULL;

    plWindowData* ptWindowData = gptPlatformExtCtx->ptMainWindow->_pBackendData;

    const xcb_get_selection_owner_cookie_t tOwnerCookie =
        xcb_get_selection_owner(gptPlatformExtCtx->ptConnection, gptPlatformExtCtx->tClipboard);

    xcb_get_selection_owner_reply_t* ptOwnerReply =
        xcb_get_selection_owner_reply(
            gptPlatformExtCtx->ptConnection,
            tOwnerCookie,
            NULL);

    if(ptOwnerReply == NULL)
        return NULL;

    const xcb_window_t tOwner = ptOwnerReply->owner;
    free(ptOwnerReply);

    if(tOwner == XCB_NONE)
        return NULL;

    /*
        Avoid sending a selection request to ourselves.
    */
    if(tOwner == ptWindowData->tWindow)
    {
        const uint32_t uSize =
            gptPlatformExtCtx->sbcOwnedClipboardData ?
            pl_sb_size(gptPlatformExtCtx->sbcOwnedClipboardData) :
            0u;

        if(uSize == 0)
            return NULL;

        pl_sb_resize(ptIO->sbcClipboardData, uSize);

        memcpy(
            ptIO->sbcClipboardData,
            gptPlatformExtCtx->sbcOwnedClipboardData,
            uSize);

        return ptIO->sbcClipboardData;
    }

    /*
        Prefer UTF-8, then fall back to the legacy STRING target.
    */
    if(pl__linux_request_clipboard_target(gptPlatformExtCtx->tUtf8String))
        return ptIO->sbcClipboardData;

    if(pl__linux_request_clipboard_target(XCB_ATOM_STRING))
        return ptIO->sbcClipboardData;

    return NULL;
}

void
pl_set_clipboard_text(void* pUnused, const char* pcText)
{
    (void)pUnused;

    if(gptPlatformExtCtx->ptMainWindow == NULL || pcText == NULL)
        return;

    const size_t szTextLength = strlen(pcText);

    pl_sb_resize(
        gptPlatformExtCtx->sbcOwnedClipboardData,
        (uint32_t)szTextLength + 1u);

    memcpy(
        gptPlatformExtCtx->sbcOwnedClipboardData,
        pcText,
        szTextLength + 1u);

    plWindowData* ptWindowData = gptPlatformExtCtx->ptMainWindow->_pBackendData;

    xcb_set_selection_owner(
        gptPlatformExtCtx->ptConnection,
        ptWindowData->tWindow,
        gptPlatformExtCtx->tClipboard,
        XCB_CURRENT_TIME);

    xcb_flush(gptPlatformExtCtx->ptConnection);
}

//-----------------------------------------------------------------------------
// [SECTION] file api
//-----------------------------------------------------------------------------

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

plFileResult
pl_file_remove(const char* pcFile)
{
    int iResult = remove(pcFile);
    if(iResult)
        return PL_FILE_RESULT_FAIL;
    return PL_FILE_RESULT_SUCCESS;
}

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
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
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
    size_t bufferSize = 0u;
    pl_file_binary_read(source, &bufferSize, NULL);

    struct stat stat_buf;
    int fromfd = open(source, O_RDONLY);
    fstat(fromfd, &stat_buf);
    int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
    int n = 1;
    while (n > 0)
        n = sendfile(tofd, fromfd, 0, bufferSize * 2);
    return PL_FILE_RESULT_SUCCESS;
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
// [SECTION] atomics api
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounter
{
    atomic_int_fast64_t ilValue;
} plAtomicCounter;

plAtomicsResult
pl_atomics_create_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = PL_ALLOC(sizeof(plAtomicCounter));
    atomic_init(&(*ptCounter)->ilValue, ilValue); //-V522
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
// [SECTION] network api
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
// [SECTION] thread api
//-----------------------------------------------------------------------------

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
    sem_t tHandle;
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

void
pl_threads_sleep_thread(uint32_t millisec)
{
    struct timespec ts = {0};
    int res;

    ts.tv_sec = millisec / 1000;
    ts.tv_nsec = (millisec % 1000) * 1000000;

    do 
    {
        res = nanosleep(&ts, &ts);
    } 
    while (res);
}

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

plThreadResult
pl_threads_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** pptThreadOut)
{
    *pptThreadOut = PL_ALLOC(sizeof(plThread));
    plThread* ptThread = *pptThreadOut;
    if(pthread_create(&ptThread->tHandle, NULL, ptProcedure, pData))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    static uint64_t uThreadID = 1;
    (*pptThreadOut)->uID = uThreadID++;
    pl_sb_push(gsbtThreads, ptThread);
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
    *pptMutexOut = PL_ALLOC(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL)) //-V522
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
    PL_FREE((*pptMutex));
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
    if(sem_init(&(*pptSemaphoreOut)->tHandle, 0, uIntialCount))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_semaphore(plSemaphore** pptSemaphore)
{
    sem_destroy(&(*pptSemaphore)->tHandle);
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl_threads_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    sem_wait(&ptSemaphore->tHandle);
}

bool
pl_threads_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    return sem_trywait(&ptSemaphore->tHandle) == 0;
}

void
pl_threads_release_semaphore(plSemaphore* ptSemaphore)
{
    sem_post(&ptSemaphore->tHandle);
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
    pthread_cond_broadcast(&ptConditionVariable->tHandle);
}

void               
pl_threads_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    pthread_cond_wait(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle);
}

//-----------------------------------------------------------------------------
// [SECTION] virtual memory api
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
    int iResult = madvise(pAddress, szSize, MADV_DONTNEED);
    PL_ASSERT(iResult == 0);

    iResult = mprotect(pAddress, szSize, PROT_NONE);
    PL_ASSERT(iResult == 0);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_platform_ext.c"