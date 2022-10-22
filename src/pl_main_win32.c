/*
   win32_pl.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] entry point
// [SECTION] key code conversion
// [SECTION] windows procedure
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_io.h"    // io context
#include "pl_os.h"    // shared library api
#include <stdlib.h>   // exit
#include <wchar.h>    // mbsrtowcs, wcsrtombs
#include <float.h>    // FLT_MAX, FLT_MIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

#ifdef _DEBUG
#pragma comment(lib, "ucrtd.lib")
#else
#pragma comment(lib, "ucrt.lib")
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

static LRESULT CALLBACK pl__windows_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static void             pl__convert_to_wide_string(const char* narrowValue, wchar_t* wideValue);
static void             pl__render_frame(void);

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static HWND            gHandle = NULL;
static HWND            gMouseHandle = NULL;
static bool            gMouseTracked = false;
static plSharedLibrary gAppLibrary = {0};
static void*           gUserData = NULL;
static bool            gRunning = true;
static INT64           gTime;
static INT64           gTicksPerSecond;
plIOContext            gtIOContext = {0};

typedef struct plAppData_t plAppData;
static void* (*pl_app_load)    (plIOContext* ptIOCtx, plAppData* userData);
static void  (*pl_app_setup)   (plAppData* userData);
static void  (*pl_app_shutdown)(plAppData* userData);
static void  (*pl_app_resize)  (plAppData* userData);
static void  (*pl_app_render)  (plAppData* userData);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{

    // setup & retrieve io context
    pl_initialize_io_context(&gtIOContext);
    plIOContext* ptIOCtx = pl_get_io_context();

    if (!QueryPerformanceFrequency((LARGE_INTEGER*)&gTicksPerSecond))
        return -1;
    if (!QueryPerformanceCounter((LARGE_INTEGER*)&gTime))
        return -1;

    // load library
    if(pl_load_library(&gAppLibrary, "app.dll", "app_", "lock.tmp"))
    {
        pl_app_load     = (void* (__cdecl  *)(plIOContext*, plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_load");
        pl_app_setup    = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_setup");
        pl_app_shutdown = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_resize");
        pl_app_render   = (void  (__cdecl  *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_render");
        gUserData = pl_app_load(ptIOCtx, NULL);
    }

    // register window class
    WNDCLASSEXW wc = {
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
    RegisterClassExW(&wc);

    // calculate window size based on desired client region size
    RECT wr = 
    {
        .left = 0,
        .right = 500 + wr.left,
        .top = 0,
        .bottom = 500 + wr.top
    };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    wchar_t wideTitle[PL_MAX_NAME_LENGTH];
    #ifdef PL_VULKAN_BACKEND
    pl__convert_to_wide_string("Pilot Light (win32/vulkan)", wideTitle);
    #elif PL_DX11_BACKEND
    pl__convert_to_wide_string("Pilot Light (win32/dx11)", wideTitle);
    #endif

    // create window & get handle
    gHandle = CreateWindowExW(
        0,
        wc.lpszClassName,
        wideTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        0, 0, wr.right - wr.left, wr.bottom - wr.top,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL // user data
    );
    ptIOCtx->pBackendPlatformData = &gHandle;

    // setup console
    DWORD tCurrentMode = 0;
    HANDLE tStdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(tStdOutHandle == INVALID_HANDLE_VALUE) exit(GetLastError());
    if(!GetConsoleMode(tStdOutHandle, &tCurrentMode)) exit(GetLastError());
    const DWORD tOriginalMode = tCurrentMode;
    tCurrentMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // enable ANSI escape codes
    if(!SetConsoleMode(tStdOutHandle, tCurrentMode)) exit(GetLastError());

    // app specific setup
    pl_app_setup(gUserData);

    // show window
    ShowWindow(gHandle, SW_SHOWDEFAULT);

    // main loop
    while (gRunning)
    {

        // while queue has messages, remove and dispatch them (but do not block on empty queue)
        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // check for quit because peekmessage does not signal this via return val
            if (msg.message == WM_QUIT)
            {
                gRunning = false;
                break;
            }
            // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // reload library
        if(pl_has_library_changed(&gAppLibrary))
        {
            pl_reload_library(&gAppLibrary);
            pl_app_load     = (void* (__cdecl *)(plIOContext*, plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_load");
            pl_app_setup    = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_setup");
            pl_app_shutdown = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_resize");
            pl_app_render   = (void  (__cdecl *)(plAppData*)) pl_load_library_function(&gAppLibrary, "pl_app_render");
            gUserData = pl_app_load(ptIOCtx, gUserData);
        }

        // render a frame
        if(gRunning)
            pl__render_frame();
    }

    // app cleanup
    pl_app_shutdown(gUserData);

    // cleanup win32 stuff
    UnregisterClassW(wc.lpszClassName, GetModuleHandle(NULL));
    DestroyWindow(gHandle);
    gHandle = NULL;
    ptIOCtx->pBackendPlatformData = NULL;

    // cleanup io context
    pl_cleanup_io_context();

    // return console to original mode
    if(!SetConsoleMode(tStdOutHandle, tOriginalMode)) exit(GetLastError());
}

//-----------------------------------------------------------------------------
// [SECTION] key code conversion
//-----------------------------------------------------------------------------

#define PL_VK_KEYPAD_ENTER (VK_RETURN + 256)

static bool
pl__is_vk_down(int vk)
{
    return (GetKeyState(vk) & 0x8000) != 0;
}

static plKey
pl__virtual_key_to_pl_key(WPARAM wParam)
{
    switch (wParam)
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
pl__windows_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    LRESULT result = 0;
    static UINT_PTR puIDEvent = 0;
    switch (msg)
    {

        case WM_SIZE:
        case WM_SIZING:
        {
            if (wparam != SIZE_MINIMIZED)
            {
                // client window size
                RECT crect;
                int cwidth = 0;
                int cheight = 0;
                if (GetClientRect(hwnd, &crect))
                {
                    cwidth = crect.right - crect.left;
                    cheight = crect.bottom - crect.top;
                }
                ptIOCtx->afMainViewportSize[0] = (float)cwidth;
                ptIOCtx->afMainViewportSize[1] = (float)cheight;

                // give app change to handle resize
                pl_app_resize(gUserData);

                // send paint message
                InvalidateRect(hwnd, NULL, TRUE);
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
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(hwnd, &Paint);  
            EndPaint(hwnd, &Paint); 
            break;
        }

        case WM_DESTROY:
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
            SetTimer(hwnd, puIDEvent, USER_TIMER_MINIMUM , NULL);
            break;
        }

        case WM_EXITSIZEMOVE:
        {
            KillTimer(hwnd, puIDEvent);
            break;
        }

        case WM_TIMER:
        {
            if(wparam == puIDEvent)
                pl__render_frame();
            break;
        }

        case WM_MOUSEMOVE:
        {
            gMouseHandle = hwnd;
            if(!gMouseTracked)
            {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                gMouseTracked = true;        
            }
            POINT mouse_pos = { (LONG)GET_X_LPARAM(lparam), (LONG)GET_Y_LPARAM(lparam) };
            pl_add_mouse_pos_event((float)mouse_pos.x, (float)mouse_pos.y);
            break;
        }
        case WM_MOUSELEAVE:
        {
            if(hwnd == gMouseHandle)
            {
                gMouseHandle = NULL;
                pl_add_mouse_pos_event(-FLT_MAX, -FLT_MAX);
            }
            gMouseTracked = false;
            break;
        }

        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        {
            int button = 0;
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
            if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
            if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
            if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? 3 : 4; }
            pl_add_mouse_button_event(button, true);
            break;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            int button = 0;
            if (msg == WM_LBUTTONUP) { button = 0; }
            if (msg == WM_RBUTTONUP) { button = 1; }
            if (msg == WM_MBUTTONUP) { button = 2; }
            if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? 3 : 4; }
            pl_add_mouse_button_event(button, false);
            break;
        }

        case WM_MOUSEWHEEL:
        {
            pl_add_mouse_wheel_event(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA);
            return 0;
        }

        case WM_MOUSEHWHEEL:
        {
            pl_add_mouse_wheel_event((float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA, 0.0f);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            const bool bKeyDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            if (wparam < 256)
            {

                // Submit modifiers
                pl_add_key_event(PL_KEY_MOD_CTRL,  pl__is_vk_down(VK_CONTROL));
                pl_add_key_event(PL_KEY_MOD_SHIFT, pl__is_vk_down(VK_SHIFT));
                pl_add_key_event(PL_KEY_MOD_ALT,   pl__is_vk_down(VK_MENU));
                pl_add_key_event(PL_KEY_MOD_SUPER, pl__is_vk_down(VK_APPS));

                // obtain virtual key code
                int iVk = (int)wparam;
                if ((wparam == VK_RETURN) && (HIWORD(lparam) & KF_EXTENDED))
                {
                    iVk = PL_VK_KEYPAD_ENTER;
                }

                // submit key event
                const plKey key = pl__virtual_key_to_pl_key(iVk);

                if (key != PL_KEY_NONE)
                {
                    pl_add_key_event(key, bKeyDown);
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
                result = 0;
                break;
            }
        }

        default:
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

//-----------------------------------------------------------------------------
// [SECTION] misc
//-----------------------------------------------------------------------------

static void
pl__convert_to_wide_string(const char* narrowValue, wchar_t* wideValue)
{
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    size_t len = 1 + mbsrtowcs(NULL, &narrowValue, 0, &state);
    mbsrtowcs(wideValue, &narrowValue, len, &state);
}

static void
pl__render_frame(void)
{
    // setup time step
    INT64 currentTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);
    pl_get_io_context()->fDeltaTime = (float)(currentTime - gTime) / gTicksPerSecond;
    gTime = currentTime;
    
    pl_app_render(gUserData);   
}