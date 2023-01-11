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
#include <xcb/xcb.h>
#include <X11/XKBlib.h> // XkbKeycodeToKeysym
#include <xkbcommon/xkbcommon-keysyms.h>

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
static double            gdTime = 0.0;
static double            gdFrequency = 0.0;

typedef struct _plAppData plAppData;
static void* (*pl_app_load)    (plIOContext* ptIOCtx, plAppData* ptAppData);
static void  (*pl_app_shutdown)(plAppData* ptAppData);
static void  (*pl_app_resize)  (plAppData* ptAppData);
static void  (*pl_app_update)  (plAppData* ptAppData);

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plKey pl__xcb_key_to_pl_key(uint32_t x_keycode);
static inline double pl__get_linux_absolute_time(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / gdFrequency;    
}

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{

    // setup & retrieve io context
    pl_initialize_io_context(&gtIOContext);
    plIOContext* ptIOCtx = pl_get_io_context();

    // setup timers
    static struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_getres() failed");
    }
    gdFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
    gdTime = pl__get_linux_absolute_time();

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

                case XCB_MOTION_NOTIFY: 
                {
                    xcb_motion_notify_event_t* motion = (xcb_motion_notify_event_t*)event;
                    pl_add_mouse_pos_event((float)motion->event_x, (float)motion->event_y);
                    break;
                }

                case XCB_BUTTON_PRESS:
                {
                    xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
                    switch (press->detail)
                    {
                        case XCB_BUTTON_INDEX_1: pl_add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, true);   break;
                        case XCB_BUTTON_INDEX_2: pl_add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, true); break;
                        case XCB_BUTTON_INDEX_3: pl_add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, true);  break;
                        case XCB_BUTTON_INDEX_4: pl_add_mouse_wheel_event (0.0f, 1.0f);                   break;
                        case XCB_BUTTON_INDEX_5: pl_add_mouse_wheel_event (0.0f, -1.0f);                  break;
                        default:                 pl_add_mouse_button_event(press->detail, true);          break;
                    }
                    break;
                }
                
                case XCB_BUTTON_RELEASE:
                {
                    xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
                    switch (press->detail)
                    {
                        case XCB_BUTTON_INDEX_1: pl_add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, false);   break;
                        case XCB_BUTTON_INDEX_2: pl_add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, false); break;
                        case XCB_BUTTON_INDEX_3: pl_add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, false);  break;
                        case XCB_BUTTON_INDEX_4: pl_add_mouse_wheel_event (0.0f, 1.0f);                   break;
                        case XCB_BUTTON_INDEX_5: pl_add_mouse_wheel_event (0.0f, -1.0f);                  break;
                        default:                 pl_add_mouse_button_event(press->detail, false);          break;
                    }
                    break;
                }

                case XCB_KEY_PRESS:
                {
                    const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
                    xcb_keycode_t code = keyEvent->detail;
                    KeySym key_sym = XkbKeycodeToKeysym(
                        gDisplay, 
                        (KeyCode)code,  // event.xkey.keycode,
                        0,
                        0 /*code & ShiftMask ? 1 : 0*/);
                    pl_add_key_event(pl__xcb_key_to_pl_key(key_sym), true);
                    break;
                }
                case XCB_KEY_RELEASE:
                {
                    const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
                    xcb_keycode_t code = keyEvent->detail;
                    KeySym key_sym = XkbKeycodeToKeysym(
                        gDisplay, 
                        (KeyCode)code,  // event.xkey.keycode,
                        0,
                        0 /*code & ShiftMask ? 1 : 0*/);
                    pl_add_key_event(pl__xcb_key_to_pl_key(key_sym), false);
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
                    if(configure_event->width != gtIOContext.afMainViewportSize[0] || configure_event->height != gtIOContext.afMainViewportSize[1])
                    {
                        gtIOContext.afMainViewportSize[0] = configure_event->width;
                        gtIOContext.afMainViewportSize[1] = configure_event->height;
                        pl_app_resize(gUserData);
                    }
                    break;
                } 
                default: break;
            }
            free(event);
        }

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

        double dCurrentTime = pl__get_linux_absolute_time();
        pl_get_io_context()->fDeltaTime = (float)(dCurrentTime - gdTime);
        gdTime = dCurrentTime;

        pl_app_update(gUserData);
    }

    // app cleanup
    pl_app_shutdown(gUserData);

    // platform cleanup
    XAutoRepeatOn(gDisplay);
    xcb_destroy_window(gConnection, gWindow);

    // cleanup io context
    pl_cleanup_io_context();
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementation
//-----------------------------------------------------------------------------

static plKey
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
        default:                  return PL_KEY_NONE;
    }            
}