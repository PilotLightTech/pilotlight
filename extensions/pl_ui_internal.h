/*
   pl_ui_internal.h
   - FORWARD COMPATIBILITY NOT GUARANTEED
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] helper macros
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] context
// [SECTION] enums
// [SECTION] math
// [SECTION] stretchy buffer internal
// [SECTION] stretchy buffer
// [SECTION] internal structs
// [SECTION] plUiStorage
// [SECTION] plUiWindow
// [SECTION] plUiContext
// [SECTION] internal api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_INTERNAL_H
#define PL_UI_INTERNAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint32_t
#include <stdlib.h>  // malloc, free
#include <string.h>  // memset, memmove
#include <stdbool.h> // bool
#include <stdarg.h>  // arg vars
#include <stdio.h>   // vsprintf
#include <assert.h>
#include <math.h>
#include "pl_string.h"
#include "pl_ui_ext.h"
#include "pl_draw_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] helper macros
//-----------------------------------------------------------------------------

// stb
#undef STB_TEXTEDIT_STRING
#undef STB_TEXTEDIT_CHARTYPE
#define STB_TEXTEDIT_STRING           plUiInputTextState
#define STB_TEXTEDIT_CHARTYPE         plUiWChar
#define STB_TEXTEDIT_GETWIDTH_NEWLINE (-1.0f)
#define STB_TEXTEDIT_UNDOSTATECOUNT   99
#define STB_TEXTEDIT_UNDOCHARCOUNT    999
#include "stb_textedit.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

// Helper: Unicode defines
#define PL_UNICODE_CODEPOINT_INVALID 0xFFFD     // Invalid Unicode code point (standard value).
#define PL_UNICODE_CODEPOINT_MAX     0xFFFF     // Maximum Unicode code point supported by this build.

#define PLU_PI_2 1.57079632f // pi/2
#define PLU_2PI  6.28318530f // pi

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plUiStyle          plUiStyle;
typedef union  _plUiColorScheme    plUiColorScheme;
typedef struct _plUiColorStackItem plUiColorStackItem;
typedef struct _plUiWindow         plUiWindow;
typedef struct _plUiTabBar         plUiTabBar;
typedef struct _plUiPrevItemData   plUiPrevItemData;
typedef struct _plUiNextWindowData plUiNextWindowData;
typedef struct _plUiTempWindowData plUiTempWindowData;
typedef struct _plUiStorage        plUiStorage;
typedef struct _plUiStorageEntry   plUiStorageEntry;
typedef struct _plUiInputTextState plUiInputTextState;

// enums
typedef int plUiNextWindowFlags;
typedef int plUiAxis;
typedef int plUiLayoutRowEntryType;
typedef int plUiLayoutSystemType;
typedef int plDebugLogFlags;

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

plUiContext* gptCtx = NULL;
const plIOI* gptIOI = NULL;
const plDrawI* gptDraw = NULL;
plIO* gptIO = NULL;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plDebugLogFlags_
{
    PL_UI_DEBUG_LOG_FLAGS_NONE            = 0,
    PL_UI_DEBUG_LOG_FLAGS_EVENT_IO        = 1 << 0,
    PL_UI_DEBUG_LOG_FLAGS_EVENT_ACTIVE_ID = 1 << 1,
};

enum plUiAxis_
{
    PL_UI_AXIS_NONE = -1,
    PL_UI_AXIS_X    =  0,
    PL_UI_AXIS_Y    =  1,
};

enum _plUiNextWindowFlags
{
    PL_NEXT_WINDOW_DATA_FLAGS_NONE          = 0,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS       = 1 << 0,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE      = 1 << 1,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED = 1 << 2,   
};

enum plUiLayoutRowEntryType_
{
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_NONE,
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE,
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC,
    PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC
};

enum plUiLayoutSystemType_
{
    PL_UI_LAYOUT_SYSTEM_TYPE_NONE,
    PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
    PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
    PL_UI_LAYOUT_SYSTEM_TYPE_SPACE,
    PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
    PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
    PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX
};

enum plUiInputTextFlags_ // borrowed from "Dear ImGui"
{
    PL_UI_INPUT_TEXT_FLAGS_NONE                    = 0,
    PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL           = 1 << 0,   // allow 0123456789.+-*/
    PL_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL       = 1 << 1,   // allow 0123456789ABCDEFabcdef
    PL_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE         = 1 << 2,   // turn a..z into A..Z
    PL_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK          = 1 << 3,   // filter out spaces, tabs
    PL_UI_INPUT_TEXT_FLAGS_AUTO_SELECT_ALL         = 1 << 4,   // select entire text when first taking mouse focus
    PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE      = 1 << 5,   // return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider looking at the IsItemDeactivatedAfterEdit() function.
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_COMPLETION     = 1 << 6,   // callback on pressing TAB (for completion handling)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_HISTORY        = 1 << 7,   // callback on pressing Up/Down arrows (for history handling)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_ALWAYS         = 1 << 8,   // callback on each iteration. User code may query cursor position, modify text buffer.
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_CHAR_FILTER    = 1 << 9,   // callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
    PL_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT         = 1 << 10,  // pressing TAB input a '\t' character into the text field
    PL_UI_INPUT_TEXT_FLAGS_CTRL_ENTER_FOR_NEW_LINE = 1 << 11,  // in multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).
    PL_UI_INPUT_TEXT_FLAGS_NO_HORIZONTAL_SCROLL    = 1 << 12,  // disable following the cursor horizontally
    PL_UI_INPUT_TEXT_FLAGS_ALWAYS_OVERWRITE        = 1 << 13,  // overwrite mode
    PL_UI_INPUT_TEXT_FLAGS_READ_ONLY               = 1 << 14,  // read-only mode
    PL_UI_INPUT_TEXT_FLAGS_PASSWORD                = 1 << 15,  // password mode, display all characters as '*'
    PL_UI_INPUT_TEXT_FLAGS_NO_UNDO_REDO            = 1 << 16,  // disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().
    PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC        = 1 << 17,  // allow 0123456789.+-*/eE (Scientific notation input)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_RESIZE         = 1 << 18,  // callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_EDIT           = 1 << 19,  // callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)
    PL_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL       = 1 << 20,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    
    // internal
    PL_UI_INPUT_TEXT_FLAGS_MULTILINE      = 1 << 21,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    PL_UI_INPUT_TEXT_FLAGS_NO_MARK_EDITED = 1 << 22,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
};

