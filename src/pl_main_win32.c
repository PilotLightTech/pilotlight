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
#include <winsock2.h>    // sockets
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()

#include "pl_win32.h"

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
static bool            gbMinimized = false;
static plSharedLibrary gtAppLibrary = {0};
static void*           gpUserData = NULL;
static bool            gbRunning = true;
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
    ptIOCtx->tCurrentCursor = PL_MOUSE_CURSOR_ARROW;
    ptIOCtx->tNextCursor = ptIOCtx->tCurrentCursor;
    ptIOCtx->afMainViewportSize[0] = 500.0f;
    ptIOCtx->afMainViewportSize[1] = 500.0f;

    // register window class
    const WNDCLASSEXW tWc = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = pl__windows_procedure,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = GetModuleHandle(NULL),
        .hIcon = NULL,
        .hCursor = NULL,
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

    wchar_t awWideTitle[1024];
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
    pl_init_win32(gtHandle);
    
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

        pl_update_mouse_cursor_win32();

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

    pl_cleanup_win32();

    // return console to original mode
    if(!SetConsoleMode(tStdOutHandle, tOriginalMode)) exit(GetLastError());

    // cleanup winsock
    WSACleanup();
}

//-----------------------------------------------------------------------------
// [SECTION] windows procedure
//-----------------------------------------------------------------------------

static LRESULT CALLBACK 
pl__windows_procedure(HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam)
{

    if(pl_windows_procedure(tHwnd, tMsg, tWParam, tLParam))
        return 0;

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

                // give app change to handle resize
                if(iCWidth > 0 && iCHeight > 0)
                {
                    gbMinimized = false;
                    pl_app_resize(gpUserData);
                }
                else
                {
                    gbMinimized = true;
                }

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

        case WM_SYSCOMMAND:
        {
            if(tWParam == SC_MINIMIZE)     gbMinimized = true;
            else if(tWParam == SC_RESTORE) gbMinimized = false;
            return DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
        }

        default:
            return DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
    }
    return 0;
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
    pl_new_frame_win32();
    
    if(!gbMinimized)
        pl_app_update(gpUserData);   
}