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
// [SECTION] context & apis
// [SECTION] enums
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
#include "pl.h"
#include "pl_string.h"
#include "pl_ui_ext.h"
#include "pl_draw_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ds.h"

#include "pl_ext.inc"

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
typedef struct _plUiPopupData      plUiPopupData;

// enums
typedef int plUiNextWindowFlags;
typedef int plUiAxis;
typedef int plUiLayoutRowEntryType;
typedef int plUiLayoutSystemType;
typedef int plDebugLogFlags;

//-----------------------------------------------------------------------------
// [SECTION] context & apis
//-----------------------------------------------------------------------------

static plUiContext*   gptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

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
        plVec4 tTextDisabledCol;
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
        plVec4 tSeparatorCol;
        plVec4 tTabCol;
        plVec4 tTabHoveredCol;
        plVec4 tTabSelectedCol;
        plVec4 tResizeGripCol;
        plVec4 tResizeGripHoveredCol;
        plVec4 tResizeGripActiveCol;
        plVec4 tSliderCol;
        plVec4 tSliderHoveredCol;
        plVec4 tSliderActiveCol;
        plVec4 tPopupBgColor;
    };
    plVec4 atColors[PL_UI_COLOR_COUNT];
} plUiColorScheme;

typedef struct _plUiStyle
{
    // main
    float  fTitlePadding;
    float  fFontSize;
    float  fIndentSize;
    float  fWindowHorizontalPadding;
    float  fWindowVerticalPadding;
    int    iWindowBorderSize;
    float  fScrollbarSize;
    float  fSliderSize;
    plVec2 tItemSpacing;
    plVec2 tInnerSpacing;
    plVec2 tFramePadding;

    // widgets
    float  fSeparatorTextLineSize;
    plVec2 tSeparatorTextAlignment;
    plVec2 tSeparatorTextPadding;

    // rounding
    float fWindowRounding;
    float fChildRounding;
    float fFrameRounding;
    float fScrollbarRounding;
    float fGrabRounding;
    float fTabRounding;
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
    float                fSpecifiedHeight;     // height user passed in
    float                fWidgetWidth;         // widget width (could be relative or absolute)
    float                fWidgetHeight;        // widget height (could be relative or absolute)
    float                fWidgetXOffset;       // offset where the next widget should start from (usually previous widget + item spacing)
    float                fWidgetYOffset;       // used by space layout system (system 6) to temporary offset widget placement
    uint32_t             uColumns;             // number of columns in row
    uint32_t             uCurrentColumn;       // current column
    float                fMaxWidth;            // maximum row width  (to help set max cursor position)
    float                fMaxHeight;           // maximum row height (to help set next row position + max cursor position)
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
    plUiWChar*         sbTextW;                // edit buffer, we need to persist but can't guarantee the persistence of the user-provided buffer. so we copy into own buffer.
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
    plUiInputTextFlags tFlags;                 // copy of InputText() flags. may be used to check if e.g. ImGuiInputTextFlags_Password is set.
} plUiInputTextState;

typedef struct _plUiPopupData
{
    uint32_t uId;
    uint64_t ulOpenFrameCount;
}plUiPopupData;

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

typedef struct _plUiCursorStackItem
{
    plVec2 tPos;
} plUiCursorStackItem;

typedef struct _plUiTempWindowData
{
    float         fTitleBarHeight;   // titlebar height
    plVec2        tCursorStartPos;   // position where widgets begin drawing (could be outside window if scrolling)
    plVec2        tCursorMaxPos;     // maximum cursor position (could be outside window if scrolling)
    plVec2        tRowCursorPos;     // current row starting position
    plUiLayoutRow tLayoutRow;        // current layout row to use
    uint32_t      uTreeDepth;        // current level inside trees
    float         fExtraIndent;      // extra indent added by pl_indent
    float         fTempMinWidth;     // template layout system
    float         fTempStaticWidth;  // template layout system
} plUiTempWindowData;

