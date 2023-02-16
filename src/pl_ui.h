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
#include "pl_draw.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
PL_DECLARE_STRUCT(plUiContext);        // internal
PL_DECLARE_STRUCT(plUiStyle);          // internal
PL_DECLARE_STRUCT(plUiWindow);         // internal
PL_DECLARE_STRUCT(plUiTabBar);         // internal
PL_DECLARE_STRUCT(plUiPrevItemData);   // internal
PL_DECLARE_STRUCT(plUiNextWindowData); // internal
PL_DECLARE_STRUCT(plUiTempWindowData); // internal
PL_DECLARE_STRUCT(plUiStorage);        // internal
PL_DECLARE_STRUCT(plUiStorageEntry);   // internal

// enums
typedef int plUiConditionFlags;
typedef int plUiLayoutRowType;
typedef int plUiNextWindowFlags;    // internal
typedef int plUiWindowFlags;        // internal
typedef int plUiAxis;               // internal
typedef int plUiLayoutRowEntryType; // internal
typedef int plUiLayoutSystemType;   // internal

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context creation & access
void         pl_ui_setup_context  (plUiContext* ptCtx);
void         pl_ui_cleanup_context(void);
void         pl_ui_set_context    (plUiContext* ptCtx); // must be set when crossing DLL boundary
plUiContext* pl_ui_get_context    (void);

// main
void         pl_ui_new_frame(void); // start a new pilotlight ui frame, this should be the first command before calling any commands below
void         pl_ui_end_frame(void); // ends pilotlight ui frame, automatically called by pl_ui_render()
void         pl_ui_render   (void); // submits draw layers, you can then submit the ptDrawlist & ptDebugDrawlist from context

// tools
void         pl_ui_debug(bool* pbOpen);
void         pl_ui_style(bool* pbOpen);
void         pl_ui_demo(bool* pbOpen);

// styling
void         pl_ui_set_dark_theme(void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~windows~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// windows
// - only call "pl_ui_end_window()" if "pl_ui_begin_window()" returns true (its call automatically if false)
// - passing a valid pointer to pbOpen will show a red circle that will turn pbOpen false when clicked
// - "pl_ui_end_window()" will return false if collapsed or clipped
// - if you use autosize, make sure at least 1 row has a static component (or the window will grow unbounded)
bool         pl_ui_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize);
void         pl_ui_end_window  (void);

// child windows
// - only call "pl_ui_end_child()" if "pl_ui_begin_child()" returns true (its call automatically if false)
// - self-contained window with scrolling & clipping
bool         pl_ui_begin_child(const char* pcName);
void         pl_ui_end_child  (void);

// tooltips
// - window that follows the mouse (usually used in combination with "pl_ui_was_last_item_hovered()")
void         pl_ui_begin_tooltip(void);
void         pl_ui_end_tooltip  (void);

// window utilities
// - refers to current window (between "pl_ui_begin_window()" & "pl_ui_end_window()")
plVec2       pl_ui_get_window_pos (void);
plVec2       pl_ui_get_window_size(void);

// window manipulation
// - call before "pl_ui_begin_window()"
void         pl_ui_set_next_window_pos     (plVec2 tPos, plUiConditionFlags tCondition);
void         pl_ui_set_next_window_size    (plVec2 tSize, plUiConditionFlags tCondition);
void         pl_ui_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widgets~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// main
bool         pl_ui_button      (const char* pcText);
bool         pl_ui_selectable  (const char* pcText, bool* bpValue);
bool         pl_ui_checkbox    (const char* pcText, bool* pbValue);
bool         pl_ui_radio_button(const char* pcText, int* piValue, int iButtonValue);
void         pl_ui_image       (plTextureId tTexture, plVec2 tSize);
void         pl_ui_image_ex    (plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);

// plotting
void         pl_ui_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay);

