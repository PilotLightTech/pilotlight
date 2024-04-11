/*
   linux_pl.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] internal api
// [SECTION] entry point
// [SECTION] internal implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h" // data registry, api registry, extension registry
#include "pl_ui.h"      // io context
#include "pl_ds.h"      // hashmap
#include "pl_os.h"      // os services

#include <time.h>     // clock_gettime, clock_getres
#include <string.h>   // strlen
#include <stdlib.h>   // free
#include <assert.h>
#include <semaphore.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h> //xcb_xfixes_query_version, apt install libxcb-xfixes0-dev
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xcb/xcb_cursor.h> // apt install libxcb-cursor-dev, libxcb-cursor0
#include <xcb/xcb_keysyms.h>
#include <X11/XKBlib.h>
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
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// internal
void pl_update_mouse_cursor_linux(void);
void pl_linux_procedure          (xcb_generic_event_t* event);
plKey pl__xcb_key_to_pl_key(uint32_t x_keycode);

// window api
plWindow* pl__create_window(const plWindowDesc* ptDesc);
void      pl__destroy_window(plWindow* ptWindow);

// os services
void  pl__read_file            (const char* pcFile, uint32_t* puSize, char* pcBuffer, const char* pcMode);
void  pl__copy_file            (const char* pcSource, const char* pcDestination);
void  pl__create_udp_socket    (plSocket** pptSocketOut, bool bNonBlocking);
void  pl__bind_udp_socket      (plSocket* ptSocket, int iPort);
bool  pl__send_udp_data        (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
bool  pl__get_udp_data         (plSocket* ptSocket, void* pData, size_t szSize);
bool  pl__has_library_changed  (plSharedLibrary* ptLibrary);
bool  pl__load_library         (const char* pcName, const char* pcTransitionalName, const char* pcLockFile, plSharedLibrary** pptLibraryOut);
void  pl__reload_library       (plSharedLibrary* ptLibrary);
void* pl__load_library_function(plSharedLibrary* ptLibrary, const char* pcName);

// thread api
void     pl__sleep(uint32_t millisec);
uint32_t pl__get_hardware_thread_count(void);
void     pl__create_thread(plThreadProcedure ptProcedure, void* pData, plThread** ppThreadOut);
void     pl__join_thread(plThread* ptThread);
void     pl__yield_thread(void);
void     pl__create_mutex(plMutex** ppMutexOut);
void     pl__lock_mutex(plMutex* ptMutex);
void     pl__unlock_mutex(plMutex* ptMutex);
void     pl__destroy_mutex(plMutex** pptMutex);
void     pl__create_critical_section(plCriticalSection** pptCriticalSectionOut);
void     pl__destroy_critical_section(plCriticalSection** pptCriticalSection);
void     pl__enter_critical_section  (plCriticalSection* ptCriticalSection);
void     pl__leave_critical_section  (plCriticalSection* ptCriticalSection);
void     pl__create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
void     pl__wait_on_semaphore(plSemaphore* ptSemaphore);
bool     pl__try_wait_on_semaphore(plSemaphore* ptSemaphore);
void     pl__release_semaphore(plSemaphore* ptSemaphore);
void     pl__destroy_semaphore(plSemaphore** pptSemaphore);
void     pl__allocate_thread_local_key(plThreadKey** pptKeyOut);
void     pl__free_thread_local_key(plThreadKey** ppuIndex);
void*    pl__allocate_thread_local_data(plThreadKey* ptKey, size_t szSize);
void*    pl__get_thread_local_data(plThreadKey* ptKey);
void     pl__free_thread_local_data(plThreadKey* ptKey, void* pData);
void     pl__create_condition_variable(plConditionVariable** pptConditionVariableOut);
void     pl__destroy_condition_variable(plConditionVariable** pptConditionVariable);
void     pl__wake_condition_variable(plConditionVariable* ptConditionVariable);
void     pl__wake_all_condition_variable(plConditionVariable* ptConditionVariable);
void     pl__sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection);
void     pl__create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut);
void     pl__destroy_barrier(plBarrier** pptBarrier);
void     pl__wait_on_barrier(plBarrier* ptBarrier);

// atomics
void    pl__create_atomic_counter  (int64_t ilValue, plAtomicCounter** ptCounter);
void    pl__destroy_atomic_counter (plAtomicCounter** ptCounter);
void    pl__atomic_store           (plAtomicCounter* ptCounter, int64_t ilValue);
int64_t pl__atomic_load            (plAtomicCounter* ptCounter);
bool    pl__atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue);
void    pl__atomic_increment       (plAtomicCounter* ptCounter);
void    pl__atomic_decrement       (plAtomicCounter* ptCounter);

static inline time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
}

typedef struct _plSharedLibrary
{
    bool     bValid;
    uint32_t uTempIndex;
    char     acPath[PL_MAX_PATH_LENGTH];
    char     acTransitionalName[PL_MAX_PATH_LENGTH];
    char     acLockFile[PL_MAX_PATH_LENGTH];
    void*    handle;
    time_t   lastWriteTime;
} plSharedLibrary;

typedef struct _plAtomicCounter
{
    atomic_int_fast64_t ilValue;
} plAtomicCounter;

typedef struct _plWindowData
{
    xcb_connection_t* ptConnection;
    xcb_window_t      tWindow;
} plWindowData;

typedef struct _plSocket
{
    int iPort;
    int iSocket;
} plSocket;

typedef struct _plThread
{
    pthread_t tHandle;
} plThread;

typedef struct _plMutex
{
    pthread_mutex_t tHandle;
} plMutex;

typedef struct _plCriticalSection
{
    pthread_mutex_t tHandle;
} plCriticalSection;

typedef struct _plSemaphore
{
    sem_t tHandle;
} plSemaphore;

typedef struct _plBarrier
{
    pthread_barrier_t tHandle;
} plBarrier;

typedef struct _plConditionVariable
{
    pthread_cond_t tHandle;
} plConditionVariable;

typedef struct _plThreadKey
{
    pthread_key_t tKey;
} plThreadKey;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// x11 & xcb stuff
Display*              gDisplay       = NULL;
xcb_connection_t*     gConnection    = NULL;
xcb_key_symbols_t*    gKeySyms       = NULL;
xcb_screen_t*         gScreen        = NULL;
bool                  gRunning       = true;
xcb_atom_t            gWmProtocols;
xcb_atom_t            gWmDeleteWin;
plSharedLibrary*      gptAppLibrary   = NULL;
void*                 gUserData       = NULL;
double                dTime           = 0.0;
double                dFrequency      = 0.0;
xcb_cursor_context_t* ptCursorContext = NULL;

// ui
plIO*           gptIOCtx = NULL;
plUiContext*    gptUiCtx = NULL;

// apis
const plDataRegistryI*      gptDataRegistry      = NULL;
const plApiRegistryI*       gptApiRegistry       = NULL;
const plExtensionRegistryI* gptExtensionRegistry = NULL;

// memory tracking
plHashMap       gtMemoryHashMap = {0};
plMemoryContext gtMemoryContext = {.ptHashMap = &gtMemoryHashMap};

// windows
plWindow** gsbtWindows = NULL;

// app function pointers
void* (*pl_app_load)    (const plApiRegistryI* ptApiRegistry, void* ptAppData);
void  (*pl_app_shutdown)(void* ptAppData);
void  (*pl_app_resize)  (void* ptAppData);
void  (*pl_app_update)  (void* ptAppData);

static inline double
pl__get_linux_absolute_time(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / dFrequency;    
}

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main()
{
    gptUiCtx = pl_create_context();
    gptIOCtx = pl_get_io();

    // os provided apis

    static const plWindowI tWindowApi = {
        .create_window  = pl__create_window,
        .destroy_window = pl__destroy_window
    };

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

    // load CORE apis
    gptApiRegistry       = pl_load_core_apis();
    gptDataRegistry      = gptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptExtensionRegistry = gptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // add os specific apis
    gptApiRegistry->add(PL_API_WINDOW, &tWindowApi);
    gptApiRegistry->add(PL_API_LIBRARY, &tLibraryApi);
    gptApiRegistry->add(PL_API_FILE, &tFileApi);
    gptApiRegistry->add(PL_API_UDP, &tUdpApi);
    gptApiRegistry->add(PL_API_THREADS, &tThreadApi);
    gptApiRegistry->add(PL_API_ATOMICS, &tAtomicsApi);

    // add contexts to data registry
    gptDataRegistry->set_data("ui", gptUiCtx);
    gptDataRegistry->set_data(PL_CONTEXT_MEMORY, &gtMemoryContext);

    // connect to x
    gDisplay = XOpenDisplay(NULL);

    // turn off auto repeat (we handle this internally)
    XAutoRepeatOff(gDisplay);

    int screen_p = 0;
    gConnection = xcb_connect(NULL, &screen_p);
    if(xcb_connection_has_error(gConnection))
    {
        assert(false && "Failed to connect to X server via XCB.");
    }

    // get data from x server
    const xcb_setup_t* setup = xcb_get_setup(gConnection);

    // loop through screens using iterator
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    
    for (int s = screen_p; s > 0; s--) 
    {
        xcb_screen_next(&it);
    }

    // after screens have been looped through, assign it.
    gScreen = it.data;

    // setup timers
    static struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
    {
        assert(false && "clock_getres() failed");
    }
    dFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
    dTime = pl__get_linux_absolute_time();

    // Notify X for mouse cursor handling
    xcb_discard_reply(gConnection, xcb_xfixes_query_version(gConnection, 4, 0).sequence);

    // Cursor context for looking up cursors for the current X cursor theme
    xcb_cursor_context_new(gConnection, gScreen, &ptCursorContext);

    // get the current key map
    gKeySyms = xcb_key_symbols_alloc(gConnection);

    // load library
    const plLibraryApiI* ptLibraryApi = gptApiRegistry->first(PL_API_LIBRARY);
    static char acLibraryName[256] = {0};
    static char acTransitionalName[256] = {0};
    pl_sprintf(acLibraryName, "./%s.so", "app");
    pl_sprintf(acTransitionalName, "./%s_", "app");
    if(ptLibraryApi->load(acLibraryName, acTransitionalName, "./lock.tmp", &gptAppLibrary))
    {
        pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
        pl_app_shutdown = (void  (__attribute__(()) *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
        pl_app_resize   = (void  (__attribute__(()) *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
        pl_app_update   = (void  (__attribute__(()) *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
        gUserData = pl_app_load(gptApiRegistry, NULL);
    }

    // main loop
    while (gRunning)
    {
        
        // Poll for events until null is returned.
        xcb_generic_event_t* event;
        while (event = xcb_poll_for_event(gConnection)) 
            pl_linux_procedure(event);

        if(gptIOCtx->bViewportSizeChanged) //-V547
            pl_app_resize(gUserData);

        pl_update_mouse_cursor_linux();

        // reload library
        if(ptLibraryApi->has_changed(gptAppLibrary))
        {
            ptLibraryApi->reload(gptAppLibrary);
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*))                     ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(void*))                     ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*))                     ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            gUserData = pl_app_load(gptApiRegistry, gUserData);
        }

        // render a frame
        const double dCurrentTime = pl__get_linux_absolute_time();
        gptIOCtx->fDeltaTime = (float)(dCurrentTime - dTime);
        dTime = dCurrentTime;

        gptDataRegistry->garbage_collect();
        pl_app_update(gUserData);
        gptExtensionRegistry->reload();
    }

    // app cleanup
    pl_app_shutdown(gUserData);

    // platform cleanup
    XAutoRepeatOn(gDisplay);
    xcb_cursor_context_free(ptCursorContext);
    xcb_key_symbols_free(gKeySyms);
    
    gptExtensionRegistry->unload_all();
    pl_unload_core_apis();

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

void
pl_linux_procedure(xcb_generic_event_t* event)
{
    xcb_client_message_event_t* cm;

    switch (event->response_type & ~0x80) 
    {

        case XCB_CLIENT_MESSAGE: 
        {
            cm = (xcb_client_message_event_t*)event;

            // Window close
            if (cm->data.data32[0] == gWmDeleteWin) 
            {
                gRunning  = false;
            }
            break;
        }

        case XCB_MOTION_NOTIFY: 
        {
            xcb_motion_notify_event_t* motion = (xcb_motion_notify_event_t*)event;
            pl_add_mouse_pos_event((float)motion->event_x, (float)motion->event_y);
            break;
        }

        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: pl_add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, true);   break;
                case XCB_BUTTON_INDEX_2: pl_add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, true); break;
                case XCB_BUTTON_INDEX_3: pl_add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, true);  break;
                case XCB_BUTTON_INDEX_4: pl_add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: pl_add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:                 pl_add_mouse_button_event(press->detail, true);          break;
            }
            break;
        }
        
        case XCB_BUTTON_RELEASE:
        {
            xcb_button_press_event_t* press = (xcb_button_press_event_t*)event;
            switch (press->detail)
            {
                case XCB_BUTTON_INDEX_1: pl_add_mouse_button_event(PL_MOUSE_BUTTON_LEFT, false);   break;
                case XCB_BUTTON_INDEX_2: pl_add_mouse_button_event(PL_MOUSE_BUTTON_MIDDLE, false); break;
                case XCB_BUTTON_INDEX_3: pl_add_mouse_button_event(PL_MOUSE_BUTTON_RIGHT, false);  break;
                case XCB_BUTTON_INDEX_4: pl_add_mouse_wheel_event (0.0f, 1.0f);                   break;
                case XCB_BUTTON_INDEX_5: pl_add_mouse_wheel_event (0.0f, -1.0f);                  break;
                default:                 pl_add_mouse_button_event(press->detail, false);          break;
            }
            break;
        }

        case XCB_KEY_PRESS:
        {
            xcb_key_release_event_t *keyEvent = (xcb_key_release_event_t *)event;

            xcb_keycode_t code = keyEvent->detail;
            uint32_t uCol = gptIOCtx->bKeyShift ? 1 : 0;
            KeySym key_sym = XkbKeycodeToKeysym(
                gDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                uCol);
            xcb_keysym_t k = xcb_key_press_lookup_keysym(gKeySyms, keyEvent, uCol);
            pl_add_key_event(pl__xcb_key_to_pl_key(key_sym), true);
            if(k < 0xFF)
                pl_add_text_event(k);
            else if (k >= 0x1000100 && k <= 0x110ffff) // utf range
                pl_add_text_event_utf16(k);
            break;
        }
        case XCB_KEY_RELEASE:
        {
            const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
            xcb_keycode_t code = keyEvent->detail;
            KeySym key_sym = XkbKeycodeToKeysym(
                gDisplay, 
                (KeyCode)code,  // event.xkey.keycode,
                0,
                0 /*code & ShiftMask ? 1 : 0*/);
            pl_add_key_event(pl__xcb_key_to_pl_key(key_sym), false);
            break;
        }
        case XCB_CONFIGURE_NOTIFY: 
        {
            // Resizing - note that this is also triggered by moving the window, but should be
            // passed anyway since a change in the x/y could mean an upper-left resize.
            // The application layer can decide what to do with this.
            xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)event;

                gsbtWindows[0]->tDesc.iXPos = configure_event->x;
                gsbtWindows[0]->tDesc.iYPos = configure_event->y;

            // Fire the event. The application layer should pick this up, but not handle it
            // as it shouldn be visible to other parts of the application.
            if(configure_event->width != gptIOCtx->afMainViewportSize[0] || configure_event->height != gptIOCtx->afMainViewportSize[1])
            {
                gptIOCtx->afMainViewportSize[0] = configure_event->width;
                gptIOCtx->afMainViewportSize[1] = configure_event->height;
                gptIOCtx->bViewportSizeChanged = true;
                gsbtWindows[0]->tDesc.uWidth = configure_event->width;
                gsbtWindows[0]->tDesc.uHeight = configure_event->height;

            }
            break;
        } 
        default: break;
    }
    free(event);
}

