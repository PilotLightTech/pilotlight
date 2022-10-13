/*
   linux_pl.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] entry point
// [SECTION] windows procedure
// [SECTION] misc
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_os.h"
#include "pl_ds.h"
#include "vulkan_pl_graphics.h"
#include "vulkan_pl.h"
#include <xcb/xcb.h>
#include <X11/Xlib.h>

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static Display*          gDisplay;
static xcb_connection_t* gConnection;
static xcb_window_t      gWindow;
static xcb_screen_t*     gScreen;
static xcb_atom_t        gWmProtocols;
static xcb_atom_t        gWmDeleteWin;

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
    if(pl_load_library(&gSharedLibrary, "./app.so", "./app_", "./lock.tmp"))
    {
        pl_app_load = (void* (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_load");
        pl_app_setup = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_setup");
        pl_app_shutdown = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_shutdown");
        pl_app_resize = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_resize");
        pl_app_render = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_render");
        gUserData = pl_app_load(&gAppData, NULL);
    }

    // connect to x
    gDisplay = XOpenDisplay(NULL);

    int screen_p = 0;
    gConnection = xcb_connect(NULL, &screen_p);
    if(xcb_connection_has_error(gConnection))
    {
        PL_ASSERT(false && "Failed to connect to X server via XCB.");
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
        gAppData.clientWidth,  // width
        gAppData.clientHeight, // height
        0,                     // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT,  //class
        gScreen->root_visual,
        event_mask,
        value_list);

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

    // create vulkan instance
    pl_create_instance(&gAppData.graphics, VK_API_VERSION_1_1, true);

    // create surface
    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .window = gWindow,
        .connection = gConnection
    };
    PL_VULKAN(vkCreateXcbSurfaceKHR(gAppData.graphics.instance, &surfaceCreateInfo, NULL, &gAppData.graphics.surface));

    // create devices
    pl_create_device(gAppData.graphics.instance, gAppData.graphics.surface, &gAppData.device, true);
    
    // create swapchain
    pl_create_swapchain(&gAppData.device, gAppData.graphics.surface, gAppData.clientWidth, gAppData.clientHeight, &gAppData.swapchain);

    // app specific setup
    pl_app_setup(&gAppData, gUserData);

    // main loop
    while (gAppData.running)
    {
        xcb_generic_event_t* event;
        xcb_client_message_event_t* cm;

        // Poll for events until null is returned.
        while (event = xcb_poll_for_event(gConnection)) 
        {
            switch (event->response_type & ~0x80) 
            {

                case XCB_CLIENT_MESSAGE: 
                {
                    cm = (xcb_client_message_event_t*)event;

                    // Window close
                    if (cm->data.data32[0] == gWmDeleteWin) 
                    {
                        gAppData.running  = false;
                    }
                    break;
                }

                case XCB_CONFIGURE_NOTIFY: 
                {
                    // Resizing - note that this is also triggered by moving the window, but should be
                    // passed anyway since a change in the x/y could mean an upper-left resize.
                    // The application layer can decide what to do with this.
                    xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)event;

                    // Fire the event. The application layer should pick this up, but not handle it
                    // as it shouldn be visible to other parts of the application.
                    if(configure_event->width != gAppData.clientWidth || configure_event->height != gAppData.clientHeight)
                    {
                        gAppData.clientWidth = configure_event->width;
                        gAppData.clientHeight = configure_event->height;
                        pl_app_resize(&gAppData, gUserData);
                    }

                } break;
                default: break;
            }
            free(event);
        }

        // reload library
        if(pl_has_library_changed(&gSharedLibrary))
        {
            pl_reload_library(&gSharedLibrary);
            pl_app_load = (void* (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_load");
            pl_app_setup = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_setup");
            pl_app_shutdown = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_shutdown");
            pl_app_resize = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_resize");
            pl_app_render = (void (__attribute__(())  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_render");
            gUserData = pl_app_load(&gAppData, gUserData);
        }

        // render a frame
        pl_app_render(&gAppData, gUserData);
    }

    // app cleanup
    pl_app_shutdown(&gAppData, gUserData);

    // cleanup graphics context
    pl_cleanup_graphics(&gAppData.graphics, &gAppData.device);

    // platform cleanup
    XAutoRepeatOn(gDisplay);
    xcb_destroy_window(gConnection, gWindow);
}