/*
   pl_io.h
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

// library version
#define PL_IO_VERSION    "0.3.0"
#define PL_IO_VERSION_NUM 00300

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api structs
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
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_IO "IO API"
typedef struct _plIOApiI plIOApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plIOContext  plIOContext;
typedef struct _plInputEvent plInputEvent;
typedef struct _plKeyData    plKeyData;
typedef union  _plVec2       plVec2;

// enums
typedef int plKey;
typedef int plMouseButton;
typedef int plCursor;
typedef int plInputEventType;
typedef int plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plIOApiI* pl_load_io_api(void);
void      pl_unload_io_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plIOApiI
{
    // context
    plIOContext* (*get_context)(void);
    void         (*new_frame)  (void);
    void         (*end_frame)  (void);

    // keyboard
    bool         (*is_key_down)           (plKey tKey);
    bool         (*is_key_pressed)        (plKey tKey, bool bRepeat);
    bool         (*is_key_released)       (plKey tKey);
    int          (*get_key_pressed_amount)(plKey tKey, float fRepeatDelay, float fRate);

    // mouse
    bool         (*is_mouse_down)          (plMouseButton tButton);
    bool         (*is_mouse_clicked)       (plMouseButton tButton, bool bRepeat);
    bool         (*is_mouse_released)      (plMouseButton tButton);
    bool         (*is_mouse_double_clicked)(plMouseButton tButton);
    bool         (*is_mouse_dragging)      (plMouseButton tButton, float fThreshold);
    bool         (*is_mouse_hovering_rect) (plVec2 minVec, plVec2 maxVec);
    void         (*reset_mouse_drag_delta) (plMouseButton tButton);
    plVec2       (*get_mouse_drag_delta)   (plMouseButton tButton, float fThreshold);
    plVec2       (*get_mouse_pos)          (void);
    float        (*get_mouse_wheel)        (void);
    bool         (*is_mouse_pos_valid)     (plVec2 tPos);
    void         (*set_mouse_cursor)       (plCursor tCursor);

    // input functions
    plKeyData*   (*get_key_data)          (plKey tKey);
    void         (*add_key_event)         (plKey tKey, bool bDown);
    void         (*add_mouse_pos_event)   (float fX, float fY);
    void         (*add_mouse_button_event)(int iButton, bool bDown);
    void         (*add_mouse_wheel_event) (float fX, float fY);
} plIOApiI;

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

#ifndef PL_MATH_VEC2_DEFINED
#define PL_MATH_VEC2_DEFINED
typedef union _plVec2
{
    struct { float x, y; };
    struct { float r, g; };
    struct { float u, v; };
    float d[2];
} plVec2;
#endif

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
        
    };

} plInputEvent;

typedef struct _plIOContext
{
    double   dTime;
    float    fDeltaTime;
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

} plIOContext;

#endif // PL_IO_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] context
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifdef PL_IO_IMPLEMENTATION

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#ifndef PL_IO_ALLOC
    #include <stdlib.h>
    #define PL_IO_ALLOC(x) malloc((x))
    #define PL_IO_FREE(x)  free((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>   // floorf
#include <string.h> // memset
#include <float.h>  // FLT_MAX

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef enum
{
    PL_INPUT_EVENT_TYPE_NONE = 0,
    PL_INPUT_EVENT_TYPE_MOUSE_POS,
    PL_INPUT_EVENT_TYPE_MOUSE_WHEEL,
    PL_INPUT_EVENT_TYPE_MOUSE_BUTTON,
    PL_INPUT_EVENT_TYPE_KEY,
    
    PL_INPUT_EVENT_TYPE_COUNT
} _plInputEventType;

typedef enum
{
    PL_INPUT_EVENT_SOURCE_NONE = 0,
    PL_INPUT_EVENT_SOURCE_MOUSE,
    PL_INPUT_EVENT_SOURCE_KEYBOARD,
    
    PL_INPUT_EVENT_SOURCE_COUNT
} _plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plIOContext* gptIOContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api 
//-----------------------------------------------------------------------------

#define PL_IO_VEC2_LENGTH_SQR(vec) (((vec).x * (vec).x) + ((vec).y * (vec).y))

#ifdef __cplusplus
    #define PL_IO_VEC2_SUBTRACT(v1, v2) { (v1).x - (v2).x, (v1).y - (v2).y }
    #define PL_IO_VEC2(v1, v2) {(v1), (v2)}
#else
    #define PL_IO_VEC2_SUBTRACT(v1, v2) (plVec2){ (v1).x - (v2).x, (v1).y - (v2).y}
    #define PL_IO_VEC2(v1, v2) (plVec2){(v1), (v2)}
#endif
#define PL_IO_MAX(x, y) (x) > (y) ? (x) : (y)

static void          pl__update_events(void);
static void          pl__update_mouse_inputs(void);
static void          pl__update_keyboard_inputs(void);
static int           pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate);
static plInputEvent* pl__get_last_event(plInputEventType tType, int iButtonOrKey);
static plInputEvent* pl__get_event(void);

// context
static plIOContext* pl_get_io_context       (void);
static void         pl_new_io_frame         (void);
static void         pl_end_io_frame         (void);

// keyboard
static bool         pl_is_key_down           (plKey tKey);
static bool         pl_is_key_pressed        (plKey tKey, bool bRepeat);
static bool         pl_is_key_released       (plKey tKey);
static int          pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate);

// mouse
static bool         pl_is_mouse_down          (plMouseButton tButton);
static bool         pl_is_mouse_clicked       (plMouseButton tButton, bool bRepeat);
static bool         pl_is_mouse_released      (plMouseButton tButton);
static bool         pl_is_mouse_double_clicked(plMouseButton tButton);
static bool         pl_is_mouse_dragging      (plMouseButton tButton, float fThreshold);
static bool         pl_is_mouse_hovering_rect (plVec2 minVec, plVec2 maxVec);
static void         pl_reset_mouse_drag_delta (plMouseButton tButton);
static plVec2       pl_get_mouse_drag_delta   (plMouseButton tButton, float fThreshold);
static plVec2       pl_get_mouse_pos          (void);
static float        pl_get_mouse_wheel        (void);
static bool         pl_is_mouse_pos_valid     (plVec2 tPos);
static void         pl_set_mouse_cursor       (plCursor tCursor);

// input functions
static plKeyData*   pl_get_key_data          (plKey tKey);
static void         pl_add_key_event         (plKey tKey, bool bDown);
static void         pl_add_mouse_pos_event   (float fX, float fY);
static void         pl_add_mouse_button_event(int iButton, bool bDown);
static void         pl_add_mouse_wheel_event (float fX, float fY);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plIOApiI*
pl_load_io_api(void)
{
    static plIOApiI tIoI = {
        .get_context             = pl_get_io_context,
        .new_frame               = pl_new_io_frame,
        .end_frame               = pl_end_io_frame,
        .is_key_down             = pl_is_key_down,
        .is_key_pressed          = pl_is_key_pressed,
        .is_key_released         = pl_is_key_released,
        .get_key_pressed_amount  = pl_get_key_pressed_amount,
        .is_mouse_down           = pl_is_mouse_down,
        .is_mouse_clicked        = pl_is_mouse_clicked,
        .is_mouse_released       = pl_is_mouse_released,
        .is_mouse_double_clicked = pl_is_mouse_double_clicked,
        .is_mouse_dragging       = pl_is_mouse_dragging,
        .is_mouse_hovering_rect  = pl_is_mouse_hovering_rect,
        .reset_mouse_drag_delta  = pl_reset_mouse_drag_delta,
        .get_mouse_drag_delta    = pl_get_mouse_drag_delta,
        .get_mouse_pos           = pl_get_mouse_pos,
        .get_mouse_wheel         = pl_get_mouse_wheel,
        .is_mouse_pos_valid      = pl_is_mouse_pos_valid,
        .set_mouse_cursor        = pl_set_mouse_cursor,
        .get_key_data            = pl_get_key_data,
        .add_key_event           = pl_add_key_event,
        .add_mouse_pos_event     = pl_add_mouse_pos_event,
        .add_mouse_button_event  = pl_add_mouse_button_event,
        .add_mouse_wheel_event   = pl_add_mouse_wheel_event
    };

    static plIOContext tIOContext;
    memset(&tIOContext, 0, sizeof(plIOContext));

    gptIOContext = &tIOContext;
    gptIOContext->fMouseDoubleClickTime    = 0.3f;
    gptIOContext->fMouseDoubleClickMaxDist = 6.0f;
    gptIOContext->fMouseDragThreshold      = 6.0f;
    gptIOContext->fKeyRepeatDelay          = 0.275f;
    gptIOContext->fKeyRepeatRate           = 0.050f;
    
    gptIOContext->_sbtInputEvents = gptIOContext->_atInputEvents;
    gptIOContext->_uInputEventCapacity = 64;
    gptIOContext->afMainFramebufferScale[0] = 1.0f;
    gptIOContext->afMainFramebufferScale[1] = 1.0f;

    return &tIoI;
}

void
pl_unload_io_api(void)
{
    if(gptIOContext->_bOverflowInUse)
        PL_IO_FREE(gptIOContext->_sbtInputEvents);
    gptIOContext = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plIOContext*
pl_get_io_context(void)
{
    return gptIOContext;
}

static void
pl_new_io_frame(void)
{
    plIOContext* ptIO = gptIOContext;

    ptIO->dTime += (double)ptIO->fDeltaTime;
    ptIO->ulFrameCount++;
    ptIO->bViewportSizeChanged = false;

    pl__update_events();
    pl__update_keyboard_inputs();
    pl__update_mouse_inputs();
}

static void
pl_end_io_frame(void)
{
    plIOContext* ptIO = gptIOContext;
    ptIO->_fMouseWheel = 0.0f;
    ptIO->_fMouseWheelH = 0.0f;
}

static plKeyData*
pl_get_key_data(plKey tKey)
{
    PL_ASSERT(tKey > PL_KEY_NONE && tKey < PL_KEY_COUNT && "Key not valid");
    return &gptIOContext->_tKeyData[tKey];
}

static void
pl_add_key_event(plKey tKey, bool bDown)
{
    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_KEY, (int)tKey);
    if(ptLastEvent && ptLastEvent->bKeyDown == bDown)
        return;

    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType    = PL_INPUT_EVENT_TYPE_KEY;
    ptEvent->tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD;
    ptEvent->tKey     = tKey;
    ptEvent->bKeyDown = bDown;
}

static void
pl_add_mouse_pos_event(float fX, float fY)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_POS, -1);
    if(ptLastEvent && ptLastEvent->fPosX == fX && ptLastEvent->fPosY == fY)
        return;

    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType   = PL_INPUT_EVENT_TYPE_MOUSE_POS;
    ptEvent->tSource = PL_INPUT_EVENT_SOURCE_MOUSE;
    ptEvent->fPosX   = fX;
    ptEvent->fPosY   = fY;
}

static void
pl_add_mouse_button_event(int iButton, bool bDown)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_BUTTON, iButton);
    if(ptLastEvent && ptLastEvent->bMouseDown == bDown)
        return;

    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType      = PL_INPUT_EVENT_TYPE_MOUSE_BUTTON;
    ptEvent->tSource    = PL_INPUT_EVENT_SOURCE_MOUSE;
    ptEvent->iButton    = iButton;
    ptEvent->bMouseDown = bDown;
}

static void
pl_add_mouse_wheel_event(float fX, float fY)
{
    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType   = PL_INPUT_EVENT_TYPE_MOUSE_WHEEL;
    ptEvent->tSource = PL_INPUT_EVENT_SOURCE_MOUSE;
    ptEvent->fWheelX = fX;
    ptEvent->fWheelY = fY;
}

static bool
pl_is_key_down(plKey tKey)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    return ptData->bDown;
}

static bool
pl_is_key_pressed(plKey tKey, bool bRepeat)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return false;
    const float fT = ptData->fDownDuration;
    if (fT < 0.0f)
        return false;

    bool bPressed = (fT == 0.0f);
    if (!bPressed && bRepeat)
    {
        const float fRepeatDelay = gptIOContext->fKeyRepeatDelay;
        const float fRepeatRate = gptIOContext->fKeyRepeatRate;
        bPressed = (fT > fRepeatDelay) && pl_get_key_pressed_amount(tKey, fRepeatDelay, fRepeatRate) > 0;
    }

    if (!bPressed)
        return false;
    return true;
}

static bool
pl_is_key_released(plKey tKey)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (ptData->fDownDurationPrev < 0.0f || ptData->bDown)
        return false;
    return true;
}

static int
pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate)
{
    plIOContext* ptIO = gptIOContext;
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return 0;
    const float fT = ptData->fDownDuration;
    return pl__calc_typematic_repeat_amount(fT - ptIO->fDeltaTime, fT, fRepeatDelay, fRate);
}

static bool
pl_is_mouse_down(plMouseButton tButton)
{
    return gptIOContext->_abMouseDown[tButton];
}

static bool
pl_is_mouse_clicked(plMouseButton tButton, bool bRepeat)
{
    plIOContext* ptIO = gptIOContext;
    if(!ptIO->_abMouseDown[tButton])
        return false;
    const float fT = ptIO->_afMouseDownDuration[tButton];
    if(fT == 0.0f)
        return true;
    if(bRepeat && fT > ptIO->fKeyRepeatDelay)
        return pl__calc_typematic_repeat_amount(fT - ptIO->fDeltaTime, fT, ptIO->fKeyRepeatDelay, ptIO->fKeyRepeatRate) > 0;
    return false;
}

static bool
pl_is_mouse_released(plMouseButton tButton)
{
    return gptIOContext->_abMouseReleased[tButton];
}

static bool
pl_is_mouse_double_clicked(plMouseButton tButton)
{
    return gptIOContext->_auMouseClickedCount[tButton] == 2;
}

static bool
pl_is_mouse_dragging(plMouseButton tButton, float fThreshold)
{
    plIOContext* ptIO = gptIOContext;
    if(!ptIO->_abMouseDown[tButton])
        return false;
    if(fThreshold < 0.0f)
        fThreshold = ptIO->fMouseDragThreshold;
    return ptIO->_afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold;
}

static bool
pl_is_mouse_hovering_rect(plVec2 minVec, plVec2 maxVec)
{
    const plVec2 tMousePos = gptIOContext->_tMousePos;
    return ( tMousePos.x >= minVec.x && tMousePos.y >= minVec.y && tMousePos.x <= maxVec.x && tMousePos.y <= maxVec.y);
}

static void
pl_reset_mouse_drag_delta(plMouseButton tButton)
{
    gptIOContext->_atMouseClickedPos[tButton] = gptIOContext->_tMousePos;
}

static plVec2
pl_get_mouse_drag_delta(plMouseButton tButton, float fThreshold)
{
    plIOContext* ptIO = gptIOContext;
    if(fThreshold < 0.0f)
        fThreshold = ptIO->fMouseDragThreshold;
    if(ptIO->_abMouseDown[tButton] || ptIO->_abMouseReleased[tButton])
    {
        if(ptIO->_afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold)
        {
            if(pl_is_mouse_pos_valid(ptIO->_tMousePos) && pl_is_mouse_pos_valid(ptIO->_atMouseClickedPos[tButton]))
                return PL_IO_VEC2_SUBTRACT(ptIO->_tLastValidMousePos, ptIO->_atMouseClickedPos[tButton]);
        }
    }
    
    return PL_IO_VEC2(0.0f, 0.0f);
}

static plVec2
pl_get_mouse_pos(void)
{
    return gptIOContext->_tMousePos;
}

static float
pl_get_mouse_wheel(void)
{
    return gptIOContext->_fMouseWheel;
}

static bool
pl_is_mouse_pos_valid(plVec2 tPos)
{
    return tPos.x >= -FLT_MAX && tPos.y >= -FLT_MAX;
}

static void
pl_set_mouse_cursor(plCursor tCursor)
{
    gptIOContext->tNextCursor = tCursor;
    gptIOContext->bCursorChanged = true;
}

static void
pl__update_events(void)
{
    plIOContext* ptIO = gptIOContext;

    for(uint32_t i = 0; i < ptIO->_uInputEventSize; i++)
    {
        plInputEvent* ptEvent = &ptIO->_sbtInputEvents[i];

        switch(ptEvent->tType)
        {
            case PL_INPUT_EVENT_TYPE_MOUSE_POS:
            {

                if(ptEvent->fPosX != -FLT_MAX && ptEvent->fPosY != -FLT_MAX)
                {
                    ptIO->_tMousePos.x = ptEvent->fPosX;
                    ptIO->_tMousePos.y = ptEvent->fPosY;
                }
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_WHEEL:
            {
                ptIO->_fMouseWheelH += ptEvent->fWheelX;
                ptIO->_fMouseWheel += ptEvent->fWheelY;
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_BUTTON:
            {
                PL_ASSERT(ptEvent->iButton >= 0 && ptEvent->iButton < PL_MOUSE_BUTTON_COUNT);
                ptIO->_abMouseDown[ptEvent->iButton] = ptEvent->bMouseDown;
                break;
            }

            case PL_INPUT_EVENT_TYPE_KEY:
            {
                plKey tKey = ptEvent->tKey;
                PL_ASSERT(tKey != PL_KEY_NONE);
                plKeyData* ptKeyData = pl_get_key_data(tKey);
                ptKeyData->bDown = ptEvent->bKeyDown;
                break;
            }

            default:
            {
                PL_ASSERT(false && "unknown input event type");
                break;
            }
        }
    }

    ptIO->_uInputEventSize = 0;
}

static void
pl__update_keyboard_inputs(void)
{
   plIOContext* ptIO = gptIOContext; 

    // Update keys
    for (uint32_t i = 0; i < PL_KEY_COUNT; i++)
    {
        plKeyData* ptKeyData = &ptIO->_tKeyData[i];
        ptKeyData->fDownDurationPrev = ptKeyData->fDownDuration;
        ptKeyData->fDownDuration = ptKeyData->bDown ? (ptKeyData->fDownDuration < 0.0f ? 0.0f : ptKeyData->fDownDuration + ptIO->fDeltaTime) : -1.0f;
    }
}

static void
pl__update_mouse_inputs(void)
{
    plIOContext* ptIO = gptIOContext;

    if(pl_is_mouse_pos_valid(ptIO->_tMousePos))
    {
        ptIO->_tMousePos.x = floorf(ptIO->_tMousePos.x);
        ptIO->_tMousePos.y = floorf(ptIO->_tMousePos.y);
        ptIO->_tLastValidMousePos = ptIO->_tMousePos;
    }

    if(pl_is_mouse_pos_valid(ptIO->_tMousePos) && pl_is_mouse_pos_valid(ptIO->_tMousePosPrev))
        ptIO->_tMouseDelta = PL_IO_VEC2_SUBTRACT(ptIO->_tMousePos, ptIO->_tMousePosPrev);
    else
    {
        ptIO->_tMouseDelta.x = 0.0f;
        ptIO->_tMouseDelta.y = 0.0f;
    }

    ptIO->_tMousePosPrev = ptIO->_tMousePos;

    for(uint32_t i = 0; i < PL_MOUSE_BUTTON_COUNT; i++)
    {
        ptIO->_abMouseClicked[i] = ptIO->_abMouseDown[i] && ptIO->_afMouseDownDuration[i] < 0.0f;
        ptIO->_auMouseClickedCount[i] = 0;
        ptIO->_abMouseReleased[i] = !ptIO->_abMouseDown[i] && ptIO->_afMouseDownDuration[i] >= 0.0f;
        ptIO->_afMouseDownDurationPrev[i] = ptIO->_afMouseDownDuration[i];
        ptIO->_afMouseDownDuration[i] = ptIO->_abMouseDown[i] ? (ptIO->_afMouseDownDuration[i] < 0.0f ? 0.0f : ptIO->_afMouseDownDuration[i] + ptIO->fDeltaTime) : -1.0f;

        if(ptIO->_abMouseClicked[i])
        {

            bool bIsRepeatedClick = false;
            if((float)(ptIO->dTime - ptIO->_adMouseClickedTime[i]) < ptIO->fMouseDoubleClickTime)
            {
                plVec2 tDeltaFromClickPos = PL_IO_VEC2(0.0f, 0.0f);
                if(pl_is_mouse_pos_valid(ptIO->_tMousePos))
                    tDeltaFromClickPos = PL_IO_VEC2_SUBTRACT(ptIO->_tMousePos, ptIO->_atMouseClickedPos[i]);

                if(PL_IO_VEC2_LENGTH_SQR(tDeltaFromClickPos) < ptIO->fMouseDoubleClickMaxDist * ptIO->fMouseDoubleClickMaxDist)
                    bIsRepeatedClick = true;
            }

            if(bIsRepeatedClick)
                ptIO->_auMouseClickedLastCount[i]++;
            else
                ptIO->_auMouseClickedLastCount[i] = 1;

            ptIO->_adMouseClickedTime[i] = ptIO->dTime;
            ptIO->_atMouseClickedPos[i] = ptIO->_tMousePos;
            ptIO->_afMouseDragMaxDistSqr[i] = 0.0f;
            ptIO->_auMouseClickedCount[i] = ptIO->_auMouseClickedLastCount[i];
        }
        else if(ptIO->_abMouseDown[i])
        {
            const plVec2 tClickPos = PL_IO_VEC2_SUBTRACT(ptIO->_tLastValidMousePos, ptIO->_atMouseClickedPos[i]);
            float fDeltaSqrClickPos = PL_IO_VEC2_LENGTH_SQR(tClickPos);
            ptIO->_afMouseDragMaxDistSqr[i] = PL_IO_MAX(fDeltaSqrClickPos, ptIO->_afMouseDragMaxDistSqr[i]);
        }


    }
}

static int
pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate)
{
    if(fT1 == 0.0f)
        return 1;
    if(fT0 >= fT1)
        return 0;
    if(fRepeatRate <= 0.0f)
        return (fT0 < fRepeatDelay) && (fT1 >= fRepeatDelay);
    
    const int iCountT0 = (fT0 < fRepeatDelay) ? -1 : (int)((fT0 - fRepeatDelay) / fRepeatRate);
    const int iCountT1 = (fT1 < fRepeatDelay) ? -1 : (int)((fT1 - fRepeatDelay) / fRepeatRate);
    const int iCount = iCountT1 - iCountT0;
    return iCount;
}

static plInputEvent*
pl__get_last_event(plInputEventType tType, int iButtonOrKey)
{
    plIOContext* ptIO = gptIOContext;
    for(uint32_t i = 0; i < ptIO->_uInputEventSize; i++)
    {
        plInputEvent* ptEvent = &ptIO->_sbtInputEvents[ptIO->_uInputEventSize - i - 1];
        if(ptEvent->tType != tType)
            continue;
        if(tType == PL_INPUT_EVENT_TYPE_KEY && (int)ptEvent->tKey != iButtonOrKey)
            continue;
        if(tType == PL_INPUT_EVENT_TYPE_MOUSE_BUTTON && ptEvent->iButton != iButtonOrKey)
            continue;
        return ptEvent;
    }
    return NULL;
}

static plInputEvent*
pl__get_event(void)
{
    plInputEvent* ptEvent = NULL;

    // check if new overflow
    if(!gptIOContext->_bOverflowInUse && gptIOContext->_uInputEventSize == gptIOContext->_uInputEventCapacity)
    {
        gptIOContext->_sbtInputEvents = (plInputEvent*)PL_IO_ALLOC(sizeof(plInputEvent) * 256);
        memset(gptIOContext->_sbtInputEvents, 0, sizeof(plInputEvent) * 256);
        gptIOContext->_uInputEventOverflowCapacity = 256;

        // copy stack samples
        memcpy(gptIOContext->_sbtInputEvents, gptIOContext->_atInputEvents, sizeof(plInputEvent) * gptIOContext->_uInputEventCapacity);
        gptIOContext->_bOverflowInUse = true;
    }
    // check if overflow reallocation is needed
    else if(gptIOContext->_bOverflowInUse && gptIOContext->_uInputEventSize == gptIOContext->_uInputEventOverflowCapacity)
    {
        plInputEvent* sbtOldInputEvents = gptIOContext->_sbtInputEvents;
        gptIOContext->_sbtInputEvents = (plInputEvent*)PL_IO_ALLOC(sizeof(plInputEvent) * gptIOContext->_uInputEventOverflowCapacity * 2);
        memset(gptIOContext->_sbtInputEvents, 0, sizeof(plInputEvent) * gptIOContext->_uInputEventOverflowCapacity * 2);
        
        // copy old values
        memcpy(gptIOContext->_sbtInputEvents, sbtOldInputEvents, sizeof(plInputEvent) * gptIOContext->_uInputEventOverflowCapacity);
        gptIOContext->_uInputEventOverflowCapacity *= 2;

        PL_IO_FREE(sbtOldInputEvents);
    }

    ptEvent = &gptIOContext->_sbtInputEvents[gptIOContext->_uInputEventSize];
    gptIOContext->_uInputEventSize++;

    return ptEvent;
}

#endif // PL_IO_IMPLEMENTATION