#define PLU_VEC2_LENGTH_SQR(vec) (((vec).x * (vec).x) + ((vec).y * (vec).y))

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct  _plUiColorStackItem
{
    plUiColor tIndex;
    plVec4    tColor;
} plUiColorStackItem;

typedef union _plUiColorScheme
{
    struct 
    {
        plVec4 tTitleActiveCol;
        plVec4 tTitleBgCol;
        plVec4 tTitleBgCollapsedCol;
        plVec4 tWindowBgColor;
        plVec4 tWindowBorderColor;
        plVec4 tChildBgColor;
        plVec4 tButtonCol;
        plVec4 tButtonHoveredCol;
        plVec4 tButtonActiveCol;
        plVec4 tTextCol;
        plVec4 tProgressBarCol;
        plVec4 tCheckmarkCol;
        plVec4 tFrameBgCol;
        plVec4 tFrameBgHoveredCol;
        plVec4 tFrameBgActiveCol;
        plVec4 tHeaderCol;
        plVec4 tHeaderHoveredCol;
        plVec4 tHeaderActiveCol;
        plVec4 tScrollbarBgCol;
        plVec4 tScrollbarHandleCol;
        plVec4 tScrollbarFrameCol;
        plVec4 tScrollbarActiveCol;
        plVec4 tScrollbarHoveredCol;
    };
    plVec4 atColors[23];
} plUiColorScheme;

typedef struct _plUiStyle
{
    // style
    float  fTitlePadding;
    float  fFontSize;
    float  fIndentSize;
    float  fWindowHorizontalPadding;
    float  fWindowVerticalPadding;
    float  fScrollbarSize;
    float  fSliderSize;
    plVec2 tItemSpacing;
    plVec2 tInnerSpacing;
    plVec2 tFramePadding;
} plUiStyle;

