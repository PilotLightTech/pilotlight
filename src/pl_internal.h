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
plWindow**       gsbtWindows = NULL;

// apis
const plDataRegistryI*      gptDataRegistry      = NULL;
const plApiRegistryI*       gptApiRegistry       = NULL;
const plExtensionRegistryI* gptExtensionRegistry = NULL;
const plIOI*                gptIOI               = NULL;
const plMemoryI*            gptMemory            = NULL;

// app function pointers
void* (*pl_app_load)    (const plApiRegistryI*, void*);
void  (*pl_app_shutdown)(void*);
void  (*pl_app_resize)  (void*);
void  (*pl_app_update)  (void*);
bool  (*pl_app_info)    (const plApiRegistryI*);

//-----------------------------------------------------------------------------
// [SECTION] os declarations
//-----------------------------------------------------------------------------

typedef struct _plRuntimeMutex plRuntimeMutex;

uint32_t pl_get_hardware_thread_count(void);

// window api
plOSResult pl_create_window (plWindowDesc, plWindow** pptWindowOut);
void       pl_destroy_window(plWindow*);

// clip board
const char* pl_get_clipboard_text(void* user_data_ctx);
void        pl_set_clipboard_text(void* pUnused, const char* text);

// file api
bool         pl_file_exists      (const char* pcFile);
plFileResult pl_file_delete      (const char* pcFile);
plFileResult pl_binary_read_file (const char* pcFile, size_t* pszSize, uint8_t* pcBuffer);
plFileResult pl_copy_file        (const char* pcSource, const char* pcDestination);
plFileResult pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer);

// library api
bool       pl_has_library_changed  (plSharedLibrary*);
plOSResult pl_load_library         (plLibraryDesc, plSharedLibrary** pptLibraryOut);
void       pl_reload_library       (plSharedLibrary*);
void*      pl_load_library_function(plSharedLibrary*, const char* pcName);

// thread api
void           pl_sleep(uint32_t uMillisec);
plRuntimeMutex pl_create_runtime_mutex (void);
void           pl_lock_runtime_mutex   (plRuntimeMutex*);
void           pl_unlock_runtime_mutex (plRuntimeMutex*);
void           pl_destroy_runtime_mutex(plRuntimeMutex*);

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