void
pl_update_mouse_cursor_linux(void)
{
    // updating mouse cursor
    if(gptIOCtx->tCurrentCursor != PL_MOUSE_CURSOR_ARROW && gptIOCtx->tNextCursor == PL_MOUSE_CURSOR_ARROW)
        gptIOCtx->bCursorChanged = true;

    if(gptIOCtx->bCursorChanged && gptIOCtx->tNextCursor != gptIOCtx->tCurrentCursor)
    {
        gptIOCtx->tCurrentCursor = gptIOCtx->tNextCursor;
        const char* tX11Cursor = NULL;
        switch (gptIOCtx->tNextCursor)
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

        xcb_font_t font = xcb_generate_id(gConnection);
        // There is xcb_xfixes_cursor_change_cursor_by_name. However xcb_cursor_load_cursor guarantees
        // finding the cursor for the current X theme.
        xcb_cursor_t cursor = xcb_cursor_load_cursor(ptCursorContext, tX11Cursor);
        // IM_ASSERT(cursor && "X cursor not found!");

        uint32_t value_list = cursor;
        plWindowData* ptData = gsbtWindows[0]->_pPlatformData;
        xcb_change_window_attributes(gConnection, ptData->tWindow, XCB_CW_CURSOR, &value_list);
        xcb_free_cursor(gConnection, cursor);
        xcb_close_font_checked(gConnection, font);
    }
    gptIOCtx->tNextCursor = PL_MOUSE_CURSOR_ARROW;
    gptIOCtx->bCursorChanged = false;
}

