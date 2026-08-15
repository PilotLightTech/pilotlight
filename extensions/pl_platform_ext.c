
void
pl_load_platform_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTimerI tTimerI = {
        .get_time = pl_timer_get_time
    };

    const plWindowI tWindowApi = {
        .create                = pl_window_create,
        .destroy               = pl_window_destroy,
        .show                  = pl_window_show,
        .set_callback          = pl_window_set_callback,
        .get_callback          = pl_window_get_callback,
        .set_attribute         = pl_window_set_attribute,
        .get_attribute         = pl_window_get_attribute,
        .set_cursor_mode       = pl_window_set_cursor_mode,
        .get_cursor_mode       = pl_window_get_cursor_mode,
        .set_raw_mouse_input   = pl_window_set_raw_mouse_input,
        .set_fullscreen        = pl_window_set_fullscreen,
        .create_surface        = pl_window_create_surface,
        .destroy_surface       = pl_window_destroy_surface,
        .acquire_surface_image = pl_window_acquire_surface_image,
        .present_surface_image = pl_window_present_surface_image,
        .get_capabilities      = pl_window_get_capabilities
    };

    const plFileI tFileApi = {
        .copy                   = pl_file_copy,
        .exists                 = pl_file_exists,
        .remove                 = pl_file_remove,
        .binary_read            = pl_file_binary_read,
        .binary_write           = pl_file_binary_write,
        .directory_exists       = pl_file_directory_exists,
        .create_directory       = pl_file_create_directory,
        .remove_directory       = pl_file_remove_directory,
        .get_directory_info     = pl_file_get_directory_info,
        .cleanup_directory_info = pl_file_cleanup_directory_info,
    };

    const plNetworkI tNetworkApi = {
        .initialize           = pl_network_initialize,
        .cleanup              = pl_network_cleanup,
        .create_address       = pl_network_create_address,
        .destroy_address      = pl_network_destroy_address,
        .create_socket        = pl_network_create_socket,
        .destroy_socket       = pl_network_destroy_socket,
        .bind_socket          = pl_network_bind_socket,
        .send_socket_data_to  = pl_network_send_socket_data_to,
        .get_socket_data_from = pl_network_get_socket_data_from,
        .connect_socket       = pl_network_connect_socket,
        .get_socket_data      = pl_network_get_socket_data,
        .listen_socket        = pl_network_listen_socket,
        .select_sockets       = pl_network_select_sockets,
        .accept_socket        = pl_network_accept_socket,
        .send_socket_data     = pl_network_send_socket_data,
    };

    const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl_threads_get_hardware_thread_count,
        .create_thread               = pl_threads_create_thread,
        .destroy_thread              = pl_threads_destroy_thread,
        .join_thread                 = pl_threads_join_thread,
        .yield_thread                = pl_threads_yield_thread,
        .sleep_thread                = pl_threads_sleep_thread,
        .get_thread_id               = pl_threads_get_thread_id,
        .get_current_thread_id       = pl_threads_get_current_thread_id,
        .create_mutex                = pl_threads_create_mutex,
        .destroy_mutex               = pl_threads_destroy_mutex,
        .lock_mutex                  = pl_threads_lock_mutex,
        .unlock_mutex                = pl_threads_unlock_mutex,
        .create_semaphore            = pl_threads_create_semaphore,
        .destroy_semaphore           = pl_threads_destroy_semaphore,
        .wait_on_semaphore           = pl_threads_wait_on_semaphore,
        .try_wait_on_semaphore       = pl_threads_try_wait_on_semaphore,
        .release_semaphore           = pl_threads_release_semaphore,
        .allocate_thread_local_key   = pl_threads_allocate_thread_local_key,
        .allocate_thread_local_data  = pl_threads_allocate_thread_local_data,
        .free_thread_local_key       = pl_threads_free_thread_local_key, 
        .get_thread_local_data       = pl_threads_get_thread_local_data, 
        .free_thread_local_data      = pl_threads_free_thread_local_data, 
        .create_critical_section     = pl_threads_create_critical_section,
        .destroy_critical_section    = pl_threads_destroy_critical_section,
        .enter_critical_section      = pl_threads_enter_critical_section,
        .leave_critical_section      = pl_threads_leave_critical_section,
        .create_condition_variable   = pl_threads_create_condition_variable,
        .destroy_condition_variable  = pl_threads_destroy_condition_variable,
        .wake_condition_variable     = pl_threads_wake_condition_variable,
        .wake_all_condition_variable = pl_threads_wake_all_condition_variable,
        .sleep_condition_variable    = pl_threads_sleep_condition_variable,
        .create_barrier              = pl_threads_create_barrier,
        .destroy_barrier             = pl_threads_destroy_barrier,
        .wait_on_barrier             = pl_threads_wait_on_barrier
    };

    const plAtomicsI tAtomicsApi = {
        .create_counter   = pl_atomics_create_counter,
        .destroy_counter  = pl_atomics_destroy_counter,
        .store            = pl_atomics_store,
        .load             = pl_atomics_load,
        .compare_exchange = pl_atomics_compare_exchange,
        .increment        = pl_atomics_increment,
        .decrement        = pl_atomics_decrement
    };

    const plVirtualMemoryI tVirtualMemoryApi = {
        .get_page_size = pl_virtual_memory_get_page_size,
        .alloc         = pl_virtual_memory_alloc,
        .reserve       = pl_virtual_memory_reserve,
        .commit        = pl_virtual_memory_commit,
        .uncommit      = pl_virtual_memory_uncommit,
        .free          = pl_virtual_memory_free,
    };

    pl_set_api(ptApiRegistry, plWindowI, &tWindowApi);
    pl_set_api(ptApiRegistry, plFileI, &tFileApi);
    pl_set_api(ptApiRegistry, plVirtualMemoryI, &tVirtualMemoryApi);
    pl_set_api(ptApiRegistry, plAtomicsI, &tAtomicsApi);
    pl_set_api(ptApiRegistry, plThreadsI, &tThreadApi);
    pl_set_api(ptApiRegistry, plNetworkI, &tNetworkApi);
    pl_set_api(ptApiRegistry, plTimerI, &tTimerI);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptIOI = pl_get_api_latest(ptApiRegistry, plIOI);
    gptIO = gptIOI->get_io();

    static plPlatformExtData stPlatformExtCtx = {0};
    gptPlatformExtCtx = &stPlatformExtCtx;

    #if defined(_WIN32)
        stPlatformExtCtx.bMouseTracked = true;
        QueryPerformanceFrequency((LARGE_INTEGER*)&stPlatformExtCtx.ilTicksPerSecond);
        QueryPerformanceCounter((LARGE_INTEGER*)&stPlatformExtCtx.ilTime);

        #ifndef PL_NO_PLATFORM_WINDOW
        // register win32 class
        stPlatformExtCtx.tWc.cbSize        = sizeof(WNDCLASSEX);
        stPlatformExtCtx.tWc.style         = CS_HREDRAW | CS_VREDRAW;
        stPlatformExtCtx.tWc.lpfnWndProc   = pl__windows_procedure;
        stPlatformExtCtx.tWc.cbClsExtra    = 0;
        stPlatformExtCtx.tWc.cbWndExtra    = 0;
        stPlatformExtCtx.tWc.hInstance     = GetModuleHandle(NULL);
        stPlatformExtCtx.tWc.hIcon         = NULL;
        stPlatformExtCtx.tWc.hCursor       = NULL;
        stPlatformExtCtx.tWc.hbrBackground = NULL;
        stPlatformExtCtx.tWc.lpszMenuName  = NULL;
        stPlatformExtCtx.tWc.lpszClassName = L"Pilot Light (win32)";
        stPlatformExtCtx.tWc.hIconSm       = NULL;
        RegisterClassExW(&stPlatformExtCtx.tWc);
        #endif

    #elif defined(__APPLE__)
        gptIO->pBackendPlatformData = gptPlatformExtCtx;
        gptPlatformExtCtx->dInitialTime = (CFTimeInterval)((double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9);

        #ifndef PL_NO_PLATFORM_WINDOW
        aptMouseCursors[PL_MOUSE_CURSOR_ARROW] = [NSCursor arrowCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_TEXT_INPUT] = [NSCursor IBeamCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_ALL] = [NSCursor closedHandCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_HAND] = [NSCursor pointingHandCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_NOT_ALLOWED] = [NSCursor operationNotAllowedCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NS] = [NSCursor respondsToSelector:@selector(_windowResizeNorthSouthCursor)] ? [NSCursor _windowResizeNorthSouthCursor] : [NSCursor resizeUpDownCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_EW] = [NSCursor respondsToSelector:@selector(_windowResizeEastWestCursor)] ? [NSCursor _windowResizeEastWestCursor] : [NSCursor resizeLeftRightCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NESW] = [NSCursor respondsToSelector:@selector(_windowResizeNorthEastSouthWestCursor)] ? [NSCursor _windowResizeNorthEastSouthWestCursor] : [NSCursor closedHandCursor];
        aptMouseCursors[PL_MOUSE_CURSOR_RESIZE_NWSE] = [NSCursor respondsToSelector:@selector(_windowResizeNorthWestSouthEastCursor)] ? [NSCursor _windowResizeNorthWestSouthEastCursor] : [NSCursor closedHandCursor];
        pl__install_osx_event_monitor();
        gptPlatformExtCtx->gtAppDelegate = [[plNSAppDelegate alloc] init];
        #endif
    #else // linux
        {
            static struct timespec ts;
            if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
            {
                PL_ASSERT(false && "clock_getres() failed");
            }
            gptPlatformExtCtx->dFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);  
        }

        {
            struct timespec ts;
            if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
            {
                PL_ASSERT(false && "clock_gettime() failed");
            }
            uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
            gptPlatformExtCtx->dStartTime = (double)nsec_count / gptPlatformExtCtx->dFrequency; 
        }

        #ifndef PL_NO_PLATFORM_WINDOW
        // connect to x
        stPlatformExtCtx.ptDisplay = XOpenDisplay(NULL);

        if(stPlatformExtCtx.ptDisplay)
        {

            // turn off auto repeat (we handle this internally)
            XkbSetDetectableAutoRepeat(stPlatformExtCtx.ptDisplay, false, NULL);

            stPlatformExtCtx.ptConnection = xcb_connect(NULL, &stPlatformExtCtx.screen);
            if(xcb_connection_has_error(stPlatformExtCtx.ptConnection))
            {
                PL_ASSERT(false && "Failed to connect to X server via XCB.");
            }

            stPlatformExtCtx.tClipboard         = pl__intern_atom("CLIPBOARD");
            stPlatformExtCtx.tClipboardProperty = pl__intern_atom("PL_CLIPBOARD_PROPERTY");
            stPlatformExtCtx.tTargets           = pl__intern_atom("TARGETS");
            stPlatformExtCtx.tUtf8String        = pl__intern_atom("UTF8_STRING");
            stPlatformExtCtx.tText              = pl__intern_atom("TEXT");
            stPlatformExtCtx.tIncr              = pl__intern_atom("INCR");

            // get data from x server
            const xcb_setup_t* setup = xcb_get_setup(stPlatformExtCtx.ptConnection);

            // loop through screens using iterator
            xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
            
            for (int s = stPlatformExtCtx.screen; s > 0; s--) 
            {
                xcb_screen_next(&it);
            }

            // after screens have been looped through, assign it.
            stPlatformExtCtx.ptScreen = it.data;

            // Notify X for mouse cursor handling
            xcb_discard_reply(stPlatformExtCtx.ptConnection, xcb_xfixes_query_version(stPlatformExtCtx.ptConnection, 4, 0).sequence);

            // Cursor context for looking up cursors for the current X cursor theme
            xcb_cursor_context_new(stPlatformExtCtx.ptConnection, stPlatformExtCtx.ptScreen, &stPlatformExtCtx.ptCursorContext);

            // get the current key map
            stPlatformExtCtx.ptKeySyms = xcb_key_symbols_alloc(stPlatformExtCtx.ptConnection);
        }
        #endif
    #endif

    gptIO->set_clipboard_text_fn = pl_set_clipboard_text;
    gptIO->get_clipboard_text_fn = pl_get_clipboard_text;
    gptIO->platform_setup = pl_platform_setup;
    gptIO->platform_new_frame = pl_platform_new_frame;
    gptIO->platform_cleanup = pl_platform_cleanup;
}

void
pl_unload_platform_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plFileI*          ptApi0 = pl_get_api_latest(ptApiRegistry, plFileI);
    const plVirtualMemoryI* ptApi1 = pl_get_api_latest(ptApiRegistry, plVirtualMemoryI);
    const plAtomicsI*       ptApi2 = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    const plThreadsI*       ptApi3 = pl_get_api_latest(ptApiRegistry, plThreadsI);
    const plNetworkI*       ptApi4 = pl_get_api_latest(ptApiRegistry, plNetworkI);
    const plWindowI*        ptApi5 = pl_get_api_latest(ptApiRegistry, plWindowI);
    const plTimerI*         ptApi6 = pl_get_api_latest(ptApiRegistry, plTimerI);

    ptApiRegistry->remove_api(ptApi0);
    ptApiRegistry->remove_api(ptApi1);
    ptApiRegistry->remove_api(ptApi2);
    ptApiRegistry->remove_api(ptApi3);
    ptApiRegistry->remove_api(ptApi4);
    ptApiRegistry->remove_api(ptApi5);
    ptApiRegistry->remove_api(ptApi6);
}