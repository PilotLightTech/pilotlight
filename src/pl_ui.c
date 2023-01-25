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
#define PL_MATH_INCLUDE_FUNCTIONS
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
static inline void  pl__ui_register_size                     (float fWidth, float fHeight) { gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.x + fWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x); gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.y + fHeight, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y); gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.x += fWidth;gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.y = pl_maxf(fHeight, gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.y);}
static inline void  pl__ui_register_size_vec2                (plVec2 tSize)                { gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.x + tSize.x, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x); gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(gptCtx->ptCurrentWindow->tTempData.tCursorPos.y + tSize.y, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);}
static inline float pl__ui_get_window_content_width          (void)                        { if(gptCtx->ptCurrentWindow->tScrollMax.y > 0.0f) return -10.0f + gptCtx->ptCurrentWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f; return gptCtx->ptCurrentWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;}
static inline float pl__ui_get_window_content_width_available(void)                        { return pl__ui_get_window_content_width() - (gptCtx->ptCurrentWindow->tTempData.tCursorPos.x - gptCtx->ptCurrentWindow->tTempData.tCursorStartPos.x);}

// current cursor
static inline void  pl__ui_advance_cursor                    (float fX, float fY)          { gptCtx->ptCurrentWindow->tTempData.tCursorPos.x += fX; gptCtx->ptCurrentWindow->tTempData.tCursorPos.y += fY; }
static inline void  pl__ui_next_line                         (void)                        { gptCtx->ptCurrentWindow->tTempData.tCursorPos.x = gptCtx->ptCurrentWindow->tTempData.tCursorStartPos.x + (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize + gptCtx->ptCurrentWindow->tTempData.fExtraIndent; pl__ui_advance_cursor(0.0f, gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.y + gptCtx->tStyle.tItemSpacing.y * 2.0f);gptCtx->ptCurrentWindow->tTempData.tLastLineSize = gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize;gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.x = 0.0f;gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.y = 0.0f;}

// storage
static plUiStorageEntry* pl__lower_bound(plUiStorageEntry* sbtData, uint32_t uKey);

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

    if(pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->uNextActiveId = gptCtx->uActiveId;
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
        const plVec2 tCornerTopPos = pl_add_vec2(tCornerPos, (plVec2){0.0f, -20.0f});
        const plVec2 tCornerLeftPos = pl_add_vec2(tCornerPos, (plVec2){-20.0f, 0.0f});
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

            gptCtx->ptHoveredWindow->tScroll.x = pl_maxf(gptCtx->ptHoveredWindow->tScroll.x, 0.0f);
            gptCtx->ptHoveredWindow->tScroll.x = pl_minf(gptCtx->ptHoveredWindow->tScroll.x, gptCtx->ptHoveredWindow->tScrollMax.x);   
            
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
        if(gptCtx->sbtFocusedWindows[i]->uHideFrames == 0)
        {
            pl_submit_draw_layer(gptCtx->sbtFocusedWindows[i]->ptBgLayer);
            pl_submit_draw_layer(gptCtx->sbtFocusedWindows[i]->ptFgLayer);
        }
        else
        {
            gptCtx->sbtFocusedWindows[i]->uHideFrames--;
        }
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
            .tSize                   = {.x = 500.0f, .y = 500.0f},
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

    if(gptCtx->ptCurrentWindow->tScrollMax.y > 0.0f && gptCtx->ptCurrentWindow->tScrollMax.x > 0.0f)
    {
        gptCtx->ptCurrentWindow->tScrollMax.x += 10.0f;
        gptCtx->ptCurrentWindow->tScrollMax.y += 10.0f;
    }

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

        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            gptCtx->uActiveId = 2;
        }
        else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
        {
            gptCtx->ptCurrentWindow->bCollapsed = !gptCtx->ptCurrentWindow->bCollapsed;
            if(!gptCtx->ptCurrentWindow->bCollapsed)
            {
                gptCtx->ptCurrentWindow->tSize = gptCtx->ptCurrentWindow->tFullSize;
                if(bAutoSize)
                    gptCtx->ptCurrentWindow->uHideFrames = 2;
            }
        }
    }
    else
        pl_add_circle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.5f, 0.0f, 1.0f}, 12);


    if(!gptCtx->ptCurrentWindow->bCollapsed)
    {
        const plVec2 tStartClip = { gptCtx->ptCurrentWindow->tPos.x, gptCtx->ptCurrentWindow->tPos.y + fTitleBarHeight };
        const plVec2 tEndClip = { 
            gptCtx->ptCurrentWindow->tSize.x - (gptCtx->ptCurrentWindow->tScrollMax.y > 0.0f ? 12.0f : 0.0f), 
            gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight - (gptCtx->ptCurrentWindow->tScrollMax.x > 0.0f ? 12.0f : 0.0f)
            };
        pl_push_clip_rect(gptCtx->ptCurrentWindow->ptFgLayer, pl_calculate_rect(tStartClip, tEndClip));
        pl_push_clip_rect(gptCtx->ptCurrentWindow->ptBgLayer, pl_calculate_rect(tStartClip, tEndClip));
    }

    // update cursors
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - gptCtx->ptCurrentWindow->tScroll.x);
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - gptCtx->ptCurrentWindow->tScroll.y);
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
    gptCtx->ptCurrentWindow->bScrolling = false;

    if(gptCtx->ptCurrentWindow->tSize.x < gptCtx->ptCurrentWindow->tMinSize.x) gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tMinSize.x;
    if(gptCtx->ptCurrentWindow->tSize.y < gptCtx->ptCurrentWindow->tMinSize.y) gptCtx->ptCurrentWindow->tSize.y = gptCtx->ptCurrentWindow->tMinSize.y;

    // autosized non collapsed
    if(gptCtx->ptCurrentWindow->bAutoSize && !gptCtx->ptCurrentWindow->bCollapsed)
    {
        // ensure window doesn't get too small
        gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
        gptCtx->ptCurrentWindow->tSize.y = fTitleBarHeight + gptCtx->ptCurrentWindow->tContentSize.y + gptCtx->tStyle.fWindowVerticalPadding;

        if(gptCtx->ptCurrentWindow->tSize.x < gptCtx->ptCurrentWindow->tMinSize.x) gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tMinSize.x;
        if(gptCtx->ptCurrentWindow->tSize.y < gptCtx->ptCurrentWindow->tMinSize.y) gptCtx->ptCurrentWindow->tSize.y = gptCtx->ptCurrentWindow->tMinSize.y;

        pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, 
            pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){0.0f, fTitleBarHeight}), 
            pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize), gptCtx->tStyle.tWindowBgColor);

        if(pl_is_mouse_hovering_rect(gptCtx->ptCurrentWindow->tPos, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize)))
        {
            gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount; 
            gptCtx->uNextHoveredWindowId = gptCtx->ptCurrentWindow->uId;
        }
        pl_pop_clip_rect(gptCtx->ptCurrentWindow->ptFgLayer);
        pl_pop_clip_rect(gptCtx->ptCurrentWindow->ptBgLayer);
        gptCtx->ptCurrentWindow->tFullSize = gptCtx->ptCurrentWindow->tSize;
    }

    // regular window non collapsed
    else if(!gptCtx->ptCurrentWindow->bCollapsed)
    { 

        pl_pop_clip_rect(gptCtx->ptCurrentWindow->ptFgLayer);
        pl_pop_clip_rect(gptCtx->ptCurrentWindow->ptBgLayer);
        
        pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){0.0f, fTitleBarHeight}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize), gptCtx->tStyle.tWindowBgColor);

        const uint32_t uVerticalScrollHash = pl_str_hash("##scrollright", 0, pl_sb_top(gptCtx->sbuIdStack));
        const uint32_t uHorizonatalScrollHash = pl_str_hash("##scrollbottom", 0, pl_sb_top(gptCtx->sbuIdStack));
        const float fRightSidePadding = gptCtx->ptCurrentWindow->tScrollMax.y == 0.0f ? 0.0f : 12.0f;
        const float fBottomPadding = gptCtx->ptCurrentWindow->tScrollMax.x == 0.0f ? 0.0f : 12.0f;

        // vertical scroll bar
        if(gptCtx->ptCurrentWindow->tScrollMax.y != 0.0f)
        {
            const float fScrollbarHandleSize  = floorf((gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight - fBottomPadding) * ((gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight - fBottomPadding) / (gptCtx->ptCurrentWindow->tContentSize.y)));
            const float fScrollbarHandleStart = floorf((gptCtx->ptCurrentWindow->tSize.y - fTitleBarHeight - fBottomPadding - fScrollbarHandleSize) * (gptCtx->ptCurrentWindow->tScroll.y/(gptCtx->ptCurrentWindow->tScrollMax.y)));
            
            pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 12.0f, fTitleBarHeight}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 2.0f, gptCtx->ptCurrentWindow->tSize.y - fBottomPadding}), gptCtx->tStyle.tScrollbarBgCol);
            pl_add_rect       (gptCtx->ptCurrentWindow->ptFgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 12.0f, fTitleBarHeight}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 2.0f, gptCtx->ptCurrentWindow->tSize.y - fBottomPadding}), gptCtx->tStyle.tScrollbarFrameCol, 1.0f);

            const plVec2 tStartPos = pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - 12.0f, fTitleBarHeight + fScrollbarHandleStart});
            const plVec2 tFinalSize = {gptCtx->ptCurrentWindow->tSize.x - 2.0f, fTitleBarHeight + fScrollbarHandleStart + fScrollbarHandleSize};
            const plRect tBoundingBox = pl_calculate_rect(tStartPos, tFinalSize);
            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uVerticalScrollHash, &bHovered, &bHeld);
            if(bHeld || gptCtx->uActiveId == uVerticalScrollHash) pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){10.0f, fScrollbarHandleSize}), gptCtx->tStyle.tScrollbarActiveCol);
            else if(bHovered) pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){10.0f, fScrollbarHandleSize}), gptCtx->tStyle.tScrollbarHoveredCol);
            else              pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){10.0f, fScrollbarHandleSize}), gptCtx->tStyle.tScrollbarHandleCol);

            if(gptCtx->uActiveId == uVerticalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
            {
                gptCtx->ptCurrentWindow->bScrolling = true;
                gptCtx->uNextHoveredId = uVerticalScrollHash;
                gptCtx->ptCurrentWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;
                gptCtx->ptCurrentWindow->tScroll.y = pl_minf(gptCtx->ptCurrentWindow->tScroll.y, gptCtx->ptCurrentWindow->tScrollMax.y);
                gptCtx->ptCurrentWindow->tScroll.y = pl_maxf(gptCtx->ptCurrentWindow->tScroll.y, 0.0f);
                pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
            }
        }

        // horizontal scroll bar
        if(gptCtx->ptCurrentWindow->tScrollMax.x != 0.0f)
        {
            const float fScrollbarHandleSize  = floorf((gptCtx->ptCurrentWindow->tSize.x - fRightSidePadding) * ((gptCtx->ptCurrentWindow->tSize.x - fRightSidePadding) / (gptCtx->ptCurrentWindow->tContentSize.x)));
            const float fScrollbarHandleStart = floorf((gptCtx->ptCurrentWindow->tSize.x - fRightSidePadding - fScrollbarHandleSize) * (gptCtx->ptCurrentWindow->tScroll.x/(gptCtx->ptCurrentWindow->tScrollMax.x)));
            
            pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){0.0f, gptCtx->ptCurrentWindow->tSize.y - 12.0f}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - fRightSidePadding, gptCtx->ptCurrentWindow->tSize.y - 2.0f}), gptCtx->tStyle.tScrollbarBgCol);
            pl_add_rect       (gptCtx->ptCurrentWindow->ptFgLayer, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){0.0f, gptCtx->ptCurrentWindow->tSize.y - 12.0f}), pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x - fRightSidePadding, gptCtx->ptCurrentWindow->tSize.y - 2.0f}), gptCtx->tStyle.tScrollbarFrameCol, 1.0f);
            
            const plVec2 tStartPos = pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){fScrollbarHandleStart, gptCtx->ptCurrentWindow->tSize.y - 12.0f});
            const plVec2 tFinalSize = {fScrollbarHandleSize, 10.0f};
            const plRect tBoundingBox = pl_calculate_rect(tStartPos, tFinalSize);
            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__ui_button_behavior(&tBoundingBox, uHorizonatalScrollHash, &bHovered, &bHeld);   
            if(bHeld || gptCtx->uActiveId == uHorizonatalScrollHash) pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.tScrollbarActiveCol);
            else if(bHovered) pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.tScrollbarHoveredCol);
            else              pl_add_rect_filled(gptCtx->ptCurrentWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.tScrollbarHandleCol);

            if(gptCtx->uActiveId == uHorizonatalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
            {
                gptCtx->ptCurrentWindow->bScrolling = true;
                gptCtx->uNextHoveredId = uHorizonatalScrollHash;
                gptCtx->ptCurrentWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;
                gptCtx->ptCurrentWindow->tScroll.x = pl_minf(gptCtx->ptCurrentWindow->tScroll.x, gptCtx->ptCurrentWindow->tScrollMax.x);
                gptCtx->ptCurrentWindow->tScroll.x = pl_maxf(gptCtx->ptCurrentWindow->tScroll.x, 0.0f);
                pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
            }
        }

        // resizing grip
        if(gptCtx->ptCurrentWindow->bActive)
        {
            const plVec2 tCornerPos = pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize);
            const plVec2 tCornerTopPos = pl_add_vec2(tCornerPos, (plVec2){0.0f, -15.0f});
            const plVec2 tCornerLeftPos = pl_add_vec2(tCornerPos, (plVec2){-15.0f, 0.0f});
            pl_add_triangle_filled(gptCtx->ptCurrentWindow->ptFgLayer, tCornerPos, tCornerTopPos, tCornerLeftPos, (plVec4){0.33f, 0.02f, 0.10f, 1.0f});
        }

        if(pl_is_mouse_hovering_rect(gptCtx->ptCurrentWindow->tPos, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize)))
        {
            gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount; 
            gptCtx->uNextHoveredWindowId = gptCtx->ptCurrentWindow->uId;
        }

        gptCtx->ptCurrentWindow->tFullSize = gptCtx->ptCurrentWindow->tSize;
    }
    else // collapsed
    {
        if(pl_is_mouse_hovering_rect(gptCtx->ptCurrentWindow->tPos, pl_add_vec2(gptCtx->ptCurrentWindow->tPos, (plVec2){gptCtx->ptCurrentWindow->tSize.x, fTitleBarHeight})))
            gptCtx->ptCurrentWindow->ulFrameHovered = pl_get_io_context()->ulFrameCount;
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
    pl_push_clip_rect(gptCtx->ptCurrentWindow->ptFgLayer, pl_calculate_rect(tStartClip, tEndClip));
    pl_push_clip_rect(gptCtx->ptCurrentWindow->ptBgLayer, pl_calculate_rect(tStartClip, tEndClip));
}

