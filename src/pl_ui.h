/*
   pl_ui.h
*/

// library version
#define PL_UI_VERSION    "0.5.0"
#define PL_UI_VERSION_NUM 00500

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_H
#define PL_UI_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
    #define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
PL_DECLARE_STRUCT(plUiContext);
PL_DECLARE_STRUCT(plUiStyle);
PL_DECLARE_STRUCT(plUiWindow);
PL_DECLARE_STRUCT(plUiTabBar);
PL_DECLARE_STRUCT(plUiPrevItemData);
PL_DECLARE_STRUCT(plUiNextWindowData);
PL_DECLARE_STRUCT(plUiTempWindowData);
PL_DECLARE_STRUCT(plUiStorage);
PL_DECLARE_STRUCT(plUiStorageEntry);

// enums
typedef int plUiConditionFlags;
typedef int plUiNextWindowFlags;
typedef int plUiWindowFlags;
typedef int plUiAxis;

// external (from pl_draw.h)
PL_DECLARE_STRUCT(plFont);
PL_DECLARE_STRUCT(plDrawList);
PL_DECLARE_STRUCT(plDrawLayer);
PL_DECLARE_STRUCT(plDrawContext);
typedef void* plTextureId;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
void         pl_ui_setup_context  (plDrawContext* ptDrawCtx, plUiContext* ptCtx);
void         pl_ui_cleanup_context(void);
void         pl_ui_set_context    (plUiContext* ptCtx);
plUiContext* pl_ui_get_context    (void);

// main
void   pl_ui_new_frame(void);
void   pl_ui_end_frame(void);
void   pl_ui_render   (void);

// windows
bool   pl_ui_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize);
void   pl_ui_end_window  (void);

// child windows
bool   pl_ui_begin_child(const char* pcName, plVec2 tSize);
void   pl_ui_end_child  (void);

// tooltips
void   pl_ui_begin_tooltip(void);
void   pl_ui_end_tooltip  (void);

// window utils
void   pl_ui_set_next_window_pos     (plVec2 tPos, plUiConditionFlags tCondition);
void   pl_ui_set_next_window_size    (plVec2 tSize, plUiConditionFlags tCondition);
void   pl_ui_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition);

// widgets
bool   pl_ui_button      (const char* pcText);
bool   pl_ui_button_ex   (const char* pcText, plVec2 tSize);
bool   pl_ui_selectable  (const char* pcText, bool* bpValue);
bool   pl_ui_checkbox    (const char* pcText, bool* pbValue);
bool   pl_ui_radio_button(const char* pcText, int* piValue, int iButtonValue);
void   pl_ui_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay);
void   pl_ui_image       (plTextureId tTexture, plVec2 tSize);
void   pl_ui_image_ex    (plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);

// text widgets
void   pl_ui_text          (const char* pcFmt, ...);
void   pl_ui_text_v        (const char* pcFmt, va_list args);
void   pl_ui_color_text    (plVec4 tColor, const char* pcFmt, ...);
void   pl_ui_color_text_v  (plVec4 tColor, const char* pcFmt, va_list args);
void   pl_ui_labeled_text  (const char* pcLabel, const char* pcFmt, ...);
void   pl_ui_labeled_text_v(const char* pcLabel, const char* pcFmt, va_list args);

// sliders
bool   pl_ui_slider_float  (const char* pcLabel, float* pfValue, float fMin, float fMax);
bool   pl_ui_slider_float_f(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);
bool   pl_ui_slider_int    (const char* pcLabel, int* piValue, int iMin, int iMax);
bool   pl_ui_slider_int_f  (const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat);

// drags
bool   pl_ui_drag_float  (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax);
bool   pl_ui_drag_float_f(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat);

// trees
bool   pl_ui_collapsing_header(const char* pcText);
bool   pl_ui_tree_node        (const char* pcText);
bool   pl_ui_tree_node_f      (const char* pcFmt, ...);
bool   pl_ui_tree_node_v      (const char* pcFmt, va_list args);
void   pl_ui_tree_pop         (void);

// tabs
bool   pl_ui_begin_tab_bar(const char* pcText);
void   pl_ui_end_tab_bar  (void);
bool   pl_ui_begin_tab    (const char* pcText);
void   pl_ui_end_tab      (void);

// layout
void   pl_ui_separator       (void);
void   pl_ui_same_line       (float fOffsetFromStart, float fSpacing);
void   pl_ui_next_line       (void);
void   pl_ui_align_text      (void);
void   pl_ui_vertical_spacing(void);
void   pl_ui_indent          (float fIndent);
void   pl_ui_unindent        (float fIndent);

// state query
bool   pl_ui_was_last_item_hovered(void);
bool   pl_ui_was_last_item_active (void);

// styling
void   pl_ui_set_dark_theme(plUiContext* ptCtx);