typedef struct _plUiTabBar
{
    uint32_t    uId;
    plVec2      tStartPos;
    plVec2      tCursorPos;
    uint32_t    uCurrentIndex;
    uint32_t    uValue;
    uint32_t    uNextValue;
} plUiTabBar;

typedef struct _plUiPrevItemData
{
    bool bHovered;
    bool bActive;
} plUiPrevItemData;

typedef struct _plUiLayoutSortLevel
{
    float    fWidth;
    uint32_t uStartIndex;
    uint32_t uCount;
} plUiLayoutSortLevel;

typedef struct _plUiLayoutRowEntry
{
    plUiLayoutRowEntryType tType;  // entry type (PL_UI_LAYOUT_ROW_ENTRY_TYPE_*)
    float                  fWidth; // widget width (could be relative or absolute)
} plUiLayoutRowEntry;

typedef struct _plUiLayoutRow
{
    plUiLayoutRowType    tType;                // determines if width/height is relative or absolute (PL_UI_LAYOUT_ROW_TYPE_*)
    plUiLayoutSystemType tSystemType;          // this rows layout strategy

    float                fWidth;               // widget width (could be relative or absolute)
    float                fHeight;              // widget height (could be relative or absolute)
    float                fSpecifiedHeight;     // height user passed in
    float                fVerticalOffset;      // used by space layout system (system 6) to temporary offset widget placement
    float                fHorizontalOffset;    // offset where the next widget should start from (usually previous widget + item spacing)
    uint32_t             uColumns;             // number of columns in row
    uint32_t             uCurrentColumn;       // current column
    float                fMaxWidth;            // maximum row width  (to help set max cursor position)
    float                fMaxHeight;           // maximum row height (to help set next row position + max cursor position)
    float                fRowStartX;           // offset where the row starts from (think of a tab bar being the second widget in the row)
    const float*         pfSizesOrRatios;      // size or rations when using array layout system (system 4)
    uint32_t             uStaticEntryCount;    // number of static entries when using template layout system (system 5)
    uint32_t             uDynamicEntryCount;   // number of dynamic entries when using template layout system (system 5)
    uint32_t             uVariableEntryCount;  // number of variable entries when using template layout system (system 5)
    uint32_t             uEntryStartIndex;     // offset into parent window sbtRowTemplateEntries buffer
} plUiLayoutRow;

typedef struct _plUiInputTextState
{
    uint32_t           uId;
    int                iCurrentLengthW;        // widget id owning the text state
    int                iCurrentLengthA;        // we need to maintain our buffer length in both UTF-8 and wchar format. UTF-8 length is valid even if TextA is not.
    plUiWChar*           sbTextW;                // edit buffer, we need to persist but can't guarantee the persistence of the user-provided buffer. so we copy into own buffer.
    char*              sbTextA;                // temporary UTF8 buffer for callbacks and other operations. this is not updated in every code-path! size=capacity.
    char*              sbInitialTextA;         // backup of end-user buffer at the time of focus (in UTF-8, unaltered)
    bool               bTextAIsValid;          // temporary UTF8 buffer is not initially valid before we make the widget active (until then we pull the data from user argument)
    int                iBufferCapacityA;       // end-user buffer capacity
    float              fScrollX;               // horizontal scrolling/offset
    STB_TexteditState  tStb;                   // state for stb_textedit.h
    float              fCursorAnim;            // timer for cursor blink, reset on every user action so the cursor reappears immediately
    bool               bCursorFollow;          // set when we want scrolling to follow the current cursor position (not always!)
    bool               bSelectedAllMouseLock;  // after a double-click to select all, we ignore further mouse drags to update selection
    bool               bEdited;                // edited this frame
    plUiInputTextFlags tFlags;    // copy of InputText() flags. may be used to check if e.g. ImGuiInputTextFlags_Password is set.
} plUiInputTextState;

//-----------------------------------------------------------------------------
// [SECTION] plUiStorage
//-----------------------------------------------------------------------------

typedef struct _plUiStorageEntry
{
    uint32_t uKey;
    union
    {
        int   iValue;
        float fValue;
        void* pValue;
    };
} plUiStorageEntry;

typedef struct _plUiStorage
{
    plUiStorageEntry* sbtData;
} plUiStorage;

