/*
   pl_io.h
     * platform windowing & mouse/keyboard input
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

#ifndef PL_IO_H
#define PL_IO_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#include "pl.h"
#include <stdint.h>  // uint32_t
#include <stdbool.h> // bool

#ifdef _WIN32
struct HWND__; 
typedef struct HWND__ *HWND;
#else // linux
#include <xcb/xcb.h>
#include <X11/Xlib.h>
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plWindow); // platform window

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_create_window        (const char* title, int clientWidth, int clientHeigh, plWindow* windowOut, void* userData);
void pl_cleanup_window       (plWindow* window);
void pl_process_window_events(plWindow* window);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plWindow_t
{
    char   title[PL_MAX_NAME_LENGTH];
    int    width;
    int    height;
    int    clientWidth;
    int    clientHeight;
    bool   running;
    bool   needsResize;
    void*  userData;

    // platform specifics
#ifdef _WIN32
    HWND handle;
#else
    Display*          display;
    xcb_connection_t* connection;
    xcb_window_t      window;
    xcb_screen_t*     screen;
    xcb_atom_t        wm_protocols;
    xcb_atom_t        wm_delete_win;
    uint32_t          flags;
#endif
} plWindow;

#endif // PL_IO_H