// text
void         pl_ui_text          (const char* pcFmt, ...);
void         pl_ui_text_v        (const char* pcFmt, va_list args);
void         pl_ui_color_text    (plVec4 tColor, const char* pcFmt, ...);
void         pl_ui_color_text_v  (plVec4 tColor, const char* pcFmt, va_list args);
void         pl_ui_labeled_text  (const char* pcLabel, const char* pcFmt, ...);
void         pl_ui_labeled_text_v(const char* pcLabel, const char* pcFmt, va_list args);

// sliders
bool         pl_ui_slider_float  (const char* pcLabel, float* pfValue, float fMin, float fMax);
bool         pl_ui_slider_float_f(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);
bool         pl_ui_slider_int    (const char* pcLabel, int* piValue, int iMin, int iMax);
bool         pl_ui_slider_int_f  (const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat);

// drag sliders
bool         pl_ui_drag_float  (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax);
bool         pl_ui_drag_float_f(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat);

// trees
// - only call "pl_ui_tree_pop()" if "pl_ui_tree_node()" returns true (its call automatically if false)
// - only call "pl_ui_end_collapsing_header()" if "pl_ui_collapsing_header()" returns true (its call automatically if false)
bool         pl_ui_collapsing_header    (const char* pcText);
void         pl_ui_end_collapsing_header(void);
bool         pl_ui_tree_node            (const char* pcText);
bool         pl_ui_tree_node_f          (const char* pcFmt, ...);
bool         pl_ui_tree_node_v          (const char* pcFmt, va_list args);
void         pl_ui_tree_pop             (void);

// tabs & tab bars
// - only call "pl_ui_end_tab_bar()" if "pl_ui_begin_tab_bar()" returns true (its call automatically if false)
// - only call "pl_ui_end_tab()" if "pl_ui_begin_tab()" returns true (its call automatically if false)
bool         pl_ui_begin_tab_bar(const char* pcText);
void         pl_ui_end_tab_bar  (void);
bool         pl_ui_begin_tab    (const char* pcText);
void         pl_ui_end_tab      (void);

// misc.
void         pl_ui_separator       (void);
void         pl_ui_vertical_spacing(void);
void         pl_ui_indent          (float fIndent);
void         pl_ui_unindent        (float fIndent);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~layout systems~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// - layout systems are based on "Nuklear" (https://github.com/Immediate-Mode-UI/Nuklear)
// - 6 different layout strategies
// - default layout strategy is system 2 with uWidgetCount=1 & fWidth=300 & fHeight=0
// - setting fHeight=0 will cause the row height to be equal to the minimal height of the maximum height widget

// layout system 1
// - provides each widget with the same horizontal space and grows dynamically with the parent window
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void         pl_ui_layout_dynamic(float fHeight, uint32_t uWidgetCount);

// layout system 2
// - provides each widget with the same horizontal pixel widget and does not grow with the parent window
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void         pl_ui_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount);

// layout system 3
// - allows user to change the with per widget
// - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth is pixel width
// - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width
// - does not wrap
void         pl_ui_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
void         pl_ui_layout_row_push (float fWidth);
void         pl_ui_layout_row_end  (void);

// layout system 4
// - same as layout system 3 but the "array" form
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void         pl_ui_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios);

// layout system 5
// - similar to a flexbox
// - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
void         pl_ui_layout_template_begin        (float fHeight);
void         pl_ui_layout_template_push_dynamic (void);         // can go to minimum widget width if not enough space (10 pixels)
void         pl_ui_layout_template_push_variable(float fWidth); // variable width with min pixel width of fWidth but can grow bigger if enough space
void         pl_ui_layout_template_push_static  (float fWidth); // static pixel width of fWidth
void         pl_ui_layout_template_end          (void);