//-----------------------------------------------------------------------------
// [SECTION] plUiWindow
//-----------------------------------------------------------------------------

typedef struct _plUiTempWindowData
{
    plVec2               tCursorStartPos;   // position where widgets begin drawing (could be outside window if scrolling)
    plVec2               tCursorMaxPos;     // maximum cursor position (could be outside window if scrolling)
    uint32_t             uTreeDepth;        // current level inside trees
    float                fExtraIndent;      // extra indent added by pl_indent
    plUiLayoutRow        tCurrentLayoutRow; // current layout row to use
    plVec2               tRowPos;           // current row starting position
    float                fAccumRowX;        // additional indent due to a parent (like tab bar) not being the first item in a row
    float                fTitleBarHeight;   // titlebar height

    // template layout system
    float fTempMinWidth;
    float fTempStaticWidth;
} plUiTempWindowData;

typedef struct _plUiWindow
{
    const char*          pcName;
    uint32_t             uId;                     // window Id (=plu_str_hash(pcName))
    plUiWindowFlags      tFlags;                  // plUiWindowFlags, not all honored at the moment
    plVec2               tPos;                    // position of window in viewport
    plVec2               tContentSize;            // size of contents/scrollable client area
    plVec2               tMinSize;                // minimum size of window (default 200,200)
    plVec2               tMaxSize;                // maximum size of window (default 10000,10000)
    plVec2               tSize;                   // full size or title bar size if collapsed
    plVec2               tFullSize;               // used to restore size after uncollapsing
    plVec2               tScroll;                 // current scroll amount (0 < tScroll < tScrollMax)
    plVec2               tScrollMax;              // maximum scroll amount based on last frame content size & adjusted for scroll bars
    plRect               tInnerRect;              // inner rect (excludes titlebar & scrollbars)
    plRect               tOuterRect;              // outer rect (includes everything)
    plRect               tOuterRectClipped;       // outer rect clipped by parent window & viewport
    plRect               tInnerClipRect;          // inner rect clipped by parent window & viewport (includes horizontal padding on each side)
    plUiWindow*          ptParentWindow;          // parent window if child
    plUiWindow*          ptRootWindow;            // root window or self if this is the root window
    bool                 bVisible;                // true if visible (only for child windows at the moment)
    bool                 bActive;                 // window has been "seen" this frame
    bool                 bCollapsed;              // window is currently collapsed
    bool                 bScrollbarX;             // set if horizontal scroll bar is "on"
    bool                 bScrollbarY;             // set if vertical scroll bar is "on"
    plUiTempWindowData   tTempData;               // temporary data reset at the beginning of frame
    plUiWindow**         sbtChildWindows;         // child windows if any (reset every frame)
    plUiLayoutRow*       sbtRowStack;             // row stack for containers to push parents row onto and pop when they exist (reset every frame)
    plUiLayoutSortLevel* sbtTempLayoutSort;       // blah
    uint32_t*            sbuTempLayoutIndexSort;  // blah
    plUiLayoutRowEntry*  sbtRowTemplateEntries;   // row template entries (shared and reset every frame)            
    plDrawLayer2D*       ptBgLayer;               // background draw layer
    plDrawLayer2D*       ptFgLayer;               // foreground draw layer
    plUiConditionFlags   tPosAllowableFlags;      // acceptable condition flags for "pl_set_next_window_pos()"
    plUiConditionFlags   tSizeAllowableFlags;     // acceptable condition flags for "pl_set_next_window_size()"
    plUiConditionFlags   tCollapseAllowableFlags; // acceptable condition flags for "pl_set_next_window_collapse()"
    uint8_t              uHideFrames;             // hide window for this many frames (useful for autosizing)
    uint32_t             uFocusOrder;             // display rank
    plUiStorage          tStorage;                // state storage
} plUiWindow;

//-----------------------------------------------------------------------------
// [SECTION] plUiContext
//-----------------------------------------------------------------------------

typedef struct _plUiNextWindowData
{
    plUiNextWindowFlags tFlags;
    plUiConditionFlags  tPosCondition;
    plUiConditionFlags  tSizeCondition;
    plUiConditionFlags  tCollapseCondition;
    plVec2              tPos;
    plVec2              tSize;
    bool                bCollapsed;
} plUiNextWindowData;

