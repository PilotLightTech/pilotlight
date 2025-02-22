/*
   pl_console_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] enums & flags
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CONSOLE_EXT_H
#define PL_CONSOLE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plConsoleI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plConsoleI
{
    // setup/shutdown
    void (*initialize)(plConsoleSettings);
    void (*cleanup)   (void);

    void (*update)(void); // call every frame after beginning ui frame
    void (*open)  (void); // opens window
    void (*close) (void); // closes window programatically

    // add variables (returns false if name already used)
    bool (*add_toggle_variable)(const char* name,     bool*, const char* description, plConsoleVarFlags);
    bool (*add_bool_variable)  (const char* name,     bool*, const char* description, plConsoleVarFlags);
    bool (*add_int_variable)   (const char* name,      int*, const char* description, plConsoleVarFlags);
    bool (*add_uint_variable)  (const char* name, uint32_t*, const char* description, plConsoleVarFlags);
    bool (*add_float_variable) (const char* name,    float*, const char* description, plConsoleVarFlags);
    bool (*add_string_variable)(const char* name,     char*, size_t bufferSize, const char* description, plConsoleVarFlags);

    // add variables with callbacks (returns false if name already used)
    bool (*add_toggle_variable_ex)(const char* name,     bool*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool (*add_bool_variable_ex)  (const char* name,     bool*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool (*add_int_variable_ex)   (const char* name,      int*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool (*add_uint_variable_ex)  (const char* name, uint32_t*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool (*add_float_variable_ex) (const char* name,    float*, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool (*add_string_variable_ex)(const char* name,     char*, size_t bufferSize, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData);
    bool (*add_command)           (const char* name, const char* description, plConsoleVarFlags, plConsoleCallback, void* userData); // just runs a command

    // misc
    void* (*get_variable)   (const char* name, plConsoleCallback* callbackOut, void** userDataOut); // mostly used for UI tools, make sure you cast to correct type
    bool  (*remove_variable)(const char* name); // returns true if variable was found
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

#endif // PL_CONSOLE_EXT_H