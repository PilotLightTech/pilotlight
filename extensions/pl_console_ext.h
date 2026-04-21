/*
   pl_console_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums & flags
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plUiI   (v1.x)
        * plDrawI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CONSOLE_EXT_H
#define PL_CONSOLE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plConsoleI_version {1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConsoleSettings plConsoleSettings;

// callbacks
typedef void (*plConsoleCallback)(const char* name, void *userdata);

// enums
typedef int plConsoleFlags;    // -> enum _plConsoleFlags
typedef int plConsoleVarFlags; // -> enum _plConsoleVarFlags

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_console_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_console_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_console_initialize(plConsoleSettings);
PL_API void pl_console_cleanup   (void);

PL_API void pl_console_update(void); // call every frame after beginning ui frame
PL_API void pl_console_open  (void); // opens window
PL_API void pl_console_close (void); // closes window programatically

// add variables (returns false if name already used)
PL_API bool pl_console_add_toggle_variable(const char* name,     bool*, const char* description, plConsoleVarFlags);
PL_API bool pl_console_add_bool_variable  (const char* name,     bool*, const char* description, plConsoleVarFlags);
PL_API bool pl_console_add_int_variable   (const char* name,      int*, const char* description, plConsoleVarFlags);
PL_API bool pl_console_add_uint_variable  (const char* name, uint32_t*, const char* description, plConsoleVarFlags);
PL_API bool pl_console_add_float_variable (const char* name,    float*, const char* description, plConsoleVarFlags);
PL_API bool pl_console_add_string_variable(const char* name,     char*, size_t bufferSize, const char* description, plConsoleVarFlags);

// add variables with callbacks (returns false if name already used)
PL_API bool pl_console_add_toggle_variable_ex(const char* name,     bool*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
PL_API bool pl_console_add_bool_variable_ex  (const char* name,     bool*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
PL_API bool pl_console_add_int_variable_ex   (const char* name,      int*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
PL_API bool pl_console_add_uint_variable_ex  (const char* name, uint32_t*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
PL_API bool pl_console_add_float_variable_ex (const char* name,    float*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
PL_API bool pl_console_add_string_variable_ex(const char* name,     char*, size_t bufferSize, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
PL_API bool pl_console_add_command           (const char* name, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData); // just runs a command

// misc
PL_API void* pl_console_get_variable   (const char* name, plConsoleCallback* callbackOut, void** userDataOut); // mostly used for UI tools, make sure you cast to correct type
PL_API bool  pl_console_remove_variable(const char* name); // returns true if variable was found

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plConsoleI
{
    void  (*initialize)            (plConsoleSettings);
    void  (*cleanup)               (void);
    void  (*update)                (void);
    void  (*open)                  (void);
    void  (*close)                 (void);
    bool  (*add_toggle_variable)   (const char* name,     bool*, const char* description, plConsoleVarFlags);
    bool  (*add_bool_variable)     (const char* name,     bool*, const char* description, plConsoleVarFlags);
    bool  (*add_int_variable)      (const char* name,      int*, const char* description, plConsoleVarFlags);
    bool  (*add_uint_variable)     (const char* name, uint32_t*, const char* description, plConsoleVarFlags);
    bool  (*add_float_variable)    (const char* name,    float*, const char* description, plConsoleVarFlags);
    bool  (*add_string_variable)   (const char* name,     char*, size_t bufferSize, const char* description, plConsoleVarFlags);
    bool  (*add_toggle_variable_ex)(const char* name,     bool*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool  (*add_bool_variable_ex)  (const char* name,     bool*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool  (*add_int_variable_ex)   (const char* name,      int*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool  (*add_uint_variable_ex)  (const char* name, uint32_t*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool  (*add_float_variable_ex) (const char* name,    float*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool  (*add_string_variable_ex)(const char* name,     char*, size_t bufferSize, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool  (*add_command)           (const char* name, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    void* (*get_variable)          (const char* name, plConsoleCallback* callbackOut, void** userDataOut);
    bool  (*remove_variable)       (const char* name);
} plConsoleI;

//-----------------------------------------------------------------------------
// [SECTION] enums & flags
//-----------------------------------------------------------------------------

enum _plConsoleFlags
{
    PL_CONSOLE_FLAGS_NONE      = 0,
    PL_CONSOLE_FLAGS_POPUP     = 1 << 0,
    PL_CONSOLE_FLAGS_MOVABLE   = 1 << 1, // doesn't work with popup currently
    PL_CONSOLE_FLAGS_RESIZABLE = 1 << 2, // doesn't work with popup currently
};

enum _plConsoleVarFlags
{
    PL_CONSOLE_VARIABLE_FLAGS_NONE               = 0,
    PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE      = 1 << 0, // closes console after changed
    PL_CONSOLE_VARIABLE_FLAGS_UPDATE_AFTER_ENTER = 1 << 1, // don't update value until enter key pressed
    PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY          = 1 << 2
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plConsoleSettings
{
    plConsoleFlags tFlags;
} plConsoleSettings;

#ifdef __cplusplus
}
#endif

#endif // PL_CONSOLE_EXT_H