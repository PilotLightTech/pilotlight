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

#define plConsoleI_version (plVersion){0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConsoleSettings plConsoleSettings;

// enums
typedef int plConsoleFlags;  // -> enum _plConsoleFlags

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

    // add options
    bool (*add_toggle_option)(const char* name, bool*, const char* description);
} plConsoleI;

//-----------------------------------------------------------------------------
// [SECTION] enums & flags
//-----------------------------------------------------------------------------

enum _plConsoleFlags
{
    PL_CONSOLE_FLAGS_NONE = 0
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plConsoleSettings
{
    plConsoleFlags tFlags;
} plConsoleSettings;

#endif // PL_CONSOLE_EXT_H