/*
   pl_main_x11.c
     * x11 platform backend

   Missing APIs:
     [ ] Clipboard
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] linux procedure
// [SECTION] file api
// [SECTION] library api
// [SECTION] thread api
// [SECTION] window api
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
#include "pl_ds.h"
#include <time.h>     // clock_gettime, clock_getres
#include <string.h>   // strlen
#include <stdlib.h>   // free
#include <assert.h>
#include <sys/stat.h> // stat, timespec
#include <stdio.h>    // file api
#include <dlfcn.h>    // dlopen, dlsym, dlclose
#include <sys/types.h>
#include <fcntl.h>    // O_RDONLY, O_WRONLY ,O_CREAT
#include <pthread.h>
#include <unistd.h>
#include <sys/sendfile.h> // sendfile

#include <xcb/xcb.h>
#include <xcb/xfixes.h> //xcb_xfixes_query_version, apt install libxcb-xfixes0-dev
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xcb/xcb_cursor.h> // apt install libxcb-cursor-dev, libxcb-cursor0
#include <xcb/xcb_keysyms.h>
#include <X11/XKBlib.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// helpers
void pl__update_mouse_cursor(void);
void pl__linux_procedure (xcb_generic_event_t*);
plKey pl__xcb_key_to_pl_key(uint32_t x_keycode);

static inline time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
}

uint32_t
pl_get_hardware_thread_count(void)
{

    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)numCPU;
}

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acPath[PL_MAX_PATH_LENGTH];
    char          acTransitionalName[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    void*         handle;
    time_t        lastWriteTime;
} plSharedLibrary;


typedef struct _plWindowData
{
    xcb_connection_t* ptConnection;
    xcb_window_t      tWindow;
} plWindowData;

typedef struct _plRuntimeMutex
{
    pthread_mutex_t tHandle;
} plRuntimeMutex;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// x11 & xcb stuff
Display*              gptDisplay    = NULL;
xcb_connection_t*     gptConnection = NULL;
xcb_key_symbols_t*    gptKeySyms    = NULL;
xcb_screen_t*         gptScreen     = NULL;
xcb_atom_t            gtWmProtocols;
xcb_atom_t            gtWmDeleteWin;
xcb_cursor_context_t* gptCursorContext = NULL;

// linux stuff
double gdTime      = 0.0;
double gdFrequency = 0.0;

static inline double
pl__get_linux_absolute_time(void)
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

    // load core apis
    pl__load_core_apis();

    // add contexts to data registry
    gptIOCtx = gptIOI->get_io();

    #ifndef PL_HEADLESS_APP

    // connect to x
    gptDisplay = XOpenDisplay(NULL);

    // turn off auto repeat (we handle this internally)
    XAutoRepeatOff(gptDisplay);

    int screen_p = 0;
    gptConnection = xcb_connect(NULL, &screen_p);
    if(xcb_connection_has_error(gptConnection))
    {
        assert(false && "Failed to connect to X server via XCB.");
    }

    // get data from x server
    const xcb_setup_t* setup = xcb_get_setup(gptConnection);

    // loop through screens using iterator
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    
    for (int s = screen_p; s > 0; s--) 
    {
        xcb_screen_next(&it);
    }

    // after screens have been looped through, assign it.
    gptScreen = it.data;

    #endif

    // setup timers
    static struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_getres() failed");
    }
    gdFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
    gdTime = pl__get_linux_absolute_time();

    #ifndef PL_HEADLESS_APP

    // Notify X for mouse cursor handling
    xcb_discard_reply(gptConnection, xcb_xfixes_query_version(gptConnection, 4, 0).sequence);

    // Cursor context for looking up cursors for the current X cursor theme
    xcb_cursor_context_new(gptConnection, gptScreen, &gptCursorContext);

    // get the current key map
    gptKeySyms = xcb_key_symbols_alloc(gptConnection);

    #endif

    // load library
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    const plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName
    };
    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(()) *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(()) *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(()) *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");

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

    // main loop
    while (gptIOCtx->bRunning)
    {
        
        // Poll for events until null is returned.
        xcb_generic_event_t* event;
        while (event = xcb_poll_for_event(gptConnection)) 
            pl__linux_procedure(event);

        if(gptIOCtx->bViewportSizeChanged) //-V547
            pl_app_resize(gpUserData);

        pl__update_mouse_cursor();

        // reload library
        if(ptLibraryApi->has_changed(gptAppLibrary))
        {
            ptLibraryApi->reload(gptAppLibrary);
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*))                     ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(void*))                     ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*))                     ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");

            pl__handle_extension_reloads();
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }

        // render a frame
        const double dCurrentTime = pl__get_linux_absolute_time();
        gptIOCtx->fDeltaTime = (float)(dCurrentTime - gdTime);
        gdTime = dCurrentTime;

        pl__garbage_collect_data_reg();
        pl_app_update(gpUserData);
        pl__handle_extension_reloads();
    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    // platform cleanup
    XAutoRepeatOn(gptDisplay);
    xcb_cursor_context_free(gptCursorContext);
    xcb_key_symbols_free(gptKeySyms);

    pl__unload_all_extensions();
    pl__unload_core_apis();

    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    pl__check_for_leaks();
}

//-----------------------------------------------------------------------------
// [SECTION] linux procedure
//-----------------------------------------------------------------------------

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

        case XCB_CLIENT_MESSAGE: 
        {
            cm = (xcb_client_message_event_t*)event;

            // Window close
            if (cm->data.data32[0] == gtWmDeleteWin) 
            {
                gptIOCtx->bRunning  = false;
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
            uint32_t uCol = gptIOCtx->bKeyShift ? 1 : 0;
            KeySym key_sym = XkbKeycodeToKeysym(
                gptDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                uCol);
            xcb_keysym_t k = xcb_key_press_lookup_keysym(gptKeySyms, keyEvent, uCol);
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
                gptDisplay, 
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

                gsbtWindows[0]->tDesc.iXPos = configure_event->x;
                gsbtWindows[0]->tDesc.iYPos = configure_event->y;

            // Fire the event. The application layer should pick this up, but not handle it
            // as it shouldn be visible to other parts of the application.
            if(configure_event->width != gptIOCtx->tMainViewportSize.x || configure_event->height != gptIOCtx->tMainViewportSize.y)
            {
                gptIOCtx->tMainViewportSize.x = configure_event->width;
                gptIOCtx->tMainViewportSize.y = configure_event->height;
                gptIOCtx->bViewportSizeChanged = true;
                gsbtWindows[0]->tDesc.uWidth = configure_event->width;
                gsbtWindows[0]->tDesc.uHeight = configure_event->height;

            }
            break;
        } 
        default: break;
    }
    free(event);
}

void
pl__update_mouse_cursor(void)
{
    // updating mouse cursor
    if(gptIOCtx->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && gptIOCtx->tNextCursor == PL_MOUSE_CURSOR_ARROW)
        gptIOCtx->bCursorChanged = true;

    if(gptIOCtx->bCursorChanged && gptIOCtx->tNextCursor != gptIOCtx->tCurrentCursor)
    {
        gptIOCtx->tCurrentCursor = gptIOCtx->tNextCursor;
        const char* tX11Cursor = NULL;
        switch (gptIOCtx->tNextCursor)
        {
            case PL_MOUSE_CURSOR_ARROW:       tX11Cursor = "left_ptr"; break;
            case PL_MOUSE_CURSOR_TEXT_INPUT:  tX11Cursor = "xterm"; break;
            case PL_MOUSE_CURSOR_RESIZE_ALL:  tX11Cursor = "fleur"; break;
            case PL_MOUSE_CURSOR_RESIZE_EW:   tX11Cursor = "sb_h_double_arrow"; break;
            case PL_MOUSE_CURSOR_RESIZE_NS:   tX11Cursor = "sb_v_double_arrow"; break;
            case PL_MOUSE_CURSOR_RESIZE_NESW: tX11Cursor = "bottom_left_corner"; break;
            case PL_MOUSE_CURSOR_RESIZE_NWSE: tX11Cursor = "bottom_right_corner"; break;
            case PL_MOUSE_CURSOR_HAND:        tX11Cursor = "hand1"; break;
            case PL_MOUSE_CURSOR_NOT_ALLOWED: tX11Cursor = "circle"; break;
        }  

        xcb_font_t font = xcb_generate_id(gptConnection);
        // There is xcb_xfixes_cursor_change_cursor_by_name. However xcb_cursor_load_cursor guarantees
        // finding the cursor for the current X theme.
        xcb_cursor_t cursor = xcb_cursor_load_cursor(gptCursorContext, tX11Cursor);
        // IM_ASSERT(cursor && "X cursor not found!");

        uint32_t value_list = cursor;
        plWindowData* ptData = gsbtWindows[0]->_pPlatformData;
        xcb_change_window_attributes(gptConnection, ptData->tWindow, XCB_CW_CURSOR, &value_list);
        xcb_free_cursor(gptConnection, cursor);
        xcb_close_font_checked(gptConnection, font);
    }
    gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
    gptIOCtx->bCursorChanged = false;
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
pl_file_delete(const char* pcFile)
{
    int iResult = remove(pcFile);
    if(iResult)
        return PL_FILE_RESULT_FAIL;
    return PL_FILE_RESULT_SUCCESS;
}

plFileResult
pl_binary_read_file(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{
    PL_ASSERT(pszSizeIn);

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
    fseek(ptDataFile, 0, SEEK_SET);

    if(pcBuffer == NULL)
    {
        *pszSizeIn = uSize;
        fclose(ptDataFile);
        return PL_FILE_RESULT_SUCCESS;
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
        return PL_FILE_RESULT_FAIL;
    }

    fclose(ptDataFile);
    return PL_FILE_RESULT_SUCCESS;
}

plFileResult
pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer)
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
pl_copy_file(const char* source, const char* destination)
{
    size_t bufferSize = 0u;
    pl_binary_read_file(source, &bufferSize, NULL);

    struct stat stat_buf;
    int fromfd = open(source, O_RDONLY);
    fstat(fromfd, &stat_buf);
    int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
    int n = 1;
    while (n > 0)
        n = sendfile(tofd, fromfd, 0, bufferSize * 2);
    return PL_FILE_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] library api
//-----------------------------------------------------------------------------

bool
pl_has_library_changed(plSharedLibrary* library)
{
    PL_ASSERT(library);
    if(library)
    {
        time_t newWriteTime = pl__get_last_write_time(library->acPath);
        return newWriteTime != library->lastWriteTime;
    }
    return false;
}

plOSResult
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

        pl_sprintf(ptLibrary->acPath, "./%s.so", tDesc.pcName);

        if(tDesc.pcTransitionalName)
            strncpy(ptLibrary->acTransitionalName, tDesc.pcTransitionalName, PL_MAX_PATH_LENGTH);
        else
        {
            pl_sprintf(ptLibrary->acTransitionalName, "./%s_", tDesc.pcName);
        }

        if(tDesc.pcLockFile)
            strncpy(ptLibrary->acLockFile, tDesc.pcLockFile, PL_MAX_PATH_LENGTH);
        else
            strncpy(ptLibrary->acLockFile, "lock.tmp", PL_MAX_PATH_LENGTH);
    }
    else
        ptLibrary = *pptLibraryOut;

    ptLibrary->bValid = false;

    if(ptLibrary)
    {
        struct stat attr2;
        if(stat(ptLibrary->acLockFile, &attr2) == -1)  // lock file gone
        {
            char temporaryName[2024] = {0};
            ptLibrary->lastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
            
            pl_sprintf(temporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".so");
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
    }
    if(ptLibrary->bValid)
        return PL_OS_RESULT_SUCCESS;
    return PL_OS_RESULT_FAIL;
}

void
pl_reload_library(plSharedLibrary* library)
{
    library->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(library->tDesc, &library))
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

plRuntimeMutex
pl_create_runtime_mutex(void)
{
    plRuntimeMutex tMutex = {0};
    if(pthread_mutex_init(&tMutex.tHandle, NULL)) //-V522
    {
        PL_ASSERT(false);
    }
    return tMutex;
}

void
pl_lock_runtime_mutex(plRuntimeMutex* ptMutex)
{
    pthread_mutex_lock(&ptMutex->tHandle);
}

void
pl_unlock_runtime_mutex(plRuntimeMutex* ptMutex)
{
    pthread_mutex_unlock(&ptMutex->tHandle);
}

void
pl_destroy_runtime_mutex(plRuntimeMutex* ptMutex)
{
    pthread_mutex_destroy(&ptMutex->tHandle);
}
//-----------------------------------------------------------------------------
// [SECTION] window api
//-----------------------------------------------------------------------------

plOSResult
pl_create_window(plWindowDesc tDesc, plWindow** pptWindowOut)
{
    plWindowData* ptData = malloc(sizeof(plWindowData));

    ptData->tWindow = xcb_generate_id(gptConnection); //-V522
    ptData->ptConnection = gptConnection; //-V522

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
    unsigned int  value_list[] = {gptScreen->black_pixel, event_values};

    // Create the window
    xcb_create_window(
        gptConnection,
        XCB_COPY_FROM_PARENT,  // depth
        ptData->tWindow,
        gptScreen->root, // parent
        tDesc.iXPos,
        tDesc.iYPos,
        tDesc.uWidth,
        tDesc.uHeight,
        0, // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
        gptScreen->root_visual,
        event_mask,
        value_list);

    // Change the title
    xcb_change_property(
        gptConnection,
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
        gptConnection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        gptConnection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        gptConnection,
        wm_delete_cookie,
        NULL);
    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        gptConnection,
        wm_protocols_cookie,
        NULL);
    gtWmDeleteWin = wm_delete_reply->atom;
    gtWmProtocols = wm_protocols_reply->atom;

    xcb_change_property(
        gptConnection,
        XCB_PROP_MODE_REPLACE,
        ptData->tWindow,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(gptConnection, ptData->tWindow);

    int stream_result = xcb_flush(gptConnection);

    plWindow* ptWindow = malloc(sizeof(plWindow));
    ptWindow->tDesc = tDesc; //-V522
    ptWindow->_pPlatformData = ptData;
    pl_sb_push(gsbtWindows, ptWindow);
    *pptWindowOut = ptWindow;
    return PL_OS_RESULT_SUCCESS;
}

void
pl_destroy_window(plWindow* ptWindow)
{
    plWindowData* ptData = ptWindow->_pPlatformData;
    xcb_destroy_window(gptConnection, ptData->tWindow);
    free(ptData);
    free(ptWindow);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"