void
pl_ui_end_tooltip(void)
{
    
    gptCtx->ptCurrentWindow->tSize.x = gptCtx->ptCurrentWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    gptCtx->ptCurrentWindow->tSize.y = gptCtx->ptCurrentWindow->tContentSize.y;
    pl_add_rect_filled(gptCtx->ptCurrentWindow->ptBgLayer, 
        gptCtx->ptCurrentWindow->tPos, 
        pl_add_vec2(gptCtx->ptCurrentWindow->tPos, gptCtx->ptCurrentWindow->tSize), gptCtx->tStyle.tWindowBgColor);

    pl_pop_clip_rect(gptCtx->ptCurrentWindow->ptFgLayer);
    pl_pop_clip_rect(gptCtx->ptCurrentWindow->ptBgLayer);
    gptCtx->ptCurrentWindow = gptCtx->ptCurrentWindow->ptParentWindow;
    
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
    pl__ui_next_line();
    return bPressed;
}

bool
pl_ui_selectable(const char* pcText, bool* bpValue)
{
    // temporary hack
    static bool bDummyState = true;
    if(bpValue == NULL) bpValue = &bDummyState;

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
    pl__ui_advance_cursor(0.0f, pl__ui_get_frame_height());
    gptCtx->ptCurrentWindow->tTempData.tLastLineSize = gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize;
    gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.x = 0.0f;
    gptCtx->ptCurrentWindow->tTempData.tCurrentLineSize.y = 0.0f;
    return *bpValue; 
}

