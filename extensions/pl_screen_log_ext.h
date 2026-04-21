/*
   pl_screen_log_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plDrawI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SCREEN_LOG_EXT_H
#define PL_SCREEN_LOG_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plScreenLogI_version {2, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdarg.h>  // var args
#include <stdint.h> // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plScreenLogSettings plScreenLogSettings;

// enums
typedef int plScreenLogFlags; // -> enum _plScreenLogFlags // Flag:

// external
typedef struct _plFont       plFont;       // pl_draw_ext.h
typedef struct _plDrawList2D plDrawList2D; // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_screen_log_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_screen_log_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void             pl_screen_log_initialize(plScreenLogSettings);
PL_API void             pl_screen_log_cleanup   (void);

// messages
//   Displays on screen debug messages.
//   If "timeToDisplay" is < 0, it will remain until cleared.
//   Messages can be updated using the "key" parameter.
//   - timeToDisplay  : time to display message
//   - key            : key used to update previous message (default is 0 which is ignored)
//   - color          : message color (default is white & if 0)
//   - textScale      : text size multiplier (default is 1.0f)
PL_API void             pl_screen_log_add_message   (double timeToDisplay, const char* message);
PL_API void             pl_screen_log_add_message_ex(uint64_t key, double timeToDisplay, uint32_t color, float textScale, const char* format, ...);
PL_API void             pl_screen_log_add_message_va(uint64_t key, double timeToDisplay, uint32_t color, float textScale, const char* format, va_list args);

// clear all messages
PL_API void             pl_screen_log_clear(void);

// flags
PL_API void             pl_screen_log_set_flags(plScreenLogFlags);
PL_API plScreenLogFlags pl_screen_log_get_flags(void);

// drawing
PL_API plDrawList2D*    pl_screen_log_get_drawlist(float xPos, float yPos, float width, float height); // call once per frame

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plScreenLogI
{
    void             (*initialize)    (plScreenLogSettings);
    void             (*cleanup)       (void);
    void             (*add_message)   (double timeToDisplay, const char* message);
    void             (*add_message_ex)(uint64_t key, double timeToDisplay, uint32_t color, float textScale, const char* format, ...);
    void             (*add_message_va)(uint64_t key, double timeToDisplay, uint32_t color, float textScale, const char* format, va_list args);
    void             (*clear)         (void);
    void             (*set_flags)     (plScreenLogFlags);
    plScreenLogFlags (*get_flags)     (void);
    plDrawList2D*    (*get_drawlist)  (float xPos, float yPos, float width, float height); // call once per frame
} plScreenLogI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plScreenLogSettings
{
    plFont* ptFont;
} plScreenLogSettings;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plScreenLogFlags
{
    PL_SCREEN_LOG_FLAGS_NONE           = 0,
    PL_SCREEN_LOG_FLAGS_HIDE_MESSAGES  = 1 << 0,
};

#ifdef __cplusplus
}
#endif

#endif // PL_SCREEN_LOG_EXT_H