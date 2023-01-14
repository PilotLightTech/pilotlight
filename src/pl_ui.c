/*
   pl_ui.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] context
// [SECTION] internal enums
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
#include "pl_math.h"
#include "pl_string.h"
#include "pl_memory.h"
#include "pl_draw.h"

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

static plUiContext* gptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal enums
//-----------------------------------------------------------------------------

enum _plUiNextWindowFlags
{
    PL_NEXT_WINDOW_DATA_FLAGS_NONE          = 0,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS       = 1 << 0,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE      = 1 << 1,
    PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED = 1 << 2,   
};

//-----------------------------------------------------------------------------
// [SECTION] internal functions
//-----------------------------------------------------------------------------

static const char*  pl__find_renderered_text_end             (const char* pcText, const char* pcTextEnd);
static void         pl__add_text                             (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
static bool         pl__ui_button_behavior                   (const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld);
static inline float pl__ui_get_frame_height                  (void)                                   { return gptCtx->tStyle.fFontSize + gptCtx->tStyle.tFramePadding.y * 2.0f; }

// collision
static inline bool  pl__ui_does_circle_contain_point         (plVec2 cen, float radius, plVec2 point) { const float fDistanceSquared = powf(point.x - cen.x, 2) + powf(point.y - cen.y, 2); return fDistanceSquared <= radius * radius; }
static bool         pl__ui_does_triangle_contain_point       (plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point);
static bool         pl__ui_is_item_hoverable                 (const plRect* ptBox, uint32_t uHash);

// content size
static inline void  pl__ui_register_size                     (float fWidth, float fHeight)            { gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.x + fWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x); gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.y + fHeight, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);}
static inline void  pl__ui_register_size_vec2                (plVec2 tSize)                           { gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.x + tSize.x, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x); gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.y + tSize.y, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);}
static inline float pl__ui_get_window_content_width          (void)                                   { if(gptCtx->ptCurrentWindow->tScrollMax.y > 0.0f) return -10.0f + gptCtx->ptCurrentWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f; return gptCtx->ptCurrentWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;}
static inline float pl__ui_get_window_content_width_available(void)                                   { return pl__ui_get_window_content_width() - (gptCtx->ptCurrentWindow->tTempData.tCursorPos.x - gptCtx->ptCurrentWindow->tTempData.tCursorStartPos.x);}

// current cursor
static inline void  pl__ui_advance_cursor                    (float fX, float fY)                     { gptCtx->ptCurrentWindow->tTempData.tCursorPos.x += fX; gptCtx->ptCurrentWindow->tTempData.tCursorPos.y += fY; }
static inline void  pl__ui_next_line                         (void)                                   { gptCtx->ptCurrentWindow->tTempData.tCursorPos.x = gptCtx->ptCurrentWindow->tTempData.tCursorStartPos.x + (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize; pl__ui_advance_cursor(0.0f, pl__ui_get_frame_height() + gptCtx->tStyle.tItemSpacing.y * 2.0f);}

// previous cursor
static inline void  pl__ui_mark_cursor_prev                  (void)                                   { gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine = gptCtx->ptCurrentWindow->tTempData.tCursorPos; }
static inline void  pl__ui_advance_cursor_prev               (float fX, float fY)                     { gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine.x += fX; gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine.y += fY; }
static inline void  pl__ui_set_cursor_prev                   (float fX, float fY)                     { gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine.x = fX; gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine.y = fY; }
static inline void  pl__ui_set_cursor_prev_vec2              (plVec2 tPos)                            { gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine = tPos; }
static inline void  pl__ui_set_relative_cursor_prev          (float fX, float fY)                     { gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine.x = fX + gptCtx->ptCurrentWindow->tTempData.tCursorPos.x; gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine.y = gptCtx->ptCurrentWindow->tTempData.tCursorPos.y + fY; }
static inline void  pl__ui_set_relative_cursor_prev_vec2     (plVec2 tPos)                            { gptCtx->ptCurrentWindow->tTempData.tCursorPrevLine = pl_add_vec2(gptCtx->ptCurrentWindow->tTempData.tCursorPos, tPos); }

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_ui_setup_context(plDrawContext* ptDrawCtx, plUiContext* ptCtx)
{
    ptCtx->ptDrawlist = pl_alloc(sizeof(plDrawList));
    memset(ptCtx->ptDrawlist, 0, sizeof(plDrawList));
    pl_register_drawlist(ptDrawCtx, ptCtx->ptDrawlist);
    ptCtx->ptBgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Background");
    ptCtx->ptFgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Foreground");
    ptCtx->tTooltipWindow.ptBgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Tooltip Background");
    ptCtx->tTooltipWindow.ptFgLayer = pl_request_draw_layer(ptCtx->ptDrawlist, "plui Tooltip Foreground");
    pl_ui_set_dark_theme(ptCtx);
}

void
pl_ui_cleanup_context(plUiContext* ptCtx)
{
    for(uint32_t i = 0; i < pl_sb_size(ptCtx->sbtWindows); i++)
    {
        pl_return_draw_layer(ptCtx->sbtWindows[i].ptBgLayer);
        pl_return_draw_layer(ptCtx->sbtWindows[i].ptFgLayer);
    }
    pl_free(ptCtx->ptDrawlist);
    pl_sb_free(ptCtx->sbtWindows);
    pl_sb_free(ptCtx->sbtFocusedWindows);
    pl_sb_free(ptCtx->sbtSortingWindows);
    pl_sb_free(ptCtx->sbuIdStack);
    ptCtx->ptDrawlist = NULL;
    ptCtx->ptCurrentWindow = NULL;
    ptCtx->ptHoveredWindow = NULL;
    ptCtx->ptMovingWindow = NULL;
    ptCtx->ptActiveWindow = NULL;
    ptCtx->ptFocusedWindow = NULL;
    ptCtx->ptFont = NULL;
}

void
pl_ui_new_frame(plUiContext* ptCtx)
{
    gptCtx = ptCtx;

    const plVec2 tMousePos = pl_get_mouse_pos();

    // use last frame window ordering as a starting point for sorting
    if(pl_sb_size(gptCtx->sbtFocusedWindows) > 0)
    {
        pl_sb_reset(gptCtx->sbtSortingWindows);
        pl_sb_resize(gptCtx->sbtSortingWindows, pl_sb_size(gptCtx->sbtFocusedWindows));
        memcpy(gptCtx->sbtSortingWindows, gptCtx->sbtFocusedWindows, pl_sb_size(gptCtx->sbtFocusedWindows) * sizeof(plUiWindow*)); //-V1004
        pl_sb_reset(gptCtx->sbtFocusedWindows);
    }
    else
        return;

    // find most recently activated/hovered window id's
    uint64_t ulCurrentFrame = pl_get_io_context()->ulFrameCount;
    uint64_t ulMaxFrameActivated = 0u;
    uint32_t uLastActiveWindowID = 0u;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtSortingWindows); i++)
    {
        if(gptCtx->sbtSortingWindows[i]->ulFrameActivated >= ulMaxFrameActivated)
        {
            ulMaxFrameActivated = gptCtx->sbtSortingWindows[i]->ulFrameActivated;
            gptCtx->uActiveWindowId = gptCtx->sbtSortingWindows[i]->uId;
            gptCtx->ptActiveWindow = gptCtx->sbtSortingWindows[i];
            uLastActiveWindowID = i;
        }
        if(gptCtx->sbtSortingWindows[i]->ulFrameHovered == ulCurrentFrame - 1)
        {
            gptCtx->uHoveredWindowId = gptCtx->sbtSortingWindows[i]->uId;
            gptCtx->ptHoveredWindow = gptCtx->sbtSortingWindows[i];
        }

        if(gptCtx->sbtSortingWindows[i]->tPos.x > pl_get_io_context()->afMainViewportSize[0])
            gptCtx->sbtSortingWindows[i]->tPos.x = pl_get_io_context()->afMainViewportSize[0] - gptCtx->sbtSortingWindows[i]->tSize.x / 2.0f;

        if(gptCtx->sbtSortingWindows[i]->tPos.y > pl_get_io_context()->afMainViewportSize[1])
            gptCtx->sbtSortingWindows[i]->tPos.y = pl_get_io_context()->afMainViewportSize[1] - gptCtx->sbtSortingWindows[i]->tSize.y / 2.0f;

    }

    // add windows in focus order
    pl_sb_del_swap(gptCtx->sbtSortingWindows, uLastActiveWindowID); //-V1004
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtSortingWindows); i++)
    {
        gptCtx->sbtSortingWindows[i]->bHovered = false;
        gptCtx->sbtSortingWindows[i]->bActive = false;
        pl_sb_push(gptCtx->sbtFocusedWindows, gptCtx->sbtSortingWindows[i]);
    }

    // add active window last
    pl_sb_push(gptCtx->sbtFocusedWindows, gptCtx->ptActiveWindow);
    gptCtx->ptActiveWindow->bActive = true;
    gptCtx->ptFocusedWindow = gptCtx->ptActiveWindow;

    if(gptCtx->bWantMouse)
        gptCtx->bMouseOwned = true;

    if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->bWantMouseNextFrame = false;

        if(gptCtx->ptMovingWindow)
        {
            gptCtx->ptMovingWindow->bDragging = false;
            gptCtx->ptMovingWindow->bResizing = false;
            gptCtx->ptMovingWindow = NULL;
        }
    }

    if(gptCtx->ptHoveredWindow && !pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->bMouseOwned = true;
    }

    if(gptCtx->ptHoveredWindow == NULL && pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptCtx->bMouseOwned = false;
    }

    // check hovered window status
    if(gptCtx->ptHoveredWindow && pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptCtx->bMouseOwned = true;
        gptCtx->bWantMouseNextFrame = true;
        const float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;
        gptCtx->ptHoveredWindow->bDragging = pl_is_mouse_hovering_rect(gptCtx->ptHoveredWindow->tPos, pl_add_vec2(gptCtx->ptHoveredWindow->tPos, (plVec2){gptCtx->ptHoveredWindow->tSize.x, fTitleBarHeight}));

        // resizing grip
        const plVec2 tCornerPos = pl_add_vec2(gptCtx->ptHoveredWindow->tPos, gptCtx->ptHoveredWindow->tSize);
        const plVec2 tCornerTopPos = pl_add_vec2(tCornerPos, (plVec2){0.0f, -15.0f});
        const plVec2 tCornerLeftPos = pl_add_vec2(tCornerPos, (plVec2){-15.0f, 0.0f});
        gptCtx->ptHoveredWindow->bResizing = pl__ui_does_triangle_contain_point(tCornerPos, tCornerTopPos, tCornerLeftPos, tMousePos) && !gptCtx->ptHoveredWindow->bAutoSize;
        gptCtx->ptMovingWindow = gptCtx->ptHoveredWindow->bDragging || gptCtx->ptHoveredWindow->bResizing ? gptCtx->ptHoveredWindow : NULL;
    }
    else if(gptCtx->ptHoveredWindow && !gptCtx->ptHoveredWindow->bAutoSize && pl_get_mouse_wheel() != 0.0f)
    {
        gptCtx->ptHoveredWindow->tScroll.y -= pl_get_mouse_wheel() * 10.0f;
        gptCtx->ptHoveredWindow->tScroll.y = pl_maxf(gptCtx->ptHoveredWindow->tScroll.y, 0.0f);
        gptCtx->ptHoveredWindow->tScroll.y = pl_minf(gptCtx->ptHoveredWindow->tScroll.y, gptCtx->ptHoveredWindow->tScrollMax.y);   
    }

    // check moving or resizing status
    if(gptCtx->ptMovingWindow && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        if(gptCtx->ptMovingWindow->bDragging)
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
        }

        if(gptCtx->ptMovingWindow->bResizing)
        {
            gptCtx->ptMovingWindow->tSize.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;
            gptCtx->ptMovingWindow->tSize.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;

            gptCtx->ptMovingWindow->tSize.x = pl_maxf(gptCtx->ptMovingWindow->tSize.x, gptCtx->ptMovingWindow->tMinSize.x);
            gptCtx->ptMovingWindow->tSize.y = pl_maxf(gptCtx->ptMovingWindow->tSize.y, gptCtx->ptMovingWindow->tMinSize.y);

            gptCtx->ptHoveredWindow->tScroll.y = pl_maxf(gptCtx->ptHoveredWindow->tScroll.y, 0.0f);
            gptCtx->ptHoveredWindow->tScroll.y = pl_minf(gptCtx->ptHoveredWindow->tScroll.y, gptCtx->ptHoveredWindow->tScrollMax.y);   
            
        }
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
}

void
pl_ui_end_frame(void)
{
    // update state id's from previous frames
    gptCtx->uHoveredId = gptCtx->uNextHoveredId;
    gptCtx->uActiveId = gptCtx->uNextActiveId;
    gptCtx->uToggledId = gptCtx->uNextToggleId;
    gptCtx->uActiveWindowId = gptCtx->uNextActiveWindowId;
    gptCtx->uHoveredWindowId = gptCtx->uNextHoveredWindowId;
    gptCtx->bWantMouse = gptCtx->bWantMouseNextFrame;

    // null state
    gptCtx->bWantMouseNextFrame = false;
    gptCtx->ptHoveredWindow = NULL;
    gptCtx->ptFocusedWindow = NULL;
    gptCtx->ptActiveWindow = NULL;
    gptCtx->uNextToggleId = 0u;
    gptCtx->uNextHoveredId = 0u;
    gptCtx->uNextActiveId = 0u;
    gptCtx->uNextActiveWindowId = 0u;
    gptCtx->uNextHoveredWindowId = 0u;
    gptCtx->tPrevItemData.bHovered = false;
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_NONE;
}

void
pl_ui_render(void)
{
    pl_submit_draw_layer(gptCtx->ptBgLayer);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtFocusedWindows); i++)
    {
        pl_submit_draw_layer(gptCtx->sbtFocusedWindows[i]->ptBgLayer);
        pl_submit_draw_layer(gptCtx->sbtFocusedWindows[i]->ptFgLayer);
    }
    pl_submit_draw_layer(gptCtx->tTooltipWindow.ptBgLayer);
    pl_submit_draw_layer(gptCtx->tTooltipWindow.ptFgLayer);
    pl_submit_draw_layer(gptCtx->ptFgLayer);
}

bool
pl_ui_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize)
{
    const uint32_t uWindowID = pl_str_hash(pcName, 0, 0);
    pl_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcName, 0.0f);
    const float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist
    gptCtx->ptCurrentWindow = NULL;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtWindows); i++)
    {
        if(gptCtx->sbtWindows[i].uId == uWindowID)
        {
            gptCtx->ptCurrentWindow = &gptCtx->sbtWindows[i];
            break;
        }
    }

    // new window needs to be created
    if(gptCtx->ptCurrentWindow == NULL)
    {
        plDrawList tDrawlist = {0};
        plUiWindow tWindow = {
            .uId                     = uWindowID,
            .pcName                  = pcName,
            .tPos                    = {.x = 200.0f, .y = 200.0f},
            .tMinSize                = {.x = 200.0f, .y = 200.0f},
            .tSize                   = {.x = 500.0f, .y = 700.0f},
            .ptBgLayer               = pl_request_draw_layer(gptCtx->ptDrawlist, pcName),
            .ptFgLayer               = pl_request_draw_layer(gptCtx->ptDrawlist, pcName),
            .tPosAllowableFlags      = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE,
            .tSizeAllowableFlags     = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE,
            .tCollapseAllowableFlags = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE,
        };

        pl_sb_push(gptCtx->sbtWindows, tWindow);
        gptCtx->ptCurrentWindow = &pl_sb_top(gptCtx->sbtWindows);
        pl_sb_push(gptCtx->sbtFocusedWindows, gptCtx->ptCurrentWindow);
    }

    gptCtx->ptCurrentWindow->tContentSize = pl_add_vec2((plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, pl_sub_vec2(gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos, gptCtx->ptCurrentWindow->tTempData.tCursorStartPos));
    gptCtx->ptCurrentWindow->tScrollMax = pl_sub_vec2(gptCtx->ptCurrentWindow->tContentSize, (plVec2){gptCtx->ptCurrentWindow->tSize.x, gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight});
    gptCtx->ptCurrentWindow->tScrollMax.x = pl_maxf(gptCtx->ptCurrentWindow->tScrollMax.x, 0.0f);
    gptCtx->ptCurrentWindow->tScrollMax.y = pl_maxf(gptCtx->ptCurrentWindow->tScrollMax.y, 0.0f);

    if(gptCtx->ptCurrentWindow->tScrollMax.y > 0.0f)
        gptCtx->ptCurrentWindow->tContentSize.x -= 10.0f;

    memset(&gptCtx->ptCurrentWindow->tTempData, 0, sizeof(plUiTempWindowData));
    gptCtx->ptCurrentWindow->bAutoSize = bAutoSize;
    if(gptCtx->ptCurrentWindow->tSize.x < gptCtx->ptCurrentWindow->tMinSize.x) gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tMinSize.x;
    if(gptCtx->ptCurrentWindow->tSize.y < gptCtx->ptCurrentWindow->tMinSize.y) gptCtx->ptCurrentWindow->tSize.y = gptCtx->ptCurrentWindow->tMinSize.y;

    if(gptCtx->uHoveredWindowId == uWindowID)
    {
        gptCtx->ptHoveredWindow = gptCtx->ptCurrentWindow;
        gptCtx->ptCurrentWindow->bHovered = true;
    }
    else
        gptCtx->ptCurrentWindow->bHovered = false;

    // should window collapse
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED)
    {
        if(gptCtx->ptCurrentWindow->tCollapseAllowableFlags & gptCtx->tNextWindowData.tCollapseCondition)
        {
            gptCtx->ptCurrentWindow->bCollapsed = true;
            gptCtx->ptCurrentWindow->tCollapseAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    // position & size
    const plVec2 tMousePos = pl_get_mouse_pos();
    plVec2 tStartPos = gptCtx->ptCurrentWindow->tPos;

    // next window calls
    bool bWindowSizeSet = false;
    bool bWindowPosSet = false;
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS)
    {
        bWindowPosSet = gptCtx->ptCurrentWindow->tPosAllowableFlags & gptCtx->tNextWindowData.tPosCondition;
        if(bWindowPosSet)
        {
            tStartPos = gptCtx->tNextWindowData.tPos;
            gptCtx->ptCurrentWindow->tPos = tStartPos;
            gptCtx->ptCurrentWindow->tPosAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE)
    {
        bWindowSizeSet = gptCtx->ptCurrentWindow->tSizeAllowableFlags & gptCtx->tNextWindowData.tSizeCondition;
        if(bWindowSizeSet)
        {
            gptCtx->ptCurrentWindow->tSize = gptCtx->tNextWindowData.tSize;
            gptCtx->ptCurrentWindow->tSizeAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(gptCtx->ptCurrentWindow->bCollapsed)
        gptCtx->ptCurrentWindow->tSize = (plVec2){gptCtx->ptCurrentWindow->tSize.x, fTitleBarHeight};
    
    // update frame this window was hovered
    if(pl_is_mouse_hovering_rect(gptCtx->ptCurrentWindow->tPos, pl_add_vec2(tStartPos, gptCtx->ptCurrentWindow->tSize)))
        gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount;

    // draw title bar
    const plVec4 tTitleColor = gptCtx->ptCurrentWindow->bActive ? gptCtx->tStyle.tTitleActiveCol: gptCtx->tStyle.tTitleBgCol;
    pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x, fTitleBarHeight}), tTitleColor);

    // draw title text
    plVec2 titlePos = pl_add_vec2(tStartPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
    titlePos.x = roundf(titlePos.x);
    titlePos.y = roundf(titlePos.y);
    pl__add_text(gptCtx->ptCurrentWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, titlePos, gptCtx->tStyle.tTextCol, pcName, 0.0f);

    // draw close button
    const float fTitleBarButtonRadius = 8.0f;
    float fTitleButtonStartPos = fTitleBarButtonRadius * 2.0f;
    if(pbOpen)
    {
        plVec2 tCloseCenterPos = pl_add_vec2(tStartPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
        tCloseCenterPos.x = roundf(tCloseCenterPos.x);
        tCloseCenterPos.y = roundf(tCloseCenterPos.y);
        fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;
        if(pl__ui_does_circle_contain_point(tCloseCenterPos, fTitleBarButtonRadius, tMousePos) && gptCtx->ptHoveredWindow == gptCtx->ptCurrentWindow)
        {
            pl_add_circle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 12);
            if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false)) gptCtx->uActiveId = 1;
            else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT)) *pbOpen = false;       
        }
        else
            pl_add_circle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.0f, 0.0f, 1.0f}, 12);
    }

    // draw collapse button
    plVec2 tCollapsingCenterPos = pl_add_vec2(tStartPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
    tCollapsingCenterPos.x = roundf(tCollapsingCenterPos.x);
    tCollapsingCenterPos.y = roundf(tCollapsingCenterPos.y);
    fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;

    if(pl__ui_does_circle_contain_point(tCollapsingCenterPos, fTitleBarButtonRadius, tMousePos) &&  gptCtx->ptHoveredWindow == gptCtx->ptCurrentWindow)
    {
        pl_add_circle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 12);

        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false)) gptCtx->uActiveId = 2;
        else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT)) gptCtx->ptCurrentWindow->bCollapsed = !gptCtx->ptCurrentWindow->bCollapsed;
    }
    else
        pl_add_circle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.5f, 0.0f, 1.0f}, 12);


    if(!gptCtx->ptCurrentWindow->bCollapsed)
    {
        const plVec2 tStartClip = { gptCtx->ptCurrentWindow->tPos.x, gptCtx->ptCurrentWindow->tPos.y + fTitleBarHeight };
        const plVec2 tEndClip = { gptCtx->ptCurrentWindow->tSize.x, gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight };
        pl_push_clip_rect(pl_calculate_rect(tStartClip, tEndClip));
    }

    // resizing grip
    const plVec2 tCornerPos = pl_add_vec2(tStartPos, gptCtx->ptCurrentWindow->tSize);
    const plVec2 tCornerTopPos = pl_add_vec2(tCornerPos, (plVec2){0.0f, -15.0f});
    const plVec2 tCornerLeftPos = pl_add_vec2(tCornerPos, (plVec2){-15.0f, 0.0f});

    // window background
    if(!gptCtx->ptCurrentWindow->bCollapsed && !bAutoSize)
    {
        if(pl_is_mouse_hovering_rect(tStartPos, pl_add_vec2(tStartPos, gptCtx->ptCurrentWindow->tSize)))
            gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount;
    }
    else if(gptCtx->ptCurrentWindow->bCollapsed)
    {
        if(pl_is_mouse_hovering_rect(tStartPos, pl_add_vec2(tStartPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x, fTitleBarHeight})))
            gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount;
    }

    // resizing grip
    if(gptCtx->ptCurrentWindow->bActive && !gptCtx->ptCurrentWindow->bCollapsed && !bAutoSize)
        pl_add_triangle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCornerPos, tCornerTopPos, tCornerLeftPos, (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

    // update cursors
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - gptCtx->ptCurrentWindow->tScroll.x);
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - gptCtx->ptCurrentWindow->tScroll.y);
    pl__ui_mark_cursor_prev();
    gptCtx->ptCurrentWindow->tTempData.tCursorStartPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - gptCtx->ptCurrentWindow->tScroll.x);
    gptCtx->ptCurrentWindow->tTempData.tCursorStartPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - gptCtx->ptCurrentWindow->tScroll.y);
    gptCtx->ptCurrentWindow->fTextVerticalOffset = 0.0f;

    if(gptCtx->ptCurrentWindow->bHovered && pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptCtx->uNextActiveWindowId = uWindowID;
        gptCtx->ptCurrentWindow->ulFrameActivated = pl_get_io_context()->ulFrameCount;
    }

    // reset next window flags
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;

    return !gptCtx->ptCurrentWindow->bCollapsed;
}

void
pl_ui_end_window(void)
{
    const float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    if(gptCtx->ptCurrentWindow->tSize.x < gptCtx->ptCurrentWindow->tMinSize.x) gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tMinSize.x;
    if(gptCtx->ptCurrentWindow->tSize.y < gptCtx->ptCurrentWindow->tMinSize.y) gptCtx->ptCurrentWindow->tSize.y = gptCtx->ptCurrentWindow->tMinSize.y;

    // autosized non collapsed
    if(gptCtx->ptCurrentWindow->bAutoSize && !gptCtx->ptCurrentWindow->bCollapsed)
    {
        // ensure window doesn't get too small
        gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
        gptCtx->ptCurrentWindow->tSize.y = fTitleBarHeight + gptCtx->ptCurrentWindow->tContentSize.y + gptCtx->tStyle.fWindowVerticalPadding;
        pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, 
            pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){0.0f, fTitleBarHeight}), 
            pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize), gptCtx->tStyle.tWindowBgColor);

        if(pl_is_mouse_hovering_rect(gptCtx->ptCurrentWindow->tPos, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize)))
        {
            gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount; 
            gptCtx->uNextHoveredWindowId = gptCtx->ptCurrentWindow->uId;
        }
        pl_pop_clip_rect();
    }

    // regular window non collapsed
    else if(!gptCtx->ptCurrentWindow->bCollapsed)
    { 
        
        pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){0.0f, fTitleBarHeight}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize), gptCtx->tStyle.tWindowBgColor);

        // scroll bar
        if(gptCtx->ptCurrentWindow->tScrollMax.x != 0.0f || gptCtx->ptCurrentWindow->tScrollMax.y != 0.0f)
        {
            const float fScrollbarHandleSize  = floorf((gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight) * ((gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight) / (gptCtx->ptCurrentWindow->tContentSize.y)));
            const float fScrollbarHandleStart = floorf((gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight - fScrollbarHandleSize) * (gptCtx->ptCurrentWindow->tScroll.y/(gptCtx->ptCurrentWindow->tScrollMax.y)));
            
            pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 12.0f, fTitleBarHeight}),                         pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 2.0f, gptCtx->ptCurrentWindow->tSize.y}), gptCtx->tStyle.tScrollbarBgCol);
            pl_add_rect       (gptCtx->ptCurrentWindow->ptBgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 12.0f, fTitleBarHeight}),                         pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 2.0f, gptCtx->ptCurrentWindow->tSize.y}), gptCtx->tStyle.tScrollbarFrameCol, 1.0f);
            pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 12.0f, fTitleBarHeight + fScrollbarHandleStart}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 2.0f, fTitleBarHeight + fScrollbarHandleStart + fScrollbarHandleSize}), gptCtx->tStyle.tScrollbarHandleCol);
        }

        pl_pop_clip_rect();

        gptCtx->ptCurrentWindow->tFullSize = gptCtx->ptCurrentWindow->tSize;
    }
    
    gptCtx->ptCurrentWindow = NULL;
    pl_sb_pop(gptCtx->sbuIdStack);
}

void
pl_ui_begin_tooltip(void)
{
    const plVec2 tMousePos = pl_get_mouse_pos();
    plUiWindow* ptCurrentParentWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = &gptCtx->tTooltipWindow;
    gptCtx->ptCurrentWindow->ptParentWindow = ptCurrentParentWindow;

    gptCtx->ptCurrentWindow->tContentSize = pl_add_vec2((plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, pl_sub_vec2(gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos, gptCtx->ptCurrentWindow->tTempData.tCursorStartPos));
    memset(&gptCtx->ptCurrentWindow->tTempData, 0, sizeof(plUiTempWindowData));

    gptCtx->ptCurrentWindow->bAutoSize = true;
    gptCtx->ptCurrentWindow->tTempData.tCursorStartPos = pl_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    gptCtx->ptCurrentWindow->tPos = tMousePos;
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);
    gptCtx->ptCurrentWindow->fTextVerticalOffset = 0.0f;

    const plVec2 tStartClip = { gptCtx->ptCurrentWindow->tPos.x, gptCtx->ptCurrentWindow->tPos.y };
    const plVec2 tEndClip = { gptCtx->ptCurrentWindow->tSize.x, gptCtx->ptCurrentWindow->tSize.y };
    pl_push_clip_rect(pl_calculate_rect(tStartClip, tEndClip));
}

void
pl_ui_end_tooltip(void)
{
    
    gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    gptCtx->ptCurrentWindow->tSize.y = gptCtx->ptCurrentWindow->tContentSize.y;
    pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, 
        gptCtx->ptCurrentWindow->tPos, 
        pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize), gptCtx->tStyle.tWindowBgColor);

    gptCtx->ptCurrentWindow = gptCtx->ptCurrentWindow->ptParentWindow;
    pl_pop_clip_rect();
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
    const float fFrameHeight = pl__ui_get_frame_height();

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));

    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
 
    const plVec2 tStartPos = ptWindow->tTempData.tCursorPos;
    const plVec2 tFinalSize = {floorf(tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x), floorf(fFrameHeight)};

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + gptCtx->tStyle.tFramePadding.x),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tFinalSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bHeld)         pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonActiveCol);
    else if(bHovered) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonHoveredCol);
    else              pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonCol);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, 0.0f);
    pl__ui_register_size(tFinalSize.x, fFrameHeight);
    pl__ui_set_relative_cursor_prev(tFinalSize.x, 0.0f);
    pl__ui_next_line();
    return bPressed;
}

bool
pl_ui_selectable(const char* pcText, bool* bpValue)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const float fFrameHeight = pl__ui_get_frame_height();

    const plVec2 tSize = {pl__ui_get_window_content_width_available(), fFrameHeight};
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);

    const plVec2 tStartPos = ptWindow->tTempData.tCursorPos;
    const plVec2 tEndPos = pl_add_vec2(tStartPos, tSize);

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + 8.0f * 3.0f),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *bpValue = !*bpValue;

    if(bHeld)         pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderActiveCol);
    else if(bHovered) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderHoveredCol);

    if(*bpValue)
        pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderCol);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_register_size(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, fFrameHeight);
    pl__ui_set_relative_cursor_prev(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, 0.0f);
    pl__ui_advance_cursor(0.0f, pl__ui_get_frame_height());
    return *bpValue; 
}

bool
pl_ui_checkbox(const char* pcText, bool* bValue)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const bool bOriginalValue = *bValue;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
 
    const float fFrameHeight = pl__ui_get_frame_height();
    const plVec2 tStartPos = ptWindow->tTempData.tCursorPos;
    const plVec2 tSize = {floorf(tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x + gptCtx->tStyle.tInnerSpacing.x + fFrameHeight), floorf(fFrameHeight)};
    const plVec2 tEndBoxPos = pl_add_vec2(tStartPos, (plVec2){fFrameHeight, fFrameHeight});

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + fFrameHeight + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *bValue = !bOriginalValue;

    if(bHeld)         pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tFrameBgActiveCol);
    else if(bHovered) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tFrameBgHoveredCol);
    else              pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tFrameBgCol);

    if(*bValue)
        pl_add_line(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tCheckmarkCol, 2.0f);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_register_size(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, fFrameHeight);
    pl__ui_set_relative_cursor_prev(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, 0.0f);
    pl__ui_next_line();
    return bPressed;
}

bool
pl_ui_radio_button(const char* pcText, int* piValue, int iButtonValue)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const int iOriginalValue = *piValue;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);

    const float fFrameHeight = pl__ui_get_frame_height();
    const plVec2 tStartPos = ptWindow->tTempData.tCursorPos;
    const plVec2 tSize = {floorf(tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x + gptCtx->tStyle.tInnerSpacing.x + fFrameHeight), floorf(fFrameHeight)};

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + fFrameHeight + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *piValue = iButtonValue;

    if(bHeld)         pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + fFrameHeight / 2.0f, tStartPos.y + fFrameHeight / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tStyle.tFrameBgActiveCol, 12);
    else if(bHovered) pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + fFrameHeight / 2.0f, tStartPos.y + fFrameHeight / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tStyle.tFrameBgHoveredCol, 12);
    else              pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + fFrameHeight / 2.0f, tStartPos.y + fFrameHeight / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tStyle.tFrameBgCol, 12);

    if(*piValue == iButtonValue)
        pl_add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + fFrameHeight / 2.0f, tStartPos.y + fFrameHeight / 2.0f}, gptCtx->tStyle.fFontSize / 2.5f, gptCtx->tStyle.tCheckmarkCol, 12);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);

    pl__ui_register_size(fFrameHeight / 2.0f + tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, fFrameHeight);
    pl__ui_set_relative_cursor_prev(fFrameHeight / 2.0f + tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, 0.0f);
    pl__ui_next_line();
    return bPressed;
}

bool
pl_ui_collapsing_header(const char* pcText, bool* pbOpenState)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const float fFrameHeight = pl__ui_get_frame_height();

    const plVec2 tSize = {pl__ui_get_window_content_width_available(), fFrameHeight};
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);

    const plVec2 tStartPos = ptWindow->tTempData.tCursorPos;
    const plVec2 tEndPos = pl_add_vec2(tStartPos, tSize);

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + 8.0f * 3.0f),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *pbOpenState = !*pbOpenState;

    if(bHeld)         pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderActiveCol);
    else if(bHovered) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderHoveredCol);
    else              pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderCol);

    if(*pbOpenState)
    {
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + fFrameHeight / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){ 0.0f,  4.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ 4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint,  (plVec2){-4.0f, -4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
    }
    else
    {
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + fFrameHeight / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){  4.0f,  0.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ -4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint, (plVec2){ -4.0f,  4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
    }

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_register_size(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, fFrameHeight);
    pl__ui_set_relative_cursor_prev(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, 0.0f);
    pl__ui_next_line();
    return *pbOpenState; 
}

bool
pl_ui_tree_node(const char* pcText, bool* pbOpenState)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    const float fFrameHeight = pl__ui_get_frame_height();

    const plVec2 tSize = {pl__ui_get_window_content_width_available(), fFrameHeight};
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);

    const plVec2 tStartPos = ptWindow->tTempData.tCursorPos;
    const plVec2 tEndPos = pl_add_vec2(tStartPos, tSize);

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + 8.0f * 3.0f),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
        *pbOpenState = !*pbOpenState;

    if(bHeld)         pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderActiveCol);
    else if(bHovered) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tStyle.tHeaderHoveredCol);



    if(*pbOpenState)
    {
        ptWindow->tTempData.uTreeDepth++;
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + fFrameHeight / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){ 0.0f,  4.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ 4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint,  (plVec2){-4.0f, -4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
        pl__ui_advance_cursor(gptCtx->tStyle.fIndentSize, 0.0f);
    }
    else
    {
        const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + fFrameHeight / 2.0f};
        const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){  4.0f,  0.0f});
        const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ -4.0f, -4.0f});
        const plVec2 leftPos  = pl_add_vec2(centerPoint, (plVec2){ -4.0f,  4.0f});
        pl_add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
    }

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_register_size(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, fFrameHeight);
    pl__ui_set_relative_cursor_prev(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, 0.0f);
    pl__ui_next_line();

    return *pbOpenState; 
}

void
pl_ui_tree_pop(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.uTreeDepth--;
    pl__ui_advance_cursor(-gptCtx->tStyle.fIndentSize, 0.0f);
}

bool
pl_ui_begin_tab_bar(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
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
            .uId       = uHash
        };;

        pl_sb_push(gptCtx->sbtTabBars, tTabBar);
        gptCtx->ptCurrentTabBar = &pl_sb_top(gptCtx->sbtTabBars);
    }

    gptCtx->ptCurrentTabBar->tStartPos = ptWindow->tTempData.tCursorPos;
    gptCtx->ptCurrentTabBar->tStartPos.x -= gptCtx->tStyle.tInnerSpacing.x;
    gptCtx->ptCurrentTabBar->tCursorPos = ptWindow->tTempData.tCursorPos;
    gptCtx->ptCurrentTabBar->uCurrentIndex = 0u;
    return true;
}

void
pl_ui_end_tab_bar(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const float fFrameHeight = pl__ui_get_frame_height();
    pl_add_line(ptWindow->ptFgLayer, 
        (plVec2){gptCtx->ptCurrentTabBar->tStartPos.x, gptCtx->ptCurrentTabBar->tStartPos.y + fFrameHeight},
        (plVec2){gptCtx->ptCurrentTabBar->tStartPos.x + pl__ui_get_window_content_width_available(), gptCtx->ptCurrentTabBar->tStartPos.y + fFrameHeight},
        gptCtx->tStyle.tButtonActiveCol, 1.0f);

    gptCtx->ptCurrentTabBar->uValue = gptCtx->ptCurrentTabBar->uNextValue;
    pl_sb_pop(gptCtx->sbuIdStack);
}

bool
pl_ui_begin_tab(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiTabBar* ptTabBar = gptCtx->ptCurrentTabBar;
    const float fFrameHeight = pl__ui_get_frame_height();
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    pl_sb_push(gptCtx->sbuIdStack, uHash);

    if(ptTabBar->uValue == 0u) ptTabBar->uValue = uHash;

    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
    const plVec2 tStartPos = ptTabBar->tCursorPos;
    const plVec2 tFinalSize = {floorf(tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x), floorf(fFrameHeight)};

    const plVec2 tTextStartPos = {
        .x = roundf(tStartPos.x + gptCtx->tStyle.tFramePadding.x),
        .y = roundf(tStartPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f)
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tFinalSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(bPressed)
    {
        gptCtx->uNextToggleId = uHash;
        ptTabBar->uNextValue = uHash;
    }

    if(bHeld)                  pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonActiveCol);
    else if(bHovered)          pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonHoveredCol);
    else if(ptTabBar->uValue == uHash) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonActiveCol);
    else                       pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tStyle.tButtonCol);
    
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);

    if(ptTabBar->uCurrentIndex == 0)
    {
        pl__ui_register_size(tFinalSize.x, fFrameHeight);
        pl__ui_next_line();
    }
    ptTabBar->tCursorPos.x += gptCtx->tStyle.tInnerSpacing.x + tFinalSize.x;
    ptTabBar->uCurrentIndex++;

    return ptTabBar->uValue == uHash;
}

void
pl_ui_end_tab(void)
{
    pl_sb_pop(gptCtx->sbuIdStack);
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
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, acTempBuffer, -1.0f);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){ptWindow->tTempData.tCursorPos.x, ptWindow->tTempData.tCursorPos.y + ptWindow->fTextVerticalOffset}, gptCtx->tStyle.tTextCol, acTempBuffer, -1.0f);
    pl__ui_register_size(tTextSize.x, tTextSize.y);
    pl__ui_set_relative_cursor_prev(tTextSize.x, 0.0f);
    pl__ui_advance_cursor(0.0f, tTextSize.y + gptCtx->tStyle.tItemSpacing.y * 2.0f);
}

void
pl_ui_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tInitialCursorPos = ptWindow->tTempData.tCursorPos;
    const float fFrameHeight = pl__ui_get_frame_height();

    if(tSize.y == 0.0f) tSize.y = fFrameHeight;
    if(tSize.x < 0.0f) tSize.x = pl__ui_get_window_content_width_available();
    pl_add_rect_filled(ptWindow->ptFgLayer, tInitialCursorPos, pl_add_vec2(tInitialCursorPos, tSize), gptCtx->tStyle.tFrameBgCol);
    pl_add_rect_filled(ptWindow->ptFgLayer, tInitialCursorPos, pl_add_vec2(tInitialCursorPos, (plVec2){tSize.x * fFraction, tSize.y}), gptCtx->tStyle.tProgressBarCol);

    const char* pcTextPtr = pcOverlay;
    
    if(pcOverlay == NULL)
    {
        static char acBuffer[32] = {0};
        pl_sprintf(acBuffer, "%.1f%%", 100.0f * fFraction);
        pcTextPtr = acBuffer;
    }

    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcTextPtr, -1.0f);

    plVec2 tTextStartPos = {
        .x = tInitialCursorPos.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + tSize.x * fFraction,
        .y = tInitialCursorPos.y + fFrameHeight / 2.0f - tTextSize.y / 2.0f
    };

    if(tTextStartPos.x + tTextSize.x > ptWindow->tTempData.tCursorPos.x + tSize.x)
        tTextStartPos.x = ptWindow->tTempData.tCursorPos.x + tSize.x - tTextSize.x - gptCtx->tStyle.tInnerSpacing.x;

    tTextStartPos.x = roundf(tTextStartPos.x);
    tTextStartPos.y = roundf(tTextStartPos.y);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcTextPtr, -1.0f);

    const bool bHovered = pl_is_mouse_hovering_rect(tInitialCursorPos, pl_add_vec2(tInitialCursorPos, tSize)) && ptWindow == gptCtx->ptHoveredWindow;
    gptCtx->tPrevItemData.bHovered = bHovered;
    pl__ui_register_size(tSize.x, fFrameHeight);
    pl__ui_set_relative_cursor_prev(tSize.x, 0.0f);
    pl__ui_next_line();
}

void
pl_ui_same_line(float fOffsetFromStart, float fSpacing)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    if(fOffsetFromStart > 0.0f)
        ptWindow->tTempData.tCursorPos.x = ptWindow->tTempData.tCursorStartPos.x + fOffsetFromStart;
    else
        ptWindow->tTempData.tCursorPos.x = ptWindow->tTempData.tCursorPrevLine.x;

    ptWindow->tTempData.tCursorPos.y = ptWindow->tTempData.tCursorPrevLine.y;

    if(fSpacing < 0.0f)
        ptWindow->tTempData.tCursorPos.x += gptCtx->tStyle.tItemSpacing.x;
    else
        ptWindow->tTempData.tCursorPos.x += fSpacing;
}

void
pl_ui_align_text(void)
{
    const float tFrameHeight = pl__ui_get_frame_height();
    gptCtx->ptCurrentWindow->fTextVerticalOffset = tFrameHeight / 2.0f - gptCtx->tStyle.fFontSize / 2.0f;
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

void
pl_ui_set_dark_theme(plUiContext* ptCtx)
{
    // styles
    ptCtx->tStyle.fTitlePadding            = 10.0f;
    ptCtx->tStyle.fFontSize                = 13.0f;
    ptCtx->tStyle.fWindowHorizontalPadding = 15.0f;
    ptCtx->tStyle.fWindowVerticalPadding   = 15.0f;
    ptCtx->tStyle.fIndentSize              = 15.0f;
    ptCtx->tStyle.tItemSpacing  = (plVec2){8.0f, 4.0f};
    ptCtx->tStyle.tInnerSpacing = (plVec2){4.0f, 4.0f};
    ptCtx->tStyle.tFramePadding = (plVec2){4.0f, 4.0f};

    // colors
    ptCtx->tStyle.tTitleActiveCol     = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tTitleBgCol         = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    ptCtx->tStyle.tWindowBgColor      = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    ptCtx->tStyle.tButtonCol          = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tButtonHoveredCol   = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tButtonActiveCol    = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tTextCol            = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    ptCtx->tStyle.tProgressBarCol     = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    ptCtx->tStyle.tCheckmarkCol       = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tFrameBgCol         = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tFrameBgHoveredCol  = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    ptCtx->tStyle.tFrameBgActiveCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    ptCtx->tStyle.tHeaderCol          = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tHeaderHoveredCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    ptCtx->tStyle.tHeaderActiveCol    = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    ptCtx->tStyle.tScrollbarBgCol     = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    ptCtx->tStyle.tScrollbarHandleCol = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tScrollbarFrameCol  = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static bool
pl__ui_is_item_hoverable(const plRect* ptBox, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow->bResizing || !gptCtx->bMouseOwned)
        return false;

    if(!pl_is_mouse_hovering_rect(ptBox->tMin, ptBox->tMax))
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
    pl_add_text_ex(ptLayer, ptFont, fSize, tP, tColor, pcText, pl__find_renderered_text_end(pcText, pcTextEnd), fWrap);
}

static bool
pl__ui_button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    gptCtx->tPrevItemData.bActive = false;

    bool bPressed = false;
    bool bHovered = pl__ui_is_item_hoverable(ptBox, uHash);

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