typedef struct _plUiContext
{
    plUiStyle       tStyle;
    plUiColorScheme tColorScheme;
    plIO            tIO;
    
    // prev/next state
    plUiNextWindowData tNextWindowData;        // info based on pl_set_next_window_* functions
    plUiPrevItemData   tPrevItemData;          // data for use by pl_was_last_item_* functions
    uint32_t*          sbuIdStack;             // id stack for hashing IDs, container items usually push/pop these

    // widget state
    plUiInputTextState tInputTextState;
    uint32_t           uHoveredId;             // set at the end of the previous frame from uNextHoveredId
    uint32_t           uActiveId;              // set at the end of the previous frame from uNextActiveId
    uint32_t           uActiveIdIsAlive;
    uint32_t           uNextHoveredId;         // set during current frame (by end of frame, should be last item hovered)

    uint32_t           uActiveWindowId;               // current active window id
    bool               bActiveIdJustActivated;        // window was just activated, so bring it to the front

    // windows
    plUiWindow         tTooltipWindow;         // persistent tooltip window (since there can only ever be 1 at a time)
    plUiWindow*        ptCurrentWindow;        // current window we are appending into
    plUiWindow*        ptHoveredWindow;        // window being hovered
    plUiWindow*        ptMovingWindow;         // window being moved
    plUiWindow*        ptSizingWindow;         // window being resized
    plUiWindow*        ptScrollingWindow;      // window being scrolled with mouse
    plUiWindow*        ptWheelingWindow;       // window being scrolled with mouse wheel
    plUiWindow*        ptActiveWindow;         // selected/active window
    plUiWindow**       sbptWindows;            // windows stored in display order (reset every frame and non root windows)
    plUiWindow**       sbptFocusedWindows;     // root windows stored in display order
    plUiStorage        tWindows;               // windows by ID for quick retrieval

    // tabs
    plUiTabBar*        sbtTabBars;             // stretchy-buffer for persistent tab bar data
    plUiTabBar*        ptCurrentTabBar;        // current tab bar being appended to

    // theme stacks
    plUiColorStackItem* sbtColorStack;

    // drawing
    plDrawList2D*      ptDrawlist;             // main ui drawlist
    plDrawList2D*      ptDebugDrawlist;        // ui debug drawlist (i.e. overlays)
    plFontHandle       tFont;                  // current font
    plDrawLayer2D*     ptBgLayer;              // submitted before window layers
    plDrawLayer2D*     ptFgLayer;              // submitted after window layers
    plDrawLayer2D*     ptDebugLayer;           // submitted last

    // drawing context
    plDrawList2D** sbDrawlists;
    plVec2         tFrameBufferScale;
} plUiContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

