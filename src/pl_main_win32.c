/*
   win32_pl.c
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] entry point
// [SECTION] key code conversion
// [SECTION] windows procedure
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Ole32.lib")

#ifdef _DEBUG
#pragma comment(lib, "ucrtd.lib")
#else
#pragma comment(lib, "ucrt.lib")
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_io.h"       // io context
#include "pl_os.h"       // shared library api
#include "pl_registry.h" // data registry
#include <stdlib.h>      // exit
#include <stdio.h>       // printf
#include <wchar.h>       // mbsrtowcs, wcsrtombs
#include <float.h>       // FLT_MAX, FLT_MIN
#include <winsock2.h>    // sockets
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

static LRESULT CALLBACK pl__windows_procedure(HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam);
static void             pl__convert_to_wide_string(const char* narrowValue, wchar_t* wideValue);
static void             pl__render_frame(void);

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static HWND            gtHandle = NULL;
static HWND            gtMouseHandle = NULL;
static bool            gbMouseTracked = false;
static bool            gbMinimized = false;
static plSharedLibrary gtAppLibrary = {0};
static void*           gpUserData = NULL;
static bool            gbRunning = true;
static INT64           gilTime;
static INT64           gilTicksPerSecond;
plIOContext            gtIOContext = {0};

typedef struct _plAppData plAppData;
static void* (*pl_app_load)    (plIOContext* ptIOCtx, plAppData* userData);
static void  (*pl_app_shutdown)(plAppData* userData);
static void  (*pl_app_resize)  (plAppData* userData);
static void  (*pl_app_update)  (plAppData* userData);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{

    // initialize winsock
    WSADATA tWsaData = {0};
    if(WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0)
    {
        printf("Failed to start winsock with error code: %d\n", WSAGetLastError());
        return -1;
    }

    // setup & retrieve io context
    pl_initialize_io_context(&gtIOContext);
    plIOContext* ptIOCtx = pl_get_io_context();

    // setup

    if (!QueryPerformanceFrequency((LARGE_INTEGER*)&gilTicksPerSecond))
        return -1;
    if (!QueryPerformanceCounter((LARGE_INTEGER*)&gilTime))
        return -1;

    // register window class
    const WNDCLASSEXW tWc = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = pl__windows_procedure,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = GetModuleHandle(NULL),
        .hIcon = NULL,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = NULL,
        .lpszMenuName = NULL,
        .lpszClassName = L"Pilot Light (win32)",
        .hIconSm = NULL
    };
    RegisterClassExW(&tWc);

    // calculate window size based on desired client region size
    RECT tWr = 
    {
        .left = 0,
        .right = 500 + tWr.left,
        .top = 0,
        .bottom = 500 + tWr.top
    };
    AdjustWindowRect(&tWr, WS_OVERLAPPEDWINDOW, FALSE);

    ptIOCtx->afMainViewportSize[0] = 500.0f;
    ptIOCtx->afMainViewportSize[1] = 500.0f;

    wchar_t awWideTitle[PL_MAX_NAME_LENGTH];
    #ifdef PL_VULKAN_BACKEND
    pl__convert_to_wide_string("Pilot Light (win32/vulkan)", awWideTitle);
    #elif PL_DX11_BACKEND
    pl__convert_to_wide_string("Pilot Light (win32/dx11)", awWideTitle);
    #endif

    // create window & get handle
    gtHandle = CreateWindowExW(
        0,
        tWc.lpszClassName,
        awWideTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        0, 0, tWr.right - tWr.left, tWr.bottom - tWr.top,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL // user data
    );
    ptIOCtx->pBackendPlatformData = &gtHandle;

    // setup console
    DWORD tCurrentMode = 0;
    HANDLE tStdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(tStdOutHandle == INVALID_HANDLE_VALUE) exit(GetLastError());
    if(!GetConsoleMode(tStdOutHandle, &tCurrentMode)) exit(GetLastError());
    const DWORD tOriginalMode = tCurrentMode;
    tCurrentMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // enable ANSI escape codes
    if(!SetConsoleMode(tStdOutHandle, tCurrentMode)) exit(GetLastError());

    // load library
    if(pl_load_library(&gtAppLibrary, "./app.dll", "./app_", "./lock.tmp"))
    {
        pl_app_load     = (void* (__cdecl  *)(plIOContext*, plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_update");
        gpUserData = pl_app_load(ptIOCtx, NULL);
    }

    // show window
    ShowWindow(gtHandle, SW_SHOWDEFAULT);

    // main loop
    while (gbRunning)
    {

        // while queue has messages, remove and dispatch them (but do not block on empty queue)
        MSG tMsg = {0};
        while (PeekMessage(&tMsg, NULL, 0, 0, PM_REMOVE))
        {
            // check for quit because peekmessage does not signal this via return val
            if (tMsg.message == WM_QUIT)
            {
                gbRunning = false;
                break;
            }
            // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
            TranslateMessage(&tMsg);
            DispatchMessage(&tMsg);
        }

        // reload library
        if(pl_has_library_changed(&gtAppLibrary))
        {
            pl_reload_library(&gtAppLibrary);
            pl_app_load     = (void* (__cdecl *)(plIOContext*, plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gtAppLibrary, "pl_app_update");
            gpUserData = pl_app_load(ptIOCtx, gpUserData);
        }

        // render a frame
        if(gbRunning)
            pl__render_frame();
    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    // cleanup win32 stuff
    UnregisterClassW(tWc.lpszClassName, GetModuleHandle(NULL));
    DestroyWindow(gtHandle);
    gtHandle = NULL;
    ptIOCtx->pBackendPlatformData = NULL;

    // cleanup io context
    pl_cleanup_io_context();

    // return console to original mode
    if(!SetConsoleMode(tStdOutHandle, tOriginalMode)) exit(GetLastError());

    // cleanup winsock
    WSACleanup();
}

//-----------------------------------------------------------------------------
// [SECTION] key code conversion
//-----------------------------------------------------------------------------

#define PL_VK_KEYPAD_ENTER (VK_RETURN + 256)

static bool
pl__is_vk_down(int iVk)
{
    return (GetKeyState(iVk) & 0x8000) != 0;
}

static plKey
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

//-----------------------------------------------------------------------------
// [SECTION] windows procedure
//-----------------------------------------------------------------------------

static LRESULT CALLBACK 
pl__windows_procedure(HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    LRESULT tResult = 0;
    static UINT_PTR puIDEvent = 0;
    switch (tMsg)
    {

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
                ptIOCtx->afMainViewportSize[0] = (float)iCWidth;
                ptIOCtx->afMainViewportSize[1] = (float)iCHeight;

                // give app change to handle resize
                pl_app_resize(gpUserData);

                // send paint message
                InvalidateRect(tHwnd, NULL, TRUE);
            }
            break;
        }

        case WM_MOVE:
        case WM_MOVING:
        {
            pl__render_frame();
            break;
        }

        case WM_PAINT:
        {
            pl__render_frame();

            // must be called for the OS to do its thing
            PAINTSTRUCT tPaint;
            HDC tDeviceContext = BeginPaint(tHwnd, &tPaint);  
            EndPaint(tHwnd, &tPaint); 
            break;
        }

        case WM_CLOSE:
        {
            PostQuitMessage(0);
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

        case WM_MOUSEMOVE:
        {
            gtMouseHandle = tHwnd;
            if(!gbMouseTracked)
            {
                TRACKMOUSEEVENT tTme = { sizeof(tTme), TME_LEAVE, tHwnd, 0 };
                TrackMouseEvent(&tTme);
                gbMouseTracked = true;        
            }
            POINT tMousePos = { (LONG)GET_X_LPARAM(tLParam), (LONG)GET_Y_LPARAM(tLParam) };
            pl_add_mouse_pos_event((float)tMousePos.x, (float)tMousePos.y);
            break;
        }
        case WM_MOUSELEAVE:
        {
            if(tHwnd == gtMouseHandle)
            {
                gtMouseHandle = NULL;
                pl_add_mouse_pos_event(-FLT_MAX, -FLT_MAX);
            }
            gbMouseTracked = false;
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
            if(ptIOCtx->_iMouseButtonsDown == 0 && GetCapture() == NULL)
                SetCapture(tHwnd);
            ptIOCtx->_iMouseButtonsDown |= 1 << iButton;
            pl_add_mouse_button_event(iButton, true);
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
            ptIOCtx->_iMouseButtonsDown &= ~(1 << iButton);
            if(ptIOCtx->_iMouseButtonsDown == 0 && GetCapture() == tHwnd)
                ReleaseCapture();
            pl_add_mouse_button_event(iButton, false);
            break;
        }

        case WM_MOUSEWHEEL:
        {
            pl_add_mouse_wheel_event(0.0f, (float)GET_WHEEL_DELTA_WPARAM(tWParam) / (float)WHEEL_DELTA);
            return 0;
        }

        case WM_MOUSEHWHEEL:
        {
            pl_add_mouse_wheel_event((float)GET_WHEEL_DELTA_WPARAM(tWParam) / (float)WHEEL_DELTA, 0.0f);
            return 0;
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
                pl_add_key_event(PL_KEY_MOD_CTRL,  pl__is_vk_down(VK_CONTROL));
                pl_add_key_event(PL_KEY_MOD_SHIFT, pl__is_vk_down(VK_SHIFT));
                pl_add_key_event(PL_KEY_MOD_ALT,   pl__is_vk_down(VK_MENU));
                pl_add_key_event(PL_KEY_MOD_SUPER, pl__is_vk_down(VK_APPS));

                // obtain virtual key code
                int iVk = (int)tWParam;
                if ((tWParam == VK_RETURN) && (HIWORD(tLParam) & KF_EXTENDED))
                {
                    iVk = PL_VK_KEYPAD_ENTER;
                }

                // submit key event
                const plKey tKey = pl__virtual_key_to_pl_key(iVk);

                if (tKey != PL_KEY_NONE)
                {
                    pl_add_key_event(tKey, bKeyDown);
                }

                // Submit individual left/right modifier events
                if (iVk == VK_SHIFT)
                {
                    if (pl__is_vk_down(VK_LSHIFT) == bKeyDown) pl_add_key_event(PL_KEY_LEFT_SHIFT, bKeyDown);
                    if (pl__is_vk_down(VK_RSHIFT) == bKeyDown) pl_add_key_event(PL_KEY_RIGHT_SHIFT, bKeyDown);
                }
                else if (iVk == VK_CONTROL)
                {
                    if (pl__is_vk_down(VK_LCONTROL) == bKeyDown) pl_add_key_event(PL_KEY_LEFT_CTRL, bKeyDown);
                    if (pl__is_vk_down(VK_RCONTROL) == bKeyDown) pl_add_key_event(PL_KEY_RIGHT_CTRL, bKeyDown);
                }
                else if (iVk == VK_MENU)
                {
                    if (pl__is_vk_down(VK_LMENU) == bKeyDown) pl_add_key_event(PL_KEY_LEFT_ALT, bKeyDown);
                    if (pl__is_vk_down(VK_RMENU) == bKeyDown) pl_add_key_event(PL_KEY_RIGHT_ALT, bKeyDown);
                }
                tResult = 0;
                break;
            }
        }

        case WM_SYSCOMMAND:
        {
            if(tWParam == SC_MINIMIZE)     gbMinimized = true;
            else if(tWParam == SC_RESTORE) gbMinimized = false;
            tResult = DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
            break;
        }

        default:
        {
            tResult = DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
        }
    }
    return tResult;
}

//-----------------------------------------------------------------------------
// [SECTION] misc
//-----------------------------------------------------------------------------

static void
pl__convert_to_wide_string(const char* pcNarrowValue, wchar_t* pwWideValue)
{
    mbstate_t tState;
    memset(&tState, 0, sizeof(tState));
    size_t szLen = 1 + mbsrtowcs(NULL, &pcNarrowValue, 0, &tState);
    mbsrtowcs(pwWideValue, &pcNarrowValue, szLen, &tState);
}

static void
pl__render_frame(void)
{
    // setup time step
    INT64 ilCurrentTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
    pl_get_io_context()->fDeltaTime = (float)(ilCurrentTime - gilTime) / gilTicksPerSecond;
    gilTime = ilCurrentTime;
    
    if(!gbMinimized)
        pl_app_update(gpUserData);   
}