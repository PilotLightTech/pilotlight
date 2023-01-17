/*
   pl_ui.h, v0.3
*/

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
#include "pl_math.inc"

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

// enums
typedef int plUiConditionFlags;
typedef int plUiNextWindowFlags;

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
void pl_ui_setup_context  (plDrawContext* ptDrawCtx, plUiContext* ptCtx);
void pl_ui_cleanup_context(plUiContext* ptCtx);

// main
void pl_ui_new_frame(plUiContext* ptCtx);
void pl_ui_end_frame(void);
void pl_ui_render   (void);

// windows
bool pl_ui_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize);
void pl_ui_end_window  (void);

// tooltips
void pl_ui_begin_tooltip(void);
void pl_ui_end_tooltip  (void);

// window utils
void pl_ui_set_next_window_pos     (plVec2 tPos, plUiConditionFlags tCondition);
void pl_ui_set_next_window_size    (plVec2 tSize, plUiConditionFlags tCondition);
void pl_ui_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition);

// widgets
bool pl_ui_button      (const char* pcText);
bool pl_ui_selectable  (const char* pcText, bool* bpValue);
bool pl_ui_checkbox    (const char* pcText, bool* pbValue);
bool pl_ui_radio_button(const char* pcText, int* piValue, int iButtonValue);
void pl_ui_text        (const char* pcFmt, ...);
void pl_ui_text_v      (const char* pcFmt, va_list args);
void pl_ui_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay);
void pl_ui_image       (plTextureId tTexture, plVec2 tSize);
void pl_ui_image_ex    (plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);

// trees
bool pl_ui_collapsing_header(const char* pcText, bool* pbOpenState);
bool pl_ui_tree_node        (const char* pcText, bool* pbOpenState);
void pl_ui_tree_pop         (void);

// tabs
bool pl_ui_begin_tab_bar(const char* pcText);
void pl_ui_end_tab_bar  (void);
bool pl_ui_begin_tab    (const char* pcText);
void pl_ui_end_tab      (void);

// layout
void pl_ui_same_line(float fOffsetFromStart, float fSpacing);
void pl_ui_align_text(void);
void pl_ui_vertical_spacing(void);

// state query
bool pl_ui_was_last_item_hovered(void);
bool pl_ui_was_last_item_active (void);

// styling
void pl_ui_set_dark_theme(plUiContext* ptCtx);

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plUiConditionFlags_
{
    PL_UI_COND_NONE   = 0,
    PL_UI_COND_ALWAYS = 1 << 0,
    PL_UI_COND_ONCE   = 1 << 1
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plUiStyle
{
    // style
    float  fTitlePadding;
    float  fFontSize;
    float  fIndentSize;
    float  fWindowHorizontalPadding;
    float  fWindowVerticalPadding;
    plVec2 tItemSpacing;
    plVec2 tInnerSpacing;
    plVec2 tFramePadding;


    // colors
    plVec4 tTitleActiveCol;
    plVec4 tTitleBgCol;
    plVec4 tWindowBgColor;
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

} plUiTempWindowData;

typedef struct _plUiWindow
{
    uint32_t           uId;
    const char*        pcName;
    plVec2             tPos;
    plVec2             tContentSize; // size of contents/scrollable client area
    plVec2             tMinSize;
    plVec2             tSize;        // full size or title bar size if collapsed
    plVec2             tFullSize;
    plVec2             tScroll;
    plVec2             tScrollMax;
    float              fTextVerticalOffset;
    plUiWindow*        ptParentWindow;
    bool               bHovered;
    bool               bActive;
    bool               bDragging;
    bool               bResizing;
    bool               bAutoSize;
    bool               bCollapsed;
    uint64_t           ulFrameActivated;
    uint64_t           ulFrameHovered;
    plUiTempWindowData tTempData;
    plDrawLayer*       ptBgLayer;
    plDrawLayer*       ptFgLayer;
    plUiConditionFlags tPosAllowableFlags;
    plUiConditionFlags tSizeAllowableFlags;
    plUiConditionFlags tCollapseAllowableFlags;
} plUiWindow;

typedef struct _plUiContext
{
    plUiStyle tStyle;

    // prev/next state
    plUiNextWindowData tNextWindowData;
    plUiPrevItemData   tPrevItemData;
    uint32_t*          sbuIdStack;

    // state
    uint32_t uToggledId;
    uint32_t uNextToggleId;
    uint32_t uHoveredId;
    uint32_t uNextHoveredId;
    uint32_t uActiveId;
    uint32_t uNextActiveId;
    uint32_t uActiveWindowId;
    uint32_t uNextActiveWindowId;
    uint32_t uHoveredWindowId;
    uint32_t uNextHoveredWindowId;
    bool     bMouseOwned;
    bool     bWantMouse;
    bool     bWantMouseNextFrame;

    // windows
    plUiWindow   tTooltipWindow;
    plUiWindow*  sbtWindows;
    plUiWindow** sbtFocusedWindows;
    plUiWindow** sbtSortingWindows;
    plUiWindow*  ptCurrentWindow;
    plUiWindow*  ptHoveredWindow;
    plUiWindow*  ptMovingWindow;
    plUiWindow*  ptActiveWindow;
    plUiWindow*  ptFocusedWindow;

    // tabs
    plUiTabBar* sbtTabBars;
    plUiTabBar* ptCurrentTabBar;

    // drawing
    plDrawList*  ptDrawlist;
    plFont*      ptFont;
    plDrawLayer* ptBgLayer;
    plDrawLayer* ptFgLayer;
} plUiContext;

#endif // PL_UI_H
