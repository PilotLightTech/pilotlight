/*
   linux_pl.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] internal api
// [SECTION] entry point
// [SECTION] internal implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <time.h>     // clock_gettime, clock_getres
#include <string.h>   // strlen
#include <stdlib.h>   // free
#include <assert.h>
#include "pl_io.h"    // io context
#include "pl_os.h"    // shared library api
#include "pl_linux.h"
#include <xcb/xcb.h>
#include <xcb/xfixes.h> //xcb_xfixes_query_version, apt install libxcb-xfixes0-dev

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xcb/xcb_cursor.h> // apt install libxcb-cursor-dev, libxcb-cursor0

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static Display*          gDisplay;
static xcb_connection_t* gConnection;
static xcb_window_t      gWindow;
static xcb_screen_t*     gScreen;
static bool              gRunning = true;
static xcb_atom_t        gWmProtocols;
static xcb_atom_t        gWmDeleteWin;
plIOContext              gtIOContext = {0};
static plSharedLibrary   gAppLibrary = {0};
static void*             gUserData = NULL;

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

    // setup & retrieve io context
    pl_initialize_io_context(&gtIOContext);
    plIOContext* ptIOCtx = pl_get_io_context();

    // connect to x
    gDisplay = XOpenDisplay(NULL);

    // turn off auto repeat (we handle this internally)
    XAutoRepeatOff(gDisplay);

    int screen_p = 0;
    gConnection = xcb_connect(NULL, &screen_p);
    if(xcb_connection_has_error(gConnection))
    {
        assert(false && "Failed to connect to X server via XCB.");
    }

    // get data from x server
    const xcb_setup_t* setup = xcb_get_setup(gConnection);

    // loop through screens using iterator
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    
    for (int s = screen_p; s > 0; s--) 
    {
        xcb_screen_next(&it);
    }

    // allocate a XID for the window to be created.
    gWindow = xcb_generate_id(gConnection);

    // after screens have been looped through, assign it.
    gScreen = it.data;

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
    unsigned int  value_list[] = {gScreen->black_pixel, event_values};

    // Create the window
    xcb_create_window(
        gConnection,
        XCB_COPY_FROM_PARENT,  // depth
        gWindow,               // window
        gScreen->root,         // parent
        200,                   // x
        200,                   // y
        500,                   // width
        500,                   // height
        0,                     // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT,  //class
        gScreen->root_visual,
        event_mask,
        value_list);

    pl_init_linux(gDisplay, gConnection, gScreen, &gWindow);

    // Change the title
    xcb_change_property(
        gConnection,
        XCB_PROP_MODE_REPLACE,
        gWindow,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,  // data should be viewed 8 bits at a time
        strlen("Pilot Light (linux)"),
        "Pilot Light (linux)");

    // Tell the server to notify when the window manager
    // attempts to destroy the window.
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        gConnection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        gConnection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        gConnection,
        wm_delete_cookie,
        NULL);
    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        gConnection,
        wm_protocols_cookie,
        NULL);
    gWmDeleteWin = wm_delete_reply->atom;
    gWmProtocols = wm_protocols_reply->atom;

    xcb_change_property(
        gConnection,
        XCB_PROP_MODE_REPLACE,
        gWindow,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(gConnection, gWindow);

    // Flush the stream
    int stream_result = xcb_flush(gConnection);

    static struct {
        xcb_connection_t* ptConnection;
        xcb_window_t      tWindow;
    } platformData;
    platformData.ptConnection = gConnection;
    platformData.tWindow = gWindow;
    ptIOCtx->pBackendPlatformData = &platformData;

    // load library
    if(pl_load_library(&gAppLibrary, "./app.so", "./app_", "./lock.tmp"))
    {
        pl_app_load     = (void* (__attribute__(())  *)(plIOContext*, plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_update");
        gUserData = pl_app_load(ptIOCtx, NULL);
    }

    // main loop
    while (gRunning)
    {
        xcb_generic_event_t* event;
        xcb_client_message_event_t* cm;

        // Poll for events until null is returned.
        while (event = xcb_poll_for_event(gConnection)) 
        {
            pl_linux_procedure(event);

            switch (event->response_type & ~0x80) 
            {

                case XCB_CLIENT_MESSAGE: 
                {
                    cm = (xcb_client_message_event_t*)event;

                    // Window close
                    if (cm->data.data32[0] == gWmDeleteWin) 
                    {
                        gRunning  = false;
                    }
                    break;
                }
                default: break;
            }
            free(event);
        }

        if(gtIOContext.bViewportSizeChanged)
            pl_app_resize(gUserData);

        pl_update_mouse_cursor_linux();

        // reload library
        if(pl_has_library_changed(&gAppLibrary))
        {
            pl_reload_library(&gAppLibrary);
            pl_app_load     = (void* (__attribute__(())  *)(plIOContext*, plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(())  *)(plAppData*))               pl_load_library_function(&gAppLibrary, "pl_app_update");
            gUserData = pl_app_load(ptIOCtx, gUserData);
        }

        // render a frame
        pl_new_frame_linux();
        pl_app_update(gUserData);
    }

    // app cleanup
    pl_app_shutdown(gUserData);

    // platform cleanup
    XAutoRepeatOn(gDisplay);
    xcb_destroy_window(gConnection, gWindow);
    pl_cleanup_linux();
    
    // cleanup io context
    pl_cleanup_io_context();
}