typedef struct _plUiWindow
{
    char*                pcName;
    size_t               szNameBufferLength;
    uint32_t             uId;                           // window Id (=pl_str_hash(pcName))
    plUiWindowFlags      tFlags;                        // plUiWindowFlags, not all honored at the moment
    plVec2               tPos;                          // position of window in viewport
    plVec2               tContentSize;                  // size of contents/scrollable client area
    plVec2               tMinSize;                      // minimum size of window (default 200,200)
    plVec2               tMaxSize;                      // maximum size of window (default 10000,10000)
    plVec2               tSize;                         // full size or title bar size if collapsed
    plVec2               tFullSize;                     // used to restore size after uncollapsing
    plVec2               tScroll;                       // current scroll amount (0 < tScroll < tScrollMax)
    plVec2               tScrollMax;                    // maximum scroll amount based on last frame content size & adjusted for scroll bars
    plRect               tInnerRect;                    // inner rect (excludes titlebar & scrollbars)
    plRect               tOuterRect;                    // outer rect (includes everything)
    plRect               tOuterRectClipped;             // outer rect clipped by parent window & viewport
    plRect               tInnerClipRect;                // inner rect clipped by parent window & viewport (includes horizontal padding on each side)
    plUiWindow*          ptParentWindow;                // parent window if child
    plUiWindow*          ptRootWindow;                  // root window or self if this is the root window
    plUiWindow*          ptRootWindowPopupTree;         // root window or self if this is the root window
    plUiWindow*          ptRootWindowTitleBarHighlight; // root window or self if this is the root window
    bool                 bAppearing;
    bool                 bVisible;                // true if visible (only for child windows at the moment)
    bool                 bActive;                 // window has been "seen" this frame
    bool                 bCollapsed;              // window is currently collapsed
    bool                 bScrollbarX;             // set if horizontal scroll bar is "on"
    bool                 bScrollbarY;             // set if vertical scroll bar is "on"
    plUiTempWindowData   tTempData;               // temporary data reset at the beginning of frame
    plUiWindow**         sbtChildWindows;         // child windows if any (reset every frame)
    plUiLayoutRow*       sbtRowStack;             // row stack for containers to push parents row onto and pop when they exist (reset every frame)
    plUiCursorStackItem* sbtCursorStack;
    float*               sbfAvailableSizeStack;
    float*               sbfMaxCursorYStack;
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
    // cosmetics
    plUiStyle       tStyle;
    plUiColorScheme tColorScheme;

    // keyboard/mouse
    bool bWantCaptureMouse;
    bool bWantCaptureKeyboard;
    bool bWantTextInput;
    bool abMouseOwned[5];
    
    // prev/next state
    plUiNextWindowData tNextWindowData;        // info based on pl_set_next_window_* functions
    plUiPrevItemData   tPrevItemData;          // data for use by pl_was_last_item_* functions
    uint32_t*          sbuIdStack;             // id stack for hashing IDs, container items usually push/pop these

    // widget state
    plUiInputTextState tInputTextState;
    uint32_t           uHoveredId;             // set at the end of the previous frame from uNextHoveredId
    uint32_t           uActiveId;              // set by pl__set_active_id(...) function
    uint32_t           uActiveIdIsAlive;       // id of active item if seen this frame
    uint32_t           uNextHoveredId;         // set during current frame (by end of frame, should be last item hovered)
    bool               bActiveIdJustActivated; // window was just activated, so bring it to the front
    uint32_t           uMenuDepth;
    uint32_t           uComboDepth;

    // window state
    plUiWindow** sbptWindows;            // windows stored in display order
    plUiWindow** sbptFocusedWindows;     // root windows stored in display order
    plUiWindow** sbptWindowStack;
    plUiStorage  tWindows;               // windows by ID for quick retrieval
    plUiWindow   tTooltipWindow;         // persistent tooltip window (since there can only ever be 1 at a time)
    plUiWindow*  ptCurrentWindow;        // current window we are appending into
    plUiWindow*  ptHoveredWindow;        // window being hovered
    plUiWindow*  ptMovingWindow;         // window being moved
    plUiWindow*  ptSizingWindow;         // window being resized
    plUiWindow*  ptScrollingWindow;      // window being scrolled with mouse
    plUiWindow*  ptWheelingWindow;       // window being scrolled with mouse wheel
    plUiWindow*  ptActiveWindow;         // active window

    // navigation
    plUiWindow* ptNavWindow; // focused window
    uint32_t    uNavId;
    bool        bNavIdIsAlive; // id of focused item if seen this frame

    // shared stacks
    plUiPopupData* sbtBeginPopupStack;
    plUiPopupData* sbtOpenPopupStack;

    // tab bars
    plUiTabBar* sbtTabBars;             // stretchy-buffer for persistent tab bar data
    plUiTabBar* ptCurrentTabBar;        // current tab bar being appended to

    // theme stacks
    plUiColorStackItem* sbtColorStack;

    // drawing
    plDrawList2D*  ptDrawlist;             // main ui drawlist
    plDrawList2D*  ptDebugDrawlist;        // ui debug drawlist (i.e. overlays)
    plFont*        tFont;                  // current font
    plDrawLayer2D* ptBgLayer;              // submitted before window layers
    plDrawLayer2D* ptFgLayer;              // submitted after window layers
    plDrawLayer2D* ptDebugLayer;           // submitted last

    // drawing context
    plDrawList2D** sbDrawlists;
    plVec2         tFrameBufferScale;

    // misc
    char* sbcTempBuffer;
} plUiContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static const char*   pl__find_renderered_text_end(const char* pcText, const char* pcTextEnd);
static void          pl__add_text                (plDrawLayer2D*, plFont*, float fSize, plVec2 tP, uint32_t uColor, const char* pcText, float fWrap);
static void          pl__add_clipped_text        (plDrawLayer2D*, plFont*, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, uint32_t uColor, const char* pcText, float fWrap);
static plVec2        pl__calculate_text_size     (plFont*, float size, const char* text, float wrap);
static inline float  pl__get_frame_height        (void) { return gptCtx->tStyle.fFontSize + gptCtx->tStyle.tFramePadding.y * 2.0f; }