bool
pl_ui_checkbox(const char* pcText, bool* bpValue)
{
    // temporary hack
    static bool bDummyState = true;
    if(bpValue == NULL) bpValue = &bDummyState;

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const bool bOriginalValue = *bpValue;
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
        *bpValue = !bOriginalValue;

    if(bHeld)         pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tFrameBgActiveCol);
    else if(bHovered) pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tFrameBgHoveredCol);
    else              pl_add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tFrameBgCol);

    if(*bpValue)
        pl_add_line(ptWindow->ptFgLayer, tStartPos, tEndBoxPos, gptCtx->tStyle.tCheckmarkCol, 2.0f);

    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tStyle.tTextCol, pcText, -1.0f);
    pl__ui_register_size(tTextSize.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + fFrameHeight, fFrameHeight);
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
    pl__ui_next_line();
    return bPressed;
}

bool
pl_ui_collapsing_header(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));

    bool* pbOpenState = pl_ui_get_bool_ptr(&ptWindow->tStorage, uHash, false);

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
    pl__ui_next_line();
    return *pbOpenState; 
}

bool
pl_ui_tree_node(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));

    bool* pbOpenState = pl_ui_get_bool_ptr(&ptWindow->tStorage, uHash, false);

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
    pl__ui_next_line();
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
    pl__ui_next_line();
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
    const plVec2 tTextSize = pl_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, acTempBuffer, -1.0f);
    pl__add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){ptWindow->tTempData.tCursorPos.x, ptWindow->tTempData.tCursorPos.y + ptWindow->fTextVerticalOffset}, tColor, acTempBuffer, -1.0f);
    pl__ui_register_size(tTextSize.x, tTextSize.y);
    pl__ui_next_line();
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
    pl__ui_next_line();
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

    const plVec2 tFinalPos = pl_add_vec2(ptWindow->tTempData.tCursorPos, tSize);
    pl_add_image_ex(ptWindow->ptFgLayer, tTexture, ptWindow->tTempData.tCursorPos, tFinalPos, tUv0, tUv1, tTintColor);

    if(tBorderColor.a > 0.0f)
        pl_add_rect(ptWindow->ptFgLayer, ptWindow->tTempData.tCursorPos, tFinalPos, tBorderColor, 1.0f);

    pl__ui_register_size(tSize.x, tSize.y);
    pl__ui_next_line();
}

