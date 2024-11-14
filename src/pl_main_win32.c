/*
   pl_main_win32.c
     * win32 platform backend
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] windows procedure
// [SECTION] helper implementations
// [SECTION] window api
// [SECTION] file api
// [SECTION] library api
// [SECTION] thread api
// [SECTION] clipboard api
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_VK_KEYPAD_ENTER (VK_RETURN + 256)
#define WIN32_LEAN_AND_MEAN

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
#include "pl_ds.h"    // hashmap & stretchy buffer
#include <float.h>    // FLT_MAX
#include <stdlib.h>   // exit
#include <stdio.h>    // printf
#include <wchar.h>    // mbsrtowcs, wcsrtombs
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM(), GET_Y_LPARAM()

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// helpers
void             pl__render_frame(void);
LRESULT CALLBACK pl__windows_procedure(HWND, UINT, WPARAM, LPARAM);
void             pl__update_mouse_cursor(void);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRuntimeMutex
{
    HANDLE tHandle;
} plRuntimeMutex;

typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acPath[PL_MAX_PATH_LENGTH];
    char          acTransitionalName[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    HMODULE       tHandle;
    FILETIME      tLastWriteTime;
} plSharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// win32 stuff
bool        gbFirstRun                        = true;
bool        gbEnableVirtualTerminalProcessing = true;
INT64       ilTime                            = 0;
INT64       ilTicksPerSecond                  = 0;
HWND        tMouseHandle                      = NULL;
bool        bMouseTracked                     = true;
WNDCLASSEXW gtWc                              = {0};

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    const char* pcAppName = "app";

    for(int i = 1; i < argc; i++)
    { 
        if(strcmp(argv[i], "--disable_vt") == 0)
        {
            gbEnableVirtualTerminalProcessing = false;
        }
        else if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--app") == 0)
        {
            pcAppName = argv[i + 1];
            i++;
        }
        else if(strcmp(argv[i], "--version") == 0)
        {
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION);
            return 0;
        }
        else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("\nPilot Light - light weight game engine\n");
            printf("Version: %s\n\n", PILOT_LIGHT_VERSION);
            printf("Usage: pilot_light.exe [options]\n\n");
            printf("Options:\n");
            printf("-h              %s\n", "Displays this information.");
            printf("--help          %s\n", "Displays this information.");
            printf("-version        %s\n", "Displays Pilot Light version information.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'app'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'app'.");

            printf("\nWin32 Only:\n");
            printf("--disable_vt:   %s\n", "Disables escape characters.");
            return 0;
        }
    }

    // load core apis
    pl__load_core_apis();

    gptIOCtx = gptIOI->get_io();

    // command line args
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    // set clipboard functions (may need to move this to OS api)
    gptIOCtx->set_clipboard_text_fn = pl_set_clipboard_text;
    gptIOCtx->get_clipboard_text_fn = pl_get_clipboard_text;
    
    // register win32 class
    gtWc.cbSize        = sizeof(WNDCLASSEX);
    gtWc.style         = CS_HREDRAW | CS_VREDRAW;
    gtWc.lpfnWndProc   = pl__windows_procedure;
    gtWc.cbClsExtra    = 0;
    gtWc.cbWndExtra    = 0;
    gtWc.hInstance     = GetModuleHandle(NULL);
    gtWc.hIcon         = NULL;
    gtWc.hCursor       = NULL;
    gtWc.hbrBackground = NULL;
    gtWc.lpszMenuName  = NULL;
    gtWc.lpszClassName = L"Pilot Light (win32)";
    gtWc.hIconSm       = NULL;
    RegisterClassExW(&gtWc);

    // setup info for clock
    QueryPerformanceFrequency((LARGE_INTEGER*)&ilTicksPerSecond);
    QueryPerformanceCounter((LARGE_INTEGER*)&ilTime);
    
    // setup console
    DWORD tCurrentMode = 0;
    DWORD tOriginalMode = 0;
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

    // load app library
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName
    };
    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        pl_app_info     = (bool  (__cdecl  *)(const plApiRegistryI*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");

        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        gpUserData = pl_app_load(gptApiRegistry, NULL);
        bool bApisFound = pl__check_apis();
        if(!bApisFound)
            return 3;
    }
    else
        return 2;

    // main loop
    while (gptIOCtx->bRunning)
    {

        // while queue has messages, remove and dispatch them (but do not block on empty queue)
        MSG tMsg = {0};
        while (PeekMessage(&tMsg, NULL, 0, 0, PM_REMOVE))
        {
            // check for quit because peekmessage does not signal this via return val
            if (tMsg.message == WM_QUIT)
            {
                gptIOCtx->bRunning = false;
                break;
            }
            // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
            TranslateMessage(&tMsg);
            DispatchMessage(&tMsg);
        }

        // update mouse cursor icon if changed
        pl__update_mouse_cursor();

        // reload library
        if(ptLibraryApi->has_changed(gptAppLibrary))
        {
            ptLibraryApi->reload(gptAppLibrary);
            pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");

            pl__handle_extension_reloads();
            gpUserData = pl_app_load(gptApiRegistry, gpUserData);
        }

        // render frame
        if(gptIOCtx->bRunning)
            pl__render_frame();
    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    UnregisterClassW(gtWc.lpszClassName, GetModuleHandle(NULL));
    pl_sb_free(gsbtWindows);

    // unload extensions & APIs
    pl__unload_all_extensions();
    pl__unload_core_apis();

    // return console to original mode
    if(gbEnableVirtualTerminalProcessing)
    {
        if(!SetConsoleMode(tStdOutHandle, tOriginalMode))
            exit(GetLastError());
    }

    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    pl__check_for_leaks();
}

//-----------------------------------------------------------------------------
// [SECTION] windows procedure
//-----------------------------------------------------------------------------

// helpers
inline bool pl__is_vk_down(int iVk) { return (GetKeyState(iVk) & 0x8000) != 0;}
plKey       pl__virtual_key_to_pl_key (WPARAM tWParam);

LRESULT CALLBACK 
pl__windows_procedure(HWND tHwnd, UINT tMsg, WPARAM tWParam, LPARAM tLParam)
{
    // find window
    plWindow* ptWindow = NULL;
    for(uint32_t i = 0; i < pl_sb_size(gsbtWindows); i++)
    {
        HWND tHandle = gsbtWindows[i]->_pPlatformData;
        if(tHandle == tHwnd)
        {
            ptWindow = gsbtWindows[i];
            break;
        }
    }

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

                if(gptIOCtx->tMainViewportSize.x != (float)iCWidth || gptIOCtx->tMainViewportSize.y != (float)iCHeight)
                    gptIOCtx->bViewportSizeChanged = true;  

                gptIOCtx->tMainViewportSize.x = (float)iCWidth;
                gptIOCtx->tMainViewportSize.y = (float)iCHeight;

                if(ptWindow)
                {
                    ptWindow->tDesc.uWidth = (uint32_t)iCWidth;
                    ptWindow->tDesc.uHeight = (uint32_t)iCHeight;
                }

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
                gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
                gptIOCtx->tCurrentCursor = PL_MOUSE_CURSOR_NONE;
                pl__update_mouse_cursor();
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
            gptIOI->add_mouse_pos_event((float)tMousePos.x, (float)tMousePos.y);
            break;
        }
        case WM_MOUSELEAVE:
        {
            if(tHwnd == tMouseHandle)
            {
                tMouseHandle = NULL;
                gptIOI->add_mouse_pos_event(-FLT_MAX, -FLT_MAX);
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
            gptIOCtx->_iMouseButtonsDown &= ~(1 << iButton);
            if(gptIOCtx->_iMouseButtonsDown == 0 && GetCapture() == tHwnd)
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
		RECT rect = *(RECT*)(tLParam);
		ptWindow->tDesc.iXPos = rect.left;
		ptWindow->tDesc.iYPos = rect.top;
		break;
	}
    #endif
    }
    return DefWindowProcW(tHwnd, tMsg, tWParam, tLParam);
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
pl__render_frame(void)
{
    pl__garbage_collect_data_reg();

    // setup time step
    INT64 ilCurrentTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
    gptIOCtx->fDeltaTime = (float)(ilCurrentTime - ilTime) / ilTicksPerSecond;
    ilTime = ilCurrentTime;
    if(!gptIOCtx->bViewportMinimized)
    {
        pl_app_update(gpUserData);
        pl__handle_extension_reloads();
    }
}

void
pl__update_mouse_cursor(void)
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

//-----------------------------------------------------------------------------
// [SECTION] window api
//-----------------------------------------------------------------------------

plOSResult
pl_create_window(plWindowDesc tDesc, plWindow** pptWindowOut)
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
        gtWc.lpszClassName,
        awWideTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        tWr.left, tWr.top, tWr.right - tWr.left, tWr.bottom - tWr.top,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL // user data
    );

    plWindow* ptWindow = malloc(sizeof(plWindow));
    ptWindow->tDesc = tDesc;
    ptWindow->_pPlatformData = tHandle;
    pl_sb_push(gsbtWindows, ptWindow);
    *pptWindowOut = ptWindow;

    // show window
    ShowWindow(tHandle, SW_SHOWDEFAULT);
    return PL_OS_RESULT_SUCCESS;
}

void
pl_destroy_window(plWindow* ptWindow)
{
    DestroyWindow(ptWindow->_pPlatformData);
    free(ptWindow);
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
pl_file_delete(const char* pcFile)
{
    BOOL bResult = DeleteFile(pcFile);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
}

plFileResult
pl_binary_read_file(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{
    PL_ASSERT(pszSizeIn);

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
        PL_ASSERT(false && "File not read.");
        return PL_FILE_RESULT_FAIL;
    }

    fclose(ptDataFile);
    return PL_FILE_RESULT_SUCCESS;
}

plFileResult
pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer)
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
pl_copy_file(const char* pcSource, const char* pcDestination)
{
    BOOL bResult = CopyFile(pcSource, pcDestination, FALSE);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
}

//-----------------------------------------------------------------------------
// [SECTION] library api
//-----------------------------------------------------------------------------

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
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    PL_ASSERT(ptLibrary);
    if(ptLibrary)
    {
        FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return CompareFileTime(&newWriteTime, &ptLibrary->tLastWriteTime) != 0;
    }
    return false;
}

plOSResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{

    plSharedLibrary* ptLibrary = NULL;

    if(*pptLibraryOut == NULL)
    {
        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));

        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;

        pl_sprintf(ptLibrary->acPath, "%s.dll", tDesc.pcName);

        if(tDesc.pcTransitionalName)
            strncpy(ptLibrary->acTransitionalName, tDesc.pcTransitionalName, PL_MAX_PATH_LENGTH);
        else
        {
            pl_sprintf(ptLibrary->acTransitionalName, "%s_", tDesc.pcName);
        }

        if(tDesc.pcLockFile)
            strncpy(ptLibrary->acLockFile, tDesc.pcLockFile, PL_MAX_PATH_LENGTH);
        else
            strncpy(ptLibrary->acLockFile, "lock.tmp", PL_MAX_PATH_LENGTH);
    }
    else
        ptLibrary = *pptLibraryOut;

    ptLibrary->bValid = false;

    WIN32_FILE_ATTRIBUTE_DATA tIgnored;
    if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
    {
        char acTemporaryName[2024] = {0};
        ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dll");
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl_copy_file(ptLibrary->acPath, acTemporaryName);

        ptLibrary->tHandle = NULL;
        ptLibrary->tHandle = LoadLibraryA(acTemporaryName);
        if(ptLibrary->tHandle)
            ptLibrary->bValid = true;
        else
        {
            DWORD iLastError = GetLastError();
            printf("LoadLibaryA() failed with error code : %d\n", iLastError);
        }
    }

    if(ptLibrary->bValid)
        return PL_OS_RESULT_SUCCESS;
    return PL_OS_RESULT_FAIL;
}

void
pl_reload_library(plSharedLibrary* ptLibrary)
{
    ptLibrary->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(ptLibrary->tDesc, &ptLibrary))
            break;
        pl_sleep(100);
    }
}

void*
pl_load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "library not valid, should have been checked");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        pLoadedFunction = (void*)GetProcAddress(ptLibrary->tHandle, name);
    }
    return pLoadedFunction;
}

//-----------------------------------------------------------------------------
// [SECTION] thread api
//-----------------------------------------------------------------------------

void
pl_sleep(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
}

plRuntimeMutex
pl_create_runtime_mutex(void)
{
    plRuntimeMutex tMutex = {0};
    tMutex.tHandle = CreateMutex(NULL, FALSE, NULL);
    return tMutex;
}

void
pl_destroy_runtime_mutex(plRuntimeMutex* ptMutex)
{
    CloseHandle(ptMutex->tHandle);
}

void
pl_lock_runtime_mutex(plRuntimeMutex* ptMutex)
{
    DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
}

void
pl_unlock_runtime_mutex(plRuntimeMutex* ptMutex)
{
    if(!ReleaseMutex(ptMutex->tHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

uint32_t
pl_get_hardware_thread_count(void)
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

//-----------------------------------------------------------------------------
// [SECTION] clipboard api
//-----------------------------------------------------------------------------

const char*
pl_get_clipboard_text(void* user_data_ctx)
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

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"