void
pl__read_file(const char* file, uint32_t* sizeIn, char* buffer, const char* mode)
{
    PL_ASSERT(sizeIn);

    FILE* dataFile = fopen(file, mode);
    uint32_t size = 0u;

    if (dataFile == NULL)
    {
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

void
pl__copy_file(const char* source, const char* destination)
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

void
pl__create_udp_socket(plSocket** pptSocketOut, bool bNonBlocking)
{
    *pptSocketOut = PL_ALLOC(sizeof(plSocket));

    // create socket
    if(((*pptSocketOut)->iSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        printf("Could not create socket\n");
        PL_ASSERT(false && "Could not create socket");
    }

    // enable non-blocking
    if(bNonBlocking)
    {
        int iFlags = fcntl((*pptSocketOut)->iSocket, F_GETFL);
        fcntl((*pptSocketOut)->iSocket, F_SETFL, iFlags | O_NONBLOCK);
    }
}

void
pl__bind_udp_socket(plSocket* ptSocket, int iPort)
{
    ptSocket->iPort = iPort;
    
    // prepare sockaddr_in struct
    struct sockaddr_in tServer = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)iPort),
        .sin_addr.s_addr = INADDR_ANY
    };

    // bind socket
    if(bind(ptSocket->iSocket, (struct sockaddr* )&tServer, sizeof(tServer)) < 0)
    {
        printf("Bind socket failed with error code : %d\n", errno);
        PL_ASSERT(false && "Socket error");
    }
}

