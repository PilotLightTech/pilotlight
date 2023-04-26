/*
   pl_linux.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_linux.h"
#include "pl_io.h"
#include "pl_os.h"
#include <time.h> // clock_gettime, clock_getres
#include <xcb/xcb.h>
#include <xcb/xfixes.h> //xcb_xfixes_query_version, apt install libxcb-xfixes0-dev
#include <X11/XKBlib.h> // XkbKeycodeToKeysym
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xcb/xcb_cursor.h> // apt install libxcb-cursor-dev, libxcb-cursor0
#include <stdlib.h>       // malloc
#include <string.h>       // memset, strncpy
#include <sys/stat.h>     // stat, timespec
#include <stdio.h>        // file api
#include <dlfcn.h>        // dlopen, dlsym, dlclose
#include <sys/types.h>
#include <fcntl.h>        // O_RDONLY, O_WRONLY ,O_CREAT
#include <sys/sendfile.h> // sendfile
#include <sys/socket.h>   // sockets
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plLinuxBackendData
{
    double                dTime;
    double                dFrequency;
    Display*              ptDisplay;
    xcb_connection_t*     ptConnection;
    xcb_screen_t*         ptScreen;
    xcb_window_t*         ptWindow;
    xcb_cursor_context_t* ptCursorContext;
} plLinuxBackendData;

typedef struct _plLinuxSharedLibrary
{
    void*  handle;
    time_t lastWriteTime;
} plLinuxSharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

plIOApiI* gptIoApi = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plKey pl__xcb_key_to_pl_key(uint32_t x_keycode);

static inline double
pl__get_linux_absolute_time(void)
{
    plIOContext* ptIOCtx = gptIoApi->get_context();
    plLinuxBackendData* ptLinuxBackendData = ptIOCtx->pBackendData;

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / ptLinuxBackendData->dFrequency;    
}

static inline time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
}

static void        pl__read_file            (const char* pcFile, unsigned* puSize, char* pcBuffer, const char* pcMode);
static void        pl__copy_file            (const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer);
static void        pl__create_udp_socket    (plSocket* ptSocketOut, bool bNonBlocking);
static void        pl__bind_udp_socket      (plSocket* ptSocket, int iPort);
static bool        pl__send_udp_data        (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
static bool        pl__get_udp_data         (plSocket* ptSocket, void* pData, size_t szSize);
static bool        pl__has_library_changed  (plSharedLibrary* ptLibrary);
static bool        pl__load_library         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
static void        pl__reload_library       (plSharedLibrary* ptLibrary);
static void*       pl__load_library_function(plSharedLibrary* ptLibrary, const char* pcName);
static int         pl__sleep                (uint32_t millisec);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_init_linux(Display* ptDisplay, xcb_connection_t* ptConnection, xcb_screen_t* ptScreen, xcb_window_t* ptWindow, plIOApiI* ptIoApi)
{
    gptIoApi = ptIoApi;
    plIOContext* ptIOCtx = gptIoApi->get_context();
    ptIOCtx->pBackendData = malloc(sizeof(plLinuxBackendData));
    if(ptIOCtx->pBackendData == NULL)
        return;
    memset(ptIOCtx->pBackendData, 0, sizeof(plLinuxBackendData));
    plLinuxBackendData* ptLinuxBackendData = ptIOCtx->pBackendData;
    ptLinuxBackendData->ptDisplay = ptDisplay;
    ptLinuxBackendData->ptConnection = ptConnection;
    ptLinuxBackendData->ptScreen = ptScreen;
    ptLinuxBackendData->ptWindow = ptWindow;

    // setup timers
    static struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_getres() failed");
    }
    ptLinuxBackendData->dFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
    ptLinuxBackendData->dTime = pl__get_linux_absolute_time();

    // Notify X for mouse cursor handling
    xcb_discard_reply(ptLinuxBackendData->ptConnection, xcb_xfixes_query_version(ptLinuxBackendData->ptConnection, 4, 0).sequence);

    // Cursor context for looking up cursors for the current X cursor theme
    xcb_cursor_context_new(ptLinuxBackendData->ptConnection, ptLinuxBackendData->ptScreen, &ptLinuxBackendData->ptCursorContext);
}

void
pl_cleanup_linux(void)
{
    plIOContext* ptIOCtx = gptIoApi->get_context();
    plLinuxBackendData* ptLinuxBackendData = ptIOCtx->pBackendData;

    xcb_cursor_context_free(ptLinuxBackendData->ptCursorContext);
}

void
pl_new_frame_linux(void)
{
    plIOContext* ptIOCtx = gptIoApi->get_context();
    plLinuxBackendData* ptLinuxBackendData = ptIOCtx->pBackendData;

    const double dCurrentTime = pl__get_linux_absolute_time();
    ptIOCtx->fDeltaTime = (float)(dCurrentTime - ptLinuxBackendData->dTime);
    ptLinuxBackendData->dTime = dCurrentTime;
}

void
pl_update_mouse_cursor_linux(void)
{
    plIOContext* ptIOCtx = gptIoApi->get_context();
    plLinuxBackendData* ptLinuxBackendData = ptIOCtx->pBackendData;

    // updating mouse cursor
    if(ptIOCtx->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && ptIOCtx->tNextCursor == PL_MOUSE_CURSOR_ARROW)
        ptIOCtx->bCursorChanged = true;

    if(ptIOCtx->bCursorChanged && ptIOCtx->tNextCursor != ptIOCtx->tCurrentCursor)
    {
        ptIOCtx->tCurrentCursor = ptIOCtx->tNextCursor;
        const char* tX11Cursor = NULL;
        switch (ptIOCtx->tNextCursor)
        {
            case PL_MOUSE_CURSOR_ARROW:       tX11Cursor = "left_ptr"; break;
            case PL_MOUSE_CURSOR_TEXT_INPUT:  tX11Cursor = "xterm"; break;
            case PL_MOUSE_CURSOR_RESIZE_ALL:  tX11Cursor = "fleur"; break;
            case PL_MOUSE_CURSOR_RESIZE_EW:   tX11Cursor = "sb_h_double_arrow"; break;
            case PL_MOUSE_CURSOR_RESIZE_NS:   tX11Cursor = "sb_v_double_arrow"; break;
            case PL_MOUSE_CURSOR_RESIZE_NESW: tX11Cursor = "bottom_left_corner"; break;
            case PL_MOUSE_CURSOR_RESIZE_NWSE: tX11Cursor = "bottom_right_corner"; break;
            case PL_MOUSE_CURSOR_HAND:        tX11Cursor = "hand1"; break;
            case PL_MOUSE_CURSOR_NOT_ALLOWED: tX11Cursor = "circle"; break;
        }  

        xcb_font_t font = xcb_generate_id(ptLinuxBackendData->ptConnection);
        // There is xcb_xfixes_cursor_change_cursor_by_name. However xcb_cursor_load_cursor guarantees
        // finding the cursor for the current X theme.
        xcb_cursor_t cursor = xcb_cursor_load_cursor(ptLinuxBackendData->ptCursorContext, tX11Cursor);
        // IM_ASSERT(cursor && "X cursor not found!");

        uint32_t value_list = cursor;
        xcb_change_window_attributes(ptLinuxBackendData->ptConnection, *ptLinuxBackendData->ptWindow, XCB_CW_CURSOR, &value_list);
        xcb_free_cursor(ptLinuxBackendData->ptConnection, cursor);
        xcb_close_font_checked(ptLinuxBackendData->ptConnection, font);
    }
    ptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
    ptIOCtx->bCursorChanged = false;
}

void
pl_linux_procedure(xcb_generic_event_t* event)
{
    plIOContext* ptIOCtx = gptIoApi->get_context();
    plLinuxBackendData* ptLinuxBackendData = ptIOCtx->pBackendData;

    xcb_client_message_event_t* cm;

    switch (event->response_type & ~0x80) 
    {

        case XCB_MOTION_NOTIFY: 
        {
            xcb_motion_notify_event_t* motion = (xcb_motion_notify_event_t*)event;
            gptIoApi->add_mouse_pos_event((float)motion->event_x, (float)motion->event_y);
            break;
        }

        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: gptIoApi->add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, true);   break;
                case XCB_BUTTON_INDEX_2: gptIoApi->add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, true); break;
                case XCB_BUTTON_INDEX_3: gptIoApi->add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, true);  break;
                case XCB_BUTTON_INDEX_4: gptIoApi->add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: gptIoApi->add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:                 gptIoApi->add_mouse_button_event(press->detail, true);          break;
            }
            break;
        }
        
        case XCB_BUTTON_RELEASE:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: gptIoApi->add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, false);   break;
                case XCB_BUTTON_INDEX_2: gptIoApi->add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, false); break;
                case XCB_BUTTON_INDEX_3: gptIoApi->add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, false);  break;
                case XCB_BUTTON_INDEX_4: gptIoApi->add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: gptIoApi->add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:                 gptIoApi->add_mouse_button_event(press->detail, false);          break;
            }
            break;
        }

        case XCB_KEY_PRESS:
        {
            const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
            xcb_keycode_t code = keyEvent->detail;
            KeySym key_sym = XkbKeycodeToKeysym(
                ptLinuxBackendData->ptDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                0 /*code & ShiftMask ? 1 : 0*/);
            gptIoApi->add_key_event(pl__xcb_key_to_pl_key(key_sym), true);
            break;
        }
        case XCB_KEY_RELEASE:
        {
            const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
            xcb_keycode_t code = keyEvent->detail;
            KeySym key_sym = XkbKeycodeToKeysym(
                ptLinuxBackendData->ptDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                0 /*code & ShiftMask ? 1 : 0*/);
            gptIoApi->add_key_event(pl__xcb_key_to_pl_key(key_sym), false);
            break;
        }
        case XCB_CONFIGURE_NOTIFY: 
        {
            // Resizing - note that this is also triggered by moving the window, but should be
            // passed anyway since a change in the x/y could mean an upper-left resize.
            // The application layer can decide what to do with this.
            xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)event;

            // Fire the event. The application layer should pick this up, but not handle it
            // as it shouldn be visible to other parts of the application.
            if(configure_event->width != ptIOCtx->afMainViewportSize[0] || configure_event->height != ptIOCtx->afMainViewportSize[1])
            {
                ptIOCtx->afMainViewportSize[0] = configure_event->width;
                ptIOCtx->afMainViewportSize[1] = configure_event->height;
                ptIOCtx->bViewportSizeChanged = true;
            }
            break;
        } 
    }
}

