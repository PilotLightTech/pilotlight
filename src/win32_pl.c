/*
   win32_pl.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] entry point
// [SECTION] windows procedure
// [SECTION] misc
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_os.h"
#include "pl_ds.h"
#include "vulkan_pl_graphics.h"
#include "vulkan_app.c"
#include <wchar.h> // mbsrtowcs, wcsrtombs

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

struct HWND__; 
typedef struct HWND__ *HWND;

static LRESULT CALLBACK pl__windows_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static void             pl__convert_to_wide_string(const char* narrowValue, wchar_t* wideValue);

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

HWND gHandle = NULL;

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{

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
        .right = gClientWidth + wr.left,
        .top = 0,
        .bottom = gClientHeight + wr.top
    };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    gActualWidth = wr.right - wr.left;
    gActualHeight = wr.bottom - wr.top;

    wchar_t wideTitle[PL_MAX_NAME_LENGTH];
    pl__convert_to_wide_string("Pilot Light (win32)", wideTitle);

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

    // create vulkan instance
    pl_create_instance(&gGraphics, VK_API_VERSION_1_1, true);
    
    // create surface
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = gHandle
    };
    PL_VULKAN(vkCreateWin32SurfaceKHR(gGraphics.instance, &surfaceCreateInfo, NULL, &gGraphics.surface));

    // create devices
    pl_create_device(gGraphics.instance, gGraphics.surface, &gDevice, true);
    
    // create swapchain
    pl_create_swapchain(&gDevice, gGraphics.surface, gClientWidth, gClientHeight, &gSwapchain);

    // app specific setup
    pl_app_setup();

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

        // render a frame
        pl_app_render();
    }

    // app cleanup
    pl_app_shutdown();

    // cleanup graphics context
    pl_cleanup_graphics(&gGraphics, &gDevice);

    // cleanup win32 stuff
    UnregisterClassW(wc.lpszClassName, GetModuleHandle(NULL));
    DestroyWindow(gHandle);
    gHandle = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] windows procedure
//-----------------------------------------------------------------------------

static LRESULT CALLBACK 
pl__windows_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
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
                gActualWidth = awidth;
                gActualHeight = aheight;

                // client window size
                RECT crect;
                int cwidth = 0;
                int cheight = 0;
                if (GetClientRect(hwnd, &crect))
                {
                    cwidth = crect.right - crect.left;
                    cheight = crect.bottom - crect.top;
                }
                gClientWidth = cwidth;
                gClientHeight = cheight;

                // give app change to handle resize
                pl_app_resize();

                // send paint message
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        case WM_MOVE:
        {
            pl_app_render();
            break;
        }
        case WM_PAINT:
        {
            pl_app_render();

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