bool
pl__send_udp_data(plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize)
{
    struct sockaddr_in tDestSocket = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)iDestPort),
        .sin_addr.s_addr = inet_addr(pcDestIP)
    };
    static const size_t szLen = sizeof(tDestSocket);

    // send
    if(sendto(ptFromSocket->iSocket, (const char*)pData, (int)szSize, 0, (struct sockaddr*)&tDestSocket, (int)szLen) < 0)
    {
        printf("sendto() failed with error code : %d\n", errno);
        PL_ASSERT(false && "Socket error");
        return false;
    }

    return true;
}

bool
pl__get_udp_data(plSocket* ptSocket, void* pData, size_t szSize)
{
    struct sockaddr_in tSiOther = {0};
    static int iSLen = (int)sizeof(tSiOther);
    memset(pData, 0, szSize);
    int iRecvLen = recvfrom(ptSocket->iSocket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tSiOther, &iSLen);

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

bool
pl__has_library_changed(plSharedLibrary* library)
{
    time_t newWriteTime = pl__get_last_write_time(library->acPath);
    return newWriteTime != library->lastWriteTime;
}

bool
pl__load_library(const char* name, const char* transitionalName, const char* lockFile, plSharedLibrary** pptLibraryOut)
{
    if(*pptLibraryOut == NULL)
    {
        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));
        (*pptLibraryOut)->bValid = false;
    }
    plSharedLibrary* library = *pptLibraryOut;

    if(library->acPath[0] == 0)             strncpy(library->acPath, name, PL_MAX_NAME_LENGTH);
    if(library->acTransitionalName[0] == 0) strncpy(library->acTransitionalName, transitionalName, PL_MAX_NAME_LENGTH);
    if(library->acLockFile[0] == 0)         strncpy(library->acLockFile, lockFile, PL_MAX_NAME_LENGTH);
    library->bValid = false;

    if(library)
    {
        struct stat attr2;
        if(stat(library->acLockFile, &attr2) == -1)  // lock file gone
        {
            char temporaryName[2024] = {0};
            library->lastWriteTime = pl__get_last_write_time(library->acPath);
            
            pl_sprintf(temporaryName, "%s%u%s", library->acTransitionalName, library->uTempIndex, ".so");
            if(++library->uTempIndex >= 1024)
            {
                library->uTempIndex = 0;
            }
            pl__copy_file(library->acPath, temporaryName);

            library->handle = NULL;
            library->handle = dlopen(temporaryName, RTLD_NOW);
            if(library->handle)
                library->bValid = true;
            else
            {
                printf("\n\n%s\n\n", dlerror());
            }
        }
    }
    return library->bValid;
}

