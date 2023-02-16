/*
   pl_win32.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_WIN32_H
#define PL_WIN32_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void             pl_init_win32               (HWND tHandle);
void             pl_cleanup_win32            (void);
void             pl_new_frame_win32          (void);
void             pl_update_mouse_cursor_win32(void);
LRESULT CALLBACK pl_windows_procedure        (HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam);

#endif //PL_WIN32_H