void
pl_load_os_apis(plApiRegistryApiI* ptApiRegistry)
{
    static plFileApiI tApi0 = {
        .copy = pl__copy_file,
        .read = pl__read_file
    };
    
    static plUdpApiI tApi1 = {
        .create_socket = pl__create_udp_socket,
        .bind_socket   = pl__bind_udp_socket,  
        .get_data      = pl__get_udp_data,
        .send_data     = pl__send_udp_data
    };

    static plOsServicesApiI tApi2 = {
        .sleep     = pl__sleep
    };

    ptApiRegistry->add(PL_API_FILE, &tApi0);
    ptApiRegistry->add(PL_API_UDP, &tApi1);
    ptApiRegistry->add(PL_API_OS_SERVICES, &tApi2);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__read_file(const char* file, unsigned* sizeIn, char* buffer, const char* mode)
{
    PL_ASSERT(sizeIn);

    FILE* dataFile = fopen(file, mode);
    unsigned size = 0u;

    if (dataFile == NULL)
    {
        PL_ASSERT(false && "File not found.");
        *sizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(dataFile, 0, SEEK_END);
    size = ftell(dataFile);
    fseek(dataFile, 0, SEEK_SET);

    if(buffer == NULL)
    {
        *sizeIn = size;
        fclose(dataFile);
        return;
    }

    // copy the file into the buffer:
    size_t result = fread(buffer, sizeof(char), size, dataFile);
    if (result != size)
    {
        if (feof(dataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(dataFile)) {
            perror("Error reading test.bin");
        }
        PL_ASSERT(false && "File not read.");
    }

    fclose(dataFile);
}

static void
pl__copy_file(const char* source, const char* destination, unsigned* size, char* buffer)
{
    uint32_t bufferSize = 0u;
    pl__read_file(source, &bufferSize, NULL, "rb");

    struct stat stat_buf;
    int fromfd = open(source, O_RDONLY);
    fstat(fromfd, &stat_buf);
    int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
    int n = 1;
    while (n > 0)
        n = sendfile(tofd, fromfd, 0, bufferSize * 2);
}

static void
pl__create_udp_socket(plSocket* ptSocketOut, bool bNonBlocking)
{

    int iLinuxSocket = 0;

    // create socket
    if((iLinuxSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        printf("Could not create socket\n");
        PL_ASSERT(false && "Could not create socket");
    }

    // enable non-blocking
    if(bNonBlocking)
    {
        int iFlags = fcntl(iLinuxSocket, F_GETFL);
        fcntl(iLinuxSocket, F_SETFL, iFlags | O_NONBLOCK);
    }
}

static void
pl__bind_udp_socket(plSocket* ptSocket, int iPort)
{
    ptSocket->iPort = iPort;
    PL_ASSERT(ptSocket->_pPlatformData && "Socket not created yet");
    int iLinuxSocket = (int)((intptr_t )ptSocket->_pPlatformData);
    
    // prepare sockaddr_in struct
    struct sockaddr_in tServer = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)iPort),
        .sin_addr.s_addr = INADDR_ANY
    };

    // bind socket
    if(bind(iLinuxSocket, (struct sockaddr* )&tServer, sizeof(tServer)) < 0)
    {
        printf("Bind socket failed with error code : %d\n", errno);
        PL_ASSERT(false && "Socket error");
    }
}

static bool
pl__send_udp_data(plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize)
{
    PL_ASSERT(ptFromSocket->_pPlatformData && "Socket not created yet");
    int iLinuxSocket = (int)((intptr_t )ptFromSocket->_pPlatformData);

    struct sockaddr_in tDestSocket = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)iDestPort),
        .sin_addr.s_addr = inet_addr(pcDestIP)
    };
    static const size_t szLen = sizeof(tDestSocket);

    // send
    if(sendto(iLinuxSocket, (const char*)pData, (int)szSize, 0, (struct sockaddr*)&tDestSocket, (int)szLen) < 0)
    {
        printf("sendto() failed with error code : %d\n", errno);
        PL_ASSERT(false && "Socket error");
        return false;
    }

    return true;
}