void
pl__reload_library(plSharedLibrary* library)
{
    library->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl__load_library(library->acPath, library->acTransitionalName, library->acLockFile, &library))
            break;
        pl__sleep(100);
    }
}

void*
pl__load_library_function(plSharedLibrary* library, const char* name)
{
    PL_ASSERT(library->bValid && "Library not valid");
    void* loadedFunction = NULL;
    if(library->bValid)
    {
        loadedFunction = dlsym(library->handle, name);
    }
    return loadedFunction;
}

void
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
}

void
pl__create_thread(plThreadProcedure ptProcedure, void* pData, plThread** pptThreadOut)
{
    *pptThreadOut = PL_ALLOC(sizeof(plThread));
    if(pthread_create(&(*pptThreadOut)->tHandle, NULL, ptProcedure, pData))
    {
        PL_ASSERT(false);
    }
}

void
pl__join_thread(plThread* ptThread)
{
    pthread_join(ptThread->tHandle, NULL);
}

void
pl__yield_thread(void)
{
    sched_yield();
}

void
pl__create_mutex(plMutex** pptMutexOut)
{
    *pptMutexOut = PL_ALLOC(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
    }
}

void
pl__lock_mutex(plMutex* ptMutex)
{
    pthread_mutex_lock(&ptMutex->tHandle);
}

