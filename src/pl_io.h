/*
   pl_io.h, v0.1 (WIP)
   * no dependencies
   * simple
   Do this:
        #define PL_IO_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_IO_IMPLEMENTATION
   #include "pl_io.h"
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] global context
// [SECTION] public api
// [SECTION] structs
// [SECTION] enums
// [SECTION] implementation
*/

#ifndef PL_IO_H
#define PL_IO_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct name ##_t name
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
PL_DECLARE_STRUCT(plIOContext);

// enums
typedef int plKey;
typedef int plMouseButton;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

extern plIOContext* gTPIOContext;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
void         pl_initialize_io_context(plIOContext* pCtx);
void         pl_cleanup_io_context   (void);
void         pl_set_io_context       (plIOContext* pCtx);
plIOContext* pl_get_io_context       (void);

// input functions
void         pl_add_key_event         (plKey tKey, bool bDown);
void         pl_add_mouse_pos_event   (float fX, float fY);
void         pl_add_mouse_button_event(int iButton, bool bDown);
void         pl_add_mouse_wheel_event (float fX, float fY);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plIOContext_t
{
    float fDeltaTime;
} plIOContext;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plMouseButton_
{
    PL_MOUSE_BUTTON_LEFT   = 0,
    PL_MOUSE_BUTTON_RIGHT  = 1,
    PL_MOUSE_BUTTON_MIDDLE = 2,
    PL_MOUSE_BUTTON_COUNT  = 5    
};

enum plKey_
{
    PL_KEY_NONE = 0,
    PL_KEY_TAB,
    PL_KEY_LEFT_ARROW,
    PL_KEY_RIGHT_ARROW,
    PL_KEY_UP_ARROW,
    PL_KEY_DOWN_ARROW,
    PL_KEY_PAGE_UP,
    PL_KEY_PAGE_DOWN,
    PL_KEY_HOME,
    PL_KEY_END,
    PL_KEY_INSERT,
    PL_KEY_DELETE,
    PL_KEY_BACKSPACE,
    PL_KEY_SPACE,
    PL_KEY_ENTER,
    PL_KEY_ESCAPE,
    PL_KEY_LEFT_CTRL,
    PL_KEY_LEFT_SHIFT,
    PL_KEY_LEFT_ALT,
    PL_KEY_LEFT_SUPER,
    PL_KEY_RIGHT_CTRL,
    PL_KEY_RIGHT_SHIFT,
    PL_KEY_RIGHT_ALT,
    PL_KEY_RIGHT_SUPER,
    PL_KEY_MENU,
    PL_KEY_0,
    PL_KEY_1,
    PL_KEY_2,
    PL_KEY_3,
    PL_KEY_4,
    PL_KEY_5,
    PL_KEY_6,
    PL_KEY_7,
    PL_KEY_8,
    PL_KEY_9,
    PL_KEY_A,
    PL_KEY_B,
    PL_KEY_C,
    PL_KEY_D,
    PL_KEY_E,
    PL_KEY_F,
    PL_KEY_G,
    PL_KEY_H,
    PL_KEY_I,
    PL_KEY_J,
    PL_KEY_K,
    PL_KEY_L,
    PL_KEY_M,
    PL_KEY_N,
    PL_KEY_O,
    PL_KEY_P,
    PL_KEY_Q,
    PL_KEY_R,
    PL_KEY_S,
    PL_KEY_T,
    PL_KEY_U,
    PL_KEY_V,
    PL_KEY_W,
    PL_KEY_X,
    PL_KEY_Y,
    PL_KEY_Z,
    PL_KEY_F1,
    PL_KEY_F2,
    PL_KEY_F3,
    PL_KEY_F4,
    PL_KEY_F5,
    PL_KEY_F6,
    PL_KEY_F7,
    PL_KEY_F8,
    PL_KEY_F9,
    PL_KEY_F10,
    PL_KEY_F11,
    PL_KEY_F12,
    PL_KEY_F13,
    PL_KEY_F14,
    PL_KEY_F15,
    PL_KEY_F16,
    PL_KEY_F17,
    PL_KEY_F18,
    PL_KEY_F19,
    PL_KEY_F20,
    PL_KEY_F21,
    PL_KEY_F22,
    PL_KEY_F23,
    PL_KEY_F24,
    PL_KEY_APOSTROPHE,        // '
    PL_KEY_COMMA,             // ,
    PL_KEY_MINUS,             // -
    PL_KEY_PERIOD,            // .
    PL_KEY_SLASH,             // /
    PL_KEY_SEMICOLON,         // ;
    PL_KEY_EQUAL,             // =
    PL_KEY_LEFT_BRACKET,       // [
    PL_KEY_BACKSLASH,         // \ (this text inhibit multiline comment caused by backslash)
    PL_KEY_RIGHT_BRACKET,      // ]
    PL_KEY_GRAVE_ACCENT,       // `
    PL_KEY_CAPS_LOCK,
    PL_KEY_SCROLL_LOCK,
    PL_KEY_NUM_LOCK,
    PL_KEY_PRINT_SCREEN,
    PL_KEY_PAUSE,
    PL_KEY_KEYPAD_0,
    PL_KEY_KEYPAD_1,
    PL_KEY_KEYPAD_2,
    PL_KEY_KEYPAD_3,
    PL_KEY_KEYPAD_4,
    PL_KEY_KEYPAD_5,
    PL_KEY_KEYPAD_6,
    PL_KEY_KEYPAD_7,
    PL_KEY_KEYPAD_8,
    PL_KEY_KEYPAD_9,
    PL_KEY_KEYPAD_DECIMAL,
    PL_KEY_KEYPAD_DIVIDE,
    PL_KEY_KEYPAD_MULTIPLY,
    PL_KEY_KEYPAD_SUBTRACT,
    PL_KEY_KEYPAD_ADD,
    PL_KEY_KEYPAD_ENTER,
    PL_KEY_KEYPAD_EQUAL,
    PL_KEY_MOD_CTRL,
    PL_KEY_MOD_SHIFT,
    PL_KEY_MOD_ALT,
    PL_KEY_MOD_SUPER,
    PL_KEY_COUNT // No valid mvKey is ever greater than this value
};

#endif // PL_IO_H

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

#ifdef PL_IO_IMPLEMENTATION

#include <string.h> // memset

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert(x)
#endif

plIOContext* gTPIOContext = NULL;

void
pl_initialize_io_context(plIOContext* pCtx)
{
    memset(pCtx, 0, sizeof(plIOContext));
    gTPIOContext = pCtx;
}

void
pl_cleanup_io_context(void)
{

}

void
pl_set_io_context(plIOContext* pCtx)
{
    gTPIOContext = pCtx;
}

plIOContext*
pl_get_io_context(void)
{
    return gTPIOContext;
}

void
pl_add_key_event(plKey tKey, bool bDown)
{

}

void
pl_add_mouse_pos_event(float fX, float fY)
{

}

void
pl_add_mouse_button_event(int iButton, bool bDown)
{

}

void
pl_add_mouse_wheel_event(float fX, float fY)
{

}

#endif // PL_IO_IMPLEMENTATION