void
pl_ui_separator(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tInitialCursorPos = ptWindow->tTempData.tCursorPos;

    const plVec2 tSize = {
        pl__ui_get_window_content_width_available(),
        gptCtx->tStyle.tItemSpacing.y * 2    
    };

    pl_add_line(ptWindow->ptFgLayer, tInitialCursorPos, (plVec2){tInitialCursorPos.x + tSize.x, tInitialCursorPos.y}, gptCtx->tStyle.tCheckmarkCol, 1.0f);

    pl__ui_register_size(tSize.x, tSize.y);
    pl__ui_next_line();    
}

void
pl_ui_same_line(float fOffsetFromStart, float fSpacing)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    if(fOffsetFromStart > 0.0f)
        ptWindow->tTempData.tCursorPos.x = ptWindow->tTempData.tCursorStartPos.x + fOffsetFromStart;
    else
        ptWindow->tTempData.tCursorPos.x += ptWindow->tTempData.tLastLineSize.x;

    ptWindow->tTempData.tCursorPos.y -= ptWindow->tTempData.tLastLineSize.y + gptCtx->tStyle.tItemSpacing.y * 2.0f;

    if(fSpacing < 0.0f)
        ptWindow->tTempData.tCursorPos.x += gptCtx->tStyle.tItemSpacing.x;
    else
        ptWindow->tTempData.tCursorPos.x += fSpacing;

    ptWindow->tTempData.tCurrentLineSize.x = pl_maxf(ptWindow->tTempData.tLastLineSize.x, ptWindow->tTempData.tCurrentLineSize.x);
    ptWindow->tTempData.tCurrentLineSize.y = pl_maxf(ptWindow->tTempData.tLastLineSize.y, ptWindow->tTempData.tCurrentLineSize.y);

    ptWindow->tTempData.tLastLineSize.x = 0.0f;
    ptWindow->tTempData.tLastLineSize.y = 0.0f;
}