void
pl__unlock_mutex(plMutex* ptMutex)
{
    pthread_mutex_unlock(&ptMutex->tHandle);
}

void
pl__destroy_mutex(plMutex** pptMutex)
{
    pthread_mutex_destroy(&(*pptMutex)->tHandle);
    PL_FREE((*pptMutex));
    *pptMutex = NULL;
}

void
pl__create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    *pptCriticalSectionOut = PL_ALLOC(sizeof(plCriticalSection));
    if(pthread_mutex_init(&(*pptCriticalSectionOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
    }
}

void
pl__destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    pthread_mutex_destroy(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    *pptCriticalSection = NULL;
}

void
pl__enter_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_lock(&ptCriticalSection->tHandle);
}

void
pl__leave_critical_section(plCriticalSection* ptCriticalSection)
{
    pthread_mutex_unlock(&ptCriticalSection->tHandle);
}

uint32_t
pl__get_hardware_thread_count(void)
{

    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)numCPU;
}

void
pl__create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    *pptSemaphoreOut = PL_ALLOC(sizeof(plSemaphore));
    memset((*pptSemaphoreOut), 0, sizeof(plSemaphore));
    if(sem_init(&(*pptSemaphoreOut)->tHandle, 0, uIntialCount))
    {
        PL_ASSERT(false);
    }
}

void
pl__destroy_semaphore(plSemaphore** pptSemaphore)
{
    sem_destroy(&(*pptSemaphore)->tHandle);
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl__wait_on_semaphore(plSemaphore* ptSemaphore)
{
    sem_wait(&ptSemaphore->tHandle);
}

bool
pl__try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    return sem_trywait(&ptSemaphore->tHandle) == 0;
}

void
pl__release_semaphore(plSemaphore* ptSemaphore)
{
    sem_post(&ptSemaphore->tHandle);
}

void
pl__allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    *pptKeyOut = PL_ALLOC(sizeof(plThreadKey));
    int iStatus = pthread_key_create(&(*pptKeyOut)->tKey, NULL);
    if(iStatus != 0)
    {
        printf("pthread_key_create failed, errno=%d", errno);
        PL_ASSERT(false);
    }
}

void
pl__free_thread_local_key(plThreadKey** pptKey)
{
    pthread_key_delete((*pptKey)->tKey);
    PL_FREE((*pptKey));
    *pptKey = NULL;
}

void*
pl__allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    void* pData = PL_ALLOC(szSize);
    memset(pData, 0, szSize);
    pthread_setspecific(ptKey->tKey, pData);
    return pData;
}

void*
pl__get_thread_local_data(plThreadKey* ptKey)
{
    void* pData = pthread_getspecific(ptKey->tKey);
    return pData;
}

void
pl__free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    PL_FREE(pData);
}

void
pl__create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    *pptBarrierOut = PL_ALLOC(sizeof(plBarrier));
    pthread_barrier_init(&(*pptBarrierOut)->tHandle, NULL, uThreadCount);
}