// storage
int    pl_ui_get_int      (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float  pl_ui_get_float    (plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool   pl_ui_get_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void*  pl_ui_get_ptr      (plUiStorage* ptStorage, uint32_t uKey);

int*   pl_ui_get_int_ptr  (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float* pl_ui_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool*  pl_ui_get_bool_ptr (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void** pl_ui_get_ptr_ptr  (plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue);

void   pl_ui_set_int      (plUiStorage* ptStorage, uint32_t uKey, int iValue);
void   pl_ui_set_float    (plUiStorage* ptStorage, uint32_t uKey, float fValue);
void   pl_ui_set_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bValue);
void   pl_ui_set_ptr      (plUiStorage* ptStorage, uint32_t uKey, void* pValue);

// tools
void   pl_ui_debug        (bool* pbOpen);
void   pl_ui_style        (bool* pbOpen);

//-----------------------------------------------------------------------------
// [SECTION] experimental api
//-----------------------------------------------------------------------------

// fancy sliders
bool pl_ui_ex_slider_float  (const char* pcLabel, float* pfValue, float fMin, float fMax);
bool pl_ui_ex_slider_float_f(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plUiConditionFlags_
{
    PL_UI_COND_NONE   = 0,
    PL_UI_COND_ALWAYS = 1 << 0,
    PL_UI_COND_ONCE   = 1 << 1
};

enum plUiAxis_
{
    PL_UI_AXIS_NONE = -1,
    PL_UI_AXIS_X    =  0,
    PL_UI_AXIS_Y    =  1,
};

enum plUiWindowFlags_
{
    PL_UI_WINDOW_FLAGS_NONE         = 0,
    PL_UI_WINDOW_FLAGS_NO_TITLE_BAR = 1 << 0,
    PL_UI_WINDOW_FLAGS_NO_RESIZE    = 1 << 1,
    PL_UI_WINDOW_FLAGS_NO_MOVE      = 1 << 2,
    PL_UI_WINDOW_FLAGS_NO_COLLAPSE  = 1 << 3,
    PL_UI_WINDOW_FLAGS_AUTO_SIZE    = 1 << 4,
    PL_UI_WINDOW_FLAGS_CHILD_WINDOW = 1 << 5,
    PL_UI_WINDOW_FLAGS_TOOLTIP      = 1 << 6,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
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

    // colors
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
} plUiStyle;

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

typedef struct _plUiPrevItemData
{
    bool bHovered;
    bool bActive;
} plUiPrevItemData;

typedef struct _plUiTabBar
{
    uint32_t    uId;
    plVec2      tStartPos;
    plVec2      tCursorPos;
    uint32_t    uCurrentIndex;
    uint32_t    uValue;
    uint32_t    uNextValue;
} plUiTabBar;

typedef struct _plUiTempWindowData
{
    plVec2       tCursorPos;
    plVec2       tCursorStartPos;
    plVec2       tCursorMaxPos;
    uint32_t     uTreeDepth;
    plVec2       tLastLineSize;
    plVec2       tCurrentLineSize;
    float        fExtraIndent;
    bool         bChildHovered;
    plUiWindow** sbtChildWindows;
} plUiTempWindowData;

typedef struct _plUiWindow
{
    uint32_t           uId;
    const char*        pcName;
    plUiWindowFlags    tFlags;
    plVec2             tPos;
    plVec2             tContentSize; // size of contents/scrollable client area
    plVec2             tMinSize;
    plVec2             tMaxSize;
    plVec2             tSize;        // full size or title bar size if collapsed
    plVec2             tFullSize;    // used to restore size after uncollapsing
    plVec2             tScroll;
    plVec2             tScrollMax;
    plRect             tInnerRect;
    plRect             tOuterRect;
    plRect             tOuterRectClipped;
    plRect             tInnerClipRect;
    float              fTextVerticalOffset;
    plUiWindow*        ptParentWindow;
    bool               bVisible;
    bool               bActive; // referred to in current frame
    bool               bCollapsed;
    bool               bScrollbarX;
    bool               bScrollbarY;
    bool               bChildAutoSizeX;
    bool               bChildAutoSizeY;
    plUiTempWindowData tTempData;
    plDrawLayer*       ptBgLayer;
    plDrawLayer*       ptFgLayer;
    plUiConditionFlags tPosAllowableFlags;
    plUiConditionFlags tSizeAllowableFlags;
    plUiConditionFlags tCollapseAllowableFlags;
    uint8_t            uHideFrames;
    uint32_t           uFocusOrder;
    plUiStorage        tStorage;
} plUiWindow;

typedef struct _plUiContext
{
    plUiStyle tStyle;

    // prev/next state
    plUiNextWindowData tNextWindowData;
    plUiPrevItemData   tPrevItemData;
    uint32_t*          sbuIdStack;

    // state
    uint32_t uHoveredId;
    uint32_t uNextHoveredId;
    uint32_t uActiveId;
    uint32_t uNextActiveId;
    uint32_t uActiveWindowId;
    uint32_t uHoveredWindowId;
    bool     bMouseOwned;
    bool     bWantMouse;
    bool     bWantMouseNextFrame;
    bool     bActiveIdJustActivated;

    // windows
    plUiWindow   tTooltipWindow;
    plUiWindow** sbptWindows;
    plUiWindow** sbtFocusedWindows;
    plUiWindow** sbtSortingWindows;
    plUiWindow*  ptCurrentWindow;
    plUiWindow*  ptHoveredWindow;
    plUiWindow*  ptMovingWindow;
    plUiWindow*  ptSizingWindow;
    plUiWindow*  ptScrollingWindow;
    plUiWindow*  ptWheelingWindow;
    plUiWindow*  ptActiveWindow;
    plUiStorage  tWindows; // windows by ID

    // tabs
    plUiTabBar* sbtTabBars;
    plUiTabBar* ptCurrentTabBar;

    // drawing
    plDrawList*  ptDrawlist;
    plDrawList*  ptDebugDrawlist;
    plFont*      ptFont;
    plDrawLayer* ptBgLayer;
    plDrawLayer* ptFgLayer;
    plDrawLayer* ptDebugLayer;

} plUiContext;

#endif // PL_UI_H
