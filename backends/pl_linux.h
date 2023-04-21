/*
   pl_win32.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] includes
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_LINUX_H
#define PL_LINUX_H

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plIOApiI plIOApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <xcb/xcb.h>
#include <X11/XKBlib.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_init_linux               (Display* ptDisplay, xcb_connection_t* ptConnection, xcb_screen_t* ptScreen, xcb_window_t* ptWindow, plIOApiI* ptIoApi);
void pl_cleanup_linux            (void);
void pl_new_frame_linux          (void);
void pl_update_mouse_cursor_linux(void);
void pl_linux_procedure          (xcb_generic_event_t* event);

#endif //PL_LINUX_H