/*
   pl_ui_internal.h
   - FORWARD COMPATIBILITY NOT GUARANTEED
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] context
// [SECTION] enums
// [SECTION] internal structs
// [SECTION] plUiStorage
// [SECTION] plUiWindow
// [SECTION] plUiContext
// [SECTION] internal api (exposed)
// [SECTION] internal api (not exposed)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_INTERNAL_H
#define PL_UI_INTERNAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ui_ext.h"
#include "pl_string.h"
#include "pl_ds.h"
#include "pl_io.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plUiStyle          plUiStyle;
typedef struct _plUiWindow         plUiWindow;
typedef struct _plUiTabBar         plUiTabBar;
typedef struct _plUiPrevItemData   plUiPrevItemData;
typedef struct _plUiNextWindowData plUiNextWindowData;
typedef struct _plUiTempWindowData plUiTempWindowData;
typedef struct _plUiStorage        plUiStorage;
typedef struct _plUiStorageEntry   plUiStorageEntry;

// enums
typedef int plUiNextWindowFlags;
typedef int plUiWindowFlags;
typedef int plUiAxis;
typedef int plUiLayoutRowEntryType;
typedef int plUiLayoutSystemType;

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

extern plUiContext* gptCtx;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

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
    const float*         pfSizesOrRatios;      // size or rations when using array layout system (system 4)
    uint32_t             uStaticEntryCount;    // number of static entries when using template layout system (system 5)
    uint32_t             uDynamicEntryCount;   // number of dynamic entries when using template layout system (system 5)
    uint32_t             uVariableEntryCount;  // number of variable entries when using template layout system (system 5)
    uint32_t             uEntryStartIndex;     // offset into parent window sbtRowTemplateEntries buffer
} plUiLayoutRow;

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
    float                fExtraIndent;      // extra indent added by pl_ui_indent
    plUiLayoutRow        tCurrentLayoutRow; // current layout row to use
    plVec2               tRowPos;           // current row starting position
} plUiTempWindowData;

typedef struct _plUiWindow
{
    const char*          pcName;
    uint32_t             uId;                     // window Id (=pl_str_hash(pcName))
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
    plUiLayoutRowEntry*  sbtRowTemplateEntries;   // row template entries (shared and reset every frame)            
    plDrawLayer*         ptBgLayer;               // background draw layer
    plDrawLayer*         ptFgLayer;               // foreground draw layer
    plUiConditionFlags   tPosAllowableFlags;      // acceptable condition flags for "pl_ui_set_next_window_pos()"
    plUiConditionFlags   tSizeAllowableFlags;     // acceptable condition flags for "pl_ui_set_next_window_size()"
    plUiConditionFlags   tCollapseAllowableFlags; // acceptable condition flags for "pl_ui_set_next_window_collapse()"
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
    plUiStyle tStyle;
    plIOApiI* ptIo;
    plDrawApiI* ptDraw;

    // prev/next state
    plUiNextWindowData tNextWindowData;        // info based on pl_ui_set_next_window_* functions
    plUiPrevItemData   tPrevItemData;          // data for use by pl_ui_was_last_item_* functions
    uint32_t*          sbuIdStack;             // id stack for hashing IDs, container items usually push/pop these

    // widget state
    uint32_t           uHoveredId;             // set at the end of the previous frame from uNextHoveredId
    uint32_t           uActiveId;              // set at the end of the previous frame from uNextActiveId
    uint32_t           uNextHoveredId;         // set during current frame (by end of frame, should be last item hovered)
    uint32_t           uNextActiveId;          // set during current frame (by end of frame, should be last item active)


    uint32_t           uActiveWindowId;        // current active window id
    bool               bActiveIdJustActivated; // window was just activated, so bring it to the front
    bool               bMouseOwned;            // mouse is owned by us, apps should not dispatch mouse events
    bool               bWantMouse;             // we want the mouse
    bool               bWantMouseNextFrame;    // wen want the mouse next frame
    
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

    // drawing
    plDrawContext*     ptDrawCtx;              // pointer to draw context
    plDrawList*        ptDrawlist;             // main ui drawlist
    plDrawList*        ptDebugDrawlist;        // ui debug drawlist (i.e. overlays)
    plFont*            ptFont;                 // current font
    plDrawLayer*       ptBgLayer;              // submitted before window layers
    plDrawLayer*       ptFgLayer;              // submitted after window layers
    plDrawLayer*       ptDebugLayer;           // submitted last

} plUiContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api (exposed)
//-----------------------------------------------------------------------------

plUiContext*   pl_ui_create_context               (plIOApiI* ptIoI, plDrawApiI* ptDraw);
void           pl_ui_destroy_context              (plUiContext* ptContext);
void           pl_ui_set_context                  (plUiContext* ptCtx);
plUiContext*   pl_ui_get_context                  (void);
void           pl_ui_set_draw_api                 (plDrawApiI* ptDraw);
void           pl_ui_set_io_api                   (plIOApiI* ptIO);
plDrawContext* pl_ui_get_draw_context             (plUiContext* ptContext);
plDrawList*    pl_ui_get_draw_list                (plUiContext* ptContext);
plDrawList*    pl_ui_get_debug_draw_list          (plUiContext* ptContext);
void           pl_ui_new_frame                    (void);
void           pl_ui_end_frame                    (void);
void           pl_ui_render                       (void);
void           pl_ui_debug                        (bool* pbOpen);
void           pl_ui_style                        (bool* pbOpen);
void           pl_ui_demo                         (bool* pbOpen);
void           pl_ui_set_dark_theme               (void);
void           pl_ui_set_default_font             (plFont* ptFont);
bool           pl_ui_begin_window                 (const char* pcName, bool* pbOpen, bool bAutoSize);
void           pl_ui_end_window                   (void);
plDrawLayer*   pl_ui_get_window_fg_drawlayer      (void);
plDrawLayer*   pl_ui_get_window_bg_drawlayer      (void);
plVec2         pl_ui_get_cursor_pos               (void);
bool           pl_ui_begin_child                  (const char* pcName);
void           pl_ui_end_child                    (void);
void           pl_ui_begin_tooltip                (void);
void           pl_ui_end_tooltip                  (void);
plVec2         pl_ui_get_window_pos               (void);
plVec2         pl_ui_get_window_size              (void);
void           pl_ui_set_next_window_pos          (plVec2 tPos, plUiConditionFlags tCondition);
void           pl_ui_set_next_window_size         (plVec2 tSize, plUiConditionFlags tCondition);
void           pl_ui_set_next_window_collapse     (bool bCollapsed, plUiConditionFlags tCondition);
bool           pl_ui_button                       (const char* pcText);
bool           pl_ui_selectable                   (const char* pcText, bool* bpValue);
bool           pl_ui_checkbox                     (const char* pcText, bool* pbValue);
bool           pl_ui_radio_button                 (const char* pcText, int* piValue, int iButtonValue);
void           pl_ui_image                        (plTextureId tTexture, plVec2 tSize);
void           pl_ui_image_ex                     (plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);
bool           pl_ui_invisible_button             (const char* pcText, plVec2 tSize);
void           pl_ui_dummy                        (plVec2 tSize);
void           pl_ui_progress_bar                 (float fFraction, plVec2 tSize, const char* pcOverlay);
void           pl_ui_text                         (const char* pcFmt, ...);
void           pl_ui_text_v                       (const char* pcFmt, va_list args);
void           pl_ui_color_text                   (plVec4 tColor, const char* pcFmt, ...);
void           pl_ui_color_text_v                 (plVec4 tColor, const char* pcFmt, va_list args);
void           pl_ui_labeled_text                 (const char* pcLabel, const char* pcFmt, ...);
void           pl_ui_labeled_text_v               (const char* pcLabel, const char* pcFmt, va_list args);
bool           pl_ui_slider_float                 (const char* pcLabel, float* pfValue, float fMin, float fMax);
bool           pl_ui_slider_float_f               (const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);
bool           pl_ui_slider_int                   (const char* pcLabel, int* piValue, int iMin, int iMax);
bool           pl_ui_slider_int_f                 (const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat);
bool           pl_ui_drag_float                   (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax);
bool           pl_ui_drag_float_f                 (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat);
bool           pl_ui_collapsing_header            (const char* pcText);
void           pl_ui_end_collapsing_header        (void);
bool           pl_ui_tree_node                    (const char* pcText);
bool           pl_ui_tree_node_f                  (const char* pcFmt, ...);
bool           pl_ui_tree_node_v                  (const char* pcFmt, va_list args);
void           pl_ui_tree_pop                     (void);
bool           pl_ui_begin_tab_bar                (const char* pcText);
void           pl_ui_end_tab_bar                  (void);
bool           pl_ui_begin_tab                    (const char* pcText);
void           pl_ui_end_tab                      (void);
void           pl_ui_separator                    (void);
void           pl_ui_vertical_spacing             (void);
void           pl_ui_indent                       (float fIndent);
void           pl_ui_unindent                     (float fIndent);
void           pl_ui_layout_dynamic               (float fHeight, uint32_t uWidgetCount);
void           pl_ui_layout_static                (float fHeight, float fWidth, uint32_t uWidgetCount);
void           pl_ui_layout_row_begin             (plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
void           pl_ui_layout_row_push              (float fWidth);
void           pl_ui_layout_row_end               (void);
void           pl_ui_layout_row                   (plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios);
void           pl_ui_layout_template_begin        (float fHeight);
void           pl_ui_layout_template_push_dynamic (void);
void           pl_ui_layout_template_push_variable(float fWidth);
void           pl_ui_layout_template_push_static  (float fWidth);
void           pl_ui_layout_template_end          (void);
void           pl_ui_layout_space_begin           (plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
void           pl_ui_layout_space_push            (float fX, float fY, float fWidth, float fHeight);
void           pl_ui_layout_space_end             (void);
bool           pl_ui_was_last_item_hovered        (void);
bool           pl_ui_was_last_item_active         (void);
bool           pl_ui_is_mouse_owned               (void);

//-----------------------------------------------------------------------------
// [SECTION] internal api (not exposed)
//-----------------------------------------------------------------------------

const char*          pl_ui_find_renderered_text_end(const char* pcText, const char* pcTextEnd);
void                 pl_ui_add_text                (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
void                 pl_ui_add_clipped_text        (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
plVec2               pl_ui_calculate_text_size     (plFont* font, float size, const char* text, float wrap);
static inline float  pl_ui_get_frame_height        (void) { return gptCtx->tStyle.fFontSize + gptCtx->tStyle.tFramePadding.y * 2.0f; }

// collision
static inline bool   pl_ui_does_circle_contain_point  (plVec2 cen, float radius, plVec2 point) { const float fDistanceSquared = powf(point.x - cen.x, 2) + powf(point.y - cen.y, 2); return fDistanceSquared <= radius * radius; }
bool                 pl_ui_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point);
bool                 pl_ui_is_item_hoverable          (const plRect* ptBox, uint32_t uHash);

// layouts
static inline plVec2 pl__ui_get_cursor_pos    (void) { return (plVec2){gptCtx->ptCurrentWindow->tTempData.tRowPos.x + gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.fHorizontalOffset + (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize, gptCtx->ptCurrentWindow->tTempData.tRowPos.y + gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.fVerticalOffset};}
plVec2               pl_ui_calculate_item_size(float fDefaultHeight);
void                 pl_ui_advance_cursor     (float fWidth, float fHeight);

// misc
bool                 pl_ui_begin_window_ex (const char* pcName, bool* pbOpen, plUiWindowFlags tFlags);
void                 pl_ui_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis);
void                 pl_ui_submit_window   (plUiWindow* ptWindow);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widget behavior~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool                 pl_ui_button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~storage system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

plUiStorageEntry*    pl_ui_lower_bound(plUiStorageEntry* sbtData, uint32_t uKey);

int                  pl_ui_get_int      (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float                pl_ui_get_float    (plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool                 pl_ui_get_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void*                pl_ui_get_ptr      (plUiStorage* ptStorage, uint32_t uKey);

int*                 pl_ui_get_int_ptr  (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float*               pl_ui_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool*                pl_ui_get_bool_ptr (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void**               pl_ui_get_ptr_ptr  (plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue);

void                 pl_ui_set_int      (plUiStorage* ptStorage, uint32_t uKey, int iValue);
void                 pl_ui_set_float    (plUiStorage* ptStorage, uint32_t uKey, float fValue);
void                 pl_ui_set_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bValue);
void                 pl_ui_set_ptr      (plUiStorage* ptStorage, uint32_t uKey, void* pValue);

#endif // PL_UI_INTERNAL_H