const char*          pl_find_renderered_text_end(const char* pcText, const char* pcTextEnd);
void                 pl_ui_add_text             (plDrawLayer2D* ptLayer, plFontHandle, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
void                 pl_add_clipped_text        (plDrawLayer2D* ptLayer, plFontHandle, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
plVec2               pl_ui_calculate_text_size  (plFontHandle, float size, const char* text, float wrap);
static inline float  pl_get_frame_height        (void) { return gptCtx->tStyle.fFontSize + gptCtx->tStyle.tFramePadding.y * 2.0f; }

// collision
static inline bool   pl_does_circle_contain_point  (plVec2 cen, float radius, plVec2 point) { const float fDistanceSquared = powf(point.x - cen.x, 2) + powf(point.y - cen.y, 2); return fDistanceSquared <= radius * radius; }
bool                 pl_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point);
bool                 pl_is_item_hoverable          (const plRect* ptBox, uint32_t uHash);

// layouts
static inline plVec2 pl__ui_get_cursor_pos    (void) { return (plVec2){gptCtx->ptCurrentWindow->tTempData.tRowPos.x + gptCtx->ptCurrentWindow->tTempData.fAccumRowX + gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.fHorizontalOffset + (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize, gptCtx->ptCurrentWindow->tTempData.tRowPos.y + gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.fVerticalOffset};}
plVec2               pl_calculate_item_size(float fDefaultHeight);
void                 pl_advance_cursor     (float fWidth, float fHeight);

// misc
bool pl_begin_window_ex (const char* pcName, bool* pbOpen, plUiWindowFlags tFlags);
void pl_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis);
void pl_submit_window   (plUiWindow* ptWindow);
void pl__focus_window   (plUiWindow* ptWindow);

static inline bool   pl__ui_should_render(const plVec2* ptStartPos, const plVec2* ptWidgetSize) { return !(ptStartPos->y + ptWidgetSize->y < gptCtx->ptCurrentWindow->tPos.y || ptStartPos->y > gptCtx->ptCurrentWindow->tPos.y + gptCtx->ptCurrentWindow->tSize.y); }

void pl__set_active_id(uint32_t uHash, plUiWindow* ptWindow);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~text state system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static bool        pl__input_text_filter_character(unsigned int* puChar, plUiInputTextFlags tFlags);
static inline void pl__text_state_cursor_anim_reset  (plUiInputTextState* ptState) { ptState->fCursorAnim = -0.30f; }
static inline void pl__text_state_cursor_clamp       (plUiInputTextState* ptState) { ptState->tStb.cursor = pl_min(ptState->tStb.cursor, ptState->iCurrentLengthW); ptState->tStb.select_start = pl_min(ptState->tStb.select_start, ptState->iCurrentLengthW); ptState->tStb.select_end = pl_min(ptState->tStb.select_end, ptState->iCurrentLengthW);}
static inline bool pl__text_state_has_selection      (plUiInputTextState* ptState) { return ptState->tStb.select_start != ptState->tStb.select_end; }
static inline void pl__text_state_clear_selection    (plUiInputTextState* ptState) { ptState->tStb.select_start = ptState->tStb.select_end = ptState->tStb.cursor; }
static inline int  pl__text_state_get_cursor_pos     (plUiInputTextState* ptState) { return ptState->tStb.cursor; }
static inline int  pl__text_state_get_selection_start(plUiInputTextState* ptState) { return ptState->tStb.select_start; }
static inline int  pl__text_state_get_selection_end  (plUiInputTextState* ptState) { return ptState->tStb.select_end; }
static inline void pl__text_state_select_all         (plUiInputTextState* ptState) { ptState->tStb.select_start = 0; ptState->tStb.cursor = ptState->tStb.select_end = ptState->iCurrentLengthW; ptState->tStb.has_preferred_x = 0; }

static inline void pl__text_state_clear_text      (plUiInputTextState* ptState)           { ptState->iCurrentLengthA = ptState->iCurrentLengthW = 0; ptState->sbTextA[0] = 0; ptState->sbTextW[0] = 0; pl__text_state_cursor_clamp(ptState);}
static inline void pl__text_state_free_memory     (plUiInputTextState* ptState)           { pl_sb_free(ptState->sbTextA); pl_sb_free(ptState->sbTextW); pl_sb_free(ptState->sbInitialTextA);}
static inline int  pl__text_state_undo_avail_count(plUiInputTextState* ptState)           { return ptState->tStb.undostate.undo_point;}
static inline int  pl__text_state_redo_avail_count(plUiInputTextState* ptState)           { return STB_TEXTEDIT_UNDOSTATECOUNT - ptState->tStb.undostate.redo_point; }
static void        pl__text_state_on_key_press    (plUiInputTextState* ptState, int iKey);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widget behavior~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool pl_button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~storage system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

plUiStorageEntry*    pl_lower_bound(plUiStorageEntry* sbtData, uint32_t uKey);

int                  pl_get_int      (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float                pl_get_float    (plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool                 pl_get_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void*                pl_get_ptr      (plUiStorage* ptStorage, uint32_t uKey);

int*                 pl_get_int_ptr  (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float*               pl_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool*                pl_get_bool_ptr (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void**               pl_get_ptr_ptr  (plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue);

void                 pl_set_int      (plUiStorage* ptStorage, uint32_t uKey, int iValue);
void                 pl_set_float    (plUiStorage* ptStorage, uint32_t uKey, float fValue);
void                 pl_set_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bValue);
void                 pl_set_ptr      (plUiStorage* ptStorage, uint32_t uKey, void* pValue);

#endif // PL_UI_INTERNAL_H