static bool
pl__get_udp_data(plSocket* ptSocket, void* pData, size_t szSize)
{
    PL_ASSERT(ptSocket->_pPlatformData && "Socket not created yet");
    int iLinuxSocket = (int)((intptr_t )ptSocket->_pPlatformData);

    struct sockaddr_in tSiOther = {0};
    static int iSLen = (int)sizeof(tSiOther);
    memset(pData, 0, szSize);
    int iRecvLen = recvfrom(iLinuxSocket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tSiOther, &iSLen);

    if(iRecvLen < 0)
    {
        if(errno != EWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", errno);
            PL_ASSERT(false && "Socket error");
            return false;
        }
    }
    return iRecvLen > 0;
}

static bool
pl__has_library_changed(plSharedLibrary* library)
{
    time_t newWriteTime = pl__get_last_write_time(library->acPath);
    plLinuxSharedLibrary* linuxLibrary = library->_pPlatformData;
    return newWriteTime != linuxLibrary->lastWriteTime;
}

static bool
pl__load_library(plSharedLibrary* library, const char* name, const char* transitionalName, const char* lockFile)
{
    if(library->acPath[0] == 0)             strncpy(library->acPath, name, PL_MAX_NAME_LENGTH);
    if(library->acTransitionalName[0] == 0) strncpy(library->acTransitionalName, transitionalName, PL_MAX_NAME_LENGTH);
    if(library->acLockFile[0] == 0)         strncpy(library->acLockFile, lockFile, PL_MAX_NAME_LENGTH);
    library->bValid = false;

    if(library->_pPlatformData == NULL)
        library->_pPlatformData = malloc(sizeof(plLinuxSharedLibrary));
    plLinuxSharedLibrary* linuxLibrary = library->_pPlatformData;

    if(linuxLibrary)
    {
        struct stat attr2;
        if(stat(library->acLockFile, &attr2) == -1)  // lock file gone
        {
            char temporaryName[2024] = {0};
            linuxLibrary->lastWriteTime = pl__get_last_write_time(library->acPath);
            
            pl_sprintf(temporaryName, "%s%u%s", library->acTransitionalName, library->uTempIndex, ".so");
            if(++library->uTempIndex >= 1024)
            {
                library->uTempIndex = 0;
            }
            pl__copy_file(library->acPath, temporaryName, NULL, NULL);

            linuxLibrary->handle = NULL;
            linuxLibrary->handle = dlopen(temporaryName, RTLD_NOW);
            if(linuxLibrary->handle)
                library->bValid = true;
            else
            {
                printf("\n\n%s\n\n", dlerror());
            }
        }
    }
    return library->bValid;
}

