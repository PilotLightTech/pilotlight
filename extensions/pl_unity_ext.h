/*
   pl_unity_ext.h
     - allows core apis to be used with functions directly
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_EXT_H
#define PL_UNITY_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plUnityI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include "pl.h"
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ext(plApiRegistryI*, bool reload);

//--------------------------------IO api---------------------------------------


PL_API void  pl_io_new_frame(void);
PL_API plIO* pl_io_get_io(void);

// keyboard
PL_API bool pl_io_is_key_down           (plKey);
PL_API bool pl_io_is_key_pressed        (plKey, bool repeat);
PL_API bool pl_io_is_key_released       (plKey);
PL_API int  pl_io_get_key_pressed_amount(plKey, float repeatDelay, float rate);

// mouse
PL_API bool   pl_io_is_mouse_down          (plMouseButton);
PL_API bool   pl_io_is_mouse_clicked       (plMouseButton, bool repeat);
PL_API bool   pl_io_is_mouse_released      (plMouseButton);
PL_API bool   pl_io_is_mouse_double_clicked(plMouseButton);
PL_API bool   pl_io_is_mouse_dragging      (plMouseButton, float threshold);
PL_API bool   pl_io_is_mouse_hovering_rect (plVec2 minVec, plVec2 maxVec);
PL_API void   pl_io_reset_mouse_drag_delta (plMouseButton);
PL_API plVec2 pl_io_get_mouse_drag_delta   (plMouseButton, float threshold);
PL_API plVec2 pl_io_get_mouse_pos          (void);
PL_API float  pl_io_get_mouse_wheel        (void);
PL_API bool   pl_io_is_mouse_pos_valid     (plVec2);
PL_API void   pl_io_set_mouse_cursor       (plMouseCursor);

// input functions (used by backends)
PL_API void pl_io_add_key_event         (plKey, bool down);
PL_API void pl_io_add_text_event        (uint32_t uChar);
PL_API void pl_io_add_text_event_utf16  (uint16_t uChar);
PL_API void pl_io_add_text_events_utf8  (const char* text);
PL_API void pl_io_add_mouse_pos_event   (float x, float y);
PL_API void pl_io_add_mouse_button_event(int button, bool down);
PL_API void pl_io_add_mouse_wheel_event (float horizontalDelta, float verticalDelta);
PL_API void pl_io_clear_input_characters(void);

// misc.
PL_API plVersion   pl_io_get_version       (void);
PL_API const char* pl_io_get_version_string(void);

//---------------------------data registry api---------------------------------

PL_API void  pl_data_registry_set_data(const char* name, void* data);
PL_API void* pl_data_registry_get_data(const char* name);

//------------------------------memory api-------------------------------------

PL_API void* pl_memory_realloc        (void*, size_t);
PL_API void* pl_memory_tracked_realloc(void*, size_t, const char* file, int line);

//------------------------extension registry api-------------------------------

PL_API bool pl_extension_registry_load    (const char* name, const char* loadFunc, const char* unloadFunc, bool reloadable);
PL_API bool pl_extension_registry_unload  (const char* name); 
PL_API void pl_extension_registry_add_path(const char* path); 

//------------------------------library api------------------------------------

PL_API plLibraryResult pl_library_load         (plLibraryDesc, plSharedLibrary** libraryPtrOut);
PL_API bool            pl_library_has_changed  (plSharedLibrary*);
PL_API void*           pl_library_load_function(plSharedLibrary*, const char*);    

//-------------------------------window api------------------------------------

// create/destroy
PL_API plWindowResult              pl_window_create          (plWindowDesc, plWindow** windowPtrOut);
PL_API void                        pl_window_destroy         (plWindow*);
PL_API void                        pl_window_show            (plWindow*);
PL_API const plWindowCapabilities* pl_window_get_capabilities(void);

// attributes
PL_API bool pl_window_set_attribute (plWindow*, plWindowAttribute, const plWindowAttributeValue*);
PL_API bool pl_window_get_attribute (plWindow*, plWindowAttribute, plWindowAttributeValue*);

// cursor modes
PL_API bool         pl_window_set_cursor_mode     (plWindow*, plCursorMode);
PL_API plCursorMode pl_window_get_cursor_mode     (plWindow*);
PL_API bool         pl_window_set_raw_mouse_input (plWindow*, bool);

// full screen modes
PL_API bool pl_window_set_fullscreen(plWindow*, const plFullScreenDesc*);

// future callback system
PL_API void                  pl_window_set_callback(plWindow*, plWindowEventCallback, void* userData);
PL_API plWindowEventCallback pl_window_get_callback(plWindow*);

#ifdef __cplusplus
}
#endif

#endif // PL_UNITY_EXT_H