void
pl__destroy_barrier(plBarrier** pptBarrier)
{
    pthread_barrier_destroy(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
}

void
pl__wait_on_barrier(plBarrier* ptBarrier)
{
    pthread_barrier_wait(&ptBarrier->tHandle);
}

void
pl__create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    *pptConditionVariableOut = PL_ALLOC(sizeof(plConditionVariable));
    pthread_cond_init(&(*pptConditionVariableOut)->tHandle, NULL);
}

void               
pl__destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    pthread_cond_destroy(&(*pptConditionVariable)->tHandle);
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
}

void               
pl__wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_signal(&ptConditionVariable->tHandle);
}

void               
pl__wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    pthread_cond_broadcast(&ptConditionVariable->tHandle);
}

void               
pl__sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    pthread_cond_wait(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle);
}

void
pl__create_atomic_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = malloc(sizeof(plAtomicCounter));
    atomic_init(&(*ptCounter)->ilValue, ilValue); //-V522
}

void
pl__destroy_atomic_counter(plAtomicCounter** ptCounter)
{
    free((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl__atomic_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    atomic_store(&ptCounter->ilValue, ilValue);
}

int64_t
pl__atomic_load(plAtomicCounter* ptCounter)
{
    return atomic_load(&ptCounter->ilValue);
}

bool
pl__atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return atomic_compare_exchange_strong(&ptCounter->ilValue, &ilExpectedValue, ilDesiredValue);
}

void
pl__atomic_increment(plAtomicCounter* ptCounter)
{
    atomic_fetch_add(&ptCounter->ilValue, 1);
}

void
pl__atomic_decrement(plAtomicCounter* ptCounter)
{
    atomic_fetch_sub(&ptCounter->ilValue, 1);
}

plWindow*
pl__create_window(const plWindowDesc* ptDesc)
{
    plWindow* ptWindow = malloc(sizeof(plWindow));
    plWindowData* ptData = malloc(sizeof(plWindowData));
    ptWindow->tDesc = *ptDesc; //-V522
    ptWindow->_pPlatformData = ptData;

    ptData->tWindow = xcb_generate_id(gConnection); //-V522
    ptData->ptConnection = gConnection; //-V522

    // register event types.
    // XCB_CW_BACK_PIXEL = filling then window bg with a single colour
    // XCB_CW_EVENT_MASK is required.
    unsigned int event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // listen for keyboard and mouse buttons
    unsigned int  event_values = 
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // values to be sent over XCB (bg colour, events)
    unsigned int  value_list[] = {gScreen->black_pixel, event_values};

    // Create the window
    xcb_create_window(
        gConnection,
        XCB_COPY_FROM_PARENT,  // depth
        ptData->tWindow,
        gScreen->root, // parent
        ptDesc->iXPos,
        ptDesc->iYPos,
        ptDesc->uWidth,
        ptDesc->uHeight,
        0, // No border
        XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
        gScreen->root_visual,
        event_mask,
        value_list);

    // Change the title
    xcb_change_property(
        gConnection,
        XCB_PROP_MODE_REPLACE,
        ptData->tWindow,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,  // data should be viewed 8 bits at a time
        strlen(ptDesc->pcName),
        ptDesc->pcName);

    // Tell the server to notify when the window manager
    // attempts to destroy the window.
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        gConnection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        gConnection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        gConnection,
        wm_delete_cookie,
        NULL);
    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        gConnection,
        wm_protocols_cookie,
        NULL);
    gWmDeleteWin = wm_delete_reply->atom;
    gWmProtocols = wm_protocols_reply->atom;

    xcb_change_property(
        gConnection,
        XCB_PROP_MODE_REPLACE,
        ptData->tWindow,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom);

    // Map the window to the screen
    xcb_map_window(gConnection, ptData->tWindow);

    int stream_result = xcb_flush(gConnection);

    pl_sb_push(gsbtWindows, ptWindow);

    return ptWindow;
}

void
pl__destroy_window(plWindow* ptWindow)
{
    plWindowData* ptData = ptWindow->_pPlatformData;
    xcb_destroy_window(gConnection, ptData->tWindow);
    free(ptData);
    free(ptWindow);
}

plKey
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
        default:
        return PL_KEY_NONE;
    }            
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pilotlight_exe.c"