static void
pl__reload_library(plSharedLibrary* library)
{
    library->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl__load_library(library, library->acPath, library->acTransitionalName, library->acLockFile))
            break;
        pl__sleep(100);
    }
}

static void*
pl__load_library_function(plSharedLibrary* library, const char* name)
{
    PL_ASSERT(library->bValid && "Library not valid");
    void* loadedFunction = NULL;
    if(library->bValid)
    {
        plLinuxSharedLibrary* linuxLibrary = library->_pPlatformData;
        loadedFunction = dlsym(linuxLibrary->handle, name);
    }
    return loadedFunction;
}

static int
pl__sleep(uint32_t millisec)
{
    struct timespec ts = {0};
    int res;

    ts.tv_sec = millisec / 1000;
    ts.tv_nsec = (millisec % 1000) * 1000000;

    do 
    {
        res = nanosleep(&ts, &ts);
    } 
    while (res);

    return res;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plKey
pl__xcb_key_to_pl_key(uint32_t x_keycode)
{
    switch (x_keycode) 
    {
        case XKB_KEY_BackSpace:   return PL_KEY_BACKSPACE;
        case XKB_KEY_Return:      return PL_KEY_ENTER;
        case XKB_KEY_Tab:         return PL_KEY_TAB;
        case XKB_KEY_Pause:       return PL_KEY_PAUSE;
        case XKB_KEY_Caps_Lock:   return PL_KEY_CAPS_LOCK;
        case XKB_KEY_Escape:      return PL_KEY_ESCAPE;
        case XKB_KEY_space:       return PL_KEY_SPACE;
        case XKB_KEY_Prior:       return PL_KEY_PAGE_UP;
        case XKB_KEY_Next:        return PL_KEY_PAGE_DOWN;
        case XKB_KEY_End:         return PL_KEY_END;
        case XKB_KEY_Home:        return PL_KEY_HOME;
        case XKB_KEY_Left:        return PL_KEY_LEFT_ARROW;
        case XKB_KEY_Up:          return PL_KEY_UP_ARROW;
        case XKB_KEY_Right:       return PL_KEY_RIGHT_ARROW;
        case XKB_KEY_Down:        return PL_KEY_DOWN_ARROW;
        case XKB_KEY_Print:       return PL_KEY_PRINT_SCREEN;
        case XKB_KEY_Insert:      return PL_KEY_INSERT;
        case XKB_KEY_Delete:      return PL_KEY_DELETE;
        case XKB_KEY_Help:        return PL_KEY_MENU;
        case XKB_KEY_Meta_L:      return PL_KEY_LEFT_SUPER;
        case XKB_KEY_Meta_R:      return PL_KEY_RIGHT_SUPER;
        case XKB_KEY_KP_0:        return PL_KEY_KEYPAD_0;
        case XKB_KEY_KP_1:        return PL_KEY_KEYPAD_1;
        case XKB_KEY_KP_2:        return PL_KEY_KEYPAD_2;
        case XKB_KEY_KP_3:        return PL_KEY_KEYPAD_3;
        case XKB_KEY_KP_4:        return PL_KEY_KEYPAD_4;
        case XKB_KEY_KP_5:        return PL_KEY_KEYPAD_5;
        case XKB_KEY_KP_6:        return PL_KEY_KEYPAD_6;
        case XKB_KEY_KP_7:        return PL_KEY_KEYPAD_7;
        case XKB_KEY_KP_8:        return PL_KEY_KEYPAD_8;
        case XKB_KEY_KP_9:        return PL_KEY_KEYPAD_9;
        case XKB_KEY_multiply:    return PL_KEY_KEYPAD_MULTIPLY;
        case XKB_KEY_KP_Add:      return PL_KEY_KEYPAD_ADD;   ;
        case XKB_KEY_KP_Subtract: return PL_KEY_KEYPAD_SUBTRACT;
        case XKB_KEY_KP_Decimal:  return PL_KEY_KEYPAD_DECIMAL;
        case XKB_KEY_KP_Divide:   return PL_KEY_KEYPAD_DIVIDE;
        case XKB_KEY_F1:          return PL_KEY_F1;
        case XKB_KEY_F2:          return PL_KEY_F2;
        case XKB_KEY_F3:          return PL_KEY_F3;
        case XKB_KEY_F4:          return PL_KEY_F4;
        case XKB_KEY_F5:          return PL_KEY_F5;
        case XKB_KEY_F6:          return PL_KEY_F6;
        case XKB_KEY_F7:          return PL_KEY_F7;
        case XKB_KEY_F8:          return PL_KEY_F8;
        case XKB_KEY_F9:          return PL_KEY_F9;
        case XKB_KEY_F10:         return PL_KEY_F10;
        case XKB_KEY_F11:         return PL_KEY_F11;
        case XKB_KEY_F12:         return PL_KEY_F12;
        case XKB_KEY_F13:         return PL_KEY_F13;
        case XKB_KEY_F14:         return PL_KEY_F14;
        case XKB_KEY_F15:         return PL_KEY_F15;
        case XKB_KEY_F16:         return PL_KEY_F16;
        case XKB_KEY_F17:         return PL_KEY_F17;
        case XKB_KEY_F18:         return PL_KEY_F18;
        case XKB_KEY_F19:         return PL_KEY_F19;
        case XKB_KEY_F20:         return PL_KEY_F20;
        case XKB_KEY_F21:         return PL_KEY_F21;
        case XKB_KEY_F22:         return PL_KEY_F22;
        case XKB_KEY_F23:         return PL_KEY_F23;
        case XKB_KEY_F24:         return PL_KEY_F24;
        case XKB_KEY_Num_Lock:    return PL_KEY_NUM_LOCK;
        case XKB_KEY_Scroll_Lock: return PL_KEY_SCROLL_LOCK;
        case XKB_KEY_KP_Equal:    return PL_KEY_KEYPAD_EQUAL;
        case XKB_KEY_Shift_L:     return PL_KEY_LEFT_SHIFT;
        case XKB_KEY_Shift_R:     return PL_KEY_RIGHT_SHIFT;
        case XKB_KEY_Control_L:   return PL_KEY_LEFT_CTRL;
        case XKB_KEY_Control_R:   return PL_KEY_RIGHT_CTRL;
        case XKB_KEY_Alt_L:       return PL_KEY_LEFT_ALT;
        case XKB_KEY_Alt_R:       return PL_KEY_RIGHT_ALT;
        case XKB_KEY_semicolon:   return PL_KEY_SEMICOLON;
        case XKB_KEY_plus:        return PL_KEY_KEYPAD_ADD;
        case XKB_KEY_comma:       return PL_KEY_COMMA;
        case XKB_KEY_minus:       return PL_KEY_MINUS;
        case XKB_KEY_period:      return PL_KEY_PERIOD;
        case XKB_KEY_slash:       return PL_KEY_SLASH;
        case XKB_KEY_grave:       return PL_KEY_GRAVE_ACCENT;
        case XKB_KEY_0:           return PL_KEY_0;
        case XKB_KEY_1:           return PL_KEY_1;
        case XKB_KEY_2:           return PL_KEY_2;
        case XKB_KEY_3:           return PL_KEY_3;
        case XKB_KEY_4:           return PL_KEY_4;
        case XKB_KEY_5:           return PL_KEY_5;
        case XKB_KEY_6:           return PL_KEY_6;
        case XKB_KEY_7:           return PL_KEY_7;
        case XKB_KEY_8:           return PL_KEY_8;
        case XKB_KEY_9:           return PL_KEY_9;
        case XKB_KEY_a:
        case XKB_KEY_A:           return PL_KEY_A;
        case XKB_KEY_b:
        case XKB_KEY_B:           return PL_KEY_B;
        case XKB_KEY_c:
        case XKB_KEY_C:           return PL_KEY_C;
        case XKB_KEY_d:
        case XKB_KEY_D:           return PL_KEY_D;
        case XKB_KEY_e:
        case XKB_KEY_E:           return PL_KEY_E;
        case XKB_KEY_f:
        case XKB_KEY_F:           return PL_KEY_F;
        case XKB_KEY_g:
        case XKB_KEY_G:           return PL_KEY_G;
        case XKB_KEY_h:
        case XKB_KEY_H:           return PL_KEY_H;
        case XKB_KEY_i:
        case XKB_KEY_I:           return PL_KEY_I;
        case XKB_KEY_j:
        case XKB_KEY_J:           return PL_KEY_J;
        case XKB_KEY_k:
        case XKB_KEY_K:           return PL_KEY_K;
        case XKB_KEY_l:
        case XKB_KEY_L:           return PL_KEY_L;
        case XKB_KEY_m:
        case XKB_KEY_M:           return PL_KEY_M;
        case XKB_KEY_n:
        case XKB_KEY_N:           return PL_KEY_N;
        case XKB_KEY_o:
        case XKB_KEY_O:           return PL_KEY_O;
        case XKB_KEY_p:
        case XKB_KEY_P:           return PL_KEY_P;
        case XKB_KEY_q:
        case XKB_KEY_Q:           return PL_KEY_Q;
        case XKB_KEY_r:
        case XKB_KEY_R:           return PL_KEY_R;
        case XKB_KEY_s:
        case XKB_KEY_S:           return PL_KEY_S;
        case XKB_KEY_t:
        case XKB_KEY_T:           return PL_KEY_T;
        case XKB_KEY_u:
        case XKB_KEY_U:           return PL_KEY_U;
        case XKB_KEY_v:
        case XKB_KEY_V:           return PL_KEY_V;
        case XKB_KEY_w:
        case XKB_KEY_W:           return PL_KEY_W;
        case XKB_KEY_x:
        case XKB_KEY_X:           return PL_KEY_X;
        case XKB_KEY_y:
        case XKB_KEY_Y:           return PL_KEY_Y;
        case XKB_KEY_z:
        case XKB_KEY_Z:           return PL_KEY_Z;
        default:                  return PL_KEY_NONE;
    }            
}