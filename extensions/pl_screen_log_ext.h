/*
   pl_screen_log_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SCREEN_LOG_EXT_H
#define PL_SCREEN_LOG_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plScreenLogI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdarg.h>  // var args
#include <stdint.h> // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plScreenLogSettings plScreenLogSettings;

// external
typedef struct _plFont       plFont;       // pl_draw_ext.h
typedef struct _plDrawList2D plDrawList2D; // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plScreenLogI
{
    // setup/shutdown
    void (*initialize)(plScreenLogSettings);
    void (*cleanup)   (void);

    // messages
    //   Displays on screen debug messages.
    //   If "timeToDisplay" is < 0, it will remain until cleared.
    //   Messages can be updated using the "key" parameter.
    //   - timeToDisplay  : time to display message
    //   - key            : key used to update previous message (default is 0 which is ignored)
    //   - color          : message color (default is white & if 0)
    //   - textScale      : text size multiplier (default is 1.0f)
    void (*add_message)   (double timeToDisplay, const char* message);
    void (*add_message_ex)(uint64_t key, double timeToDisplay, uint32_t color, float textScale, const char* format, ...);
    void (*add_message_va)(uint64_t key, double timeToDisplay, uint32_t color, float textScale, const char* format, va_list args);
    
    // clear all messages
    void (*clear)(void);
    
    // drawing
    plDrawList2D* (*get_drawlist)(float width, float height); // call once per frame
} plScreenLogI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plScreenLogSettings
{
    plFont* ptFont;
} plScreenLogSettings;

#endif // PL_SCREEN_LOG_EXT_H