void
pl_ui_next_line(void)
{
   pl__ui_next_line(); 
}

void
pl_ui_align_text(void)
{
    const float tFrameHeight = pl__ui_get_frame_height();
    gptCtx->ptCurrentWindow->fTextVerticalOffset = tFrameHeight / 2.0f - gptCtx->tStyle.fFontSize / 2.0f;
}

void
pl_ui_vertical_spacing(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tCursorPos.y += gptCtx->tStyle.tItemSpacing.y * 2.0f;
}

void
pl_ui_indent(float fIndent)
{
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.x += fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
    gptCtx->ptCurrentWindow->tTempData.fExtraIndent += fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
}

void
pl_ui_unindent(float fIndent)
{
    gptCtx->ptCurrentWindow->tTempData.tCursorPos.x -= fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
    gptCtx->ptCurrentWindow->tTempData.fExtraIndent -= fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
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
    ptCtx->tStyle.tTitleActiveCol      = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tTitleBgCol          = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    ptCtx->tStyle.tWindowBgColor       = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    ptCtx->tStyle.tButtonCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tButtonHoveredCol    = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tButtonActiveCol     = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tTextCol             = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    ptCtx->tStyle.tProgressBarCol      = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    ptCtx->tStyle.tCheckmarkCol        = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tFrameBgCol          = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tFrameBgHoveredCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    ptCtx->tStyle.tFrameBgActiveCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    ptCtx->tStyle.tHeaderCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tHeaderHoveredCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    ptCtx->tStyle.tHeaderActiveCol     = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    ptCtx->tStyle.tScrollbarBgCol      = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    ptCtx->tStyle.tScrollbarHandleCol  = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    ptCtx->tStyle.tScrollbarFrameCol   = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    ptCtx->tStyle.tScrollbarActiveCol  = ptCtx->tStyle.tButtonActiveCol;
    ptCtx->tStyle.tScrollbarHoveredCol = ptCtx->tStyle.tButtonHoveredCol;
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
pl_ui_debug(bool* pbOpen)
{
    if(pl_ui_begin_window("Pilot Light UI Metrics/Debugger", pbOpen, false))
    {

        plIOContext* ptIOCtx = pl_get_io_context();

        pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);

        pl_ui_separator();

        if(pl_ui_tree_node("Windows"))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbtWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = &gptCtx->sbtWindows[uWindowIndex];
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
                    pl_ui_text(" - Active:       %s", ptWindow->bActive ? "1" : "0");
                    pl_ui_text(" - Hovered:      %s", ptWindow->bHovered ? "1" : "0");
                    pl_ui_text(" - Dragging:     %s", ptWindow->bDragging ? "1" : "0");
                    pl_ui_text(" - Collapsed:    %s", ptWindow->bCollapsed ? "1" : "0");
                    pl_ui_text(" - Auto Sized:   %s", ptWindow->bAutoSize ? "1" : "0");

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
            pl_ui_text("Toggle ID:      %u", gptCtx->uToggledId);
            pl_ui_text("Hovered ID:     %u", gptCtx->uHoveredId);
            pl_ui_unindent(0.0f);
            pl_ui_tree_pop();
        }
    }
    pl_ui_end_window();
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static bool
pl__ui_is_item_hoverable(const plRect* ptBox, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow->bResizing || ptWindow->bScrolling || !gptCtx->bMouseOwned)
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