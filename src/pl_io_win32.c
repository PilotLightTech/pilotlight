/*
   pl_io_win32.c
     * platform windowing & mouse/keyboard input
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] internal functions
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_io.h"
#include "pl_app.h"
#include <string.h>
#include <wchar.h> // mbsrtowcs, wcsrtombs

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

static LRESULT CALLBACK pl__windows_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

//-----------------------------------------------------------------------------
// [SECTION] internal functions
//-----------------------------------------------------------------------------

static void
pl__convert_to_wide_string(const char* narrowValue, wchar_t* wideValue)
{
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    size_t len = 1 + mbsrtowcs(NULL, &narrowValue, 0, &state);
    mbsrtowcs(wideValue, &narrowValue, len, &state);
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_create_window(const char* title, int clientWidth, int clientHeight, plWindow* windowOut, void* userData)
{
    windowOut->width = clientWidth;
    windowOut->height = clientHeight;
    windowOut->clientWidth = clientWidth;
    windowOut->clientHeight = clientHeight;
    windowOut->needsResize = false;
    windowOut->running = true;
    windowOut->userData = userData;
    strncpy(windowOut->title, title, PL_MAX_NAME_LENGTH);

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
        .lpszClassName = L"Pilot Light Window",
        .hIconSm = NULL
    };
    RegisterClassExW(&wc);

    // calculate window size based on desired client region size
    RECT wr = 
    {
        .left = 0,
        .right = clientWidth + wr.left,
        .top = 0,
        .bottom = clientHeight + wr.top
    };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    windowOut->width = wr.right - wr.left;
    windowOut->height = wr.bottom - wr.top;

    wchar_t wideTitle[PL_MAX_NAME_LENGTH];
    pl__convert_to_wide_string(title, wideTitle);

    // create window & get handle
    windowOut->handle = CreateWindowExW(
        0,
        wc.lpszClassName,
        wideTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        0, 0, wr.right - wr.left, wr.bottom - wr.top,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        windowOut // user data
    );
}

void
pl_cleanup_window(plWindow* window)
{
    WNDCLASSA wc = {0};
    GetClassInfoA(GetModuleHandle(NULL), "Pilot Light Window", &wc);
    UnregisterClassA(wc.lpszClassName, GetModuleHandle(NULL));
    DestroyWindow(window->handle);
    window->handle = NULL;
    window->userData = NULL;
}

void
pl_process_window_events(plWindow* window)
{
    // while queue has messages, remove and dispatch them (but do not block on empty queue)
    MSG msg = {0};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        // check for quit because peekmessage does not signal this via return val
        if (msg.message == WM_QUIT)
        {
            window->running = false;
            return;
        }
        // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static LRESULT CALLBACK 
pl__windows_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // retrieve window back from win32 system
    plWindow* window = NULL;
    if (msg == WM_CREATE)
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)(lparam);
        window = (plWindow*)(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    else
    {
        LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
        window = (plWindow*)(ptr);
    }

    LRESULT result = 0;
    switch (msg)
    {
        case WM_SIZE:
        case WM_SIZING:
        {
            if (wparam != SIZE_MINIMIZED)
            {
                // actual window size
                RECT rect;
                int awidth = 0;
                int aheight = 0;
                if (GetWindowRect(hwnd, &rect))
                {
                    awidth = rect.right - rect.left;
                    aheight = rect.bottom - rect.top;
                }
                window->width = awidth;
                window->height = aheight;

                // client window size
                RECT crect;
                int cwidth = 0;
                int cheight = 0;
                if (GetClientRect(hwnd, &crect))
                {
                    cwidth = crect.right - crect.left;
                    cheight = crect.bottom - crect.top;
                }
                window->clientWidth = cwidth;
                window->clientHeight = cheight;

                // give app change to handle resize
                pl_app_resize((plApp*)window->userData);

                // send paint message
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        case WM_MOVE:
        {
            pl_app_render((plApp*)window->userData);
            break;
        }
        case WM_PAINT:
        {
            pl_app_render((plApp*)window->userData);

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
        default:
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}