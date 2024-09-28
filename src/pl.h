/*
   pl.h
     - Pilot Light core APIs
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] api structs
// [SECTION] enums
// [SECTION] IO struct
// [SECTION] structs
// [SECTION] defines
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_H
#define PL_H

// framework version XYYZZ
#define PILOT_LIGHT_VERSION    "1.0.0"
#define PILOT_LIGHT_VERSION_NUM 10000

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryI plApiRegistryI;

#define PL_API_EXTENSION_REGISTRY "PL_API_EXTENSION_REGISTRY"
typedef struct _plExtensionRegistryI plExtensionRegistryI;

#define PL_API_MEMORY "PL_API_MEMORY"
typedef struct _plMemoryI plMemoryI;

#define PL_API_IO "PL_API_IO"
typedef struct _plIOI plIOI;

#define PL_API_DATA_REGISTRY "PL_API_DATA_REGISTRY"
typedef struct _plDataRegistryI plDataRegistryI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint32_t
#include <stddef.h>  // size_t
#include "pl_math.h" // plVec2

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plAllocationEntry plAllocationEntry;
typedef union  _plDataID          plDataID;
typedef struct _plDataObject      plDataObject; // opaque type
typedef struct _plIO              plIO;         // configuration & IO between app & pilotlight ui
typedef struct _plKeyData         plKeyData;    // individual key status (down, down duration, etc.)
typedef struct _plInputEvent      plInputEvent; // holds data for input events (opaque structure)

// enums
typedef int plKey;              // -> enum plKey_              // Enum: A key identifier (PL_KEY_XXX or PL_KEY_MOD_XXX value)
typedef int plMouseButton;      // -> enum plMouseButton_      // Enum: A mouse button identifier (PL_MOUSE_BUTTON_XXX)
typedef int plMouseCursor;      // -> enum plMouseCursor_      // Enum: Mouse cursor shape (PL_MOUSE_CURSOR_XXX)
typedef int plInputEventType;   // -> enum plInputEventType_   // Enum: An input event type (PL_INPUT_EVENT_TYPE_XXX)
typedef int plInputEventSource; // -> enum plInputEventSource_ // Enum: An input event source (PL_INPUT_EVENT_SOURCE_XXX)
typedef int plKeyChord;

// character types
typedef uint16_t plUiWChar;

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryI
{

    const void* (*add)   (const char* pcName, const void* pInterface);
    void        (*remove)(const void* pInterface);
    const void* (*first) (const char* pcName);
    const void* (*next)  (const void* pPrevInterface);
    
} plApiRegistryI;

typedef struct _plExtensionRegistryI
{

    bool (*load)  (const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable);
    bool (*unload)(const char* pcName); 
    
} plExtensionRegistryI;

typedef struct _plMemoryI
{

    void* (*realloc)(void*, size_t, const char* pcFile, int iLine);

    // stats
    size_t             (*get_memory_usage)(void);
    size_t             (*get_allocation_count)(void);
    size_t             (*get_free_count)(void);
    plAllocationEntry* (*get_allocations)(size_t* pszCount);
    
} plMemoryI;

typedef struct _plIOI
{

    void  (*new_frame)(void);
    plIO* (*get_io)(void);

    // keyboard
    bool (*is_key_down)           (plKey);
    bool (*is_key_pressed)        (plKey, bool bRepeat);
    bool (*is_key_released)       (plKey);
    int  (*get_key_pressed_amount)(plKey, float fRepeatDelay, float fRate);

    // mouse
    bool   (*is_mouse_down)          (plMouseButton);
    bool   (*is_mouse_clicked)       (plMouseButton, bool bRepeat);
    bool   (*is_mouse_released)      (plMouseButton);
    bool   (*is_mouse_double_clicked)(plMouseButton);
    bool   (*is_mouse_dragging)      (plMouseButton, float fThreshold);
    bool   (*is_mouse_hovering_rect) (plVec2 minVec, plVec2 maxVec);
    void   (*reset_mouse_drag_delta) (plMouseButton);
    plVec2 (*get_mouse_drag_delta)   (plMouseButton, float fThreshold);
    plVec2 (*get_mouse_pos)          (void);
    float  (*get_mouse_wheel)        (void);
    bool   (*is_mouse_pos_valid)     (plVec2);
    void   (*set_mouse_cursor)       (plMouseCursor);

    // input functions (used by backends)
    void (*add_key_event)         (plKey, bool bDown);
    void (*add_text_event)        (uint32_t uChar);
    void (*add_text_event_utf16)  (uint16_t uChar);
    void (*add_text_events_utf8)  (const char* pcText);
    void (*add_mouse_pos_event)   (float x, float y);
    void (*add_mouse_button_event)(int iButton, bool bDown);
    void (*add_mouse_wheel_event) (float fHorizontalDelta, float fVerticalDelta);
    void (*clear_input_characters)(void);

    // misc.
    int         (*get_version_number)(void);
    const char* (*get_version)       (void);

} plIOI;

typedef struct _plDataRegistryI
{

    // for convience, only use for infrequent operations (i.e. global extension data)
    void  (*set_data)(const char* pcName, void* pData);
    void* (*get_data)(const char* pcName);

    //~~~~~~~~~~~~~~~~~~~~~~~~~do not use below here yet~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // future type system, current default type property 0 is a string (name) and property 1 is buffer (pointer)
    // plDataID (*create_object_of_type)(type));

    // object creation & retrieval
    plDataID (*create_object)     (void);
    plDataID (*get_object_by_name)(const char* pcName); // assumes property 0 is name

    // reading (no locking or waiting)
    const plDataObject* (*read)      (plDataID);
    const char*         (*get_string)(const plDataObject*, uint32_t uProperty);
    void*               (*get_buffer)(const plDataObject*, uint32_t uProperty);
    void                (*end_read)  (const plDataObject*);

    // writing (global lock)
    plDataObject* (*write)     (plDataID);
    void          (*set_string)(plDataObject*, uint32_t uProperty, const char*);
    void          (*set_buffer)(plDataObject*, uint32_t uProperty, void*);
    void          (*commit)    (plDataObject*);
    
} plDataRegistryI;

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
    
    PL_KEY_RESERVED_MOD_CTRL, PL_KEY_RESERVED_MOD_SHIFT, PL_KEY_RESERVED_MOD_ALT, PL_RESERVED_KEY_MOD_SUPER,
    PL_KEY_COUNT, // no valid plKey_ is ever greater than this value

    PL_KEY_MOD_NONE     = 0,
    PL_KEY_MOD_CTRL     = 1 << 12, // ctrl
    PL_KEY_MOD_SHIFT    = 1 << 13, // shift
    PL_KEY_MOD_ALT      = 1 << 14, // option/menu
    PL_KEY_MOD_SUPER    = 1 << 15, // cmd/super/windows
    PL_KEY_MOD_SHORTCUT = 1 << 11, // alias for Ctrl (non-macOS) _or_ super (macOS)
    PL_KEY_MOD_MASK_    = 0xF800   // 5 bits
};

enum plMouseCursor_
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
// [SECTION] IO struct
//-----------------------------------------------------------------------------

typedef struct _plKeyData
{
    bool  bDown;
    float fDownDuration;
    float fDownDurationPrev;
} plKeyData;

typedef struct _plIO
{

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    float  fDeltaTime;
    float  fMouseDragThreshold;      // default 6.0f
    float  fMouseDoubleClickTime;    // default 0.3f seconds
    float  fMouseDoubleClickMaxDist; // default 6.0f
    float  fKeyRepeatDelay;          // default 0.275f
    float  fKeyRepeatRate;           // default 0.050f
    plVec2 tMainViewportSize;
    plVec2 tMainFramebufferScale;

    // miscellaneous options
    bool bConfigMacOSXBehaviors;

    //------------------------------------------------------------------
    // platform functions
    //------------------------------------------------------------------

    // access OS clipboard
    // (default to use native Win32 clipboard on Windows, otherwise uses a private clipboard. Override to access OS clipboard on other architectures)
    const char* (*get_clipboard_text_fn)(void* pUserData);
    void        (*set_clipboard_text_fn)(void* pUserData, const char* pcText);
    void*       pClipboardUserData;
    char*       sbcClipboardData;

    // command line args
    int    iArgc;
    char** apArgv;

    void* pBackendPlatformData;

    //------------------------------------------------------------------
    // Input/Output
    //------------------------------------------------------------------

    bool  bRunning;
    float fHeadlessUpdateRate; // frame rate when headless (FPS)

    //------------------------------------------------------------------
    // Output
    //------------------------------------------------------------------

    double   dTime;
    float    fFrameRate; // rough estimate(rolling average of fDeltaTime over 120 frames)
    bool     bViewportSizeChanged;
    bool     bViewportMinimized;
    uint64_t ulFrameCount;
    
    plKeyChord tKeyMods;
    bool       bKeyCtrl;  // Keyboard modifier down: Control
    bool       bKeyShift; // Keyboard modifier down: Shift
    bool       bKeyAlt;   // Keyboard modifier down: Alt
    bool       bKeySuper; // Keyboard modifier down: Cmd/Super/Windows

    // [INTERNAL]
    plInputEvent* _sbtInputEvents;
    plUiWChar*    _sbInputQueueCharacters;
    plUiWChar     _tInputQueueSurrogate; 

    // main input state
    plVec2 _tMousePos;
    bool   _abMouseDown[5];
    int    _iMouseButtonsDown;
    float  _fMouseWheel;
    float  _fMouseWheelH;

    // mouse cursor
    plMouseCursor tCurrentCursor;
    plMouseCursor tNextCursor;
    bool          bCursorChanged;

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

} plIO;

//-----------------------------------------------------------------------------
// [SECTION] structs (not for public use, subject to change)
//-----------------------------------------------------------------------------

typedef union _plDataID
{
    struct{
        uint32_t uSuperBlock : 10;
        uint32_t uBlock : 10;
        uint32_t uIndex : 10;
        uint64_t uUnused : 34;
    };
    uint64_t ulData;
} plDataID;

typedef struct _plAllocationEntry
{
    void*       pAddress;
    size_t      szSize;
    int         iLine;
    const char* pcFile; 
} plAllocationEntry;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef __cplusplus
    #if defined(_MSC_VER) //  Microsoft 
        #define PL_EXPORT extern "C" __declspec(dllexport)
        #define PL_CALL_CONVENTION (__cdecl *)
    #elif defined(__GNUC__) //  GCC or clang
        #define PL_EXPORT extern "C" __attribute__((visibility("default")))
        #define PL_CALL_CONVENTION (__attribute__(()) *)
    #else //  do nothing and hope for the best?
        #define PL_EXPORT
        #define PL_CALL_CONVENTION
        #pragma warning Unknown dynamic link import/export semantics.
    #endif
#else

    #if defined(_MSC_VER) //  Microsoft 
        #define PL_EXPORT __declspec(dllexport)
        #define PL_CALL_CONVENTION (__cdecl *)
    #elif defined(__GNUC__) //  GCC or clang
        #define PL_EXPORT __attribute__((visibility("default")))
        #define PL_CALL_CONVENTION (__attribute__(()) *)
    #else //  do nothing and hope for the best?
        #define PL_EXPORT
        #define PL_CALL_CONVENTION
        #pragma warning Unknown dynamic link import/export semantics.
    #endif
#endif

#ifdef PL_USER_CONFIG
    #include PL_USER_CONFIG
#endif
    #include "pl_config.h"

#ifdef PL_USE_STB_SPRINTF
    #include "stb_sprintf.h"
    #define pl_sprintf stbsp_sprintf
    #define pl_vsprintf stbsp_vsprintf
    #define pl_vnsprintf stbsp_vsnprintf
#else
    #define pl_sprintf sprintf
    #define pl_vsprintf vsprintf
    #define pl_vnsprintf vsnprintf
#endif

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

// settings
#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

// log settings
#ifndef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
#endif

#endif // PL_H