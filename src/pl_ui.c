/*
   pl_ui.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] context
// [SECTION] internal enums
// [SECTION] internal structs
// [SECTION] internal functions
// [SECTION] implementations
// [SECTION] internal implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_ui.h"
#include "pl_ds.h"
#include "pl_io.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_string.h"
#include "pl_draw.h"

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

static plUiContext* gptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal enums
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

typedef struct _plUiTabBar
{
    uint32_t    uId;
    plVec2      tStartPos;
    plVec2      tCursorPos;
    uint32_t    uCurrentIndex;
    uint32_t    uValue;
    uint32_t    uNextValue;
} plUiTabBar;

//-----------------------------------------------------------------------------
// [SECTION] internal functions
//-----------------------------------------------------------------------------

static const char*  pl__find_renderered_text_end             (const char* pcText, const char* pcTextEnd);
static void         pl__add_text                             (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
static void         pl__add_clipped_text                     (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
static plVec2       pl__calculate_text_size                  (plFont* font, float size, const char* text, float wrap);
static bool         pl__ui_button_behavior                   (const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld);
static inline float pl__ui_get_frame_height                  (void) { return gptCtx->tStyle.fFontSize + gptCtx->tStyle.tFramePadding.y * 2.0f; }

// collision
static inline bool  pl__ui_does_circle_contain_point         (plVec2 cen, float radius, plVec2 point) { const float fDistanceSquared = powf(point.x - cen.x, 2) + powf(point.y - cen.y, 2); return fDistanceSquared <= radius * radius; }
static bool         pl__ui_does_triangle_contain_point       (plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point);
static bool         pl__ui_is_item_hoverable                 (const plRect* ptBox, uint32_t uHash);

// storage
static plUiStorageEntry* pl__lower_bound(plUiStorageEntry* sbtData, uint32_t uKey);

// layouts
static inline plVec2 pl__ui_get_start_pos(const plUiLayoutRow* ptCurrentRow) { return (plVec2){ptCurrentRow->tRowPos.x + ptCurrentRow->fHorizontalOffset + (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize, ptCurrentRow->tRowPos.y + ptCurrentRow->fVerticalOffset};}
static plVec2        pl__ui_calculate_item_size(float fDefaultHeight);
static void          pl__ui_advance_cursor(float fWidth, float fHeight);

// misc
static bool   pl_ui_begin_window_ex     (const char* pcName, bool* pbOpen, plUiWindowFlags tFlags);
static void   pl__ui_render_scrollbar   (plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis);
static void   pl__ui_submit_window      (plUiWindow* ptWindow);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~storage system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int         pl_ui_get_int      (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float       pl_ui_get_float    (plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool        pl_ui_get_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void*       pl_ui_get_ptr      (plUiStorage* ptStorage, uint32_t uKey);

int*        pl_ui_get_int_ptr  (plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue);
float*      pl_ui_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue);
bool*       pl_ui_get_bool_ptr (plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue);
void**      pl_ui_get_ptr_ptr  (plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue);

void        pl_ui_set_int      (plUiStorage* ptStorage, uint32_t uKey, int iValue);
void        pl_ui_set_float    (plUiStorage* ptStorage, uint32_t uKey, float fValue);
void        pl_ui_set_bool     (plUiStorage* ptStorage, uint32_t uKey, bool bValue);
void        pl_ui_set_ptr      (plUiStorage* ptStorage, uint32_t uKey, void* pValue);

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_ui_setup_context(plUiContext* ptCtx)
{
    gptCtx = ptCtx;
    ptCtx->ptDrawCtx = PL_ALLOC(sizeof(plDrawList));
    memset(ptCtx->ptDrawCtx, 0, sizeof(plDrawList));
    ptCtx->ptDrawlist = PL_ALLOC(sizeof(plDrawList));
    ptCtx->ptDebugDrawlist = PL_ALLOC(sizeof(plDrawList));
    memset(ptCtx->ptDrawlist, 0, sizeof(plDrawList));
    memset(ptCtx->ptDebugDrawlist, 0, sizeof(plDrawList));
    pl_register_drawlist(ptCtx->ptDrawCtx, ptCtx->ptDrawlist);
    pl_register_drawlist(ptCtx->ptDrawCtx, ptCtx->ptDebugDrawlist);
    ptCtx->ptBgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Background");
    ptCtx->ptFgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Foreground");
    ptCtx->ptDebugLayer = pl_request_draw_layer(ptCtx->ptDebugDrawlist, "ui debug");
    ptCtx->tTooltipWindow.ptBgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Tooltip Background");
    ptCtx->tTooltipWindow.ptFgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Tooltip Foreground");
    pl_ui_set_dark_theme();
}

void
pl_ui_cleanup_context(void)
{
    pl_cleanup_draw_context(gptCtx->ptDrawCtx);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        pl_return_draw_layer(gptCtx->sbptWindows[i]->ptBgLayer);
        pl_return_draw_layer(gptCtx->sbptWindows[i]->ptFgLayer);
    }
    PL_FREE(gptCtx->ptDrawlist);
    PL_FREE(gptCtx->ptDebugDrawlist);
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbtFocusedWindows);
    pl_sb_free(gptCtx->sbtSortingWindows);
    pl_sb_free(gptCtx->sbuIdStack);
    gptCtx->ptDrawlist = NULL;
    gptCtx->ptCurrentWindow = NULL;
    gptCtx->ptHoveredWindow = NULL;
    gptCtx->ptMovingWindow = NULL;
    gptCtx->ptActiveWindow = NULL;
    gptCtx->ptFont = NULL;
}

void
pl_ui_set_context(plUiContext* ptCtx)
{
    gptCtx = ptCtx;
}

plUiContext*
pl_ui_get_context(void)
{
    return gptCtx;
}

void
pl_ui_new_frame(void)
{

    pl_new_io_frame();
    pl_new_draw_frame(gptCtx->ptDrawCtx);

    if(gptCtx->bWantMouse && !pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        gptCtx->bMouseOwned = true;

    if(pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        gptCtx->uNextActiveId = gptCtx->uActiveId;

    if(gptCtx->ptHoveredWindow == NULL && pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        gptCtx->bMouseOwned = false;
    else if (pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        gptCtx->bMouseOwned = true;
}

void
pl_ui_end_frame(void)
{

    const plVec2 tMousePos = pl_get_mouse_pos();

    // update state id's from previous frames
    gptCtx->uHoveredId = gptCtx->uNextHoveredId;
    gptCtx->uActiveId = gptCtx->uNextActiveId;
    gptCtx->bWantMouse = gptCtx->bWantMouseNextFrame;

    // null starting state
    gptCtx->bActiveIdJustActivated = false;
    gptCtx->bWantMouseNextFrame = false;
    gptCtx->ptHoveredWindow = NULL;
    gptCtx->ptActiveWindow = NULL;
    gptCtx->ptWheelingWindow = NULL;
    gptCtx->uNextHoveredId = 0u;
    gptCtx->uNextActiveId = 0u;
    gptCtx->uHoveredWindowId = 0u;
    gptCtx->tPrevItemData.bHovered = false;
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_NONE;

    if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->bWantMouseNextFrame = false;
        gptCtx->ptMovingWindow = NULL;
        gptCtx->ptSizingWindow = NULL;
        gptCtx->ptScrollingWindow = NULL;
        gptCtx->ptActiveWindow = NULL;
    }

    // reset active window
    if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        gptCtx->uActiveWindowId = 0;

    // submit windows in display order
    pl_sb_reset(gptCtx->sbptWindows);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtFocusedWindows); i++)
    {
        plUiWindow* ptRootWindow = gptCtx->sbtFocusedWindows[i];
        pl__ui_submit_window(ptRootWindow);

        // adjust window size if outside viewport
        if(ptRootWindow->tPos.x > pl_get_io_context()->afMainViewportSize[0])
            ptRootWindow->tPos.x = pl_get_io_context()->afMainViewportSize[0] - ptRootWindow->tSize.x / 2.0f;

        if(ptRootWindow->tPos.y > pl_get_io_context()->afMainViewportSize[1])
        {
            ptRootWindow->tPos.y = pl_get_io_context()->afMainViewportSize[1] - ptRootWindow->tSize.y / 2.0f;
            ptRootWindow->tPos.y = pl_maxf(ptRootWindow->tPos.y, 0.0f);
        }
    }

    // move newly activated window to front of focus order
    if(gptCtx->bActiveIdJustActivated)
    {
        pl_sb_top(gptCtx->sbtFocusedWindows)->uFocusOrder = gptCtx->ptActiveWindow->uFocusOrder;
        gptCtx->ptActiveWindow->uFocusOrder = pl_sb_size(gptCtx->sbtFocusedWindows) - 1;
        pl_sb_del_swap(gptCtx->sbtFocusedWindows, pl_sb_top(gptCtx->sbtFocusedWindows)->uFocusOrder); //-V1004
        pl_sb_push(gptCtx->sbtFocusedWindows, gptCtx->ptActiveWindow);
    }

    // scrolling window
    if(gptCtx->ptWheelingWindow)
    {
        gptCtx->ptWheelingWindow->tScroll.y -= pl_get_mouse_wheel() * 10.0f;
        gptCtx->ptWheelingWindow->tScroll.y = pl_clampf(0.0f, gptCtx->ptWheelingWindow->tScroll.y, gptCtx->ptWheelingWindow->tScrollMax.y);
    }

    // moving window
    if(gptCtx->ptMovingWindow && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {

        if(tMousePos.x > 0.0f && tMousePos.x < pl_get_io_context()->afMainViewportSize[0])
            gptCtx->ptMovingWindow->tPos.x = gptCtx->ptMovingWindow->tPos.x + pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;

        if(tMousePos.y > 0.0f && tMousePos.y < pl_get_io_context()->afMainViewportSize[1])
            gptCtx->ptMovingWindow->tPos.y = gptCtx->ptMovingWindow->tPos.y + pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;  

        // clamp x
        gptCtx->ptMovingWindow->tPos.x = pl_maxf(gptCtx->ptMovingWindow->tPos.x, -gptCtx->ptMovingWindow->tSize.x / 2.0f);   
        gptCtx->ptMovingWindow->tPos.x = pl_minf(gptCtx->ptMovingWindow->tPos.x, pl_get_io_context()->afMainViewportSize[0] - gptCtx->ptMovingWindow->tSize.x / 2.0f);

        // clamp y
        gptCtx->ptMovingWindow->tPos.y = pl_maxf(gptCtx->ptMovingWindow->tPos.y, 0.0f);   
        gptCtx->ptMovingWindow->tPos.y = pl_minf(gptCtx->ptMovingWindow->tPos.y, pl_get_io_context()->afMainViewportSize[1] - 50.0f);

        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    pl_end_io_frame(); 
}

void
pl_ui_render(void)
{
    pl_submit_draw_layer(gptCtx->ptBgLayer);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        if(gptCtx->sbptWindows[i]->uHideFrames == 0)
        {
            pl_submit_draw_layer(gptCtx->sbptWindows[i]->ptBgLayer);
            pl_submit_draw_layer(gptCtx->sbptWindows[i]->ptFgLayer);
        }
        else
        {
            gptCtx->sbptWindows[i]->uHideFrames--;
        }
    }
    pl_submit_draw_layer(gptCtx->tTooltipWindow.ptBgLayer);
    pl_submit_draw_layer(gptCtx->tTooltipWindow.ptFgLayer);
    pl_submit_draw_layer(gptCtx->ptFgLayer);
    pl_submit_draw_layer(gptCtx->ptDebugLayer);

    pl_ui_end_frame();
}

void
pl_ui_set_dark_theme(void)
{
    // styles
    gptCtx->tStyle.fTitlePadding            = 10.0f;
    gptCtx->tStyle.fFontSize                = 13.0f;
    gptCtx->tStyle.fWindowHorizontalPadding = 5.0f;
    gptCtx->tStyle.fWindowVerticalPadding   = 5.0f;
    gptCtx->tStyle.fIndentSize              = 15.0f;
    gptCtx->tStyle.fScrollbarSize           = 10.0f;
    gptCtx->tStyle.fSliderSize              = 12.0f;
    gptCtx->tStyle.tItemSpacing  = (plVec2){8.0f, 4.0f};
    gptCtx->tStyle.tInnerSpacing = (plVec2){4.0f, 4.0f};
    gptCtx->tStyle.tFramePadding = (plVec2){4.0f, 4.0f};

    // colors
    gptCtx->tStyle.tTitleActiveCol      = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tTitleBgCol          = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tStyle.tTitleBgCollapsedCol = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tStyle.tWindowBgColor       = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tStyle.tWindowBorderColor   = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tChildBgColor        = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tStyle.tButtonCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tButtonHoveredCol    = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tButtonActiveCol     = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tTextCol             = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    gptCtx->tStyle.tProgressBarCol      = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    gptCtx->tStyle.tCheckmarkCol        = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tFrameBgCol          = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tFrameBgHoveredCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    gptCtx->tStyle.tFrameBgActiveCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    gptCtx->tStyle.tHeaderCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tHeaderHoveredCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    gptCtx->tStyle.tHeaderActiveCol     = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    gptCtx->tStyle.tScrollbarBgCol      = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    gptCtx->tStyle.tScrollbarHandleCol  = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tStyle.tScrollbarFrameCol   = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    gptCtx->tStyle.tScrollbarActiveCol  = gptCtx->tStyle.tButtonActiveCol;
    gptCtx->tStyle.tScrollbarHoveredCol = gptCtx->tStyle.tButtonHoveredCol;
}

bool
pl_ui_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize)
{
    bool bResult = pl_ui_begin_window_ex(pcName, pbOpen, bAutoSize ? PL_UI_WINDOW_FLAGS_AUTO_SIZE : PL_UI_WINDOW_FLAGS_NONE);

    static const float pfRatios[] = {300.0f};
    if(bResult)
    {
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
        pl_ui_end_window();
    return bResult;
}

void
pl_ui_end_window(void)
{

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        ptWindow->tContentSize = pl_add_vec2(
            (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
            pl_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos)
        
        );
    }
    ptWindow->tScrollMax = pl_sub_vec2(ptWindow->tContentSize, (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});
    
    // clamp scrolling max
    ptWindow->tScrollMax = pl_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX)
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;

    if(ptWindow->bScrollbarY)
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;

    const bool bScrollBarsPresent = ptWindow->bScrollbarX || ptWindow->bScrollbarY;

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR)
        fTitleBarHeight = 0.0f;

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // autosized non collapsed
    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE && !ptWindow->bCollapsed)
    {

        const plRect tBgRect = pl_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        // ensure window doesn't get too small
        ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;
        ptWindow->tSize.y = fTitleBarHeight + ptWindow->tContentSize.y + gptCtx->tStyle.fWindowVerticalPadding;
        
        // clamp window size to min/max
        ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

        ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
        ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
        
        // remove scissor rect
        pl_pop_clip_rect(gptCtx->ptDrawlist);

        // draw background
        pl_add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tStyle.tWindowBgColor);

        ptWindow->tFullSize = ptWindow->tSize;
    }

    // regular window non collapsed
    else if(!ptWindow->bCollapsed)
    {
        plUiWindow* ptParentWindow = ptWindow->ptParentWindow;

        plRect tBgRect = pl_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        plRect tParentBgRect = ptParentWindow->tOuterRect;

        if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        {
            tBgRect = pl_rect_clip(&tBgRect, &tParentBgRect);
        }

        pl_pop_clip_rect(gptCtx->ptDrawlist);

        const uint32_t uResizeHash = ptWindow->uId + 1;
        const uint32_t uWestResizeHash = uResizeHash + 1;
        const uint32_t uEastResizeHash = uResizeHash + 2;
        const uint32_t uNorthResizeHash = uResizeHash + 3;
        const uint32_t uSouthResizeHash = uResizeHash + 4;
        const uint32_t uVerticalScrollHash = uResizeHash + 5;
        const uint32_t uHorizonatalScrollHash = uResizeHash + 6;
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fHoverPadding = 4.0f;

        // draw background
        pl_add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tStyle.tWindowBgColor);

        // vertical scroll bar
        if(ptWindow->bScrollbarY)
            pl__ui_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

        // horizontal scroll bar
        if(ptWindow->bScrollbarX)
            pl__ui_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

        const plVec2 tTopLeft = pl_rect_top_left(&ptWindow->tOuterRect);
        const plVec2 tBottomLeft = pl_rect_bottom_left(&ptWindow->tOuterRect);
        const plVec2 tTopRight = pl_rect_top_right(&ptWindow->tOuterRect);
        const plVec2 tBottomRight = pl_rect_bottom_right(&ptWindow->tOuterRect);

        // resizing grip
        {
            const plVec2 tCornerTopLeftPos = pl_add_vec2(tBottomRight, (plVec2){-15.0f, -15.0f});
            const plVec2 tCornerTopPos = pl_add_vec2(tBottomRight, (plVec2){0.0f, -15.0f});
            const plVec2 tCornerLeftPos = pl_add_vec2(tBottomRight, (plVec2){-15.0f, 0.0f});

            const plRect tBoundingBox = pl_calculate_rect(tCornerTopLeftPos, (plVec2){15.0f, 15.0f});
            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uResizeHash)
            {
                pl_add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.99f, 0.02f, 0.10f, 1.0f});
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
            }
            else if(gptCtx->uHoveredId == uResizeHash)
            {
                pl_add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.66f, 0.02f, 0.10f, 1.0f});
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
            }
            else
            {
                pl_add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.33f, 0.02f, 0.10f, 1.0f});   
            }
        }

        // east border
        {

            plRect tBoundingBox = pl_calculate_rect(tTopRight, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uEastResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uEastResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
            else if(gptCtx->uHoveredId == uEastResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
        }

        // west border
        {
            plRect tBoundingBox = pl_calculate_rect(tTopLeft, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uWestResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uWestResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
            else if(gptCtx->uHoveredId == uWestResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
        }

        // north border
        {
            plRect tBoundingBox = {tTopLeft, (plVec2){tTopRight.x - 15.0f, tTopRight.y}};
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uNorthResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uNorthResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
            else if(gptCtx->uHoveredId == uNorthResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
        }

        // south border
        {
            plRect tBoundingBox = {tBottomLeft, (plVec2){tBottomRight.x - 15.0f, tBottomRight.y}};
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uSouthResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uSouthResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
            else if(gptCtx->uHoveredId == uSouthResizeHash)
            {
                pl_add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
        }

        // draw border
        pl_add_rect(ptWindow->ptFgLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, gptCtx->tStyle.tWindowBorderColor, 1.0f);

        // handle corner resizing
        if(pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
        {
            const plVec2 tMousePos = pl_get_mouse_pos();

            if(gptCtx->uActiveId == uResizeHash)
            {  
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize = pl_sub_vec2(tMousePos, ptWindow->tPos);
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle east resizing
            else if(gptCtx->uActiveId == uEastResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.x = tMousePos.x - ptWindow->tPos.x;
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle west resizing
            else if(gptCtx->uActiveId == uWestResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.x = tTopRight.x - tMousePos.x;  
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tPos.x = tTopRight.x - ptWindow->tSize.x;
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle north resizing
            else if(gptCtx->uActiveId == uNorthResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.y = tBottomRight.y - tMousePos.y;  
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tPos.y = tBottomRight.y - ptWindow->tSize.y;
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle south resizing
            else if(gptCtx->uActiveId == uSouthResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.y = tMousePos.y - ptWindow->tPos.y;
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle vertical scrolling with scroll bar
            else if(gptCtx->uActiveId == uVerticalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.y > ptWindow->tPos.y && tMousePos.y < ptWindow->tPos.y + ptWindow->tSize.y)
                {
                    const float fScrollConversion = roundf(ptWindow->tTempData.tCursorMaxPos.y / ptWindow->tSize.y);
                    ptWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
                    ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }

            // handle horizontal scrolling with scroll bar
            else if(gptCtx->uActiveId == uHorizonatalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.x > ptWindow->tPos.x && tMousePos.x < ptWindow->tPos.x + ptWindow->tSize.x)
                {
                    const float fScrollConversion = roundf(ptWindow->tTempData.tCursorMaxPos.x / ptWindow->tSize.x);
                    ptWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
                    ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
                    pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }
        }
        gptCtx->ptCurrentWindow->tFullSize = ptWindow->tSize;
    }

    gptCtx->ptCurrentWindow = NULL;
    pl_sb_pop(gptCtx->sbuIdStack);
}

bool
pl_ui_begin_child(const char* pcName)
{
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(200.0f);
    // const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

    const plUiWindowFlags tFlags = 
        PL_UI_WINDOW_FLAGS_CHILD_WINDOW |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    pl_ui_set_next_window_size(tWidgetSize, PL_UI_COND_ALWAYS);
    bool bValue =  pl_ui_begin_window_ex(pcName, NULL, tFlags);
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return bValue;
}

void
pl_ui_end_child(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiWindow* ptParentWindow = ptWindow->ptParentWindow;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        ptWindow->tContentSize = pl_add_vec2(
            (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
            pl_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos)
        
        );
    }
    ptWindow->tScrollMax = pl_sub_vec2(ptWindow->tContentSize, (plVec2){ptWindow->tSize.x, ptWindow->tSize.y});
    
    // clamp scrolling max
    ptWindow->tScrollMax = pl_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX)
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;

    if(ptWindow->bScrollbarY)
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;

    const bool bScrollBarsPresent = ptWindow->bScrollbarX || ptWindow->bScrollbarY;

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    plRect tParentBgRect = ptParentWindow->tOuterRect;
    const plRect tBgRect = pl_rect_clip(&ptWindow->tOuterRect, &tParentBgRect);

    pl_pop_clip_rect(gptCtx->ptDrawlist);

    const uint32_t uResizeHash = pl_str_hash("##resize", 0, pl_sb_top(gptCtx->sbuIdStack));
    const uint32_t uVerticalScrollHash = pl_str_hash("##scrollright", 0, pl_sb_top(gptCtx->sbuIdStack));
    const uint32_t uHorizonatalScrollHash = pl_str_hash("##scrollbottom", 0, pl_sb_top(gptCtx->sbuIdStack));
    const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
    const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;

    // draw background
    pl_add_rect_filled(ptParentWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tStyle.tWindowBgColor);

    // vertical scroll bar
    if(ptWindow->bScrollbarY)
        pl__ui_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

    // horizontal scroll bar
    if(ptWindow->bScrollbarX)
        pl__ui_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

    // handle vertical scrolling with scroll bar
    if(gptCtx->uActiveId == uVerticalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tTempData.tCursorMaxPos.y / ptWindow->tSize.y);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uVerticalScrollHash;
        ptWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    // handle horizontal scrolling with scroll bar
    else if(gptCtx->uActiveId == uHorizonatalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tTempData.tCursorMaxPos.x / ptWindow->tSize.x);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uHorizonatalScrollHash;
        ptWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    ptWindow->tFullSize = ptWindow->tSize;
    pl_sb_pop(gptCtx->sbuIdStack);
    gptCtx->ptCurrentWindow = ptParentWindow;

    pl__ui_advance_cursor(ptWindow->tSize.x, ptWindow->tSize.y);
}

void
pl_ui_begin_tooltip(void)
{
    
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tContentSize = pl_add_vec2(
        (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding},
        pl_sub_vec2(
            ptWindow->tTempData.tCursorMaxPos, 
            ptWindow->tTempData.tCursorStartPos)
    );

    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));

    ptWindow->tFlags |= 
        PL_UI_WINDOW_FLAGS_TOOLTIP |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_AUTO_SIZE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    // place window at mouse position
    const plVec2 tMousePos = pl_get_mouse_pos();
    ptWindow->tTempData.tCursorStartPos = pl_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    ptWindow->tPos = tMousePos;
    ptWindow->tTempData.tNextRowStartPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    ptWindow->tTempData.tNextRowStartPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);

    const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y };
    const plVec2 tEndClip = { ptWindow->tSize.x, ptWindow->tSize.y };
    pl_push_clip_rect(gptCtx->ptDrawlist, pl_calculate_rect(tStartClip, tEndClip), true);

    ptWindow->ptParentWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = ptWindow;
}

void
pl_ui_end_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    ptWindow->tSize.y = ptWindow->tContentSize.y;
    pl_add_rect_filled(ptWindow->ptBgLayer,
        ptWindow->tPos, 
        pl_add_vec2(ptWindow->tPos, ptWindow->tSize), gptCtx->tStyle.tWindowBgColor);

    pl_pop_clip_rect(gptCtx->ptDrawlist);
    gptCtx->ptCurrentWindow = ptWindow->ptParentWindow;
}

plVec2
pl_ui_get_window_pos(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tPos;
}

plVec2
pl_ui_get_window_size(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tSize;
}

void
pl_ui_set_next_window_pos(plVec2 tPos, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tPos = tPos;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    gptCtx->tNextWindowData.tPosCondition = tCondition;
}

void
pl_ui_set_next_window_size(plVec2 tSize, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tSize = tSize;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
    gptCtx->tNextWindowData.tSizeCondition = tCondition;
}

void
pl_ui_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.bCollapsed = bCollapsed;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED;
    gptCtx->tNextWindowData.tCollapseCondition = tCondition;    
}

bool
pl_ui_button(const char* pcText)
{

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);

    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonHoveredCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonCol);

    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    plVec2 tTextStartPos = {
        .x = tStartPos.x,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    if((pl_rect_width(&tBoundingBox) < tTextSize.x)) // clipping, so start at beginning of widget
        tTextStartPos.x += gptCtx->tStyle.tFramePadding.x;
    else // not clipping, so center on widget
        tTextStartPos.x += tStartPos.x + tWidgetSize.x / 2.0f - tTextActualCenter.x;

    pl__add_clipped_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, tBoundingBox.tMin, tBoundingBox.tMax, gptCtx->tStyle.tTextCol, pcText, 0.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed;
}

bool
pl_ui_selectable(const char* pcText, bool* bpValue)
{
    // temporary hack
    static bool bDummyState = true;
    if(bpValue == NULL) bpValue = &bDummyState;

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));

    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plVec2 tTextStartPos = {
        .x = tStartPos.x + gptCtx->tStyle.tFramePadding.x,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    const plVec2 tEndPos = pl_add_vec2(tStartPos, tWidgetSize);

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *bpValue = !*bpValue;

    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderHoveredCol);

    if(*bpValue)
        pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderCol);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return *bpValue; 
}

bool
pl_ui_checkbox(const char* pcText, bool* bpValue)
{
    // temporary hack
    static bool bDummyState = true;
    if(bpValue == NULL) bpValue = &bDummyState;

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
 

    const bool bOriginalValue = *bpValue;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);


    const plVec2 tTextStartPos = {
        .x = tStartPos.x + tWidgetSize.y + gptCtx->tStyle.tInnerSpacing.x,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, (plVec2){tWidgetSize.y, tWidgetSize.y});
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *bpValue = !bOriginalValue;

    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgHoveredCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgCol);

    if(*bpValue)
        pl_add_line(ptWindow->ptFgLayer, tStartPos,  tBoundingBox.tMax, gptCtx->tStyle.tCheckmarkCol, 2.0f);

    // add label
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed;
}

bool
pl_ui_radio_button(const char* pcText, int* piValue, int iButtonValue)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plVec2 tSize = {tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x + gptCtx->tStyle.tInnerSpacing.x + tWidgetSize.y, tWidgetSize.y};
    // tSize = pl_floor_vec2(tSize);

    const plVec2 tTextStartPos = {
        .x = tStartPos.x + tWidgetSize.y + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    plRect tBoundingBox = pl_rect_expand_vec2(&tTextBounding, (plVec2){0.5f * (gptCtx->tStyle.tFramePadding.x + gptCtx->tStyle.tInnerSpacing.x + tWidgetSize.y), 0.0f});
    tBoundingBox = pl_rect_move_start_x(&tBoundingBox, tStartPos.x + gptCtx->tStyle.tFramePadding.x);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *piValue = iButtonValue;

    if(gptCtx->uActiveId == uHash)       pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tStyle.tFrameBgActiveCol, 12);
    else if(gptCtx->uHoveredId == uHash) pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tStyle.tFrameBgHoveredCol, 12);
    else                                 pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tStyle.tFrameBgCol, 12);

    if(*piValue == iButtonValue)
        pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 2.5f, gptCtx->tStyle.tCheckmarkCol, 12);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed;
}

bool
pl_ui_collapsing_header(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));

    bool* pbOpenState = pl_ui_get_bool_ptr(&ptWindow->tStorage, uHash, false);

    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plVec2 tTextStartPos = {
        .x = tStartPos.x + tWidgetSize.y * 1.5f,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *pbOpenState = !*pbOpenState;

    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tHeaderActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tHeaderHoveredCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tHeaderCol);

    if(*pbOpenState)
    {
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){ 0.0f,  4.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ 4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint,  (plVec2){-4.0f, -4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
    }
    else
    {
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){  4.0f,  0.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ -4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint, (plVec2){ -4.0f,  4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
    }

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);

    if(*pbOpenState)
        pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);

    return *pbOpenState; 
}

void
pl_ui_end_collapsing_header(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
    ptWindow->tTempData.tCurrentLayoutRow.tRowPos = ptWindow->tTempData.tNextRowStartPos;
    ptWindow->tLayoutSystemType = ptWindow->tTempData.tCurrentLayoutRow.tSystemType;
}

bool
pl_ui_tree_node(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    
    bool* pbOpenState = pl_ui_get_bool_ptr(&ptWindow->tStorage, uHash, false);

    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *pbOpenState = !*pbOpenState;

    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tHeaderActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tHeaderHoveredCol);

    if(*pbOpenState)
    {
        ptWindow->tTempData.uTreeDepth++;
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){ 0.0f,  4.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ 4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint,  (plVec2){-4.0f, -4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
        pl_sb_push(gptCtx->sbuIdStack, uHash);
    }
    else
    {
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){  4.0f,  0.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ -4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint, (plVec2){ -4.0f,  4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
    }

    const plVec2 tTextStartPos = {
        .x = tStartPos.x + tWidgetSize.y * 1.5f,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);

    if(*pbOpenState)
        pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);

    return *pbOpenState; 
}

bool
pl_ui_tree_node_f(const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    bool bOpen = pl_ui_tree_node_v(pcFmt, args);
    va_end(args);
    return bOpen;
}

bool
pl_ui_tree_node_v(const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    pl_vsprintf(acTempBuffer, pcFmt, args);
    return pl_ui_tree_node(acTempBuffer);
}

void
pl_ui_tree_pop(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.uTreeDepth--;
    pl_sb_pop(gptCtx->sbuIdStack);
    const plVec2 tRowPos = ptWindow->tTempData.tCurrentLayoutRow.tRowPos;
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
    ptWindow->tTempData.tCurrentLayoutRow.tRowPos = tRowPos;
    ptWindow->tLayoutSystemType = ptWindow->tTempData.tCurrentLayoutRow.tSystemType;
}

bool
pl_ui_begin_tab_bar(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);

    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    pl_sb_push(gptCtx->sbuIdStack, uHash);

    // check if tab bar existed
    gptCtx->ptCurrentTabBar = NULL;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtTabBars); i++)
    {
        if(gptCtx->sbtTabBars[i].uId == uHash)
        {
            gptCtx->ptCurrentTabBar = &gptCtx->sbtTabBars[i];
            break;
        }
    }

    // new tab bar needs to be created
    if(gptCtx->ptCurrentTabBar == NULL)
    {
        plUiTabBar tTabBar = {
            .uId = uHash
        };;

        pl_sb_push(gptCtx->sbtTabBars, tTabBar);
        gptCtx->ptCurrentTabBar = &pl_sb_top(gptCtx->sbtTabBars);
    }

    gptCtx->ptCurrentTabBar->tStartPos = tStartPos;
    gptCtx->ptCurrentTabBar->tCursorPos = tStartPos;
    gptCtx->ptCurrentTabBar->uCurrentIndex = 0u;

    pl_add_line(ptWindow->ptFgLayer, 
        (plVec2){gptCtx->ptCurrentTabBar->tStartPos.x, gptCtx->ptCurrentTabBar->tStartPos.y + tWidgetSize.y},
        (plVec2){gptCtx->ptCurrentTabBar->tStartPos.x + tWidgetSize.x, gptCtx->ptCurrentTabBar->tStartPos.y + tWidgetSize.y},
        gptCtx->tStyle.tButtonActiveCol, 1.0f);

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return true;
}

void
pl_ui_end_tab_bar(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentTabBar->uValue = gptCtx->ptCurrentTabBar->uNextValue;
    pl_sb_pop(gptCtx->sbuIdStack);
    const plVec2 tRowPos = ptWindow->tTempData.tCurrentLayoutRow.tRowPos;
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
    ptWindow->tTempData.tCurrentLayoutRow.tRowPos = tRowPos;
    ptWindow->tLayoutSystemType = ptWindow->tTempData.tCurrentLayoutRow.tSystemType;
}

bool
pl_ui_begin_tab(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
    plUiTabBar* ptTabBar = gptCtx->ptCurrentTabBar;
    const float fFrameHeight = pl__ui_get_frame_height();
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    pl_sb_push(gptCtx->sbuIdStack, uHash);

    if(ptTabBar->uValue == 0u) ptTabBar->uValue = uHash;

    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
    const plVec2 tStartPos = ptTabBar->tCursorPos;

    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl__find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plVec2 tFinalSize = {tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x, fFrameHeight};

    const plVec2 tTextStartPos = {
        .x = tStartPos.x + tStartPos.x + tFinalSize.x / 2.0f - tTextActualCenter.x,
        .y = tStartPos.y + tStartPos.y + fFrameHeight / 2.0f - tTextActualCenter.y
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tFinalSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(uHash == gptCtx->uActiveId)
    {
        ptTabBar->uNextValue = uHash;
    }

    if(gptCtx->uActiveId== uHash)        pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonHoveredCol);
    else if(ptTabBar->uValue == uHash)   pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonActiveCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonCol);
    
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);

    ptTabBar->tCursorPos.x += gptCtx->tStyle.tInnerSpacing.x + tFinalSize.x;
    ptTabBar->uCurrentIndex++;

    if(ptTabBar->uValue != uHash)
        pl_ui_end_tab();

    return ptTabBar->uValue == uHash;
}

void
pl_ui_end_tab(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_sb_pop(gptCtx->sbuIdStack);
    const plVec2 tRowPos = ptWindow->tTempData.tCurrentLayoutRow.tRowPos;
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
    ptWindow->tTempData.tCurrentLayoutRow.tRowPos = tRowPos;
    ptWindow->tLayoutSystemType = ptWindow->tTempData.tCurrentLayoutRow.tSystemType;
}

void
pl_ui_separator(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(gptCtx->tStyle.tItemSpacing.y * 2.0f);

    pl_add_line(ptWindow->ptFgLayer, tStartPos, (plVec2){tStartPos.x + tWidgetSize.x, tStartPos.y}, gptCtx->tStyle.tCheckmarkCol, 1.0f);

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_vertical_spacing(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tNextRowStartPos.y += gptCtx->tStyle.tItemSpacing.y * 2.0f;
}

void
pl_ui_indent(float fIndent)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fHorizontalOffset += fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
    ptWindow->tTempData.fExtraIndent += fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
}

void
pl_ui_unindent(float fIndent)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fHorizontalOffset -= fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
    ptWindow->tTempData.fExtraIndent -= fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
}

void
pl_ui_text(const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    pl_ui_text_v(pcFmt, args);
    va_end(args);
}

void
pl_ui_text_v(const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    pl_vsprintf(acTempBuffer, pcFmt, args);

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, acTempBuffer, pl__find_renderered_text_end(acTempBuffer, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y}, gptCtx->tStyle.tTextCol, acTempBuffer, -1.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_color_text(plVec4 tColor, const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    pl_ui_color_text_v(tColor, pcFmt, args);
    va_end(args);
}

void
pl_ui_color_text_v(plVec4 tColor, const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    pl_vsprintf(acTempBuffer, pcFmt, args);

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, acTempBuffer, pl__find_renderered_text_end(acTempBuffer, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y}, tColor, acTempBuffer, -1.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_labeled_text(const char* pcLabel, const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    pl_ui_labeled_text_v(pcLabel, pcFmt, args);
    va_end(args);
}

void
pl_ui_labeled_text_v(const char* pcLabel, const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    pl_vsprintf(acTempBuffer, pcFmt, args);

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, acTempBuffer, pl__find_renderered_text_end(acTempBuffer, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plVec2 tStartLocation = {tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y};
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, pl_add_vec2(tStartLocation, (plVec2){(tWidgetSize.x / 3.0f), 0.0f}), gptCtx->tStyle.tTextCol, acTempBuffer, -1.0f);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartLocation, gptCtx->tStyle.tTextCol, pcLabel, -1.0f);
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

bool
pl_ui_slider_float(const char* pcLabel, float* pfValue, float fMin, float fMax)
{
    return pl_ui_slider_float_f(pcLabel, pfValue, fMin, fMax, "%0.3f");
}

bool
pl_ui_slider_float_f(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat)
{

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };

    const float fOriginalValue = *pfValue;
    *pfValue = pl_clampf(fMin, *pfValue, fMax);
    const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));

    char acTextBuffer[64] = {0};
    pl_sprintf(acTextBuffer, pcFormat, *pfValue);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, acTextBuffer, pl__find_renderered_text_end(acTextBuffer, NULL), -1.0f);
    const plRect tLabelTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl__find_renderered_text_end(pcLabel, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
    const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

    const plVec2 tSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
    const plVec2 tTextStartPos = { 
        tFrameStartPos.x + tFrameStartPos.x + (2.0f * (tWidgetSize.x / 3.0f)) / 2.0f - tTextActualCenter.x, 
        tFrameStartPos.y + tFrameStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    const float fRange = fMax - fMin;
    const float fConv = fRange / (tSize.x - gptCtx->tStyle.fSliderSize);
    
    const plVec2 tGrabStartPos = {
        .x = tFrameStartPos.x + ((*pfValue) - fMin) / fConv,
        .y = tFrameStartPos.y
    };

    const plVec2 tGrabSize = { gptCtx->tStyle.fSliderSize, tWidgetSize.y};
    const plRect tGrabBox = pl_calculate_rect(tGrabStartPos, tGrabSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tGrabBox, uHash, &bHovered, &bHeld);

    const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tSize);
    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgHoveredCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgCol);

    pl_add_rect_filled(ptWindow->ptFgLayer, tGrabStartPos, tGrabBox.tMax, gptCtx->tStyle.tButtonCol);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tStyle.tTextCol, pcLabel, -1.0f);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, acTextBuffer, -1.0f);

    bool bDragged = false;
    if(gptCtx->uActiveId == uHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        *pfValue += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fConv;
        *pfValue = pl_clampf(fMin, *pfValue, fMax);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return fOriginalValue != *pfValue;
}

bool
pl_ui_slider_int(const char* pcLabel, int* piValue, int iMin, int iMax)
{
    return pl_ui_slider_int_f(pcLabel, piValue, iMin, iMax, "%d");
}

bool
pl_ui_slider_int_f(const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat)
{

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };

    const int iOriginalValue = *piValue;
    *piValue = pl_clampi(iMin, *piValue, iMax);
    const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));
    const int iBlocks = iMax - iMin + 1;
    const int iBlock = *piValue - iMin;

    char acTextBuffer[64] = {0};
    pl_sprintf(acTextBuffer, pcFormat, *piValue);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, acTextBuffer, pl__find_renderered_text_end(acTextBuffer, NULL), -1.0f);
    const plRect tLabelTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl__find_renderered_text_end(pcLabel, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
    const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

    const plVec2 tSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
    const plVec2 tTextStartPos = { 
        tFrameStartPos.x + tFrameStartPos.x + (2.0f * (tWidgetSize.x / 3.0f)) / 2.0f - tTextActualCenter.x, 
        tFrameStartPos.y + tFrameStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };
    const float fBlockLength = tSize.x / (float)iBlocks;
    
    const plVec2 tGrabStartPos = {
        .x = tFrameStartPos.x + (float)iBlock * fBlockLength,
        .y = tFrameStartPos.y
    };

    const plVec2 tGrabSize = { fBlockLength, tWidgetSize.y};
    const plRect tGrabBox = pl_calculate_rect(tGrabStartPos, tGrabSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tGrabBox, uHash, &bHovered, &bHeld);

    const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tSize);
    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgHoveredCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgCol);

    pl_add_rect_filled(ptWindow->ptFgLayer, tGrabStartPos, tGrabBox.tMax, gptCtx->tStyle.tButtonCol);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tStyle.tTextCol, pcLabel, -1.0f);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, acTextBuffer, -1.0f);

    bool bDragged = false;
    if(gptCtx->uActiveId == uHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMousePos = pl_get_mouse_pos();

        if(tMousePos.x > tGrabBox.tMax.x)
            (*piValue)++;
        if(tMousePos.x < tGrabStartPos.x)
            (*piValue)--;

        *piValue = pl_clampi(iMin, *piValue, iMax);
    }

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return iOriginalValue != *piValue;
}

bool
pl_ui_drag_float(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax)
{
    return pl_ui_drag_float_f(pcLabel, pfValue, fSpeed, fMin, fMax, "%.3f");
}

bool
pl_ui_drag_float_f(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };

    const float fOriginalValue = *pfValue;
    *pfValue = pl_clampf(fMin, *pfValue, fMax);
    const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));

    char acTextBuffer[64] = {0};
    pl_sprintf(acTextBuffer, pcFormat, *pfValue);
    const plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, acTextBuffer, pl__find_renderered_text_end(acTextBuffer, NULL), -1.0f);
    const plRect tLabelTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl__find_renderered_text_end(pcLabel, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
    const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

    const plVec2 tSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
    const plVec2 tTextStartPos = { 
        tFrameStartPos.x + tFrameStartPos.x + (2.0f * (tWidgetSize.x / 3.0f)) / 2.0f - tTextActualCenter.x, 
        tFrameStartPos.y + tFrameStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };
    const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tSize);

    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(gptCtx->uActiveId == uHash)       pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgActiveCol);
    else if(gptCtx->uHoveredId == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgHoveredCol);
    else                                 pl_add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tStyle.tFrameBgCol);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tStyle.tTextCol, pcLabel, -1.0f);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, acTextBuffer, -1.0f);

    bool bDragged = false;
    if(gptCtx->uActiveId == uHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        *pfValue = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fSpeed;
        *pfValue = pl_clampf(fMin, *pfValue, fMax);
    }

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return fOriginalValue != *pfValue;    
}

void
pl_ui_image(plTextureId tTexture, plVec2 tSize)
{
    pl_ui_image_ex(tTexture, tSize, (plVec2){0}, (plVec2){1.0f, 1.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, (plVec4){0});
}

void
pl_ui_image_ex(plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(gptCtx->tStyle.tItemSpacing.y * 2.0f);

    const plVec2 tFinalPos = pl_add_vec2(tStartPos, tSize);
    pl_add_image_ex(ptWindow->ptFgLayer, tTexture, tStartPos, tFinalPos, tUv0, tUv1, tTintColor);

    if(tBorderColor.a > 0.0f)
        pl_add_rect(ptWindow->ptFgLayer, tStartPos, tFinalPos, tBorderColor, 1.0f);

    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay)
{

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl__ui_calculate_item_size(pl__ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

    if(tSize.y == 0.0f) tSize.y = tWidgetSize.y;
    if(tSize.x < 0.0f) tSize.x = tWidgetSize.x;
    pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, tSize), gptCtx->tStyle.tFrameBgCol);
    pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){tSize.x * fFraction, tSize.y}), gptCtx->tStyle.tProgressBarCol);

    const char* pcTextPtr = pcOverlay;
    
    if(pcOverlay == NULL)
    {
        static char acBuffer[32] = {0};
        pl_sprintf(acBuffer, "%.1f%%", 100.0f * fFraction);
        pcTextPtr = acBuffer;
    }

    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcTextPtr, -1.0f);
    plRect tTextBounding = pl_calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcTextPtr, pl__find_renderered_text_end(pcTextPtr, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    plVec2 tTextStartPos = {
        .x = tStartPos.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + tSize.x * fFraction,
        .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
    };

    if(tTextStartPos.x + tTextSize.x > tStartPos.x + tSize.x)
        tTextStartPos.x = tStartPos.x + tSize.x - tTextSize.x - gptCtx->tStyle.tInnerSpacing.x;

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcTextPtr, -1.0f);

    const bool bHovered = pl_is_mouse_hovering_rect(tStartPos, pl_add_vec2(tStartPos, tWidgetSize)) && ptWindow == gptCtx->ptHoveredWindow;
    gptCtx->tPrevItemData.bHovered = bHovered;
    pl__ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_layout_dynamic(float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
        .uColumns         = uWidgetCount,
        .tRowPos          = ptWindow->tTempData.tNextRowStartPos,
        .fWidth           = 1.0f / (float)uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow; 
    ptWindow->tLayoutSystemType = PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC;     
}

void
pl_ui_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_STATIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
        .uColumns         = uWidgetCount,
        .tRowPos          = ptWindow->tTempData.tNextRowStartPos,
        .fWidth           = fWidth
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow; 
    ptWindow->tLayoutSystemType = PL_UI_LAYOUT_SYSTEM_TYPE_STATIC;        
}

void
pl_ui_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX,
        .uColumns         = uWidgetCount,
        .tRowPos          = ptWindow->tTempData.tNextRowStartPos
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
    ptWindow->tLayoutSystemType = PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX;
}

void
pl_ui_layout_row_push(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptWindow->tLayoutSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptCurrentRow->fWidth = fWidth;
}

void
pl_ui_layout_row_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptWindow->tLayoutSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptCurrentRow->tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);

    ptWindow->tTempData.tNextRowStartPos.y = ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
        .uColumns         = uWidgetCount,
        .tRowPos          = ptWindow->tTempData.tNextRowStartPos,
        .pfSizesOrRatios  = pfSizesOrRatios
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;    
    ptWindow->tLayoutSystemType = PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY;    
}

void
pl_ui_layout_template_begin(float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_NONE,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
        .uColumns         = 0,
        .tRowPos          = ptWindow->tTempData.tNextRowStartPos
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
    ptWindow->tLayoutSystemType = PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE;
}

void
pl_ui_layout_template_push_dynamic(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uDynamicEntryCount++;
    ptCurrentRow->atEntries[ptCurrentRow->uColumns].tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC;
    ptCurrentRow->atEntries[ptCurrentRow->uColumns].fWidth = 0.0f;
    ptCurrentRow->uColumns++;
}

void
pl_ui_layout_template_push_variable(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uVariableEntryCount++;
    ptCurrentRow->fWidth += fWidth;
    ptCurrentRow->atEntries[ptCurrentRow->uColumns].tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE;
    ptCurrentRow->atEntries[ptCurrentRow->uColumns].fWidth = fWidth;
    ptCurrentRow->uColumns++;
}

void
pl_ui_layout_template_push_static(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fWidth += fWidth;
    ptCurrentRow->uStaticEntryCount++;
    ptCurrentRow->atEntries[ptCurrentRow->uColumns].tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC;
    ptCurrentRow->atEntries[ptCurrentRow->uColumns].fWidth = fWidth;
    ptCurrentRow->uColumns++;
}

void
pl_ui_layout_template_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptWindow->tLayoutSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptCurrentRow->tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);

    ptWindow->tTempData.tNextRowStartPos.y = ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // total available width minus padding/spacing
    float fTotalWidth = 0.0f;
    if(ptWindow->bScrollbarY)
        fTotalWidth = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
    else
        fTotalWidth = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);


    // remove static widths
    for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
    {
        if(ptCurrentRow->atEntries[i].tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC)
            fTotalWidth -= ptCurrentRow->atEntries[i].fWidth;
    }

    const float fDynamicRawWidth = fTotalWidth / (ptCurrentRow->uDynamicEntryCount + ptCurrentRow->uVariableEntryCount);

    // assign initial widths & check how much width is needed
    float fWidthNeeded = 0.0f;
    float fExtraVariableWidth = 0.0f;
    for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
    {
        if(ptCurrentRow->atEntries[i].tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
        {
            ptCurrentRow->atEntries[i].fWidth = fDynamicRawWidth;
        }
        else if(ptCurrentRow->atEntries[i].tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE)
        {
            if(ptCurrentRow->atEntries[i].fWidth > fDynamicRawWidth)
            {
                fWidthNeeded += (ptCurrentRow->atEntries[i].fWidth - fDynamicRawWidth);
            }
            else
            {
                fExtraVariableWidth += fDynamicRawWidth - ptCurrentRow->atEntries[i].fWidth;
            }
        }
    }

    // see how much variable widths can contribute
    if(fWidthNeeded > 0.0f)
    {

        fWidthNeeded = fWidthNeeded - fExtraVariableWidth;
        float fDynamicWidth = fWidthNeeded / (float)ptCurrentRow->uDynamicEntryCount;
        for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
        {
            if(ptCurrentRow->atEntries[i].tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                ptCurrentRow->atEntries[i].fWidth = pl_maxf(fDynamicRawWidth - fDynamicWidth, 10.0f);
        }

    }
    else // evenly distribute
    {
        for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
        {
            if(ptCurrentRow->atEntries[i].tType != PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC)
            {
                ptCurrentRow->atEntries[i].fWidth = fDynamicRawWidth;
            }
        } 
    }
}

void
pl_ui_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fHeight : 1.0f,
        .tType            = tType,
        .uColumns         = uWidgetCount,
        .tRowPos          = ptWindow->tTempData.tNextRowStartPos
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
    ptWindow->tLayoutSystemType = PL_UI_LAYOUT_SYSTEM_TYPE_SPACE;
}

void
pl_ui_layout_space_push(float fX, float fY, float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    PL_ASSERT(ptWindow->tLayoutSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptCurrentRow->fHorizontalOffset = ptCurrentRow->tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fX * ptWindow->tSize.x : fX;
    ptCurrentRow->fVerticalOffset = fY * ptCurrentRow->fSpecifiedHeight;
    ptCurrentRow->fWidth = fWidth;
    ptCurrentRow->fHeight = fHeight * ptCurrentRow->fSpecifiedHeight;
}

void
pl_ui_layout_space_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptWindow->tLayoutSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptCurrentRow->tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);

    ptWindow->tTempData.tNextRowStartPos.y = ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

bool
pl_ui_was_last_item_hovered(void)
{
    return gptCtx->tPrevItemData.bHovered;
}

bool
pl_ui_was_last_item_active(void)
{
    return gptCtx->tPrevItemData.bActive;
}

int
pl_ui_get_int(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return iDefaultValue;
    return ptIterator->iValue;
}

float
pl_ui_get_float(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return fDefaultValue;
    return ptIterator->fValue;
}

bool
pl_ui_get_bool(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return pl_ui_get_int(ptStorage, uKey, bDefaultValue ? 1 : 0) != 0;
}

void*
pl_ui_get_ptr(plUiStorage* ptStorage, uint32_t uKey)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return NULL;
    return ptIterator->pValue;    
}

int*
pl_ui_get_int_ptr(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->iValue;
}

float*
pl_ui_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->fValue;
}

bool*
pl_ui_get_bool_ptr(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return (bool*)pl_ui_get_int_ptr(ptStorage, uKey, bDefaultValue ? 1 : 0);
}

void**
pl_ui_get_ptr_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->pValue;
}

void
pl_ui_set_int(plUiStorage* ptStorage, uint32_t uKey, int iValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iValue}));
        return;
    }
    ptIterator->iValue = iValue;
}

void
pl_ui_set_float(plUiStorage* ptStorage, uint32_t uKey, float fValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fValue}));
        return;
    }
    ptIterator->fValue = fValue;
}

void
pl_ui_set_bool(plUiStorage* ptStorage, uint32_t uKey, bool bValue)
{
    pl_ui_set_int(ptStorage, uKey, bValue ? 1 : 0);
}

void
pl_ui_set_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pValue}));
        return;
    }
    ptIterator->pValue = pValue;    
}

void
pl_ui_debug(bool* pbOpen)
{
    static bool bShowWindowOuterRect = false;
    static bool bShowWindowOuterClippedRect = false;
    static bool bShowWindowInnerRect = false;
    static bool bShowWindowInnerClipRect = false;

    if(pl_ui_begin_window("Pilot Light UI Metrics/Debugger", pbOpen, false))
    {

        plIOContext* ptIOCtx = pl_get_io_context();

        const float pfRatios[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);

        pl_ui_separator();

        pl_ui_checkbox("Show Window Inner Rect", &bShowWindowInnerRect);
        pl_ui_checkbox("Show Window Inner Clip Rect", &bShowWindowInnerClipRect);
        pl_ui_checkbox("Show Window Outer Rect", &bShowWindowOuterRect);
        pl_ui_checkbox("Show Window Outer Rect Clipped", &bShowWindowOuterClippedRect);

        for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
        {
            const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

            if(ptWindow->bActive)
            {
                if(bShowWindowInnerRect)
                    pl_add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerRect.tMin, ptWindow->tInnerRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowInnerClipRect)
                    pl_add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerClipRect.tMin, ptWindow->tInnerClipRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowOuterRect)
                    pl_add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowOuterClippedRect)
                    pl_add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRectClipped.tMin, ptWindow->tOuterRectClipped.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
            }
        }
        
        pl_ui_separator();

        if(pl_ui_tree_node("Windows"))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

                if(pl_ui_tree_node(ptWindow->pcName))
                {
                    
                    if(pl_ui_tree_node("Draw Layers"))
                    {
                        if(pl_ui_tree_node_f("Foreground %d vtx, %d indices, %d cmds", ptWindow->ptFgLayer->vertexCount, pl_sb_size(ptWindow->ptFgLayer->sbIndexBuffer), pl_sb_size(ptWindow->ptFgLayer->sbCommandBuffer)))
                        {
                            for(uint32_t i = 0; i < pl_sb_size(ptWindow->ptFgLayer->sbCommandBuffer); i++)
                            {
                                const plDrawCommand* ptDrawCmd = &ptWindow->ptFgLayer->sbCommandBuffer[i];
                                if(pl_ui_tree_node_f("Cmd: %d tris, ClipRect(%0.1f, %0.1f)-(%0.1f, %0.1f)", ptDrawCmd->elementCount / 3, ptDrawCmd->tClip.tMin.x, ptDrawCmd->tClip.tMin.y, ptDrawCmd->tClip.tMax.x, ptDrawCmd->tClip.tMax.y))
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        pl_add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                    pl_ui_tree_pop();
                                }
                                else
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        pl_add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                }
                            }
                            
                            pl_ui_tree_pop(); 
                        }
                        if(pl_ui_tree_node_f("Background %d vtx, %d indices, %d cmds", ptWindow->ptBgLayer->vertexCount, pl_sb_size(ptWindow->ptBgLayer->sbIndexBuffer), pl_sb_size(ptWindow->ptBgLayer->sbCommandBuffer)))
                        {
                            for(uint32_t i = 0; i < pl_sb_size(ptWindow->ptBgLayer->sbCommandBuffer); i++)
                            {
                                const plDrawCommand* ptDrawCmd = &ptWindow->ptBgLayer->sbCommandBuffer[i];
                                if(pl_ui_tree_node_f("Cmd: %d tris, ClipRect(%0.1f, %0.1f)-(%0.1f, %0.1f)", ptDrawCmd->elementCount / 3, ptDrawCmd->tClip.tMin.x, ptDrawCmd->tClip.tMin.y, ptDrawCmd->tClip.tMax.x, ptDrawCmd->tClip.tMax.y))
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        pl_add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                    pl_ui_tree_pop();
                                }
                                else
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        pl_add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                }
                            }
                            
                            pl_ui_tree_pop(); 
                        }
                        pl_ui_tree_pop(); 
                    }

                    pl_ui_text(" - Pos:          (%0.1f, %0.1f)", ptWindow->tPos.x, ptWindow->tPos.y);
                    pl_ui_text(" - Size:         (%0.1f, %0.1f)", ptWindow->tSize.x, ptWindow->tSize.y);
                    pl_ui_text(" - Content Size: (%0.1f, %0.1f)", ptWindow->tContentSize.x, ptWindow->tContentSize.y);
                    pl_ui_text(" - Min Size:     (%0.1f, %0.1f)", ptWindow->tMinSize.x, ptWindow->tMinSize.y);
                    pl_ui_text(" - Scroll:       (%0.1f/%0.1f, %0.1f/%0.1f)", ptWindow->tScroll.x, ptWindow->tScrollMax.x, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_ui_text(" - Active:       %s", ptWindow->uId == gptCtx->uActiveWindowId ? "1" : "0");
                    pl_ui_text(" - Hovered:      %s", ptWindow->uId == gptCtx->uHoveredWindowId ? "1" : "0");
                    pl_ui_text(" - Dragging:     %s", ptWindow == gptCtx->ptMovingWindow ? "1" : "0");
                    pl_ui_text(" - Scrolling:    %s", ptWindow == gptCtx->ptWheelingWindow ? "1" : "0");
                    pl_ui_text(" - Resizing:     %s", ptWindow == gptCtx->ptSizingWindow ? "1" : "0");
                    pl_ui_text(" - Collapsed:    %s", ptWindow->bCollapsed ? "1" : "0");
                    pl_ui_text(" - Auto Sized:   %s", ptWindow->tFlags &  PL_UI_WINDOW_FLAGS_AUTO_SIZE ? "1" : "0");

                    pl_ui_tree_pop();
                }  
            }
            pl_ui_tree_pop();
        }
        if(pl_ui_tree_node("Internal State"))
        {
            pl_ui_text("Windows");
            pl_ui_indent(0.0f);
            pl_ui_text("Hovered Window: %s", gptCtx->ptHoveredWindow ? gptCtx->ptHoveredWindow->pcName : "NULL");
            pl_ui_text("Moving Window:  %s", gptCtx->ptMovingWindow ? gptCtx->ptMovingWindow->pcName : "NULL");
            pl_ui_unindent(0.0f);
            pl_ui_text("Items");
            pl_ui_indent(0.0f);
            pl_ui_text("Active ID:      %u", gptCtx->uActiveId);
            pl_ui_text("Hovered ID:     %u", gptCtx->uHoveredId);
            pl_ui_unindent(0.0f);
            pl_ui_tree_pop();
        }
        pl_ui_end_window();
    } 
}

void
pl_ui_style(bool* pbOpen)
{

    if(pl_ui_begin_window("Pilot Light UI Style", pbOpen, false))
    {

        const float pfRatios[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        plUiStyle* ptStyle = &gptCtx->tStyle;

        if(pl_ui_begin_tab_bar("Tabs"))
        {
            if(pl_ui_begin_tab("Colors"))
            { 
                pl_ui_end_tab();
            }
            
            if(pl_ui_begin_tab("Sizes"))
            {
                pl_ui_vertical_spacing();
                pl_ui_text("Title");
                pl_ui_slider_float("Title Padding", &ptStyle->fTitlePadding, 0.0f, 32.0f);

                pl_ui_vertical_spacing();
                pl_ui_text("Window");
                pl_ui_slider_float("Horizontal Padding## window", &ptStyle->fWindowHorizontalPadding, 0.0f, 32.0f);
                pl_ui_slider_float("Vertical Padding## window", &ptStyle->fWindowVerticalPadding, 0.0f, 32.0f);

                pl_ui_vertical_spacing();
                pl_ui_text("Scrollbar");
                pl_ui_slider_float("Size##scrollbar", &ptStyle->fScrollbarSize, 0.0f, 32.0f);
                
                pl_ui_vertical_spacing();
                pl_ui_text("Misc");
                pl_ui_slider_float("Indent", &ptStyle->fIndentSize, 0.0f, 32.0f); 
                pl_ui_slider_float("Slider Size", &ptStyle->fSliderSize, 3.0f, 32.0f); 
                pl_ui_slider_float("Font Size", &ptStyle->fFontSize, 13.0f, 48.0f); 
                pl_ui_end_tab();
            }
            pl_ui_end_tab_bar();
        }     
        pl_ui_end_window();
    }  
}

void
pl_ui_demo(bool* pbOpen)
{
    if(pl_ui_begin_window("UI Demo", pbOpen, false))
    {

        static const float pfRatios0[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios0);

        pl_ui_text("Pilot Light UI v%s", PL_UI_VERSION);

        if(pl_ui_collapsing_header("Help"))
        {
            pl_ui_text("Under construction");
            pl_ui_end_collapsing_header();
        }
    
        if(pl_ui_collapsing_header("Window Options"))
        {
            pl_ui_text("Under construction");
            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Widgets"))
        {
            if(pl_ui_tree_node("Basic"))
            {

                pl_ui_layout_static(0.0f, 100, 2);
                pl_ui_button("Button");
                pl_ui_checkbox("Checkbox", NULL);

                pl_ui_layout_dynamic(0.0f, 2);
                pl_ui_button("Button");
                pl_ui_checkbox("Checkbox", NULL);

                static int iValue = 0;
                pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

                pl_ui_layout_row_push(0.33f);
                pl_ui_radio_button("Option 1", &iValue, 0);

                pl_ui_layout_row_push(0.33f);
                pl_ui_radio_button("Option 2", &iValue, 1);

                pl_ui_layout_row_push(0.34f);
                pl_ui_radio_button("Option 3", &iValue, 2);

                pl_ui_layout_row_end();

                const float pfRatios[] = {1.0f};
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                pl_ui_separator();
                pl_ui_labeled_text("Label", "Value");
                static int iValue1 = 0;
                static float fValue1 = 23.0f;
                static float fValue2 = 100.0f;
                static int iValue2 = 3;
                pl_ui_slider_float("float slider 1", &fValue1, 0.0f, 100.0f);
                pl_ui_slider_float("float slider 2", &fValue2, -50.0f, 100.0f);
                pl_ui_slider_int("int slider 1", &iValue1, 0, 10);
                pl_ui_slider_int("int slider 2", &iValue2, -5, 10);
                pl_ui_drag_float("float drag", &fValue2, 1.0f, -100.0f, 100.0f);

                const float pfRatios22[] = {100.0f, 120.0f};
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios22);
                pl_ui_button("Hover me!");
                if(pl_ui_was_last_item_hovered())
                {
                    pl_ui_begin_tooltip();
                    pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios22);
                    pl_ui_text("I'm a tooltip!");
                    pl_ui_end_tooltip();
                }
                pl_ui_button("Just a button");

                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Selectables"))
            {
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                pl_ui_selectable("Selectable 1", &bSelectable0);
                pl_ui_selectable("Selectable 2", &bSelectable1);
                pl_ui_selectable("Selectable 3", &bSelectable2);
                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Plotting"))
            {
                pl_ui_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Trees"))
            {
                
                if(pl_ui_tree_node("Root Node"))
                {
                    if(pl_ui_tree_node("Child 1"))
                    {
                        pl_ui_button("Press me");
                        pl_ui_tree_pop();
                    }
                    if(pl_ui_tree_node("Child 2"))
                    {
                        pl_ui_button("Press me");
                        pl_ui_tree_pop();
                    }
                    pl_ui_tree_pop();
                }
                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Tabs"))
            {
                if(pl_ui_begin_tab_bar("Tabs1"))
                {
                    if(pl_ui_begin_tab("Tab 0"))
                    {
                        static bool bSelectable0 = false;
                        static bool bSelectable1 = false;
                        static bool bSelectable2 = false;
                        pl_ui_selectable("Selectable 1", &bSelectable0);
                        pl_ui_selectable("Selectable 2", &bSelectable1);
                        pl_ui_selectable("Selectable 3", &bSelectable2);
                        pl_ui_end_tab();
                    }

                    if(pl_ui_begin_tab("Tab 1"))
                    {
                        static int iValue = 0;
                        pl_ui_radio_button("Option 1", &iValue, 0);
                        pl_ui_radio_button("Option 2", &iValue, 1);
                        pl_ui_radio_button("Option 3", &iValue, 2);
                        pl_ui_end_tab();
                    }

                    if(pl_ui_begin_tab("Tab 2"))
                    {
                        if(pl_ui_begin_child("CHILD2"))
                        {
                            const float pfRatios3[] = {600.0f};
                            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                            for(uint32_t i = 0; i < 25; i++)
                                pl_ui_text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
                        }
                        pl_ui_end_child();
                        pl_ui_end_tab();
                    }
                    pl_ui_end_tab_bar();
                }
                pl_ui_tree_pop();
            }
            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Layout & Scrolling"))
        {
            const float pfRatios2[] = {0.5f, 0.50f};
            const float pfRatios3[] = {600.0f};
            
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2, pfRatios2);
            if(pl_ui_begin_child("CHILD"))
            {
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);

                for(uint32_t i = 0; i < 25; i++)
                {
                    pl_ui_text("Label");
                    pl_ui_text("Value");
                }
            }
            pl_ui_end_child();

            if(pl_ui_begin_child("CHILD2"))
            {
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                for(uint32_t i = 0; i < 25; i++)
                    pl_ui_text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
            }
            pl_ui_end_child();

            pl_ui_end_collapsing_header();
        }


        if(pl_ui_collapsing_header("Testing 0"))
        {
            // first row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 60.0f, 2);

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 60!");

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 40");

            pl_ui_layout_row_end();

            // space
            pl_ui_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

            pl_ui_layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
            pl_ui_button("Hover me A!");

            pl_ui_layout_space_push(105.0f, 105.0f, 100.0f, 100.0f);
            pl_ui_button("Hover me B!");

            pl_ui_layout_space_end();

            // space
            pl_ui_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 500.0f, 2);

            pl_ui_layout_space_push(0.0f, 0.0f, 0.5f, 0.1f);
            pl_ui_button("Hover me AA!");

            pl_ui_layout_space_push(0.25f, 0.50f, 0.5f, 0.1f);
            pl_ui_button("Hover me BB!");

            pl_ui_layout_space_end();

            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Testing 1"))
        {
            // first row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 60.0f, 2);

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 60!");

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 40");

            pl_ui_layout_row_end();

            // second row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 100.0f, 3);

            pl_ui_layout_row_push(100.0f);
            pl_ui_button("Hover me 100!");

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 200");

            pl_ui_layout_row_push(300.0f);
            pl_ui_button("Hover me 300");

            pl_ui_layout_row_end();


            // third row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

            pl_ui_layout_row_push(0.33f);
            pl_ui_button("Hover me 100!");

            pl_ui_layout_row_push(0.33f);
            pl_ui_button("Hover me 200");

            pl_ui_layout_row_push(0.34f);
            pl_ui_button("Hover me 300");

            pl_ui_layout_row_end();

            // fourth & fifth row
            const float pfRatios33[] = {0.25f, 0.25f, 0.50f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, pfRatios33);

            // row 4
            pl_ui_button("Hover me 100a!");
            pl_ui_button("Hover me 200a");
            pl_ui_button("Hover me 300a");

            // row 5
            pl_ui_button("Hover me 100b!");
            pl_ui_button("Hover me 200b");
            pl_ui_button("Hover me 300b");

            // sixth
            const float pfRatios2[] = {100.0f, 100.0f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios2);

            // row 6
            pl_ui_button("Hover me 100c!");
            pl_ui_button("Hover me 200c");

            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Testing 2"))
        {
            pl_ui_layout_template_begin(60.0f);
            pl_ui_layout_template_push_static(100.0f);
            pl_ui_layout_template_push_variable(500.0f);
            pl_ui_layout_template_push_dynamic();
            pl_ui_layout_template_end();

            pl_ui_button("static");
            pl_ui_button("variable");
            pl_ui_button("dynamic");

            pl_ui_button("static");
            pl_ui_button("variable");
            pl_ui_button("dynamic");

            pl_ui_layout_template_begin(30.0f);
            pl_ui_layout_template_push_static(100.0f);
            pl_ui_layout_template_push_dynamic();
            pl_ui_layout_template_push_variable(500.0f);
            pl_ui_layout_template_end();

            pl_ui_button("static");
            pl_ui_button("dynamic");
            pl_ui_button("variable");
            
            pl_ui_button("static##11");
            pl_ui_button("dynamic##11");
            pl_ui_button("variable##11");

            pl_ui_layout_template_begin(0.0f);
            pl_ui_layout_template_push_variable(500.0f);
            pl_ui_layout_template_push_dynamic();
            pl_ui_layout_template_push_static(100.0f);
            pl_ui_layout_template_end();

            
            pl_ui_button("variable##11");
            pl_ui_button("dynamic##11");
            pl_ui_button("static##11");

            pl_ui_button("variable##11");
            pl_ui_button("dynamic##11");
            pl_ui_button("static##11");

            pl_ui_end_collapsing_header();
        }
        pl_ui_end_window();
    }
    
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static bool
pl__ui_is_item_hoverable(const plRect* ptBox, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow == gptCtx->ptMovingWindow || ptWindow == gptCtx->ptSizingWindow || ptWindow == gptCtx->ptWheelingWindow || !gptCtx->bMouseOwned)
        return false;

    if(!pl_is_mouse_hovering_rect(ptBox->tMin, ptBox->tMax))
        return false;

    if(ptWindow != gptCtx->ptHoveredWindow)
        return false;

    return true;
}

static bool
pl__ui_is_item_hoverable_circle(plVec2 tP, float fRadius, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow == gptCtx->ptMovingWindow || ptWindow == gptCtx->ptSizingWindow || ptWindow == gptCtx->ptWheelingWindow || !gptCtx->bMouseOwned)
        return false;

    if(!pl__ui_does_circle_contain_point(tP, fRadius, pl_get_mouse_pos()))
        return false;

    if(ptWindow != gptCtx->ptHoveredWindow)
        return false;

    return true;
}

static const char*
pl__find_renderered_text_end(const char* pcText, const char* pcTextEnd)
{
    const char* pcTextDisplayEnd = pcText;
    if (!pcTextEnd)
        pcTextEnd = (const char*)-1;

    while (pcTextDisplayEnd < pcTextEnd && *pcTextDisplayEnd != '\0' && (pcTextDisplayEnd[0] != '#' || pcTextDisplayEnd[1] != '#'))
        pcTextDisplayEnd++;
    return pcTextDisplayEnd;
}

static void
pl__add_text(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    pl_add_text_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x), roundf(tP.y)}, tColor, pcText, pl__find_renderered_text_end(pcText, pcTextEnd), fWrap);
}

static void
pl__add_clipped_text(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    pl_add_text_clipped_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x + 0.5f), roundf(tP.y + 0.5f)}, tMin, tMax, tColor, pcText, pl__find_renderered_text_end(pcText, pcTextEnd), fWrap);   
}

static plVec2
pl__calculate_text_size(plFont* font, float size, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    return pl_calculate_text_size_ex(font, size, text, pl__find_renderered_text_end(text, pcTextEnd), wrap);  
}

static bool
pl__ui_button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    gptCtx->tPrevItemData.bActive = false;

    bool bPressed = false;
    bool bHovered = pl__ui_is_item_hoverable(ptBox, uHash) && (gptCtx->uActiveId == uHash || gptCtx->uActiveId == 0);

    if(bHovered)
        gptCtx->uNextHoveredId = uHash;

    bool bHeld = bHovered && pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT);

    if(bHeld && uHash == gptCtx->uActiveId)
        gptCtx->uNextActiveId = uHash;

    if(bHovered)
    {
        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            gptCtx->uNextActiveId = uHash;
            gptCtx->tPrevItemData.bActive = true;
        }
        else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
        {
            gptCtx->uNextActiveId = 0;
            bPressed = uHash == gptCtx->uActiveId;
        }
    }

    *pbOutHeld = bHeld;
    *pbOutHovered = bHovered;
    gptCtx->tPrevItemData.bHovered = bHovered;
    return bPressed;
}

static bool
pl__ui_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point)
{
    bool b1 = ((point.x - p1.x) * (p0.y - p1.y) - (point.y - p1.y) * (p0.x - p1.x)) < 0.0f;
    bool b2 = ((point.x - p2.x) * (p1.y - p2.y) - (point.y - p2.y) * (p1.x - p2.x)) < 0.0f;
    bool b3 = ((point.x - p0.x) * (p2.y - p0.y) - (point.y - p0.y) * (p2.x - p0.x)) < 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

static plUiStorageEntry*
pl__lower_bound(plUiStorageEntry* sbtData, uint32_t uKey)
{
    plUiStorageEntry* ptFirstEntry = sbtData;
    uint32_t uCount = pl_sb_size(sbtData);
    while (uCount > 0)
    {
        uint32_t uCount2 = uCount >> 1;
        plUiStorageEntry* ptMiddleEntry = ptFirstEntry + uCount2;
        if(ptMiddleEntry->uKey < uKey)
        {
            ptFirstEntry = ++ptMiddleEntry;
            uCount -= uCount2 + 1;
        }
        else
            uCount = uCount2;
    }

    return ptFirstEntry;
}

static bool
pl_ui_begin_window_ex(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    
    plUiWindow* ptWindow = NULL;                          // window we are working on
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow; // parent window if there any

    // generate hashed ID
    const uint32_t uWindowID = pl_str_hash(pcName, 0, ptParentWindow ? pl_sb_top(gptCtx->sbuIdStack) : 0);
    pl_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text & title bar sizes
    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcName, 0.0f);
    const float fTitleBarHeight = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist in storage
    ptWindow = pl_ui_get_ptr(&gptCtx->tWindows, uWindowID);

    // new window needs to be created
    if(ptWindow == NULL)
    {
        // allocate new window
        ptWindow = PL_ALLOC(sizeof(plUiWindow));
        memset(ptWindow, 0, sizeof(plUiWindow));
        ptWindow->uId                     = uWindowID;
        ptWindow->pcName                  = pcName;
        ptWindow->tPos                    = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMinSize                = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMaxSize                = (plVec2){ 10000.0f, 10000.0f};
        ptWindow->tSize                   = (plVec2){ 500.0f, 500.0f};
        ptWindow->ptBgLayer               = pl_request_draw_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->ptFgLayer               = pl_request_draw_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->tPosAllowableFlags      = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tSizeAllowableFlags     = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tCollapseAllowableFlags = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->ptParentWindow          = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? ptParentWindow : ptWindow;
        ptWindow->uFocusOrder             = pl_sb_size(gptCtx->sbtFocusedWindows);
        ptWindow->tFlags                  = PL_UI_WINDOW_FLAGS_NONE;

        // add to focused windows
        if(!(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW))
            pl_sb_push(gptCtx->sbtFocusedWindows, ptWindow);

        // add window to storage
        pl_ui_set_ptr(&gptCtx->tWindows, uWindowID, ptWindow);
    }

    // seen this frame (obviously)
    ptWindow->bActive = true;
    ptWindow->tFlags = tFlags;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {

        plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
        const plVec2 tStartPos   = pl__ui_get_start_pos(ptCurrentRow);

        // set window position to parent window current cursor
        ptWindow->tPos = tStartPos;

        pl_sb_push(ptParentWindow->sbtChildWindows, ptWindow);
    }

    // reset per frame window temporary data (preserve child window so we can reset properly)
    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));
    pl_sb_reset(ptWindow->sbtChildWindows);
    pl_sb_reset(ptWindow->sbtRowStack);

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // should window collapse
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED)
    {
        if(ptWindow->tCollapseAllowableFlags & gptCtx->tNextWindowData.tCollapseCondition)
        {
            ptWindow->bCollapsed = true;
            ptWindow->tCollapseAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    // position & size
    const plVec2 tMousePos = pl_get_mouse_pos();
    plVec2 tStartPos = ptWindow->tPos;

    // next window calls
    bool bWindowSizeSet = false;
    bool bWindowPosSet = false;
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS)
    {
        bWindowPosSet = ptWindow->tPosAllowableFlags & gptCtx->tNextWindowData.tPosCondition;
        if(bWindowPosSet)
        {
            tStartPos = gptCtx->tNextWindowData.tPos;
            ptWindow->tPos = tStartPos;
            ptWindow->tPosAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE)
    {
        bWindowSizeSet = ptWindow->tSizeAllowableFlags & gptCtx->tNextWindowData.tSizeCondition;
        if(bWindowSizeSet)
        {
            ptWindow->tSize = gptCtx->tNextWindowData.tSize;
            if(ptWindow->tSize.x < 0.0f && ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) 
                ptWindow->tSize.x = -(ptWindow->tPos.x - ptParentWindow->tPos.x) + ptParentWindow->tSize.x - ptWindow->tSize.x - (ptParentWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f) - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f);
            if(ptWindow->tSize.y < 0.0f && ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) 
                ptWindow->tSize.y = -(ptWindow->tPos.y - ptParentWindow->tPos.y) + ptParentWindow->tSize.y - ptWindow->tSize.y - (ptParentWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f) - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f);
            
            ptWindow->tSizeAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(ptWindow->bCollapsed)
        ptWindow->tSize = (plVec2){ptWindow->tSize.x, fTitleBarHeight};

    // updating outer rect here but autosized windows do so again in pl_ui_end_window(..)
    ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
    ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
    ptWindow->tInnerRect = ptWindow->tOuterRect;

    // remove scrollbars from inner rect
    if(ptWindow->bScrollbarX)
        ptWindow->tInnerRect.tMax.y -= gptCtx->tStyle.fScrollbarSize + 2.0f;
    if(ptWindow->bScrollbarY)
        ptWindow->tInnerRect.tMax.x -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    // decorations
    if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR))
    {

        ptWindow->tInnerRect.tMin.y += fTitleBarHeight;

        // draw title bar
        plVec4 tTitleColor;
        if(ptWindow->uId == gptCtx->uActiveWindowId)
            tTitleColor = gptCtx->tStyle.tTitleActiveCol;
        else if(ptWindow->bCollapsed)
            tTitleColor = gptCtx->tStyle.tTitleBgCollapsedCol;
        else
            tTitleColor = gptCtx->tStyle.tTitleBgCol;
        pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x, fTitleBarHeight}), tTitleColor);

        // draw title text
        const plVec2 titlePos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
        pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, titlePos, gptCtx->tStyle.tTextCol, pcName, 0.0f);

        // draw close button
        const float fTitleBarButtonRadius = 8.0f;
        float fTitleButtonStartPos = fTitleBarButtonRadius * 2.0f;
        if(pbOpen)
        {
            plVec2 tCloseCenterPos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;
            if(pl__ui_does_circle_contain_point(tCloseCenterPos, fTitleBarButtonRadius, tMousePos) && gptCtx->ptHoveredWindow == ptWindow)
            {
                pl_add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 12);
                if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false)) gptCtx->uActiveId = 1;
                else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT)) *pbOpen = false;       
            }
            else
                pl_add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.0f, 0.0f, 1.0f}, 12);
        }

        if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        {
            // draw collapse button
            plVec2 tCollapsingCenterPos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;

            if(pl__ui_does_circle_contain_point(tCollapsingCenterPos, fTitleBarButtonRadius, tMousePos) &&  gptCtx->ptHoveredWindow == ptWindow)
            {
                pl_add_circle_filled(ptWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 12);

                if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
                {
                    gptCtx->uActiveId = 2;
                }
                else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
                {
                    ptWindow->bCollapsed = !ptWindow->bCollapsed;
                    if(!ptWindow->bCollapsed)
                    {
                        ptWindow->tSize = ptWindow->tFullSize;
                        if(tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE)
                            ptWindow->uHideFrames = 2;
                    }
                }
            }
            else
                pl_add_circle_filled(ptWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.5f, 0.0f, 1.0f}, 12);
        }

    }

    // remove padding for inner clip rect
    ptWindow->tInnerClipRect = pl_rect_expand_vec2(&ptWindow->tInnerRect, (plVec2){-gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});

    if(!ptWindow->bCollapsed)
    {
        const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight };

        const plVec2 tInnerClip = { 
            ptWindow->tSize.x - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f),
            ptWindow->tSize.y - fTitleBarHeight - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f)
        };

        if(pl_sb_size(gptCtx->ptDrawlist->sbClipStack) > 0)
        {
            ptWindow->tInnerClipRect = pl_rect_clip_full(&ptWindow->tInnerClipRect, &pl_sb_back(gptCtx->ptDrawlist->sbClipStack));
            ptWindow->tOuterRectClipped = pl_rect_clip_full(&ptWindow->tOuterRectClipped, &pl_sb_back(gptCtx->ptDrawlist->sbClipStack));
        }
        pl_push_clip_rect(gptCtx->ptDrawlist, ptWindow->tInnerClipRect, false);

    }

    // update cursors
    ptWindow->tTempData.tCursorStartPos.x = gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - ptWindow->tScroll.x;
    ptWindow->tTempData.tCursorStartPos.y = gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - ptWindow->tScroll.y;
    ptWindow->tTempData.tNextRowStartPos = ptWindow->tTempData.tCursorStartPos;
    ptWindow->tTempData.tCursorStartPos = pl_floor_vec2(ptWindow->tTempData.tCursorStartPos);

    // reset next window flags
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;

    gptCtx->ptCurrentWindow = ptWindow;
    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        ptWindow->bVisible = pl_rect_overlaps_rect(&ptWindow->tInnerClipRect, &ptParentWindow->tInnerClipRect);
        return ptWindow->bVisible && !pl_rect_is_inverted(&ptWindow->tInnerClipRect);
    }

    ptWindow->bVisible = true;
    return !ptWindow->bCollapsed;
}

static void
pl__ui_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis)
{
    const plRect tParentBgRect = ptWindow->ptParentWindow->tOuterRect;
    if(tAxis == PL_UI_AXIS_X)
    {
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);

        const float fScrollbarHandleSize  = floorf((ptWindow->tSize.x - fRightSidePadding) * ((ptWindow->tSize.x - fRightSidePadding) / (ptWindow->tContentSize.x)));
        const float fScrollbarHandleStart = floorf((ptWindow->tSize.x - fRightSidePadding - fScrollbarHandleSize) * (ptWindow->tScroll.x/(ptWindow->tScrollMax.x)));
        
        const plVec2 tStartPos = pl_add_vec2(ptWindow->tPos, (plVec2){fScrollbarHandleStart, ptWindow->tSize.y - gptCtx->tStyle.fScrollbarSize - 2.0f});
        
        plRect tScrollBackground = {
            pl_add_vec2(ptWindow->tPos, (plVec2){0.0f, ptWindow->tSize.y - gptCtx->tStyle.fScrollbarSize - 2.0f}),
            pl_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x - fRightSidePadding, ptWindow->tSize.y - 2.0f})
        };

        if(pl_rect_overlaps_rect(&tParentBgRect, &tScrollBackground))
        {
            const plVec2 tFinalSize = {fScrollbarHandleSize, gptCtx->tStyle.fScrollbarSize};
            plRect tHandleBox = pl_calculate_rect(tStartPos, tFinalSize);
            tScrollBackground = pl_rect_clip(&tScrollBackground, &ptWindow->tOuterRectClipped);
            tHandleBox = pl_rect_clip(&tHandleBox, &ptWindow->tOuterRectClipped);

            pl_add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tStyle.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);   
            if(gptCtx->uActiveId == uHash)
                pl_add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash)
                pl_add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.tScrollbarHoveredCol);
            else
                pl_add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.tScrollbarHandleCol);
        }
    }
    else if(tAxis == PL_UI_AXIS_Y)
    {
          
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fTopPadding = (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);

        const float fScrollbarHandleSize  = floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding) * ((ptWindow->tSize.y - fTopPadding - fBottomPadding) / (ptWindow->tContentSize.y)));
        const float fScrollbarHandleStart = floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding - fScrollbarHandleSize) * (ptWindow->tScroll.y / (ptWindow->tScrollMax.y)));

        const plVec2 tStartPos = pl_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x - gptCtx->tStyle.fScrollbarSize - 2.0f, fTopPadding + fScrollbarHandleStart});
        
        plRect tScrollBackground = pl_calculate_rect(pl_add_vec2(ptWindow->tPos, 
            (plVec2){ptWindow->tSize.x - gptCtx->tStyle.fScrollbarSize - 2.0f, fTopPadding}),
            (plVec2){gptCtx->tStyle.fScrollbarSize, ptWindow->tSize.y - fBottomPadding});

        if(pl_rect_overlaps_rect(&tParentBgRect, &tScrollBackground))
        {    

            const plVec2 tFinalSize = {gptCtx->tStyle.fScrollbarSize, fScrollbarHandleSize};
            plRect tHandleBox = pl_calculate_rect(tStartPos, tFinalSize);
            tScrollBackground = pl_rect_clip(&tScrollBackground, &ptWindow->tOuterRectClipped);
            tHandleBox = pl_rect_clip(&tHandleBox, &ptWindow->tOuterRectClipped);

            // scrollbar background
            pl_add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tStyle.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);

            // scrollbar handle
            if(gptCtx->uActiveId == uHash) 
                pl_add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tStyle.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash) 
                pl_add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tStyle.tScrollbarHoveredCol);
            else
                pl_add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tStyle.tScrollbarHandleCol);
        }
    }
}

static plVec2
pl__ui_calculate_item_size(float fDefaultHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    float fHeight = ptCurrentRow->fHeight;

    if(fHeight == 0.0f)
        fHeight = fDefaultHeight;

    if(ptCurrentRow->tSystemType ==  PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE)
    {
        const plUiLayoutRowEntry* ptCurrentEntry = &ptCurrentRow->atEntries[ptCurrentRow->uCurrentColumn];
        const plVec2 tWidgetSize = { ptCurrentEntry->fWidth, fHeight};
        return tWidgetSize;

    }
    else
    {
        // when passed array of sizes/ratios, override
        if(ptCurrentRow->pfSizesOrRatios)
            ptCurrentRow->fWidth = ptCurrentRow->pfSizesOrRatios[ptCurrentRow->uCurrentColumn];

        float fWidth = ptCurrentRow->fWidth;

        if(ptCurrentRow->tType ==  PL_UI_LAYOUT_ROW_TYPE_DYNAMIC) // width was a ratio
        {
            if(ptWindow->bScrollbarY)
                fWidth *= (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
            else
                fWidth *= (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
            
        }

        const plVec2 tWidgetSize = { fWidth, fHeight};
        return tWidgetSize;
    }
}

static void
pl__ui_advance_cursor(float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    ptCurrentRow->uCurrentColumn++;
    
    ptCurrentRow->fMaxWidth = pl_maxf(ptCurrentRow->fHorizontalOffset + fWidth, ptCurrentRow->fMaxWidth);
    ptCurrentRow->fMaxHeight = pl_maxf(ptCurrentRow->fMaxHeight, ptCurrentRow->fVerticalOffset + fHeight);

    // not yet at end of row
    if(ptCurrentRow->uCurrentColumn < ptCurrentRow->uColumns)
    {
        ptCurrentRow->fHorizontalOffset += fWidth + gptCtx->tStyle.tItemSpacing.x;
    }

    // automatic wrap
    if(ptCurrentRow->uCurrentColumn == ptCurrentRow->uColumns && ptWindow->tLayoutSystemType != PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        ptWindow->tTempData.tNextRowStartPos.y = ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptCurrentRow->tRowPos.x + ptCurrentRow->fMaxWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x);
        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptCurrentRow->tRowPos.y + ptCurrentRow->fMaxHeight, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);   
        ptCurrentRow->tRowPos = ptWindow->tTempData.tNextRowStartPos;

        // reset
        ptCurrentRow->uCurrentColumn = 0;
        ptCurrentRow->fMaxWidth = 0.0f;
        ptCurrentRow->fMaxHeight = 0.0f;
        ptCurrentRow->fHorizontalOffset = 0.0f + ptWindow->tTempData.fExtraIndent;
        ptCurrentRow->fVerticalOffset = 0.0f;
    }

    // passed end of row
    if(ptCurrentRow->uCurrentColumn > ptCurrentRow->uColumns && ptWindow->tLayoutSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        PL_ASSERT(false);
    }
}

static void
pl__ui_submit_window(plUiWindow* ptWindow)
{
    ptWindow->bActive = false; // no longer active (for next frame)

    const size_t ulCurrentFrame = pl_get_io_context()->ulFrameCount;
    // const plVec2 tMousePos = pl_get_mouse_pos();
    const float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    const plRect tTitleBarHitRegion = {
        .tMin = {ptWindow->tPos.x + 2.0f, ptWindow->tPos.y + 2.0f},
        .tMax = {ptWindow->tPos.x + ptWindow->tSize.x - 2.0f, ptWindow->tPos.y + fTitleBarHeight}
    };

    plRect tBoundBox = ptWindow->tOuterRectClipped;

    // add padding for resizing from borders
    if(!(ptWindow->tFlags & (PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_AUTO_SIZE)))
        tBoundBox = pl_rect_expand(&tBoundBox, 2.0f);

    // check if window is hovered
    if(pl_is_mouse_hovering_rect(tBoundBox.tMin, tBoundBox.tMax))
    {
        gptCtx->uHoveredWindowId = ptWindow->uId;
        gptCtx->ptHoveredWindow = ptWindow;
        gptCtx->bWantMouseNextFrame = true;

        // check if window is activated
        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            gptCtx->uActiveWindowId = ptWindow->ptParentWindow->uId;
            gptCtx->bActiveIdJustActivated = true;
            gptCtx->ptActiveWindow = ptWindow->ptParentWindow;

            gptCtx->bMouseOwned = true;
            gptCtx->bWantMouseNextFrame = true;

            // check if window titlebar is clicked
            if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) && pl_is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                    gptCtx->ptMovingWindow = ptWindow;
        }

        // scrolling
        if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE) && pl_get_mouse_wheel() != 0.0f)
            gptCtx->ptWheelingWindow = ptWindow;
    }

    pl_sb_push(gptCtx->sbptWindows, ptWindow);
    for(uint32_t j = 0; j < pl_sb_size(ptWindow->sbtChildWindows); j++)
        pl__ui_submit_window(ptWindow->sbtChildWindows[j]);
}