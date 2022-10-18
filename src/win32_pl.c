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
#include "vulkan_pl.h"
#include "vulkan_pl_graphics.h"
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

static HWND              gHandle = NULL;
static plSharedLibrary   gSharedLibrary = {0};
static void*             gUserData = NULL;
static plAppData         gAppData = { .running = true, .clientWidth = 500, .clientHeight = 500};

typedef struct plUserData_t plUserData;
static void* (*pl_app_load)(plAppData* appData, plUserData* userData);
static void  (*pl_app_setup)(plAppData* appData, plUserData* userData);
static void  (*pl_app_shutdown)(plAppData* appData, plUserData* userData);
static void  (*pl_app_resize)(plAppData* appData, plUserData* userData);
static void  (*pl_app_render)(plAppData* appData, plUserData* userData);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{

    // load library
    if(pl_load_library(&gSharedLibrary, "app.dll", "app_", "lock.tmp"))
    {
        pl_app_load = (void* (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_load");
        pl_app_setup = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_setup");
        pl_app_shutdown = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_shutdown");
        pl_app_resize = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_resize");
        pl_app_render = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_render");
        gUserData = pl_app_load(&gAppData, NULL);
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
        .right = gAppData.clientWidth + wr.left,
        .top = 0,
        .bottom = gAppData.clientHeight + wr.top
    };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    gAppData.actualWidth = wr.right - wr.left;
    gAppData.actualHeight = wr.bottom - wr.top;

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
    pl_create_instance(&gAppData.graphics, VK_API_VERSION_1_1, true);
    
    // create surface
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = gHandle
    };
    PL_VULKAN(vkCreateWin32SurfaceKHR(gAppData.graphics.instance, &surfaceCreateInfo, NULL, &gAppData.graphics.surface));

    // create devices
    pl_create_device(gAppData.graphics.instance, gAppData.graphics.surface, &gAppData.device, true);
    
    // create swapchain
    pl_create_swapchain(&gAppData.device, gAppData.graphics.surface, gAppData.clientWidth, gAppData.clientHeight, &gAppData.swapchain);

    // app specific setup
    pl_app_setup(&gAppData, gUserData);

    // show window
    ShowWindow(gHandle, SW_SHOWDEFAULT);

    // main loop
    while (gAppData.running)
    {

        // while queue has messages, remove and dispatch them (but do not block on empty queue)
        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // check for quit because peekmessage does not signal this via return val
            if (msg.message == WM_QUIT)
            {
                gAppData.running = false;
                break;
            }
            // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // reload library
        if(pl_has_library_changed(&gSharedLibrary))
        {
            pl_reload_library(&gSharedLibrary);
            pl_app_load = (void* (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_load");
            pl_app_setup = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_setup");
            pl_app_shutdown = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_shutdown");
            pl_app_resize = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_resize");
            pl_app_render = (void (__cdecl  *)(plAppData*, plUserData*)) pl_load_library_function(&gSharedLibrary, "pl_app_render");
            gUserData = pl_app_load(&gAppData, gUserData);
        }

        // render a frame
        if(gAppData.running)
            pl_app_render(&gAppData, gUserData);
    }

    // app cleanup
    pl_app_shutdown(&gAppData, gUserData);

    // cleanup graphics context
    pl_cleanup_graphics(&gAppData.graphics, &gAppData.device);

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
                gAppData.actualWidth = awidth;
                gAppData.actualHeight = aheight;

                // client window size
                RECT crect;
                int cwidth = 0;
                int cheight = 0;
                if (GetClientRect(hwnd, &crect))
                {
                    cwidth = crect.right - crect.left;
                    cheight = crect.bottom - crect.top;
                }
                gAppData.clientWidth = cwidth;
                gAppData.clientHeight = cheight;

                // give app change to handle resize
                pl_app_resize(&gAppData, gUserData);

                // send paint message
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        case WM_MOVE:
        {
            pl_app_render(&gAppData, gUserData);
            break;
        }
        case WM_PAINT:
        {
            pl_app_render(&gAppData, gUserData);

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