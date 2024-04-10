/*
   pl_main_win32.c
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] windows procedure
// [SECTION] implementations
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#pragma comment(lib, "ucrtd.lib")
#else
#pragma comment(lib, "ucrt.lib")
#endif

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_VK_KEYPAD_ENTER (VK_RETURN + 256)
#define WIN32_LEAN_AND_MEAN

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h" // data registry, api registry, extension registry
#include "pl_ui.h"      // io context
#include "pl_os.h"      // os apis
#include "pl_ds.h"      // hashmap
#include "pl_json.h"

#include <float.h>      // FLT_MAX
#include <stdlib.h>     // exit
#include <stdio.h>      // printf
#include <wchar.h>      // mbsrtowcs, wcsrtombs
#include <winsock2.h>   // sockets
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM(), GET_Y_LPARAM()

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// internal helpers
LRESULT CALLBACK pl__windows_procedure     (HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam);
void             pl__convert_to_wide_string(const char* narrowValue, wchar_t* wideValue);
void             pl__render_frame          (void);
void             pl_update_mouse_cursor    (void);
inline bool      pl__is_vk_down            (int iVk) { return (GetKeyState(iVk) & 0x8000) != 0;}
plKey            pl__virtual_key_to_pl_key (WPARAM tWParam);

// clip board
static const char* pl__get_clipboard_text(void* user_data_ctx);
static void        pl__set_clipboard_text(void* pUnused, const char* text);

// file api
void pl__read_file(const char* pcFile, unsigned* puSize, char* pcBuffer, const char* pcMode);
void pl__copy_file(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer);

// udp api
void pl__create_udp_socket (plSocket* ptSocketOut, bool bNonBlocking);
void pl__bind_udp_socket   (plSocket* ptSocket, int iPort);
bool pl__send_udp_data     (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
bool pl__get_udp_data      (plSocket* ptSocket, void* pData, size_t szSize);

// library api
bool  pl__has_library_changed  (plSharedLibrary* ptLibrary);
bool  pl__load_library         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
void  pl__reload_library       (plSharedLibrary* ptLibrary);
void* pl__load_library_function(plSharedLibrary* ptLibrary, const char* pcName);

// thread api
void                pl__sleep(uint32_t millisec);
uint32_t            pl__get_hardware_thread_count(void);
plThread            pl__create_thread(plThreadProcedure ptProcedure, void* pData);
void                pl__join_thread(plThread* ptThread);
void                pl__yield_thread(void);
plMutex             pl__create_mutex(void);
void                pl__lock_mutex(plMutex* ptMutex);
void                pl__unlock_mutex(plMutex* ptMutex);
void                pl__destroy_mutex(plMutex* ptMutex);
plCriticalSection   pl__create_critical_section (void);
void                pl__destroy_critical_section(plCriticalSection* ptCriticalSection);
void                pl__enter_critical_section  (plCriticalSection* ptCriticalSection);
void                pl__leave_critical_section  (plCriticalSection* ptCriticalSection);
plSemaphore         pl__create_semaphore(uint32_t uIntialCount);
void                pl__wait_on_semaphore(plSemaphore* ptSemaphore);
bool                pl__try_wait_on_semaphore(plSemaphore* ptSemaphore);
void                pl__release_semaphore(plSemaphore* ptSemaphore);
void                pl__destroy_semaphore(plSemaphore* ptSemaphore);
plThreadKey         pl__allocate_thread_local_key(void);
void                pl__free_thread_local_key(plThreadKey* puIndex);
void*               pl__allocate_thread_local_data(plThreadKey* ptKey, size_t szSize);
void*               pl__get_thread_local_data(plThreadKey* ptKey);
void                pl__free_thread_local_data(plThreadKey* ptKey, void* pData);
plConditionVariable pl__create_condition_variable(void);
void                pl__destroy_condition_variable(plConditionVariable* ptConditionVariable);
void                pl__wake_condition_variable(plConditionVariable* ptConditionVariable);
void                pl__wake_all_condition_variable(plConditionVariable* ptConditionVariable);
void                pl__sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection);
plBarrier           pl__create_barrier(uint32_t uThreadCount);
void                pl__destroy_barrier(plBarrier* ptBarrier);
void                pl__wait_on_barrier(plBarrier* ptBarrier);

// atomics
void    pl__create_atomic_counter  (int64_t ilValue, plAtomicCounter** ptCounter);
void    pl__destroy_atomic_counter (plAtomicCounter** ptCounter);
void    pl__atomic_store           (plAtomicCounter* ptCounter, int64_t ilValue);
int64_t pl__atomic_load            (plAtomicCounter* ptCounter);
bool    pl__atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue);
void    pl__atomic_increment       (plAtomicCounter* ptCounter);
void    pl__atomic_decrement       (plAtomicCounter* ptCounter);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plWin32SharedLibrary
{
    HMODULE   tHandle;
    FILETIME  tLastWriteTime;
} plWin32SharedLibrary;

typedef struct _plAtomicCounter
{
    int64_t ilValue;
} plAtomicCounter;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// win32 stuff
HWND            gtHandle                          = NULL;
plSharedLibrary gtAppLibrary                      = {0};
void*           gpUserData                        = NULL;
bool            gbRunning                         = true;
bool            gbFirstRun                        = true;
bool            gbEnableVirtualTerminalProcessing = true;
INT64           ilTime                            = 0;
INT64           ilTicksPerSecond                  = 0;
HWND            tMouseHandle                      = NULL;
bool            bMouseTracked                     = true;
plIO*           gptIOCtx                          = NULL;
plUiContext*    gptUiCtx                          = NULL;

// apis
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;

// memory tracking
plHashMap       gtMemoryHashMap = {0};
plMemoryContext gtMemoryContext = {.ptHashMap = &gtMemoryHashMap};

// app config
char acAppName[256] = {0};
char acWindowName[256] = {0};
plVec2 tViewportSize = {500.0f, 500.0f};
plVec2 tViewportPos  = {200.0f, 200.0f};

// app function pointers
void* (*pl_app_load)    (const plApiRegistryApiI* ptApiRegistry, void* userData);
void  (*pl_app_shutdown)(void* userData);
void  (*pl_app_resize)  (void* userData);
void  (*pl_app_update)  (void* userData);

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{

    gptUiCtx = pl_create_context();
    gptIOCtx = pl_get_io();

    // check for disabling of escape characters.
    // this is necessary for some vkconfig's "console"
    for(int i = 1; i < argc; i++)
    { 
        if(strcmp(argv[i], "--disable_vt") == 0)
            gbEnableVirtualTerminalProcessing = false;
    }

    // initialize winsock
    WSADATA tWsaData = {0};
    if(WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0)
    {
        printf("Failed to start winsock with error code: %d\n", WSAGetLastError());
        return -1;
    }

    // os provided apis

    static const plLibraryApiI tLibraryApi = {
        .has_changed   = pl__has_library_changed,
        .load          = pl__load_library,
        .load_function = pl__load_library_function,
        .reload        = pl__reload_library
    };

    static const plFileApiI tFileApi = {
        .copy = pl__copy_file,
        .read = pl__read_file
    };
    
    static const plUdpApiI tUdpApi = {
        .create_socket = pl__create_udp_socket,
        .bind_socket   = pl__bind_udp_socket,  
        .get_data      = pl__get_udp_data,
        .send_data     = pl__send_udp_data
    };

    static const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl__get_hardware_thread_count,
        .create_thread               = pl__create_thread,
        .join_thread                 = pl__join_thread,
        .yield_thread                = pl__yield_thread,
        .sleep_thread                = pl__sleep,
        .create_mutex                = pl__create_mutex,
        .destroy_mutex               = pl__destroy_mutex,
        .lock_mutex                  = pl__lock_mutex,
        .unlock_mutex                = pl__unlock_mutex,
        .create_semaphore            = pl__create_semaphore,
        .destroy_semaphore           = pl__destroy_semaphore,
        .wait_on_semaphore           = pl__wait_on_semaphore,
        .try_wait_on_semaphore       = pl__try_wait_on_semaphore,
        .release_semaphore           = pl__release_semaphore,
        .allocate_thread_local_key   = pl__allocate_thread_local_key,
        .allocate_thread_local_data  = pl__allocate_thread_local_data,
        .free_thread_local_key       = pl__free_thread_local_key, 
        .get_thread_local_data       = pl__get_thread_local_data, 
        .free_thread_local_data      = pl__free_thread_local_data, 
        .create_critical_section     = pl__create_critical_section,
        .destroy_critical_section    = pl__destroy_critical_section,
        .enter_critical_section      = pl__enter_critical_section,
        .leave_critical_section      = pl__leave_critical_section,
        .create_condition_variable   = pl__create_condition_variable,
        .destroy_condition_variable  = pl__destroy_condition_variable,
        .wake_condition_variable     = pl__wake_condition_variable,
        .wake_all_condition_variable = pl__wake_all_condition_variable,
        .sleep_condition_variable    = pl__sleep_condition_variable,
        .create_barrier              = pl__create_barrier,
        .destroy_barrier             = pl__destroy_barrier,
        .wait_on_barrier             = pl__wait_on_barrier
    };

    static const plAtomicsI tAtomicsApi = {
        .create_atomic_counter   = pl__create_atomic_counter,
        .destroy_atomic_counter  = pl__destroy_atomic_counter,
        .atomic_store            = pl__atomic_store,
        .atomic_load             = pl__atomic_load,
        .atomic_compare_exchange = pl__atomic_compare_exchange,
        .atomic_increment        = pl__atomic_increment,
        .atomic_decrement        = pl__atomic_decrement
    };

    uint32_t uFileSize = 0;
    tFileApi.read("pl_config.json", &uFileSize, NULL, "rb");
    char* pcFileData = PL_ALLOC(uFileSize + 1);
    memset(pcFileData, 0, uFileSize + 1);
    tFileApi.read("pl_config.json", &uFileSize, pcFileData, "rb");

    plJsonObject tJsonRoot = {0};
    pl_load_json(pcFileData, &tJsonRoot);
    pl_json_string_member(&tJsonRoot, "app name", acAppName, 256);
    pl_json_string_member(&tJsonRoot, "viewport title", acWindowName, 256);
    pl_json_float_array_member(&tJsonRoot, "viewport size", tViewportSize.d, NULL);
    pl_json_float_array_member(&tJsonRoot, "viewport pos", tViewportPos.d, NULL);
    pl_unload_json(&tJsonRoot);
    PL_FREE(pcFileData);

    // load core apis
    gptApiRegistry       = pl_load_core_apis();
    gptDataRegistry      = gptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptExtensionRegistry = gptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // add os specific apis
    gptApiRegistry->add(PL_API_LIBRARY, &tLibraryApi);
    gptApiRegistry->add(PL_API_FILE, &tFileApi);
    gptApiRegistry->add(PL_API_UDP, &tUdpApi);
    gptApiRegistry->add(PL_API_THREADS, &tThreadApi);
    gptApiRegistry->add(PL_API_ATOMICS, &tAtomicsApi);

    // set clipboard functions (may need to move this to OS api)
    gptIOCtx->set_clipboard_text_fn = pl__set_clipboard_text;
    gptIOCtx->get_clipboard_text_fn = pl__get_clipboard_text;

    // add contexts to data registry
    gptDataRegistry->set_data("ui", gptUiCtx);
    gptDataRegistry->set_data(PL_CONTEXT_MEMORY, &gtMemoryContext);

    // register window class
    const WNDCLASSEXW tWc = {
        .cbSize        = sizeof(WNDCLASSEX),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = pl__windows_procedure,
        .cbClsExtra    = 0,
        .cbWndExtra    = 0,
        .hInstance     = GetModuleHandle(NULL),
        .hIcon         = NULL,
        .hCursor       = NULL,
        .hbrBackground = NULL,
        .lpszMenuName  = NULL,
        .lpszClassName = L"Pilot Light (win32)",
        .hIconSm       = NULL
    };
    RegisterClassExW(&tWc);

    // calculate window size based on desired client region size
    RECT tWr = 
    {
        .left = (LONG)tViewportPos.x,
        .right = (LONG)(tViewportSize.x + tViewportPos.x),
        .top = (LONG)tViewportPos.y,
        .bottom = (LONG)(tViewportSize.y + tViewportPos.y)
    };
    AdjustWindowRect(&tWr, WS_OVERLAPPEDWINDOW, FALSE);

    // convert title to wide chars
    wchar_t awWideTitle[1024];
    pl__convert_to_wide_string(acWindowName, awWideTitle);

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
    gptIOCtx->pBackendPlatformData = &gtHandle; // required to create surfaces for vulkan

    // setup info for clock
    QueryPerformanceFrequency((LARGE_INTEGER*)&ilTicksPerSecond);
    QueryPerformanceCounter((LARGE_INTEGER*)&ilTime);
    
    // setup console
    DWORD tCurrentMode   = 0;
    DWORD tOriginalMode  = 0;
    HANDLE tStdOutHandle = NULL;

    if(gbEnableVirtualTerminalProcessing)
    {
        tStdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        if(tStdOutHandle == INVALID_HANDLE_VALUE)
            exit(GetLastError());
        if(!GetConsoleMode(tStdOutHandle, &tCurrentMode))
            exit(GetLastError());
        tOriginalMode = tCurrentMode;
        tCurrentMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // enable ANSI escape codes
        if(!SetConsoleMode(tStdOutHandle, tCurrentMode))
            exit(GetLastError());
    }

    // load library
    const plLibraryApiI* ptLibraryApi = gptApiRegistry->first(PL_API_LIBRARY);
    static char acLibraryName[256] = {0};
    static char acTransitionalName[256] = {0};
    pl_sprintf(acLibraryName, "./%s.dll", acAppName);
    pl_sprintf(acTransitionalName, "./%s_", acAppName);
    if(ptLibraryApi->load(&gtAppLibrary, acLibraryName, acTransitionalName, "./lock.tmp"))
    {
        pl_app_load     = (void* (__cdecl  *)(const plApiRegistryApiI*, void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_update");
        gpUserData = pl_app_load(gptApiRegistry, NULL);
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

        // update mouse cursor icon if changed
        pl_update_mouse_cursor();

        // reload library
        if(ptLibraryApi->has_changed(&gtAppLibrary))
        {
            ptLibraryApi->reload(&gtAppLibrary);
            pl_app_load     = (void* (__cdecl  *)(const plApiRegistryApiI*, void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(&gtAppLibrary, "pl_app_update");
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }

        // render frame
        if(gbRunning)
            pl__render_frame();
    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    // cleanup win32 stuff
    UnregisterClassW(tWc.lpszClassName, GetModuleHandle(NULL));
    DestroyWindow(gtHandle);
    gtHandle = NULL;

    // cleanup io context
    gptExtensionRegistry->unload_all();
    pl_unload_core_apis();

    // return console to original mode
    if(gbEnableVirtualTerminalProcessing)
    {
        if(!SetConsoleMode(tStdOutHandle, tOriginalMode))
            exit(GetLastError());
    }

    // cleanup winsock
    WSACleanup();

    // check for unfreed memory
    uint32_t uMemoryLeakCount = 0;
    for(uint32_t i = 0; i < pl_sb_size(gtMemoryContext.sbtAllocations); i++)
    {
        if(gtMemoryContext.sbtAllocations[i].pAddress != NULL)
        {
            printf("Unfreed memory from line %i in file '%s'.\n", gtMemoryContext.sbtAllocations[i].iLine, gtMemoryContext.sbtAllocations[i].pcFile);
            uMemoryLeakCount++;
        }
    }
        
    assert(uMemoryLeakCount == gtMemoryContext.szActiveAllocations);
    if(uMemoryLeakCount > 0)
        printf("%u unfreed allocations.\n", uMemoryLeakCount);
}

//-----------------------------------------------------------------------------
// [SECTION] windows procedure
//-----------------------------------------------------------------------------

LRESULT CALLBACK 
pl__windows_procedure(HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam)
{
    static UINT_PTR puIDEvent = 0;
    switch (tMsg)
    {

        case WM_SYSCOMMAND:
        {

            if(tWParam == SC_MINIMIZE)
                gptIOCtx->bViewportMinimized = true;
            else if(tWParam == SC_RESTORE)
                gptIOCtx->bViewportMinimized = false;
            else if(tWParam == SC_MAXIMIZE)
                gptIOCtx->bViewportMinimized = false;
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
                    gptIOCtx->bViewportMinimized = false;
                else
                    gptIOCtx->bViewportMinimized = true;

                if(gptIOCtx->afMainViewportSize[0] != (float)iCWidth || gptIOCtx->afMainViewportSize[1] != (float)iCHeight)
                    gptIOCtx->bViewportSizeChanged = true;  

                gptIOCtx->afMainViewportSize[0] = (float)iCWidth;
                gptIOCtx->afMainViewportSize[1] = (float)iCHeight;    

                if(gptIOCtx->bViewportSizeChanged && !gptIOCtx->bViewportMinimized && !gbFirstRun)
                {
                    pl_app_resize(gpUserData);
                    gbFirstRun = false;
                }
                gbFirstRun = false;

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
                    pl_add_text_event_utf16((uint16_t)tWParam);
            }
            else
            {
                wchar_t wch = 0;
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (char*)&tWParam, 1, &wch, 1);
                pl_add_text_event(wch);
            }
            break;

        case WM_SETCURSOR:
            // required to restore cursor when transitioning from e.g resize borders to client area.
            if (LOWORD(tLParam) == HTCLIENT)
            {
                gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
                gptIOCtx->tCurrentCursor = PL_MOUSE_CURSOR_NONE;
                pl_update_mouse_cursor();
            }
            break;

        case WM_PAINT:
        {
            pl__render_frame();

            // must be called for the OS to do its thing
            PAINTSTRUCT tPaint;
            HDC tDeviceContext = BeginPaint(tHwnd, &tPaint);  
            EndPaint(tHwnd, &tPaint); 
            break;
        }

        case WM_MOUSEMOVE:
        {
            tMouseHandle = tHwnd;
            if(!bMouseTracked)
            {
                TRACKMOUSEEVENT tTme = { sizeof(tTme), TME_LEAVE, tHwnd, 0 };
                TrackMouseEvent(&tTme);
                bMouseTracked = true;        
            }
            POINT tMousePos = { (LONG)GET_X_LPARAM(tLParam), (LONG)GET_Y_LPARAM(tLParam) };
            pl_add_mouse_pos_event((float)tMousePos.x, (float)tMousePos.y);
            break;
        }
        case WM_MOUSELEAVE:
        {
            if(tHwnd == tMouseHandle)
            {
                tMouseHandle = NULL;
                pl_add_mouse_pos_event(-FLT_MAX, -FLT_MAX);
            }
            bMouseTracked = false;
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
            if(gptIOCtx->_iMouseButtonsDown == 0 && GetCapture() == NULL)
                SetCapture(tHwnd);
            gptIOCtx->_iMouseButtonsDown |= 1 << iButton;
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
            gptIOCtx->_iMouseButtonsDown &= ~(1 << iButton);
            if(gptIOCtx->_iMouseButtonsDown == 0 && GetCapture() == tHwnd)
                ReleaseCapture();
            pl_add_mouse_button_event(iButton, false);
            break;
        }

        case WM_MOUSEWHEEL:
        {
            pl_add_mouse_wheel_event(0.0f, (float)GET_WHEEL_DELTA_WPARAM(tWParam) / (float)WHEEL_DELTA);
            break;
        }

        case WM_MOUSEHWHEEL:
        {
            pl_add_mouse_wheel_event((float)GET_WHEEL_DELTA_WPARAM(tWParam) / (float)WHEEL_DELTA, 0.0f);
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
                pl_add_key_event(PL_KEY_MOD_CTRL,  pl__is_vk_down(VK_CONTROL));
                pl_add_key_event(PL_KEY_MOD_SHIFT, pl__is_vk_down(VK_SHIFT));
                pl_add_key_event(PL_KEY_MOD_ALT,   pl__is_vk_down(VK_MENU));
                pl_add_key_event(PL_KEY_MOD_SUPER, pl__is_vk_down(VK_APPS));

                // obtain virtual key code
                int iVk = (int)tWParam;
                if ((tWParam == VK_RETURN) && (HIWORD(tLParam) & KF_EXTENDED))
                    iVk = PL_VK_KEYPAD_ENTER;

                // submit key event
                const plKey tKey = pl__virtual_key_to_pl_key(iVk);

                if (tKey != PL_KEY_NONE)
                    pl_add_key_event(tKey, bKeyDown);

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
    #endif
    }
    return DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl__convert_to_wide_string(const char* pcNarrowValue, wchar_t* pwWideValue)
{
    mbstate_t tState;
    memset(&tState, 0, sizeof(tState));
    size_t szLen = 1 + mbsrtowcs(NULL, &pcNarrowValue, 0, &tState);
    mbsrtowcs(pwWideValue, &pcNarrowValue, szLen, &tState);
}

void
pl__render_frame(void)
{
    // setup time step
    INT64 ilCurrentTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
    gptIOCtx->fDeltaTime = (float)(ilCurrentTime - ilTime) / ilTicksPerSecond;
    ilTime = ilCurrentTime;
    if(!gptIOCtx->bViewportMinimized)
    {
        pl_app_update(gpUserData);
        gptExtensionRegistry->reload();
    }
}

void
pl_update_mouse_cursor(void)
{
    // updating mouse cursor
    if(gptIOCtx->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && gptIOCtx->tNextCursor == PL_MOUSE_CURSOR_ARROW)
        gptIOCtx->bCursorChanged = true;

    if(gptIOCtx->bCursorChanged && gptIOCtx->tNextCursor != gptIOCtx->tCurrentCursor)
    {
        gptIOCtx->tCurrentCursor = gptIOCtx->tNextCursor;
        LPTSTR tWin32Cursor = IDC_ARROW;
        switch (gptIOCtx->tNextCursor)
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
    gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
    gptIOCtx->bCursorChanged = false;
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

void
pl__read_file(const char* pcFile, unsigned* puSizeIn, char* pcBuffer, const char* pcMode)
{
    PL_ASSERT(puSizeIn);

    FILE* ptDataFile = fopen(pcFile, pcMode);
    unsigned uSize = 0u;

    if (ptDataFile == NULL)
    {
        PL_ASSERT(false && "File not found.");
        *puSizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    fseek(ptDataFile, 0, SEEK_SET);

    if(pcBuffer == NULL)
    {
        *puSizeIn = uSize;
        fclose(ptDataFile);
        return;
    }

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
        PL_ASSERT(false && "File not read.");
    }

    fclose(ptDataFile);
}

void
pl__copy_file(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer)
{
    CopyFile(pcSource, pcDestination, FALSE);
}

void
pl__create_udp_socket(plSocket* ptSocketOut, bool bNonBlocking)
{

    UINT_PTR tWin32Socket = 0;

    // create socket
    if((tWin32Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d\n", WSAGetLastError());
        PL_ASSERT(false && "Could not create socket");
    }

    // enable non-blocking
    if(bNonBlocking)
    {
        u_long uMode = 1;
        ioctlsocket(tWin32Socket, FIONBIO, &uMode);
    }

    ptSocketOut->_pPlatformData = (void*)tWin32Socket;
}

void
pl__bind_udp_socket(plSocket* ptSocket, int iPort)
{
    ptSocket->iPort = iPort;
    PL_ASSERT(ptSocket->_pPlatformData && "Socket not created yet");
    UINT_PTR tWin32Socket = (UINT_PTR)ptSocket->_pPlatformData;
    
    // prepare sockaddr_in struct
    struct sockaddr_in tServer = {
        .sin_family      = AF_INET,
        .sin_port        = htons((u_short)iPort),
        .sin_addr.s_addr = INADDR_ANY
    };

    // bind socket
    if(bind(tWin32Socket, (struct sockaddr* )&tServer, sizeof(tServer)) == SOCKET_ERROR)
    {
        printf("Bind socket failed with error code : %d\n", WSAGetLastError());
        PL_ASSERT(false && "Socket error");
    }
}

bool
pl__send_udp_data(plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize)
{
    PL_ASSERT(ptFromSocket->_pPlatformData && "Socket not created yet");
    UINT_PTR tWin32Socket = (UINT_PTR)ptFromSocket->_pPlatformData;

    struct sockaddr_in tDestSocket = {
        .sin_family           = AF_INET,
        .sin_port             = htons((u_short)iDestPort),
        .sin_addr.S_un.S_addr = inet_addr(pcDestIP)
    };
    static const size_t szLen = sizeof(tDestSocket);

    // send
    if(sendto(tWin32Socket, (const char*)pData, (int)szSize, 0, (struct sockaddr*)&tDestSocket, (int)szLen) == SOCKET_ERROR)
    {
        printf("sendto() failed with error code : %d\n", WSAGetLastError());
        PL_ASSERT(false && "Socket error");
        return false;
    }

    return true;
}

bool
pl__get_udp_data(plSocket* ptSocket, void* pData, size_t szSize)
{
    PL_ASSERT(ptSocket->_pPlatformData && "Socket not created yet");
    UINT_PTR tWin32Socket = (UINT_PTR)ptSocket->_pPlatformData;

    struct sockaddr_in tSiOther = {0};
    static int iSLen = (int)sizeof(tSiOther);
    memset(pData, 0, szSize);
    int iRecvLen = recvfrom(tWin32Socket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tSiOther, &iSLen);

    if(iRecvLen == SOCKET_ERROR)
    {
        const int iLastError = WSAGetLastError();
        if(iLastError != WSAEWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            PL_ASSERT(false && "Socket error");
            return false;
        }
    }

    return iRecvLen > 0;
}

static inline FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}

bool
pl__has_library_changed(plSharedLibrary* ptLibrary)
{
    FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
    plWin32SharedLibrary* win32Library = ptLibrary->_pPlatformData;
    return CompareFileTime(&newWriteTime, &win32Library->tLastWriteTime) != 0;
}

bool
pl__load_library(plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile)
{
    if(ptLibrary->acPath[0] == 0)             strncpy(ptLibrary->acPath, pcName, PL_MAX_PATH_LENGTH);
    if(ptLibrary->acTransitionalName[0] == 0) strncpy(ptLibrary->acTransitionalName, pcTransitionalName, PL_MAX_PATH_LENGTH);
    if(ptLibrary->acLockFile[0] == 0)         strncpy(ptLibrary->acLockFile, pcLockFile, PL_MAX_PATH_LENGTH);
    ptLibrary->bValid = false;

    if(ptLibrary->_pPlatformData == NULL)
        ptLibrary->_pPlatformData = malloc(sizeof(plWin32SharedLibrary));
    plWin32SharedLibrary* ptWin32Library = ptLibrary->_pPlatformData;

    WIN32_FILE_ATTRIBUTE_DATA tIgnored;
    if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
    {
        char acTemporaryName[2024] = {0};
        ptWin32Library->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        stbsp_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dll");
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl__copy_file(ptLibrary->acPath, acTemporaryName, NULL, NULL);

        ptWin32Library->tHandle = NULL;
        ptWin32Library->tHandle = LoadLibraryA(acTemporaryName);
        if(ptWin32Library->tHandle)
            ptLibrary->bValid = true;
        else
        {
            DWORD iLastError = GetLastError();
            printf("LoadLibaryA() failed with error code : %d\n", iLastError);
        }
    }

    return ptLibrary->bValid;
}

void
pl__reload_library(plSharedLibrary* ptLibrary)
{
    ptLibrary->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl__load_library(ptLibrary, ptLibrary->acPath, ptLibrary->acTransitionalName, ptLibrary->acLockFile))
            break;
        pl__sleep(100);
    }
}

void*
pl__load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "Library not valid");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        plWin32SharedLibrary* ptWin32Library = ptLibrary->_pPlatformData;
        pLoadedFunction = (void*)GetProcAddress(ptWin32Library->tHandle, name);
        if(pLoadedFunction == NULL)
            printf("Failed to load function with error code: %d\n", WSAGetLastError());
    }
    return pLoadedFunction;
}

void
pl__sleep(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
}

typedef struct _plThreadData
{
  plThreadProcedure ptProcedure;
  void*             pData;
} plThreadData;

static DWORD 
thread_procedure(void* lpParam)
{
    plThreadData* ptData = lpParam;
    ptData->ptProcedure(ptData->pData);
    return 1;
}

static void
thread_yield(void)
{
    SwitchToThread();
}

plThread
pl__create_thread(plThreadProcedure ptProcedure, void* pData)
{
    plThreadData* ptData = PL_ALLOC(sizeof(plThreadData));
    ptData->ptProcedure = ptProcedure;
    ptData->pData       = pData;

    plThread tThread = {
        ._pPlatformData = CreateThread(0, 1024, thread_procedure, ptData, 0, NULL)
    };
    return tThread;
}

void
pl__join_thread(plThread* ptThread)
{
    HANDLE tThreadHandle = ptThread->_pPlatformData;
    WaitForSingleObject(tThreadHandle, INFINITE);
}

void
pl__yield_thread(void)
{
    thread_yield();
}

plMutex
pl__create_mutex(void)
{
    plMutex tMutex = {
        ._pPlatformData = CreateMutex(NULL, FALSE, NULL)
    };
    return tMutex;
}

void
pl__destroy_mutex(plMutex* ptMutex)
{
    HANDLE* ptMutexHandle = ptMutex->_pPlatformData;
    CloseHandle(ptMutexHandle);
    PL_FREE(ptMutex->_pPlatformData);
    ptMutex->_pPlatformData = NULL;
}

void
pl__lock_mutex(plMutex* ptMutex)
{
    HANDLE tMutexHandle = ptMutex->_pPlatformData;

    DWORD dwWaitResult = WaitForSingleObject(tMutexHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
}

void
pl__unlock_mutex(plMutex* ptMutex)
{
    HANDLE tMutexHandle = ptMutex->_pPlatformData;
    if(!ReleaseMutex(tMutexHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

plCriticalSection
pl__create_critical_section(void)
{
    CRITICAL_SECTION* ptCriticalSection = PL_ALLOC(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(ptCriticalSection);
    plCriticalSection tCriticalSection = {
        ._pPlatformData = ptCriticalSection
    };
    return tCriticalSection;
}

void
pl__destroy_critical_section(plCriticalSection* ptCriticalSection)
{
    CRITICAL_SECTION* ptCritSection = ptCriticalSection->_pPlatformData;
    DeleteCriticalSection(ptCritSection);
    PL_FREE(ptCriticalSection->_pPlatformData);
    ptCriticalSection->_pPlatformData = NULL;
}

void
pl__enter_critical_section(plCriticalSection* ptCriticalSection)
{
    CRITICAL_SECTION* ptCritSection = ptCriticalSection->_pPlatformData;
    EnterCriticalSection(ptCritSection);
}

void
pl__leave_critical_section(plCriticalSection* ptCriticalSection)
{
    CRITICAL_SECTION* ptCritSection = ptCriticalSection->_pPlatformData;
    LeaveCriticalSection(ptCritSection);
}

uint32_t
pl__get_hardware_thread_count(void)
{

    static uint32_t uThreadCount = 0;

    if(uThreadCount == 0)
    {
        DWORD dwLength = 0;
        GetLogicalProcessorInformation(NULL, &dwLength);
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION atInfo = PL_ALLOC(dwLength);
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

plBarrier
pl__create_barrier(uint32_t uThreadCount)
{
    SYNCHRONIZATION_BARRIER* ptSyncBarrier = PL_ALLOC(sizeof(SYNCHRONIZATION_BARRIER));
    InitializeSynchronizationBarrier(ptSyncBarrier, uThreadCount, -1);
    plBarrier tBarrier = {
        ._pPlatformData = ptSyncBarrier
    };
    return tBarrier;
}

void
pl__destroy_barrier(plBarrier* ptBarrier)
{
    SYNCHRONIZATION_BARRIER* ptSyncBarrier = ptBarrier->_pPlatformData;
    DeleteSynchronizationBarrier(ptSyncBarrier);
    PL_FREE(ptBarrier->_pPlatformData);
    ptBarrier->_pPlatformData = NULL;
}

void
pl__wait_on_barrier(plBarrier* ptBarrier)
{
    SYNCHRONIZATION_BARRIER* ptSyncBarrier = ptBarrier->_pPlatformData;
    EnterSynchronizationBarrier(ptSyncBarrier, 0);
}

plSemaphore
pl__create_semaphore(uint32_t uIntialCount)
{
    plSemaphore tSemaphore = {
        ._pPlatformData = CreateSemaphore(NULL, uIntialCount, uIntialCount, NULL)
    };
    return tSemaphore;
}

void
pl__destroy_semaphore(plSemaphore* ptSemaphore)
{
    HANDLE* ptSemaphoreHandle = ptSemaphore->_pPlatformData;
    CloseHandle(ptSemaphoreHandle);
    PL_FREE(ptSemaphore->_pPlatformData);
    ptSemaphore->_pPlatformData = NULL;
}

void
pl__wait_on_semaphore(plSemaphore* ptSemaphore)
{
    HANDLE tSemaphoreHandle = ptSemaphore->_pPlatformData;
    WaitForSingleObject(tSemaphoreHandle, INFINITE);
}

bool
pl__try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    HANDLE tSemaphoreHandle = ptSemaphore->_pPlatformData;
    DWORD dwWaitResult = WaitForSingleObject(tSemaphoreHandle, 0);
    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0: return true;
        case WAIT_TIMEOUT:  return false;
    }
    PL_ASSERT(false);
    return false;
}

void
pl__release_semaphore(plSemaphore* ptSemaphore)
{
    HANDLE tSemaphoreHandle = ptSemaphore->_pPlatformData;
    if (!ReleaseSemaphore( 
            tSemaphoreHandle,  // handle to semaphore
            1,            // increase count by one
            NULL) )       // not interested in previous count
    {
        printf("ReleaseSemaphore error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

typedef struct _plWin32ThreadKey
{
    DWORD dwIndex;
} plWin32ThreadKey;

plThreadKey
pl__allocate_thread_local_key(void)
{
    plWin32ThreadKey* ptKey = PL_ALLOC(sizeof(plWin32ThreadKey));
    ptKey->dwIndex = TlsAlloc();
    plThreadKey tKey = {
        ._pPlatformData = ptKey
    };
    return tKey;
}

void
pl__free_thread_local_key(plThreadKey* ptKey)
{
    plWin32ThreadKey* ptWin32Key = ptKey->_pPlatformData;
    TlsFree(ptWin32Key->dwIndex);
    PL_FREE(ptKey->_pPlatformData);
    ptKey->_pPlatformData = NULL;
}

void*
pl__allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    plWin32ThreadKey* ptWin32Key = ptKey->_pPlatformData;
    LPVOID lpvData = LocalAlloc(LPTR, szSize);
    if (! TlsSetValue(ptWin32Key->dwIndex, lpvData)) 
    {
        PL_ASSERT(false);
    }
    return lpvData;
}

void*
pl__get_thread_local_data(plThreadKey* ptKey)
{
    plWin32ThreadKey* ptWin32Key = ptKey->_pPlatformData;
    LPVOID lpvData =  TlsGetValue(ptWin32Key->dwIndex);
    if(lpvData == NULL)
    {
        PL_ASSERT(false);
    }
    return lpvData;
}

void
pl__free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    plWin32ThreadKey* ptWin32Key = ptKey->_pPlatformData;
    LPVOID lpvData = TlsGetValue(ptWin32Key->dwIndex);
    LocalFree(lpvData);
}

void
pl__create_atomic_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = _aligned_malloc(sizeof(plAtomicCounter), 8);
    (*ptCounter)->ilValue = ilValue;
}

void
pl__destroy_atomic_counter(plAtomicCounter** ptCounter)
{
    _aligned_free((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl__atomic_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    ptCounter->ilValue = ilValue;
}

int64_t
pl__atomic_load(plAtomicCounter* ptCounter)
{
    return ptCounter->ilValue;
}

bool
pl__atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return InterlockedCompareExchange64(&ptCounter->ilValue, ilDesiredValue, ilExpectedValue) == ilExpectedValue;
}

void
pl__atomic_increment(plAtomicCounter* ptCounter)
{
    InterlockedIncrement64(&ptCounter->ilValue);
}

void
pl__atomic_decrement(plAtomicCounter* ptCounter)
{
    InterlockedDecrement64(&ptCounter->ilValue);
}

plConditionVariable
pl__create_condition_variable(void)
{
    CONDITION_VARIABLE* ptConditionVariable = PL_ALLOC(sizeof(CONDITION_VARIABLE ));
    InitializeConditionVariable(ptConditionVariable);
    plConditionVariable tConditionalVariable = {
        ._pPlatformData = ptConditionVariable
    };
    return tConditionalVariable;
}

void               
pl__destroy_condition_variable(plConditionVariable* ptConditionVariable)
{
    PL_FREE(ptConditionVariable->_pPlatformData);
    ptConditionVariable->_pPlatformData = NULL;
}

void               
pl__wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    CONDITION_VARIABLE* ptCondVariable = ptConditionVariable->_pPlatformData;
    WakeConditionVariable(ptCondVariable);
}

void               
pl__wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    CONDITION_VARIABLE* ptCondVariable = ptConditionVariable->_pPlatformData;
    WakeAllConditionVariable(ptCondVariable);
}

void               
pl__sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    CRITICAL_SECTION* ptCritSection = ptCriticalSection->_pPlatformData;
    CONDITION_VARIABLE* ptCondVariable = ptConditionVariable->_pPlatformData;
    SleepConditionVariableCS(ptCondVariable, ptCritSection, INFINITE);
}

const char*
pl__get_clipboard_text(void* user_data_ctx)
{
    pl_sb_reset(gptIOCtx->sbcClipboardData);
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
        pl_sb_resize(gptIOCtx->sbcClipboardData, buf_len);
        WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, gptIOCtx->sbcClipboardData, buf_len, NULL, NULL);
    }
    GlobalUnlock(wbuf_handle);
    CloseClipboard();
    return gptIOCtx->sbcClipboardData;
}

void
pl__set_clipboard_text(void* pUnused, const char* text)
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

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pilotlight_exe.c"