// layout system 6
// - allows user to place widgets freely
// - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth/fHeight is pixel width/height
// - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth/fHeight is a ratio of the available width/height (for pl_ui_layout_space_begin())
// - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width & fHeight is a ratio of fHeight given to "pl_ui_layout_space_begin()" (for pl_ui_layout_space_push())
void        pl_ui_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
void        pl_ui_layout_space_push (float fX, float fY, float fWidth, float fHeight);
void        pl_ui_layout_space_end  (void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~state query~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool        pl_ui_was_last_item_hovered(void);
bool        pl_ui_was_last_item_active (void);

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plUiConditionFlags_
{
    PL_UI_COND_NONE   = 0,
    PL_UI_COND_ALWAYS = 1 << 0,
    PL_UI_COND_ONCE   = 1 << 1
};

enum plUiLayoutRowType_
{
    PL_UI_LAYOUT_ROW_TYPE_NONE, // don't use (internal only)
    PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
    PL_UI_LAYOUT_ROW_TYPE_STATIC
};

//-----------------------------------------------------------------------------
// [SECTION] structs (do not modify directly, will be moved internally later)
//-----------------------------------------------------------------------------

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

typedef struct _plUiLayoutRowEntry
{
    plUiLayoutRowEntryType tType;
    float                  fWidth;
} plUiLayoutRowEntry;

typedef struct _plUiLayoutRow
{
    plUiLayoutRowType    tType;
    plUiLayoutSystemType tSystemType;
    plVec2               tRowPos;
    float                fWidth;
    float                fHeight;
    float                fSpecifiedHeight;
    float                fVerticalOffset;
    float                fHorizontalOffset;
    uint32_t             uColumns;
    uint32_t             uCurrentColumn;
    float                fMaxWidth;
    float                fMaxHeight;
    const float*         pfSizesOrRatios;
    uint32_t             uStaticEntryCount;
    uint32_t             uDynamicEntryCount;
    uint32_t             uVariableEntryCount;
    plUiLayoutRowEntry   atEntries[64];
} plUiLayoutRow;

typedef struct _plUiTempWindowData
{
    plVec2               tCursorStartPos;
    plVec2               tCursorMaxPos;
    uint32_t             uTreeDepth;
    float                fExtraIndent;
    bool                 bChildHovered;
    plUiLayoutRow        tCurrentLayoutRow;
    plVec2               tNextRowStartPos;
    float                fMaxContentWidth;
    float                fMaxContentHeight;
} plUiTempWindowData;

typedef struct _plUiWindow
{
    uint32_t             uId;
    const char*          pcName;
    plUiWindowFlags      tFlags;
    plVec2               tPos;
    plVec2               tContentSize; // size of contents/scrollable client area
    plVec2               tMinSize;
    plVec2               tMaxSize;
    plVec2               tSize;        // full size or title bar size if collapsed
    plVec2               tFullSize;    // used to restore size after uncollapsing
    plVec2               tScroll;
    plVec2               tScrollMax;
    plRect               tInnerRect;
    plRect               tOuterRect;
    plRect               tOuterRectClipped;
    plRect               tInnerClipRect;
    plUiWindow*          ptParentWindow;
    plUiWindow**         sbtChildWindows;
    plUiLayoutSystemType tLayoutSystemType;
    plUiLayoutRow*       sbtRowStack;
    bool                 bVisible;
    bool                 bActive; // referred to in current frame
    bool                 bCollapsed;
    bool                 bScrollbarX;
    bool                 bScrollbarY;
    plUiTempWindowData   tTempData;
    plDrawLayer*         ptBgLayer;
    plDrawLayer*         ptFgLayer;
    plUiConditionFlags   tPosAllowableFlags;
    plUiConditionFlags   tSizeAllowableFlags;
    plUiConditionFlags   tCollapseAllowableFlags;
    uint8_t              uHideFrames;
    uint32_t             uFocusOrder;
    plUiStorage          tStorage;
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
    plDrawContext* ptDrawCtx;
    plDrawList*    ptDrawlist;
    plDrawList*    ptDebugDrawlist;
    plFont*        ptFont;
    plDrawLayer*   ptBgLayer;
    plDrawLayer*   ptFgLayer;
    plDrawLayer*   ptDebugLayer;

} plUiContext;

#endif // PL_UI_H
