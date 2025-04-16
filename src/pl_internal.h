/*
    pl_internal.h
      * mostly forward declarations for functions defined
        in pl.c for use by platform backends
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] globals
// [SECTION] os declarations
// [SECTION] helper declarations
// [SECTION] helpers
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_INTERNAL_H
#define PL_INTERNAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// general
plSharedLibrary* gptAppLibrary = NULL;
void*            gpUserData    = NULL;
plIO*            gptIOCtx      = NULL;

// windows
plWindow* gptMainWindow = NULL;
plWindow** gsbtWindows = NULL;

// window callbacks
plMousePosCallback    gtMousePosCallback    = NULL;
plMouseEnterCallback  gtMouseEnterCallback  = NULL;
plMouseButtonCallback gtMouseButtonCallback = NULL;
plWindowFocusCallback gtWindowFocusCallback = NULL;
plScrollCallback      gtScrollCallback      = NULL;
plKeyCallback         gtKeyCallback         = NULL;
plCharCallback        gtCharCallback        = NULL;

// apis
const plDataRegistryI*      gptDataRegistry      = NULL;
const plApiRegistryI*       gptApiRegistry       = NULL;
const plExtensionRegistryI* gptExtensionRegistry = NULL;
const plIOI*                gptIOI               = NULL;
const plMemoryI*            gptMemory            = NULL;
bool                        gbApisDirty          = false;

// app function pointers
void* (*pl_app_load)    (const plApiRegistryI*, void*);
void  (*pl_app_shutdown)(void*);
void  (*pl_app_resize)  (plWindow*, void*);
void  (*pl_app_update)  (void*);
bool  (*pl_app_info)    (const plApiRegistryI*);

//-----------------------------------------------------------------------------
// [SECTION] os declarations
//-----------------------------------------------------------------------------

// window api
plWindowResult pl_create_window (plWindowDesc, plWindow** pptWindowOut);
void           pl_destroy_window(plWindow*);
void           pl_show_window(plWindow*);

#ifdef PL_EXPERIMENTAL
void pl_hide_window        (plWindow*);
void pl_set_window_size    (plWindow*, uint32_t, uint32_t);
void pl_set_window_pos     (plWindow*, int, int);
void pl_get_window_size    (plWindow*, uint32_t*, uint32_t*);
void pl_get_window_pos     (plWindow*, int*, int*);
void pl_minimize_window    (plWindow*);
void pl_maximize_window    (plWindow*);
void pl_restore_window     (plWindow*);
void pl_focus_window       (plWindow*);
void pl_hide_cursor        (plWindow*);
void pl_capture_cursor     (plWindow*);
void pl_normal_cursor      (plWindow*);
void pl_set_raw_mouse_input(plWindow*, bool);
bool pl_is_window_maximized(plWindow*);
bool pl_is_window_minimized(plWindow*);
bool pl_is_window_focused  (plWindow*);
bool pl_is_window_hovered  (plWindow*);
bool pl_is_window_resizable(plWindow*);
bool pl_is_window_decorated(plWindow*);
bool pl_is_window_top_most (plWindow*);
#endif

// clip board
const char* pl_get_clipboard_text(void* user_data_ctx);
void        pl_set_clipboard_text(void* pUnused, const char* text);

// library api
bool            pl_has_library_changed  (plSharedLibrary*);
plLibraryResult pl_load_library         (plLibraryDesc, plSharedLibrary** pptLibraryOut);
void            pl_reload_library       (plSharedLibrary*);
void*           pl_load_library_function(plSharedLibrary*, const char* pcName);

// thread api: mutex
typedef struct _plMutex plMutex; 
void pl_create_mutex (plMutex** ppMutexOut);
void pl_lock_mutex   (plMutex*);
void pl_unlock_mutex (plMutex*);
void pl_destroy_mutex(plMutex**);

//-----------------------------------------------------------------------------
// [SECTION] helper declarations
//-----------------------------------------------------------------------------

void pl__handle_extension_reloads(void);
void pl__unload_all_extensions(void);
void pl__load_core_apis(void);
void pl__unload_core_apis(void);
void pl__check_for_leaks(void);
void pl__garbage_collect_data_reg(void);
bool pl__check_apis(void);

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

// memory helpers
#define PL_ALLOC(x) gptMemory->tracked_realloc(NULL, x, __FILE__, __LINE__)
#define PL_FREE(x)  gptMemory->tracked_realloc(x, 0, __FILE__, __LINE__)

#endif // PL_INTERNAL_H