// collision
static inline bool   pl__does_circle_contain_point  (plVec2 cen, float radius, plVec2 point) { const float fDistanceSquared = powf(point.x - cen.x, 2) + powf(point.y - cen.y, 2); return fDistanceSquared <= radius * radius; }
static bool          pl__is_item_hoverable          (const plRect* ptBox, uint32_t uHash);

// layouts
static plVec2 pl__calculate_item_size(float fDefaultHeight);
static void   pl__smart_advance_cursor(float fWidth, float fHeight);
static void   pl__advance_cursor(plVec2 tOffset);
static void   pl__set_cursor(plVec2 tPos);

static inline plVec2
pl__get_cursor_pos(void)
{
    return (plVec2) {
        gptCtx->ptCurrentWindow->tTempData.tRowCursorPos.x + gptCtx->ptCurrentWindow->tTempData.tLayoutRow.fWidgetXOffset,
        gptCtx->ptCurrentWindow->tTempData.tRowCursorPos.y + gptCtx->ptCurrentWindow->tTempData.tLayoutRow.fWidgetYOffset
    };
}

// misc
static bool        pl__begin_window_ex(const char* pcName, bool* pbOpen, plUiWindowFlags);
static void        pl__render_scrollbar(plUiWindow*, uint32_t uHash, plUiAxis);
static void        pl__submit_window   (plUiWindow*);
static void        pl__focus_window    (plUiWindow*);
static inline bool pl__ui_should_render(const plVec2* ptStartPos, const plVec2* ptWidgetSize) { return !(ptStartPos->y + ptWidgetSize->y < gptCtx->ptCurrentWindow->tPos.y || ptStartPos->y > gptCtx->ptCurrentWindow->tPos.y + gptCtx->ptCurrentWindow->tSize.y); }
static void        pl__set_active_id(uint32_t uHash, plUiWindow*);
static void        pl__set_nav_id   (uint32_t uHash, plUiWindow*);
static void        pl__add_widget(uint32_t uHash);
static bool        pl__input_text_ex(const char* pcLabel, const char* pcHint, char* pcBuffer, size_t szBufferSize, plUiInputTextFlags, const plVec2* ptWidgetSize, const plVec2* ptStartPos);

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

static bool pl__button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~storage system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static plUiStorageEntry* pl__lower_bound(plUiStorageEntry* sbtData, uint32_t uKey);

static int   pl__get_int  (plUiStorage*, uint32_t uKey, int iDefaultValue);
static float pl__get_float(plUiStorage*, uint32_t uKey, float fDefaultValue);
static bool  pl__get_bool (plUiStorage*, uint32_t uKey, bool bDefaultValue);
static void* pl__get_ptr  (plUiStorage*, uint32_t uKey);

static int*   pl__get_int_ptr  (plUiStorage*, uint32_t uKey, int iDefaultValue);
static float* pl__get_float_ptr(plUiStorage*, uint32_t uKey, float fDefaultValue);
static bool*  pl__get_bool_ptr (plUiStorage*, uint32_t uKey, bool bDefaultValue);
static void** pl__get_ptr_ptr  (plUiStorage*, uint32_t uKey, void* pDefaultValue);

static void pl__set_int  (plUiStorage*, uint32_t uKey, int iValue);
static void pl__set_float(plUiStorage*, uint32_t uKey, float fValue);
static void pl__set_bool (plUiStorage*, uint32_t uKey, bool bValue);
static void pl__set_ptr  (plUiStorage*, uint32_t uKey, void* pValue);

#endif // PL_UI_INTERNAL_H