/*
   pl_io.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] context name
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IO_H
#define PL_IO_H

//-----------------------------------------------------------------------------
// [SECTION] context name
//-----------------------------------------------------------------------------

#define PL_CONTEXT_IO_NAME "PL_CONTEXT_IO_NAME"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plIOContext  plIOContext;
typedef struct _plInputEvent plInputEvent;
typedef struct _plKeyData    plKeyData;
typedef union  _plVec2       plVec2;

// character types
typedef uint16_t plWChar;

// enums
typedef int plKey;
typedef int plMouseButton;
typedef int plCursor;
typedef int plInputEventType;
typedef int plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
void         pl_set_io_context       (plIOContext* ptCtx);
plIOContext* pl_get_io_context       (void);
void         pl_new_io_frame         (void);
void         pl_end_io_frame         (void);

// keyboard
bool         pl_is_key_down           (plKey tKey);
bool         pl_is_key_pressed        (plKey tKey, bool bRepeat);
bool         pl_is_key_released       (plKey tKey);
int          pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate);

// mouse
bool         pl_is_mouse_down          (plMouseButton tButton);
bool         pl_is_mouse_clicked       (plMouseButton tButton, bool bRepeat);
bool         pl_is_mouse_released      (plMouseButton tButton);
bool         pl_is_mouse_double_clicked(plMouseButton tButton);
bool         pl_is_mouse_dragging      (plMouseButton tButton, float fThreshold);
bool         pl_is_mouse_hovering_rect (plVec2 minVec, plVec2 maxVec);
void         pl_reset_mouse_drag_delta (plMouseButton tButton);
plVec2       pl_get_mouse_drag_delta   (plMouseButton tButton, float fThreshold);
plVec2       pl_get_mouse_pos          (void);
float        pl_get_mouse_wheel        (void);
bool         pl_is_mouse_pos_valid     (plVec2 tPos);
void         pl_set_mouse_cursor       (plCursor tCursor);

// input functions
plKeyData*   pl_get_key_data          (plKey tKey);
void         pl_add_key_event         (plKey tKey, bool bDown);
void         pl_add_text_event        (uint32_t uChar);
void         pl_add_text_event_utf16  (uint16_t uChar);
void         pl_add_text_events_utf8  (const char* pcText);
void         pl_add_mouse_pos_event   (float fX, float fY);
void         pl_add_mouse_button_event(int iButton, bool bDown);
void         pl_add_mouse_wheel_event (float fX, float fY);

void         pl_clear_input_characters(void);

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
    PL_KEY_0, PL_KEY_1, PL_KEY_2, PL_KEY_3, PL_KEY_4, PL_KEY_5, PL_KEY_6, PL_KEY_7, PL_KEY_8, PL_KEY_9,
    PL_KEY_A, PL_KEY_B, PL_KEY_C, PL_KEY_D, PL_KEY_E, PL_KEY_F, PL_KEY_G, PL_KEY_H, PL_KEY_I, PL_KEY_J,
    PL_KEY_K, PL_KEY_L, PL_KEY_M, PL_KEY_N, PL_KEY_O, PL_KEY_P, PL_KEY_Q, PL_KEY_R, PL_KEY_S, PL_KEY_T,
    PL_KEY_U, PL_KEY_V, PL_KEY_W, PL_KEY_X, PL_KEY_Y, PL_KEY_Z,
    PL_KEY_F1, PL_KEY_F2, PL_KEY_F3,PL_KEY_F4, PL_KEY_F5, PL_KEY_F6, PL_KEY_F7, PL_KEY_F8, PL_KEY_F9,
    PL_KEY_F10, PL_KEY_F11, PL_KEY_F12, PL_KEY_F13, PL_KEY_F14, PL_KEY_F15, PL_KEY_F16, PL_KEY_F17,
    PL_KEY_F18, PL_KEY_F19, PL_KEY_F20, PL_KEY_F21, PL_KEY_F22, PL_KEY_F23, PL_KEY_F24,
    PL_KEY_APOSTROPHE,    // '
    PL_KEY_COMMA,         // ,
    PL_KEY_MINUS,         // -
    PL_KEY_PERIOD,        // .
    PL_KEY_SLASH,         // /
    PL_KEY_SEMICOLON,     // ;
    PL_KEY_EQUAL,         // =
    PL_KEY_LEFT_BRACKET,  // [
    PL_KEY_BACKSLASH,     // \ (this text inhibit multiline comment caused by backslash)
    PL_KEY_RIGHT_BRACKET, // ]
    PL_KEY_GRAVE_ACCENT,  // `
    PL_KEY_CAPS_LOCK,
    PL_KEY_SCROLL_LOCK,
    PL_KEY_NUM_LOCK,
    PL_KEY_PRINT_SCREEN,
    PL_KEY_PAUSE,
    PL_KEY_KEYPAD_0, PL_KEY_KEYPAD_1, PL_KEY_KEYPAD_2, PL_KEY_KEYPAD_3, 
    PL_KEY_KEYPAD_4, PL_KEY_KEYPAD_5, PL_KEY_KEYPAD_6, PL_KEY_KEYPAD_7, 
    PL_KEY_KEYPAD_8, PL_KEY_KEYPAD_9,
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
    
    PL_KEY_COUNT // no valid plKey_ is ever greater than this value
};

enum plCursor_
{
    PL_MOUSE_CURSOR_NONE  = -1,
    PL_MOUSE_CURSOR_ARROW =  0,
    PL_MOUSE_CURSOR_TEXT_INPUT,
    PL_MOUSE_CURSOR_RESIZE_ALL,
    PL_MOUSE_CURSOR_RESIZE_NS,
    PL_MOUSE_CURSOR_RESIZE_EW,
    PL_MOUSE_CURSOR_RESIZE_NESW,
    PL_MOUSE_CURSOR_RESIZE_NWSE,
    PL_MOUSE_CURSOR_HAND,
    PL_MOUSE_CURSOR_NOT_ALLOWED,
    PL_MOUSE_CURSOR_COUNT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plKeyData
{
    bool  bDown;
    float fDownDuration;
    float fDownDurationPrev;
} plKeyData;

typedef struct _plInputEvent
{
    plInputEventType   tType;
    plInputEventSource tSource;

    union
    {
        struct // mouse pos event
        {
            float fPosX;
            float fPosY;
        };

        struct // mouse wheel event
        {
            float fWheelX;
            float fWheelY;
        };
        
        struct // mouse button event
        {
            int  iButton;
            bool bMouseDown;
        };

        struct // key event
        {
            plKey tKey;
            bool  bKeyDown;
        };

        struct // text event
        {
            uint32_t uChar;
        };
        
    };

} plInputEvent;

typedef struct _plIOContext
{
    double   dTime;
    float    fDeltaTime;
    float    fFrameRate; // rough estimate(rolling average of fDeltaTime over 120 frames)
    bool     bViewportSizeChanged;
    bool     bViewportMinimized;
    float    afMainViewportSize[2];
    float    afMainFramebufferScale[2];
    uint64_t ulFrameCount;

    // settings
    float  fMouseDragThreshold;      // default 6.0f
    float  fMouseDoubleClickTime;    // default 0.3f seconds
    float  fMouseDoubleClickMaxDist; // default 6.0f
    float  fKeyRepeatDelay;          // default 0.275f
    float  fKeyRepeatRate;           // default 0.050f
    void*  pUserData;
    void*  pBackendPlatformData;
    void*  pBackendRendererData;
    void*  pBackendData;

    // [INTERNAL]
    bool          _bOverflowInUse;
    plInputEvent  _atInputEvents[64];
    plInputEvent* _sbtInputEvents;
    uint32_t      _uInputEventSize;
    uint32_t      _uInputEventCapacity;
    uint32_t      _uInputEventOverflowCapacity;
    plWChar*      _sbInputQueueCharacters;
    plWChar       _tInputQueueSurrogate; 

    // main input state
    plVec2 _tMousePos;
    bool   _abMouseDown[5];
    int    _iMouseButtonsDown;
    float  _fMouseWheel;
    float  _fMouseWheelH;

    // mouse cursor
    plCursor tCurrentCursor;
    plCursor tNextCursor;
    bool     bCursorChanged;

    // other state
    plKeyData _tKeyData[PL_KEY_COUNT];
    plVec2    _tLastValidMousePos;
    plVec2    _tMouseDelta;
    plVec2    _tMousePosPrev;              // previous mouse position
    plVec2    _atMouseClickedPos[5];       // position when clicked
    double    _adMouseClickedTime[5];      // time of last click
    bool      _abMouseClicked[5];          // mouse went from !down to down
    uint32_t  _auMouseClickedCount[5];     // 
    uint32_t  _auMouseClickedLastCount[5]; // 
    bool      _abMouseReleased[5];         // mouse went from down to !down
    float     _afMouseDownDuration[5];     // duration mouse button has been down (0.0f == just clicked)
    float     _afMouseDownDurationPrev[5]; // previous duration of mouse button down
    float     _afMouseDragMaxDistSqr[5];   // squared max distance mouse traveled from clicked position

    // frame rate calcs
    float _afFrameRateSecPerFrame[120];
    int   _iFrameRateSecPerFrameIdx;
    int   _iFrameRateSecPerFrameCount;
    float _fFrameRateSecPerFrameAccum;


} plIOContext;

#endif // PL_IO_H