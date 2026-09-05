/*
   pl_platform_win32_ext.c
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
// [SECTION] threads api
// [SECTION] virtual memory api
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// #include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // memset
#include <float.h>    // FLT_MAX
#include "pl.h"

// extensions
#include "pl_platform_ext.h"

#define PL_VK_KEYPAD_ENTER (VK_RETURN + 256)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <sysinfoapi.h> // page size
#include <winsock2.h> // sockets
#include <ws2tcpip.h>
#include <windowsx.h>   // GET_X_LPARAM(), GET_Y_LPARAM()
#include <wchar.h>    // mbsrtowcs, wcsrtombs
#pragma comment(lib, "ws2_32.lib")

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static const plIOI*      gptIOI    = NULL;
static const plMemoryI*  gptMemory = NULL;
static plIO*             gptIO     = NULL;

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
    plWindow*   ptMainWindow;
    plWindow**  sbtWindows;
    HWND        tMouseHandle;
    bool        bMouseTracked;
    WNDCLASSEXW tWc;

    // timer stuff
    INT64 ilTime;
    INT64 ilTicksPerSecond;
} plPlatformExtData;

typedef struct _plWindowSurfaceImageWin32
{
    HDC      tDc;
    uint32_t uWidth;
    uint32_t uHeight;
    void*    pPixels;
    uint32_t uPixelCapacity;
} plWindowSurfaceImageWin32;

typedef struct _plWindowSurface
{
    plWindow* ptWindow;
    uint32_t  uCurrentImage;
    uint32_t  uImageCount;
    plWindowSurfaceImageWin32* atImages;
} plWindowSurface;

static plPlatformExtData* gptPlatformExtCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] timer api
//-----------------------------------------------------------------------------

double
pl_timer_get_time(void)
{
    INT64 ilCurrentTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
    return (double)(ilCurrentTime - gptPlatformExtCtx->ilTime) / (double)gptPlatformExtCtx->ilTicksPerSecond;
}

double
pl_timer_get_raw_time(void)
{
    INT64 ilCurrentTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
    return (double)ilCurrentTime / (double)gptPlatformExtCtx->ilTicksPerSecond;
}

//-----------------------------------------------------------------------------
// [SECTION] window api
//-----------------------------------------------------------------------------

// helpers
inline bool pl__is_vk_down(int iVk) { return (GetKeyState(iVk) & 0x8000) != 0;}
plKey       pl__virtual_key_to_pl_key (WPARAM tWParam);
void pl__update_mouse_cursor(void);

// functions passed to backend

void*
pl_platform_setup(void)
{
    return gptPlatformExtCtx;
}

void
pl_platform_new_frame(void* pPlatformData)
{
    pl__update_mouse_cursor();
}

void
pl_platform_cleanup(plPlatformExtData* ptPlatformData)
{
    UnregisterClassW(ptPlatformData->tWc.lpszClassName, GetModuleHandle(NULL));
    pl_sb_free(ptPlatformData->sbtWindows);
    gptPlatformExtCtx = NULL;
}

LRESULT CALLBACK 
pl__windows_procedure(HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam)
{

    // find window
    plWindow* ptWindow = NULL;
    for(uint32_t i = 0; i < pl_sb_size(gptPlatformExtCtx->sbtWindows); i++)
    {
        HWND tHandle = gptPlatformExtCtx->sbtWindows[i]->_pBackendData;
        if(tHandle == tHwnd)
        {
            ptWindow = gptPlatformExtCtx->sbtWindows[i];
            break;
        }
    }

    static UINT_PTR puIDEvent = 0;
    switch (tMsg)
    {

        case WM_SYSCOMMAND:
        {

            if(tWParam == SC_MINIMIZE)
                gptIO->bViewportMinimized = true;
            else if(tWParam == SC_RESTORE)
                gptIO->bViewportMinimized = false;
            else if(tWParam == SC_MAXIMIZE)
                gptIO->bViewportMinimized = false;
            break;
        }

        case WM_SIZE:
        case WM_SIZING:
        {
            if (tWParam != SIZE_MINIMIZED)
            {
                // client window size
                RECT tCRect;
                int iCWidth = 0;
                int iCHeight = 0;
                if (GetClientRect(tHwnd, &tCRect))
                {
                    iCWidth = tCRect.right - tCRect.left;
                    iCHeight = tCRect.bottom - tCRect.top;
                }

                if(iCWidth > 0 && iCHeight > 0)
                    gptIO->bViewportMinimized = false;
                else
                    gptIO->bViewportMinimized = true;

                if(gptIO->tMainViewportSize.x != (float)iCWidth || gptIO->tMainViewportSize.y != (float)iCHeight)
                    gptIO->bViewportSizeChanged = true;  

                gptIO->tMainViewportSize.x = (float)iCWidth;
                gptIO->tMainViewportSize.y = (float)iCHeight;

                if(gptIO->bViewportSizeChanged && !gptIO->bViewportMinimized && gptIO->_bFirstLoadComplete)
                {
                    gptIO->pl_app_resize(gptPlatformExtCtx->ptMainWindow, gptIO->pAppUserData);
                }
                // send paint message
                InvalidateRect(tHwnd, NULL, TRUE);
            }
            break;
        }

        case WM_CHAR:
            if (IsWindowUnicode(tHwnd))
            {
                // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
                if (tWParam > 0 && tWParam < 0x10000)
                    gptIOI->add_text_event_utf16((uint16_t)tWParam);
            }
            else
            {
                wchar_t wch = 0;
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (char*)&tWParam, 1, &wch, 1);
                gptIOI->add_text_event(wch);
            }
            break;

        case WM_SETCURSOR:
            // required to restore cursor when transitioning from e.g resize borders to client area.
            if (LOWORD(tLParam) == HTCLIENT)
            {
                gptIO->tNextCursor = PL_MOUSE_CURSOR_ARROW;
                gptIO->tCurrentCursor = PL_MOUSE_CURSOR_NONE;
                pl__update_mouse_cursor();
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            gptIO->pl_app_update(gptIO->pAppUserData);

            

            // must be called for the OS to do its thing
            PAINTSTRUCT tPaint;
            HDC tDeviceContext = BeginPaint(tHwnd, &tPaint);
            EndPaint(tHwnd, &tPaint); 
            break;
        }

        case WM_MOUSEMOVE:
        {
            gptPlatformExtCtx->tMouseHandle = tHwnd;
            if(!gptPlatformExtCtx->bMouseTracked)
            {
                TRACKMOUSEEVENT tTme = { sizeof(tTme), TME_LEAVE, tHwnd, 0 };
                TrackMouseEvent(&tTme);
                gptPlatformExtCtx->bMouseTracked = true;        
            }
            POINT tMousePos = { (LONG)GET_X_LPARAM(tLParam), (LONG)GET_Y_LPARAM(tLParam) };
            gptIOI->add_mouse_pos_event((float)tMousePos.x, (float)tMousePos.y);
            break;
        }
        case WM_MOUSELEAVE:
        {
            if(tHwnd == gptPlatformExtCtx->tMouseHandle)
            {
                gptPlatformExtCtx->tMouseHandle = NULL;
                gptIOI->add_mouse_pos_event(-FLT_MAX, -FLT_MAX);
            }
            gptPlatformExtCtx->bMouseTracked = false;
            break;
        }

        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        {
            int iButton = 0;
            if (tMsg == WM_LBUTTONDOWN || tMsg == WM_LBUTTONDBLCLK) { iButton = 0; }
            if (tMsg == WM_RBUTTONDOWN || tMsg == WM_RBUTTONDBLCLK) { iButton = 1; }
            if (tMsg == WM_MBUTTONDOWN || tMsg == WM_MBUTTONDBLCLK) { iButton = 2; }
            if (tMsg == WM_XBUTTONDOWN || tMsg == WM_XBUTTONDBLCLK) { iButton = (GET_XBUTTON_WPARAM(tWParam) == XBUTTON1) ? 3 : 4; }
            if(gptIO->_iMouseButtonsDown == 0 && GetCapture() == NULL)
                SetCapture(tHwnd);
            gptIO->_iMouseButtonsDown |= 1 << iButton;
            gptIOI->add_mouse_button_event(iButton, true);
            break;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            int iButton = 0;
            if (tMsg == WM_LBUTTONUP) { iButton = 0; }
            if (tMsg == WM_RBUTTONUP) { iButton = 1; }
            if (tMsg == WM_MBUTTONUP) { iButton = 2; }
            if (tMsg == WM_XBUTTONUP) { iButton = (GET_XBUTTON_WPARAM(tWParam) == XBUTTON1) ? 3 : 4; }
            gptIO->_iMouseButtonsDown &= ~(1 << iButton);
            if(gptIO->_iMouseButtonsDown == 0 && GetCapture() == tHwnd)
                ReleaseCapture();
            gptIOI->add_mouse_button_event(iButton, false);
            break;
        }

        case WM_MOUSEWHEEL:
        {
            gptIOI->add_mouse_wheel_event(0.0f, (float)GET_WHEEL_DELTA_WPARAM(tWParam) / (float)WHEEL_DELTA);
            break;
        }

        case WM_MOUSEHWHEEL:
        {
            gptIOI->add_mouse_wheel_event((float)GET_WHEEL_DELTA_WPARAM(tWParam) / (float)WHEEL_DELTA, 0.0f);
            break;
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            const bool bKeyDown = (tMsg == WM_KEYDOWN || tMsg == WM_SYSKEYDOWN);
            if (tWParam < 256)
            {

                // Submit modifiers
                gptIOI->add_key_event(PL_KEY_MOD_CTRL,  pl__is_vk_down(VK_CONTROL));
                gptIOI->add_key_event(PL_KEY_MOD_SHIFT, pl__is_vk_down(VK_SHIFT));
                gptIOI->add_key_event(PL_KEY_MOD_ALT,   pl__is_vk_down(VK_MENU));
                gptIOI->add_key_event(PL_KEY_MOD_SUPER, pl__is_vk_down(VK_APPS));

                // obtain virtual key code
                int iVk = (int)tWParam;
                if ((tWParam == VK_RETURN) && (HIWORD(tLParam) & KF_EXTENDED))
                    iVk = PL_VK_KEYPAD_ENTER;

                // submit key event
                const plKey tKey = pl__virtual_key_to_pl_key(iVk);

                if (tKey != PL_KEY_NONE)
                    gptIOI->add_key_event(tKey, bKeyDown);

                // Submit individual left/right modifier events
                if (iVk == VK_SHIFT)
                {
                    if (pl__is_vk_down(VK_LSHIFT) == bKeyDown) gptIOI->add_key_event(PL_KEY_LEFT_SHIFT, bKeyDown);
                    if (pl__is_vk_down(VK_RSHIFT) == bKeyDown) gptIOI->add_key_event(PL_KEY_RIGHT_SHIFT, bKeyDown);
                }
                else if (iVk == VK_CONTROL)
                {
                    if (pl__is_vk_down(VK_LCONTROL) == bKeyDown) gptIOI->add_key_event(PL_KEY_LEFT_CTRL, bKeyDown);
                    if (pl__is_vk_down(VK_RCONTROL) == bKeyDown) gptIOI->add_key_event(PL_KEY_RIGHT_CTRL, bKeyDown);
                }
                else if (iVk == VK_MENU)
                {
                    if (pl__is_vk_down(VK_LMENU) == bKeyDown) gptIOI->add_key_event(PL_KEY_LEFT_ALT, bKeyDown);
                    if (pl__is_vk_down(VK_RMENU) == bKeyDown) gptIOI->add_key_event(PL_KEY_RIGHT_ALT, bKeyDown);
                }
            }
            break;
        }

        case WM_CLOSE:
        {
            PostQuitMessage(0);
            break;
        }

    #ifdef PL_EXPERIMENTAL_RENDER_WHILE_RESIZE
        case WM_MOVE:
        case WM_MOVING:
        {
            pl__render_frame();
            break;
        }

        case WM_ENTERSIZEMOVE:
        {
            // DefWindowProc below will block until mouse is released or moved.
            // Timer events can still be caught so here we add a timer so we
            // can continue rendering when catching the WM_TIMER event.
            // Timer is killed in the WM_EXITSIZEMOVE case below.
            puIDEvent = SetTimer(NULL, puIDEvent, USER_TIMER_MINIMUM , NULL);
            SetTimer(tHwnd, puIDEvent, USER_TIMER_MINIMUM , NULL);
            break;
        }

        case WM_EXITSIZEMOVE:
        {
            KillTimer(tHwnd, puIDEvent);
            break;
        }

        case WM_TIMER:
        {
            if(tWParam == puIDEvent)
                pl__render_frame();
            break;
        }
    #else
	case WM_MOVING:
	{
        // TODO: possibly handle horizontal shift? check dpg
		// RECT rect = *(RECT*)(tLParam);
		// ptWindow->tDesc.iXPos = rect.left;
		// ptWindow->tDesc.iYPos = rect.top;
		break;
	}
    #endif
    }
    return DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
}

void
pl__convert_to_wide_string(const char* pcNarrowValue, wchar_t* pwWideValue)
{
    mbstate_t tState;
    memset(&tState, 0, sizeof(tState));
    size_t szLen = 1 + mbsrtowcs(NULL, &pcNarrowValue, 0, &tState);
    mbsrtowcs(pwWideValue, &pcNarrowValue, szLen, &tState);
}

plKey
pl__virtual_key_to_pl_key(WPARAM tWParam)
{
    switch (tWParam)
    {
        case VK_TAB:             return PL_KEY_TAB;
        case VK_LEFT:            return PL_KEY_LEFT_ARROW;
        case VK_RIGHT:           return PL_KEY_RIGHT_ARROW;
        case VK_UP:              return PL_KEY_UP_ARROW;
        case VK_DOWN:            return PL_KEY_DOWN_ARROW;
        case VK_PRIOR:           return PL_KEY_PAGE_UP;
        case VK_NEXT:            return PL_KEY_PAGE_DOWN;
        case VK_HOME:            return PL_KEY_HOME;
        case VK_END:             return PL_KEY_END;
        case VK_INSERT:          return PL_KEY_INSERT;
        case VK_DELETE:          return PL_KEY_DELETE;
        case VK_BACK:            return PL_KEY_BACKSPACE;
        case VK_SPACE:           return PL_KEY_SPACE;
        case VK_RETURN:          return PL_KEY_ENTER;
        case VK_ESCAPE:          return PL_KEY_ESCAPE;
        case VK_OEM_7:           return PL_KEY_APOSTROPHE;
        case VK_OEM_COMMA:       return PL_KEY_COMMA;
        case VK_OEM_MINUS:       return PL_KEY_MINUS;
        case VK_OEM_PERIOD:      return PL_KEY_PERIOD;
        case VK_OEM_2:           return PL_KEY_SLASH;
        case VK_OEM_1:           return PL_KEY_SEMICOLON;
        case VK_OEM_PLUS:        return PL_KEY_EQUAL;
        case VK_OEM_4:           return PL_KEY_LEFT_BRACKET;
        case VK_OEM_5:           return PL_KEY_BACKSLASH;
        case VK_OEM_6:           return PL_KEY_RIGHT_BRACKET;
        case VK_OEM_3:           return PL_KEY_GRAVE_ACCENT;
        case VK_CAPITAL:         return PL_KEY_CAPS_LOCK;
        case VK_SCROLL:          return PL_KEY_SCROLL_LOCK;
        case VK_NUMLOCK:         return PL_KEY_NUM_LOCK;
        case VK_SNAPSHOT:        return PL_KEY_PRINT_SCREEN;
        case VK_PAUSE:           return PL_KEY_PAUSE;
        case VK_NUMPAD0:         return PL_KEY_KEYPAD_0;
        case VK_NUMPAD1:         return PL_KEY_KEYPAD_1;
        case VK_NUMPAD2:         return PL_KEY_KEYPAD_2;
        case VK_NUMPAD3:         return PL_KEY_KEYPAD_3;
        case VK_NUMPAD4:         return PL_KEY_KEYPAD_4;
        case VK_NUMPAD5:         return PL_KEY_KEYPAD_5;
        case VK_NUMPAD6:         return PL_KEY_KEYPAD_6;
        case VK_NUMPAD7:         return PL_KEY_KEYPAD_7;
        case VK_NUMPAD8:         return PL_KEY_KEYPAD_8;
        case VK_NUMPAD9:         return PL_KEY_KEYPAD_9;
        case VK_DECIMAL:         return PL_KEY_KEYPAD_DECIMAL;
        case VK_DIVIDE:          return PL_KEY_KEYPAD_DIVIDE;
        case VK_MULTIPLY:        return PL_KEY_KEYPAD_MULTIPLY;
        case VK_SUBTRACT:        return PL_KEY_KEYPAD_SUBTRACT;
        case VK_ADD:             return PL_KEY_KEYPAD_ADD;
        case PL_VK_KEYPAD_ENTER: return PL_KEY_KEYPAD_ENTER;
        case VK_LSHIFT:          return PL_KEY_LEFT_SHIFT;
        case VK_LCONTROL:        return PL_KEY_LEFT_CTRL;
        case VK_LMENU:           return PL_KEY_LEFT_ALT;
        case VK_LWIN:            return PL_KEY_LEFT_SUPER;
        case VK_RSHIFT:          return PL_KEY_RIGHT_SHIFT;
        case VK_RCONTROL:        return PL_KEY_RIGHT_CTRL;
        case VK_RMENU:           return PL_KEY_RIGHT_ALT;
        case VK_RWIN:            return PL_KEY_RIGHT_SUPER;
        case VK_APPS:            return PL_KEY_MENU;
        case '0':                return PL_KEY_0;
        case '1':                return PL_KEY_1;
        case '2':                return PL_KEY_2;
        case '3':                return PL_KEY_3;
        case '4':                return PL_KEY_4;
        case '5':                return PL_KEY_5;
        case '6':                return PL_KEY_6;
        case '7':                return PL_KEY_7;
        case '8':                return PL_KEY_8;
        case '9':                return PL_KEY_9;
        case 'A':                return PL_KEY_A;
        case 'B':                return PL_KEY_B;
        case 'C':                return PL_KEY_C;
        case 'D':                return PL_KEY_D;
        case 'E':                return PL_KEY_E;
        case 'F':                return PL_KEY_F;
        case 'G':                return PL_KEY_G;
        case 'H':                return PL_KEY_H;
        case 'I':                return PL_KEY_I;
        case 'J':                return PL_KEY_J;
        case 'K':                return PL_KEY_K;
        case 'L':                return PL_KEY_L;
        case 'M':                return PL_KEY_M;
        case 'N':                return PL_KEY_N;
        case 'O':                return PL_KEY_O;
        case 'P':                return PL_KEY_P;
        case 'Q':                return PL_KEY_Q;
        case 'R':                return PL_KEY_R;
        case 'S':                return PL_KEY_S;
        case 'T':                return PL_KEY_T;
        case 'U':                return PL_KEY_U;
        case 'V':                return PL_KEY_V;
        case 'W':                return PL_KEY_W;
        case 'X':                return PL_KEY_X;
        case 'Y':                return PL_KEY_Y;
        case 'Z':                return PL_KEY_Z;
        case VK_F1:              return PL_KEY_F1;
        case VK_F2:              return PL_KEY_F2;
        case VK_F3:              return PL_KEY_F3;
        case VK_F4:              return PL_KEY_F4;
        case VK_F5:              return PL_KEY_F5;
        case VK_F6:              return PL_KEY_F6;
        case VK_F7:              return PL_KEY_F7;
        case VK_F8:              return PL_KEY_F8;
        case VK_F9:              return PL_KEY_F9;
        case VK_F10:             return PL_KEY_F10;
        case VK_F11:             return PL_KEY_F11;
        case VK_F12:             return PL_KEY_F12;
        default:                 return PL_KEY_NONE;
    }   
}

plWindowResult
pl_window_create(plWindowDesc tDesc, plWindow** pptWindowOut)
{

    // calculate window size based on desired client region size
    RECT tWr = 
    {
        .left = (LONG)tDesc.iXPos,
        .right = (LONG)(tDesc.uWidth + tDesc.iXPos),
        .top = (LONG)tDesc.iYPos,
        .bottom = (LONG)(tDesc.uHeight + tDesc.iYPos)
    };
    AdjustWindowRect(&tWr, WS_OVERLAPPEDWINDOW, FALSE);

    // convert title to wide chars
    wchar_t awWideTitle[1024];
    pl__convert_to_wide_string(tDesc.pcTitle, awWideTitle);

    // create window & get handle
    HWND tHandle = CreateWindowExW(
        0,
        gptPlatformExtCtx->tWc.lpszClassName,
        awWideTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        tWr.left, tWr.top, tWr.right - tWr.left, tWr.bottom - tWr.top,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL // user data
    );

    plWindow* ptWindow = PL_ALLOC(sizeof(plWindow));
    ptWindow->_pBackendData = tHandle;
    pl_sb_push(gptPlatformExtCtx->sbtWindows, ptWindow);
    *pptWindowOut = ptWindow;

    if(gptPlatformExtCtx->ptMainWindow == NULL)
        gptPlatformExtCtx->ptMainWindow = ptWindow;

    // show window
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy(plWindow* ptWindow)
{
    DestroyWindow(ptWindow->_pBackendData);
    PL_FREE(ptWindow);
}

void
pl_window_show(plWindow* ptWindow)
{
    ShowWindow(ptWindow->_pBackendData, SW_SHOWDEFAULT);
}

plWindowResult
pl_window_create_surface(plWindow* ptWindow, const plWindowSurfaceDesc* ptDesc, plWindowSurface** ptSurfaceOut)
{
    plWindowSurface* ptSurface = PL_ALLOC(sizeof(plWindowSurface));
    
    ptSurface->ptWindow = ptWindow;
    ptSurface->atImages = PL_ALLOC(sizeof(plWindowSurfaceImageWin32) * ptDesc->uImageCount);
    memset(ptSurface->atImages, 0, sizeof(plWindowSurfaceImageWin32) * ptDesc->uImageCount);
    ptSurface->uImageCount = ptDesc->uImageCount;

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

    HWND tHandle = ptWindow->_pBackendData;
    plWindowSurfaceImageWin32* ptImageWin32 = &ptSurface->atImages[ptSurface->uCurrentImage];
    ptImageWin32->tDc = GetDC(tHandle);
    PL_ASSERT(ptImageWin32->tDc != NULL);

    RECT tRect;
    GetClientRect(tHandle, &tRect);

    int iClientWidth  = tRect.right - tRect.left;
    int iClientHeight = tRect.bottom - tRect.top;

    uint32_t uPixelsNeeded = (uint32_t)iClientWidth * (uint32_t)iClientHeight;
    bool bResizeNeeded = uPixelsNeeded > ptImageWin32->uPixelCapacity;

    if(bResizeNeeded)
    {
        if(ptImageWin32->pPixels)
        {
            PL_FREE(ptImageWin32->pPixels);
            ptImageWin32->pPixels = NULL;
        }
        ptImageWin32->pPixels = PL_ALLOC(uPixelsNeeded * sizeof(uint32_t));
        memset(ptImageWin32->pPixels, 0, uPixelsNeeded * sizeof(uint32_t));
        ptImageWin32->uPixelCapacity = uPixelsNeeded;
    }

    ptImageOut->pPixels = ptImageWin32->pPixels;
    ptImageOut->uWidth = (uint32_t)iClientWidth;
    ptImageOut->uHeight = (uint32_t)iClientHeight;
    ptImageOut->uRowPitch = ptImageOut->uWidth * sizeof(uint32_t);
    ptImageOut->uImageIndex = ptSurface->uCurrentImage;
    ptImageOut->tFormat = PL_WINDOW_SURFACE_FORMAT_B8G8R8A8_UNORM;

    ptImageWin32->uWidth = ptImageOut->uWidth;
    ptImageWin32->uHeight = ptImageOut->uHeight;

    return true;
}


void
pl_window_present_surface_image(plWindowSurface* ptSurface, uint32_t uImageIndex)
{
    plWindow* ptWindow = ptSurface->ptWindow;
    HWND tHandle = ptWindow->_pBackendData;
    plWindowSurfaceImageWin32* ptImageWin32 = &ptSurface->atImages[uImageIndex];

    // PL_FORMAT_B8G8R8A8_UNORM
    BITMAPINFO bitmapInfo = {0};
    bitmapInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth       = (LONG)ptImageWin32->uWidth;
    bitmapInfo.bmiHeader.biHeight      = -(LONG)ptImageWin32->uHeight;
    bitmapInfo.bmiHeader.biPlanes      = 1;
    bitmapInfo.bmiHeader.biBitCount    = 32; // valid 32, 24, 16
    bitmapInfo.bmiHeader.biCompression = BI_RGB; // also valid BI_BITFIELDS
    bitmapInfo.bmiHeader.biSizeImage   = ptImageWin32->uWidth * ptImageWin32->uHeight * sizeof(uint32_t);

    int iResult = StretchDIBits(
        ptImageWin32->tDc,                      // device context
        0,  // x-dest
        0,  // y-dest
        ptImageWin32->uWidth,         // width-dest
        ptImageWin32->uHeight,        // height-dest
        0,                        // x-source
        0,                        // y-source
        ptImageWin32->uWidth,         // width-source
        ptImageWin32->uHeight,        // height-source
        ptImageWin32->pPixels,         // memory-source
        &bitmapInfo,              // bitmap info
        DIB_RGB_COLORS,           // array actually contains rgb values (instead of indices into palette)
        SRCCOPY);                 // copies the source rectangle directly to the destination rectangle

    PL_ASSERT(iResult != GDI_ERROR);

    ReleaseDC(tHandle, ptImageWin32->tDc);
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
    static plWindowCapabilities tCapabilities = {0};

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

void
pl__update_mouse_cursor(void)
{

    bool bCursorChanged = false;

    // updating mouse cursor
    if(gptIO->tCurrentCursor != gptIO->tNextCursor)
        bCursorChanged = true;

    if(bCursorChanged)
    {
        gptIO->tCurrentCursor = gptIO->tNextCursor;
        LPTSTR tWin32Cursor = IDC_ARROW;
        switch (gptIO->tNextCursor)
        {
            case PL_MOUSE_CURSOR_ARROW:       tWin32Cursor = IDC_ARROW; break;
            case PL_MOUSE_CURSOR_TEXT_INPUT:  tWin32Cursor = IDC_IBEAM; break;
            case PL_MOUSE_CURSOR_RESIZE_ALL:  tWin32Cursor = IDC_SIZEALL; break;
            case PL_MOUSE_CURSOR_RESIZE_EW:   tWin32Cursor = IDC_SIZEWE; break;
            case PL_MOUSE_CURSOR_RESIZE_NS:   tWin32Cursor = IDC_SIZENS; break;
            case PL_MOUSE_CURSOR_RESIZE_NESW: tWin32Cursor = IDC_SIZENESW; break;
            case PL_MOUSE_CURSOR_RESIZE_NWSE: tWin32Cursor = IDC_SIZENWSE; break;
            case PL_MOUSE_CURSOR_HAND:        tWin32Cursor = IDC_HAND; break;
            case PL_MOUSE_CURSOR_NOT_ALLOWED: tWin32Cursor = IDC_NO; break;
        }
        SetCursor(LoadCursor(NULL, tWin32Cursor));    
    }
    gptIO->tNextCursor = gptIO->tCurrentCursor;
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
    BOOL bResult = DeleteFile(pcFile);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
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
pl_file_copy(const char* pcSource, const char* pcDestination)
{
    BOOL bResult = CopyFile(pcSource, pcDestination, FALSE);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
}

bool
pl_file_directory_exists(const char* pcPath)
{
  DWORD dwAttrib = GetFileAttributes(pcPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

plFileResult
pl_file_create_directory(const char* pcPath)
{
    if(pl_file_directory_exists(pcPath))
        return PL_FILE_DIRECTORY_ALREADY_EXIST;
    BOOL bResult = CreateDirectoryA(pcPath, NULL);
    if(bResult != 0)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
}

plFileResult
pl_file_remove_directory(const char* pcPath)
{
    BOOL bResult = RemoveDirectoryA(pcPath);
    if(bResult != 0)
        return PL_FILE_RESULT_SUCCESS;
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
    char acFixedName[PL_MAX_PATH_LENGTH] = {0};
    size_t szLen = strnlen(pcPath, PL_MAX_PATH_LENGTH);
    if(pcPath[szLen - 1] == '/')
        sprintf(acFixedName, "%s*", pcPath);
    else
        sprintf(acFixedName, "%s/*", pcPath);
    WIN32_FIND_DATAA tFindData = {0};
    HANDLE tFoundHandle = FindFirstFileA(acFixedName, &tFindData); // should be "."

    BOOL bResult = FindNextFileA(tFoundHandle, &tFindData); // should be ".."
    bResult = FindNextFileA(tFoundHandle, &tFindData);

    while(bResult != 0)
    {
        

        pl_sb_add(ptInfoOut->sbtEntries);
        plDirectoryEntry* ptNewEntry = &pl_sb_top(ptInfoOut->sbtEntries);

        if(tFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ptInfoOut->uDirectoryCount++;
            ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_DIRECTORY;
        }
        else
        {
            ptInfoOut->uFileCount++;
            ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_FILE;
        }
        strncpy(ptNewEntry->acName, tFindData.cFileName, PL_MAX_PATH_LENGTH);

        // (nFileSizeHigh * (MAXDWORD+1)) + nFileSizeLow

        bResult = FindNextFileA(tFoundHandle, &tFindData);
    }
    ptInfoOut->uEntryCount = pl_sb_size(ptInfoOut->sbtEntries);
    FindClose(tFoundHandle);
    return PL_FILE_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] atomics api
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounter
{
    int64_t ilValue;
} plAtomicCounter;

plAtomicsResult
pl_atomics_create_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = (plAtomicCounter*)_aligned_malloc(sizeof(plAtomicCounter), 8);
    (*ptCounter)->ilValue = ilValue;
    return PL_ATOMICS_RESULT_SUCCESS;
}

void
pl_atomics_destroy_counter(plAtomicCounter** ptCounter)
{
    _aligned_free((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl_atomics_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    ptCounter->ilValue = ilValue;
}

int64_t
pl_atomics_load(plAtomicCounter* ptCounter)
{
    return ptCounter->ilValue;
}

bool
pl_atomics_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return InterlockedCompareExchange64(&ptCounter->ilValue, ilDesiredValue, ilExpectedValue) == ilExpectedValue;
}

int64_t
pl_atomics_increment(plAtomicCounter* ptCounter)
{
    return InterlockedIncrement64(&ptCounter->ilValue);
}

int64_t
pl_atomics_decrement(plAtomicCounter* ptCounter)
{
    return InterlockedDecrement64(&ptCounter->ilValue);
}

//-----------------------------------------------------------------------------
// [SECTION] network api
//-----------------------------------------------------------------------------

typedef struct _plNetworkAddress
{
    struct addrinfo* tInfo;
} plNetworkAddress;

typedef struct _plSocket
{
    SOCKET tSocket;
    bool     bInitialized;
    plSocketFlags tFlags;
} plSocket;

bool
pl_network_initialize(void)
{
    WSADATA tWsaData = {0};
    if(WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0)
    {
        printf("Failed to start winsock with error code: %d\n", WSAGetLastError());
        return false;
    }
    return true;
}

void
pl_network_cleanup(void)
{
    WSACleanup();
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
        printf("Could not create address : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }

    *pptAddress = (plNetworkAddress*)PL_ALLOC(sizeof(plNetworkAddress));
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
    *pptSocketOut = (plSocket*)PL_ALLOC(sizeof(plSocket));
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

    closesocket(ptSocket->tSocket);

    PL_FREE(ptSocket);
    *pptSocket = NULL;
}

plNetworkResult
pl_network_send_socket_data_to(plSocket* ptFromSocket, plNetworkAddress* ptAddress, const void* pData, size_t szSize, size_t* pszSentSize)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptFromSocket->tSocket == INVALID_SOCKET)
        {
            printf("Could not create socket : %d\n", WSAGetLastError());
            return 0;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            u_long uMode = 1;
            ioctlsocket(ptFromSocket->tSocket, FIONBIO, &uMode);
        }

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = sendto(ptFromSocket->tSocket, (const char*)pData, (int)szSize, 0, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);
    if(iResult == SOCKET_ERROR)
    {
        printf("sendto() failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }

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

        if(ptSocket->tSocket == INVALID_SOCKET)
        {
            printf("Could not create socket : %d\n", WSAGetLastError());
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            u_long uMode = 1;
            ioctlsocket(ptSocket->tSocket, FIONBIO, &uMode);
        }

        ptSocket->bInitialized = true;
    }

    // bind socket
    if(bind(ptSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Bind socket failed with error code : %d\n", WSAGetLastError());
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
   

    if(iRecvLen == SOCKET_ERROR)
    {
        const int iLastError = WSAGetLastError();
        if(iLastError != WSAEWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
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

        if(ptFromSocket->tSocket == INVALID_SOCKET)
        {
            printf("Could not create socket : %d\n", WSAGetLastError());
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            u_long uMode = 1;
            ioctlsocket(ptFromSocket->tSocket, FIONBIO, &uMode);
        }

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = connect(ptFromSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);
    if(iResult)
    {
        printf("connect() failed with error code : %d\n", WSAGetLastError());
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
    fd_set tReads;
    FD_ZERO(&tReads);
    for(uint32_t i = 0; i < uSocketCount; i++)
    {
        FD_SET(ptSockets[i]->tSocket, &tReads);
    }

    struct timeval tTimeout = {0};
    tTimeout.tv_sec = 0;
    tTimeout.tv_usec = (int)uTimeOutMilliSec * 1000;

    if(select(0, &tReads, NULL, NULL, &tTimeout) < 0)
    {
        printf("select socket failed with error code : %d\n", WSAGetLastError());
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

    if(tSocketClient == INVALID_SOCKET)
        return PL_NETWORK_RESULT_FAIL;

    *pptSocketOut = (plSocket*)PL_ALLOC(sizeof(plSocket));
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
    if(iResult == SOCKET_ERROR)
        return PL_NETWORK_RESULT_FAIL;
    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] threads api
//-----------------------------------------------------------------------------

typedef struct _plThreadData
{
  plThreadProcedure ptProcedure;
  void*             pData;
} plThreadData;

typedef struct _plThread
{
    HANDLE        tHandle;
    plThreadData* ptData;
    uint64_t      uID;
} plThread;

typedef struct _plMutex
{
    HANDLE tHandle;
} plMutex;

typedef struct _plCriticalSection
{
    CRITICAL_SECTION tHandle;
} plCriticalSection;

typedef struct _plSemaphore
{
    HANDLE tHandle;
} plSemaphore;

typedef struct _plBarrier
{
    SYNCHRONIZATION_BARRIER tHandle;
} plBarrier;

typedef struct _plConditionVariable
{
    CONDITION_VARIABLE tHandle;
} plConditionVariable;

typedef struct _plThreadKey
{
    DWORD dwIndex;
} plThreadKey;

void
pl_threads_sleep_thread(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
}

static DWORD 
thread_procedure(void* lpParam)
{
    plThreadData* ptData = (plThreadData*)lpParam;
    ptData->ptProcedure(ptData->pData);
    return 1;
}

static void
thread_yield(void)
{
    SwitchToThread();
}

plThreadResult
pl_threads_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** ppThreadOut)
{
    plThreadData* ptData = (plThreadData*)PL_ALLOC(sizeof(plThreadData));
    ptData->ptProcedure = ptProcedure;
    ptData->pData       = pData;

    HANDLE tHandle = CreateThread(0, 1024, thread_procedure, ptData, 0, NULL);
    if(tHandle)
    {
        DWORD tID = GetThreadId(tHandle);
        *ppThreadOut = (plThread*)PL_ALLOC(sizeof(plThread));
        (*ppThreadOut)->ptData = ptData;
        (*ppThreadOut)->tHandle = tHandle;
        (*ppThreadOut)->uID = (uint64_t)tID;
        return PL_THREAD_RESULT_SUCCESS;
    }
    PL_FREE(ptData);
    return PL_THREAD_RESULT_FAIL;
    
}

void
pl_threads_join_thread(plThread* ptThread)
{
    WaitForSingleObject(ptThread->tHandle, INFINITE);
}

void
pl_threads_destroy_thread(plThread** ppThread)
{
    pl_threads_join_thread(*ppThread);
    CloseHandle((*ppThread)->tHandle);
    PL_FREE((*ppThread)->ptData);
    PL_FREE(*ppThread);
    *ppThread = NULL;
}

void
pl_threads_yield_thread(void)
{
    thread_yield();
}

plThreadResult
pl_threads_create_mutex(plMutex** ppMutexOut)
{
    HANDLE tHandle = CreateMutex(NULL, FALSE, NULL);
    if(tHandle)
    {
        (*ppMutexOut) = (plMutex*)PL_ALLOC(sizeof(plMutex));
        // (*ppMutexOut)->tHandle = CreateMutex(NULL, FALSE, NULL);
        (*ppMutexOut)->tHandle = tHandle;
        return PL_THREAD_RESULT_SUCCESS;
    }
    return PL_THREAD_RESULT_FAIL;
}

void
pl_threads_destroy_mutex(plMutex** ptMutex)
{
    CloseHandle((*ptMutex)->tHandle);
    PL_FREE((*ptMutex));
    (*ptMutex) = NULL;
}

void
pl_threads_lock_mutex(plMutex* ptMutex)
{
    DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
}

void
pl_threads_unlock_mutex(plMutex* ptMutex)
{
    if(!ReleaseMutex(ptMutex->tHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

plThreadResult
pl_threads_create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    (*pptCriticalSectionOut) = (plCriticalSection*)PL_ALLOC(sizeof(plCriticalSection));
    InitializeCriticalSection(&(*pptCriticalSectionOut)->tHandle);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    DeleteCriticalSection(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    (*pptCriticalSection) = NULL;
}

void
pl_threads_enter_critical_section(plCriticalSection* ptCriticalSection)
{
    EnterCriticalSection(&ptCriticalSection->tHandle);
}

void
pl_threads_leave_critical_section(plCriticalSection* ptCriticalSection)
{
    LeaveCriticalSection(&ptCriticalSection->tHandle);
}

uint32_t
pl_threads_get_hardware_thread_count(void)
{

    static uint32_t uThreadCount = 0;

    if(uThreadCount == 0)
    {
        DWORD dwLength = 0;
        GetLogicalProcessorInformation(NULL, &dwLength);
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION atInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)PL_ALLOC(dwLength);
        GetLogicalProcessorInformation(atInfo, &dwLength);
        uint32_t uEntryCount = dwLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for(uint32_t i = 0; i < uEntryCount; i++)
        {
            if(atInfo[i].Relationship == RelationProcessorCore)
                uThreadCount++;
        }
        PL_FREE(atInfo);
    }
    return uThreadCount;
}

plThreadResult
pl_threads_create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    (*pptBarrierOut) = (plBarrier*)PL_ALLOC(sizeof(plBarrier));
    InitializeSynchronizationBarrier(&(*pptBarrierOut)->tHandle, uThreadCount, -1);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_barrier(plBarrier** pptBarrier)
{
    DeleteSynchronizationBarrier(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
}

void
pl_threads_wait_on_barrier(plBarrier* ptBarrier)
{
    EnterSynchronizationBarrier(&ptBarrier->tHandle, 0);
}

plThreadResult
pl_threads_create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    (*pptSemaphoreOut) = (plSemaphore*)PL_ALLOC(sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = CreateSemaphore(NULL, 0, uIntialCount, NULL);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_destroy_semaphore(plSemaphore** pptSemaphore)
{
    CloseHandle((*pptSemaphore)->tHandle);
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl_threads_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    WaitForSingleObject(ptSemaphore->tHandle, INFINITE);
}

bool
pl_threads_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    DWORD dwWaitResult = WaitForSingleObject(ptSemaphore->tHandle, 0);
    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0: return true;
        case WAIT_TIMEOUT:  return false;
    }
    PL_ASSERT(false);
    return false;
}

void
pl_threads_release_semaphore(plSemaphore* ptSemaphore)
{
    if (!ReleaseSemaphore( 
            ptSemaphore->tHandle,  // handle to semaphore
            1,            // increase count by one
            NULL) )       // not interested in previous count
    {
        printf("ReleaseSemaphore error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

plThreadResult
pl_threads_allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    *pptKeyOut = (plThreadKey*)PL_ALLOC(sizeof(plThreadKey));
    memset(*pptKeyOut, 0, sizeof(plThreadKey));
    (*pptKeyOut)->dwIndex = TlsAlloc();
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_threads_free_thread_local_key(plThreadKey** pptKey)
{
    TlsFree((*pptKey)->dwIndex);
    PL_FREE((*pptKey));
    *pptKey = NULL;
}

void*
pl_threads_allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    LPVOID lpvData = LocalAlloc(LPTR, szSize);
    if(!TlsSetValue(ptKey->dwIndex, lpvData)) 
    {
        PL_ASSERT(false);
        return NULL;
    }
    return lpvData;
}

void*
pl_threads_get_thread_local_data(plThreadKey* ptKey)
{
    LPVOID lpvData =  TlsGetValue(ptKey->dwIndex);
    if(lpvData == NULL)
    {
        PL_ASSERT(false);
    }
    return lpvData;
}

uint64_t
pl_threads_get_thread_id(plThread* ptThread)
{
    return ptThread->uID;
}

uint64_t
pl_threads_get_current_thread_id(void)
{
    DWORD tID = GetCurrentThreadId();
    return (uint64_t)tID;
}

void
pl_threads_free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    LPVOID lpvData = TlsGetValue(ptKey->dwIndex);
    LocalFree(lpvData);
}

plThreadResult
pl_threads_create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    *pptConditionVariableOut =(plConditionVariable*)PL_ALLOC(sizeof(plConditionVariable));
    InitializeConditionVariable(&(*pptConditionVariableOut)->tHandle);
    return PL_THREAD_RESULT_SUCCESS;
}

void               
pl_threads_destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
}

void               
pl_threads_wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    WakeConditionVariable(&ptConditionVariable->tHandle);
}

void               
pl_threads_wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    WakeAllConditionVariable(&ptConditionVariable->tHandle);
}

void               
pl_threads_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    SleepConditionVariableCS(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle, INFINITE);
}

//-----------------------------------------------------------------------------
// [SECTION] virtual memory api
//-----------------------------------------------------------------------------

size_t
pl_virtual_memory_get_page_size(void)
{
    SYSTEM_INFO tInfo = {0};
    GetSystemInfo(&tInfo);
    return (size_t)tInfo.dwPageSize;
}

void*
pl_virtual_memory_alloc(size_t szSize)
{
    return VirtualAlloc(NULL, szSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void*
pl_virtual_memory_reserve(size_t szSize)
{
    return VirtualAlloc(NULL, szSize, MEM_RESERVE, PAGE_NOACCESS);
}

void*
pl_virtual_memory_commit(void* pAddress, size_t szSize)
{
    return VirtualAlloc(pAddress, szSize, MEM_COMMIT, PAGE_READWRITE);
}

void
pl_virtual_memory_free(void* pAddress, size_t szSize)
{
    BOOL bResult = VirtualFree(pAddress, 0, MEM_RELEASE);
    if(!bResult)
    {
        printf("VirtualFree failed : %d\n", GetLastError());
        PL_ASSERT(false);
    };
}

void
pl_virtual_memory_decommit(void* pAddress, size_t szSize)
{
    BOOL bResult = VirtualFree(pAddress, szSize, MEM_DECOMMIT);
    if(!bResult)
    {
        printf("VirtualFree failed : %d\n", GetLastError());
        PL_ASSERT(false);
    };
}

const char*
pl_get_clipboard_text(void* user_data_ctx)
{

    pl_sb_reset(gptIO->sbcClipboardData);
    if (!OpenClipboard(NULL))
        return NULL;
    HANDLE wbuf_handle = GetClipboardData(CF_UNICODETEXT);
    if (wbuf_handle == NULL)
    {
        CloseClipboard();
        return NULL;
    }
    const WCHAR* wbuf_global = (const WCHAR*)GlobalLock(wbuf_handle);
    if (wbuf_global)
    {
        int buf_len = WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, NULL, 0, NULL, NULL);
        pl_sb_resize(gptIO->sbcClipboardData, buf_len);
        WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, gptIO->sbcClipboardData, buf_len, NULL, NULL);
    }
    GlobalUnlock(wbuf_handle);
    CloseClipboard();
    return gptIO->sbcClipboardData;
}

void
pl_set_clipboard_text(void* pUnused, const char* text)
{
    if (!OpenClipboard(NULL))
        return;
    const int wbuf_length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    HGLOBAL wbuf_handle = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)wbuf_length * sizeof(WCHAR));
    if (wbuf_handle == NULL)
    {
        CloseClipboard();
        return;
    }
    WCHAR* wbuf_global = (WCHAR*)GlobalLock(wbuf_handle);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wbuf_global, wbuf_length);
    GlobalUnlock(wbuf_handle);
    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, wbuf_handle) == NULL)
        GlobalFree(wbuf_handle);
    CloseClipboard();
}

#include "pl_platform_ext.c"