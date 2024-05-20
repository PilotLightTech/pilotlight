/*
   pl_ui.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] context
// [SECTION] enums
// [SECTION] internal api
// [SECTION] stb_text mess
// [SECTION] public api implementations
// [SECTION] internal api implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_ui_internal.h"
#include <float.h> // FLT_MAX
#include <stdio.h>

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ds.h"

// extensions
#include "pl_draw_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_set_dark_theme(void)
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
    gptCtx->tColorScheme.tTitleActiveCol      = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCol          = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCollapsedCol = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tWindowBgColor       = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tWindowBorderColor   = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tChildBgColor        = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tButtonCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonHoveredCol    = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonActiveCol     = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTextCol             = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    gptCtx->tColorScheme.tProgressBarCol      = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    gptCtx->tColorScheme.tCheckmarkCol        = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgCol          = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgHoveredCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    gptCtx->tColorScheme.tFrameBgActiveCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    gptCtx->tColorScheme.tHeaderCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tHeaderHoveredCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    gptCtx->tColorScheme.tHeaderActiveCol     = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    gptCtx->tColorScheme.tScrollbarBgCol      = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    gptCtx->tColorScheme.tScrollbarHandleCol  = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tScrollbarFrameCol   = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    gptCtx->tColorScheme.tScrollbarActiveCol  = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tScrollbarHoveredCol = gptCtx->tColorScheme.tButtonHoveredCol;
}

plDrawList2D*
pl_get_draw_list(void)
{
    return gptCtx->ptDrawlist;
}

plDrawList2D*
pl_get_debug_draw_list(void)
{
    return gptCtx->ptDebugDrawlist;
}

void
pl_new_frame(void)
{

    gptIO->bWantTextInput = false;
    gptIO->bWantCaptureMouse = false;

    // update state id's from previous frame
    gptCtx->uHoveredId = gptCtx->uNextHoveredId;
    gptCtx->uNextHoveredId = 0;
    gptIO->bWantCaptureKeyboard = gptCtx->uActiveId != 0;
    gptIO->bWantCaptureMouse = gptIO->_abMouseOwned[0] || gptCtx->uActiveId != 0 || gptCtx->ptMovingWindow != NULL;

    // null starting state
    gptCtx->bActiveIdJustActivated = false;
    
    // reset previous item data
    gptCtx->tPrevItemData.bHovered = false;

    // reset next window data
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_NONE;

    // reset active window
    if(gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false) && gptCtx->ptHoveredWindow == NULL)
        gptCtx->uActiveWindowId = 0;

    // reset active id if no longer alive
    if(gptCtx->uActiveId != 0 && gptCtx->uActiveIdIsAlive != gptCtx->uActiveId)
    {
        pl__set_active_id(0, NULL);
    }
    gptCtx->uActiveIdIsAlive = 0;

    if(gptCtx->uActiveWindowId == 0)
        gptCtx->ptActiveWindow = NULL;

    // track click ownership
    for(uint32_t i = 0; i < 5; i++)
    {
        if(gptIO->_abMouseClicked[i])
        {
            gptIO->_abMouseOwned[i] = (gptCtx->ptHoveredWindow != NULL);
        }
    }
}

void
pl__focus_window(plUiWindow* ptWindow)
{
    
    pl_sb_del(gptCtx->sbptFocusedWindows, ptWindow->ptRootWindow->uFocusOrder);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        gptCtx->sbptFocusedWindows[i]->uFocusOrder = i;
    }
    ptWindow->ptRootWindow->uFocusOrder = pl_sb_size(gptCtx->sbptFocusedWindows);
    pl_sb_push(gptCtx->sbptFocusedWindows, ptWindow->ptRootWindow);
}

void
pl_end_frame(void)
{

    const plVec2 tMousePos = gptIOI->get_mouse_pos();

    // submit windows in display order
    pl_sb_reset(gptCtx->sbptWindows);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        plUiWindow* ptRootWindow = gptCtx->sbptFocusedWindows[i];

        if(ptRootWindow->bActive)
            pl_submit_window(ptRootWindow);

        // adjust window size if outside viewport
        if(ptRootWindow->tPos.x > gptIO->afMainViewportSize[0])
            ptRootWindow->tPos.x = gptIO->afMainViewportSize[0] - ptRootWindow->tSize.x / 2.0f;

        if(ptRootWindow->tPos.y > gptIO->afMainViewportSize[1])
        {
            ptRootWindow->tPos.y = gptIO->afMainViewportSize[1] - ptRootWindow->tSize.y / 2.0f;
            ptRootWindow->tPos.y = pl_maxf(ptRootWindow->tPos.y, 0.0f);
        }
    }

    // find windows
    gptCtx->ptHoveredWindow = NULL;
    gptCtx->ptWheelingWindow = NULL;
    if(gptIOI->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->ptMovingWindow = NULL;
        gptCtx->ptSizingWindow = NULL;
        gptCtx->ptScrollingWindow = NULL;  
    }

    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        plUiWindow* ptWindow = gptCtx->sbptWindows[i];
        if(pl_rect_contains_point(&ptWindow->tOuterRectClipped, gptIO->_tMousePos))
        {
            gptCtx->ptHoveredWindow = ptWindow;

            // scrolling
            if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE) && gptIOI->get_mouse_wheel() != 0.0f)
                gptCtx->ptWheelingWindow = ptWindow;
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        plUiWindow* ptWindow = gptCtx->sbptWindows[i];
        plRect tBoundBox = ptWindow->tOuterRectClipped;

        float fTitleBarHeight = 0.0f;

        if(ptWindow->ptRootWindow == ptWindow)
        {
            fTitleBarHeight = ptWindow->tTempData.fTitleBarHeight;

            // add padding for resizing from borders
            if(!(ptWindow->tFlags & (PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_AUTO_SIZE)))
                tBoundBox = pl_rect_expand(&tBoundBox, 2.0f);
        }

        if(gptIOI->is_mouse_hovering_rect(tBoundBox.tMin, tBoundBox.tMax))
        {

            // check if window is activated
            if(gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false) && gptCtx->ptHoveredWindow == ptWindow)
            {
                gptCtx->ptMovingWindow = NULL;
                gptCtx->uActiveWindowId = ptWindow->ptRootWindow->uId;
                gptCtx->ptActiveWindow = ptWindow;

                gptIO->_abMouseOwned[PL_MOUSE_BUTTON_LEFT] = true;

                const plRect tTitleBarHitRegion = {
                    .tMin = {ptWindow->tPos.x + 2.0f, ptWindow->tPos.y + 2.0f},
                    .tMax = {ptWindow->tPos.x + ptWindow->tSize.x - 2.0f, ptWindow->tPos.y + fTitleBarHeight}
                };

                // check if window titlebar is clicked
                if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) && gptIOI->is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                    gptCtx->ptMovingWindow = ptWindow;

                pl__focus_window(ptWindow);
            }
        }
    }

    // scroll window
    if(gptCtx->ptWheelingWindow)
    {
        gptCtx->ptWheelingWindow->tScroll.y -= gptIOI->get_mouse_wheel() * 10.0f;
        gptCtx->ptWheelingWindow->tScroll.y = pl_clampf(0.0f, gptCtx->ptWheelingWindow->tScroll.y, gptCtx->ptWheelingWindow->tScrollMax.y);
    }

    // moving window
    if(gptCtx->ptMovingWindow && gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f) && !(gptCtx->ptMovingWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_MOVE))
    {

        if(tMousePos.x > 0.0f && tMousePos.x < gptIO->afMainViewportSize[0])
            gptCtx->ptMovingWindow->tPos.x = gptCtx->ptMovingWindow->tPos.x + gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;

        if(tMousePos.y > 0.0f && tMousePos.y < gptIO->afMainViewportSize[1])
            gptCtx->ptMovingWindow->tPos.y = gptCtx->ptMovingWindow->tPos.y + gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;  

        // clamp x
        gptCtx->ptMovingWindow->tPos.x = pl_maxf(gptCtx->ptMovingWindow->tPos.x, -gptCtx->ptMovingWindow->tSize.x / 2.0f);   
        gptCtx->ptMovingWindow->tPos.x = pl_minf(gptCtx->ptMovingWindow->tPos.x, gptIO->afMainViewportSize[0] - gptCtx->ptMovingWindow->tSize.x / 2.0f);

        // clamp y
        gptCtx->ptMovingWindow->tPos.y = pl_maxf(gptCtx->ptMovingWindow->tPos.y, 0.0f);   
        gptCtx->ptMovingWindow->tPos.y = pl_minf(gptCtx->ptMovingWindow->tPos.y, gptIO->afMainViewportSize[1] - 50.0f);

        gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    gptIO->_fMouseWheel = 0.0f;
    gptIO->_fMouseWheelH = 0.0f;
    pl_sb_reset(gptIO->_sbInputQueueCharacters);
}

void
pl_render(plRenderEncoder tEncoder, float fWidth, float fHeight, uint32_t uMSAASampleCount)
{
    gptDraw->submit_2d_layer(gptCtx->ptBgLayer);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        if(gptCtx->sbptWindows[i]->uHideFrames == 0)
        {
            gptDraw->submit_2d_layer(gptCtx->sbptWindows[i]->ptBgLayer);
            gptDraw->submit_2d_layer(gptCtx->sbptWindows[i]->ptFgLayer);
        }
        else
        {
            gptCtx->sbptWindows[i]->uHideFrames--;
        }
    }
    gptDraw->submit_2d_layer(gptCtx->tTooltipWindow.ptBgLayer);
    gptDraw->submit_2d_layer(gptCtx->tTooltipWindow.ptFgLayer);
    gptDraw->submit_2d_layer(gptCtx->ptFgLayer);
    gptDraw->submit_2d_layer(gptCtx->ptDebugLayer);

    gptDraw->submit_2d_drawlist(gptCtx->ptDrawlist, tEncoder, fWidth, fHeight, uMSAASampleCount);
    gptDraw->submit_2d_drawlist(gptCtx->ptDebugDrawlist, tEncoder, fWidth, fHeight, uMSAASampleCount);

    pl_end_frame();
}

void
pl_push_theme_color(plUiColor tColor, const plVec4* ptColor)
{
    const plUiColorStackItem tPrevItem = {
        .tIndex = tColor,
        .tColor = gptCtx->tColorScheme.atColors[tColor]
    };
    gptCtx->tColorScheme.atColors[tColor] = *ptColor;
    pl_sb_push(gptCtx->sbtColorStack, tPrevItem);
}

void
pl_pop_theme_color(uint32_t uCount)
{
    for(uint32_t i = 0; i < uCount; i++)
    {
        const plUiColorStackItem tPrevItem = pl_sb_last(gptCtx->sbtColorStack);
        gptCtx->tColorScheme.atColors[tPrevItem.tIndex] = tPrevItem.tColor;
        pl_sb_pop(gptCtx->sbtColorStack);
    }
}

void
pl_set_default_font(plFont* ptFont)
{
    gptCtx->ptFont = ptFont;
}

plFont*
pl_get_default_font(void)
{
    return gptCtx->ptFont;
}

void
pl_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
        .uColumns         = uWidgetCount,
        .pfSizesOrRatios  = pfSizesOrRatios
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_end_window(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    float fTitleBarHeight = ptWindow->tTempData.fTitleBarHeight;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        // cursor max pos - start pos + padding
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

    if(ptWindow->bScrollbarX && ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }
    else if(!ptWindow->bScrollbarY)
        ptWindow->tScroll.y = 0;
    else if(!ptWindow->bScrollbarX)
        ptWindow->tScroll.x = 0;

    const bool bScrollBarsPresent = ptWindow->bScrollbarX || ptWindow->bScrollbarY;

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
        gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

        // draw background
        gptDraw->add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

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

        gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

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
        gptDraw->add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

        // vertical scroll bar
        if(ptWindow->bScrollbarY)
            pl_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

        // horizontal scroll bar
        if(ptWindow->bScrollbarX)
            pl_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

        const plVec2 tTopLeft = pl_rect_top_left(&ptWindow->tOuterRect);
        const plVec2 tBottomLeft = pl_rect_bottom_left(&ptWindow->tOuterRect);
        const plVec2 tTopRight = pl_rect_top_right(&ptWindow->tOuterRect);
        const plVec2 tBottomRight = pl_rect_bottom_right(&ptWindow->tOuterRect);

        // resizing grip
        if (!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_RESIZE))
        {
            {
                const plVec2 tCornerTopLeftPos = pl_add_vec2(tBottomRight, (plVec2){-15.0f, -15.0f});
                const plVec2 tCornerTopPos = pl_add_vec2(tBottomRight, (plVec2){0.0f, -15.0f});
                const plVec2 tCornerLeftPos = pl_add_vec2(tBottomRight, (plVec2){-15.0f, 0.0f});

                const plRect tBoundingBox = pl_calculate_rect(tCornerTopLeftPos, (plVec2){15.0f, 15.0f});
                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uResizeHash)
                {
                    gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.99f, 0.02f, 0.10f, 1.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
                }
                else if(gptCtx->uHoveredId == uResizeHash)
                {
                    gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.66f, 0.02f, 0.10f, 1.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
                }
                else
                {
                    gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.33f, 0.02f, 0.10f, 1.0f});   
                }
            }

            // east border
            {

                plRect tBoundingBox = pl_calculate_rect(tTopRight, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uEastResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uEastResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
                else if(gptCtx->uHoveredId == uEastResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
            }

            // west border
            {
                plRect tBoundingBox = pl_calculate_rect(tTopLeft, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uWestResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uWestResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
                else if(gptCtx->uHoveredId == uWestResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
            }

            // north border
            {
                plRect tBoundingBox = {tTopLeft, (plVec2){tTopRight.x - 15.0f, tTopRight.y}};
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uNorthResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uNorthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
                else if(gptCtx->uHoveredId == uNorthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
            }

            // south border
            {
                plRect tBoundingBox = {tBottomLeft, (plVec2){tBottomRight.x - 15.0f, tBottomRight.y}};
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl_button_behavior(&tBoundingBox, uSouthResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uSouthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
                else if(gptCtx->uHoveredId == uSouthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
            }
        }

        // draw border
        gptDraw->add_rect(ptWindow->ptFgLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, gptCtx->tColorScheme.tWindowBorderColor, 1.0f);

        // handle corner resizing
        if(gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
        {
            const plVec2 tMousePos = gptIOI->get_mouse_pos();

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
                    const float fScrollConversion = roundf(ptWindow->tContentSize.y / ptWindow->tSize.y);
                    ptWindow->tScroll.y += gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
                    ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }

            // handle horizontal scrolling with scroll bar
            else if(gptCtx->uActiveId == uHorizonatalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.x > ptWindow->tPos.x && tMousePos.x < ptWindow->tPos.x + ptWindow->tSize.x)
                {
                    const float fScrollConversion = roundf(ptWindow->tContentSize.x / ptWindow->tSize.x);
                    ptWindow->tScroll.x += gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
                    ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
                    gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }
        }
        gptCtx->ptCurrentWindow->tFullSize = ptWindow->tSize;

        if(gptCtx->uActiveId >= uResizeHash && gptCtx->uActiveId < uResizeHash + 7 && gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
            pl__set_active_id(gptCtx->uActiveId, ptWindow);
    }

    gptCtx->ptCurrentWindow = NULL;
    pl_sb_pop(gptCtx->sbuIdStack);
}

bool
pl_begin_window(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    bool bResult = pl_begin_window_ex(pcName, pbOpen, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bResult)
    {
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
        pl_end_window();
    return bResult;
}

plDrawLayer2D*
pl_get_window_fg_drawlayer(void)
{
    PL_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptFgLayer;
}

plDrawLayer2D*
pl_get_window_bg_drawlayer(void)
{
    PL_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptBgLayer; 
}

plVec2
pl_get_cursor_pos(void)
{
    return pl__ui_get_cursor_pos();
}

void
pl_set_next_window_size(plVec2 tSize, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tSize = tSize;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
    gptCtx->tNextWindowData.tSizeCondition = tCondition;
}

void
pl_end_child(void)
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
    ptWindow->tScrollMax = pl_sub_vec2(ptWindow->tContentSize, ptWindow->tSize);
    
    // clamp scrolling max
    ptWindow->tScrollMax = pl_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX && ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    plRect tParentBgRect = ptParentWindow->tOuterRect;
    const plRect tBgRect = pl_rect_clip(&ptWindow->tOuterRect, &tParentBgRect);

    gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

    const uint32_t uVerticalScrollHash = pl_str_hash("##scrollright", 0, pl_sb_top(gptCtx->sbuIdStack));
    const uint32_t uHorizonatalScrollHash = pl_str_hash("##scrollbottom", 0, pl_sb_top(gptCtx->sbuIdStack));

    // draw background
    gptDraw->add_rect_filled(ptParentWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

    // vertical scroll bar
    if(ptWindow->bScrollbarY)
        pl_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

    // horizontal scroll bar
    if(ptWindow->bScrollbarX)
        pl_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

    // handle vertical scrolling with scroll bar
    if(gptCtx->uActiveId == uVerticalScrollHash && gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tContentSize.y / ptWindow->tSize.y);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uVerticalScrollHash;
        ptWindow->tScroll.y += gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
        gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    // handle horizontal scrolling with scroll bar
    else if(gptCtx->uActiveId == uHorizonatalScrollHash && gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tContentSize.x / ptWindow->tSize.x);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uHorizonatalScrollHash;
        ptWindow->tScroll.x += gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
        gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    if((gptCtx->uActiveId == uHorizonatalScrollHash || gptCtx->uActiveId == uVerticalScrollHash) && gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        pl__set_active_id(gptCtx->uActiveId, ptWindow);

    ptWindow->tFullSize = ptWindow->tSize;
    pl_sb_pop(gptCtx->sbuIdStack);
    gptCtx->ptCurrentWindow = ptParentWindow;

    pl_advance_cursor(ptWindow->tSize.x, ptWindow->tSize.y);
}

bool
pl_begin_child(const char* pcName)
{
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_calculate_item_size(200.0f);

    const plUiWindowFlags tFlags = 
        PL_UI_WINDOW_FLAGS_CHILD_WINDOW |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    pl_set_next_window_size(tWidgetSize, PL_UI_COND_ALWAYS);
    bool bValue =  pl_begin_window_ex(pcName, NULL, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bValue)
    {
        plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
        ptWindow->tMinSize = pl_min_vec2(ptWindow->tMinSize, tWidgetSize);
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
    {
        pl_end_child();
    }
    return bValue;
}

void
pl_begin_tooltip(void)
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
    const plVec2 tMousePos = gptIOI->get_mouse_pos();
    ptWindow->tTempData.tCursorStartPos = pl_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    ptWindow->tPos = tMousePos;
    ptWindow->tTempData.tRowPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    ptWindow->tTempData.tRowPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);

    const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y };
    const plVec2 tEndClip = { ptWindow->tSize.x, ptWindow->tSize.y };
    gptDraw->push_clip_rect(gptCtx->ptDrawlist, pl_calculate_rect(tStartClip, tEndClip), false);

    ptWindow->ptParentWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = ptWindow;

    static const float pfRatios[] = {300.0f};
    pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
}

void
pl_end_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    ptWindow->tSize.y = ptWindow->tContentSize.y;

    gptDraw->add_rect_filled(ptWindow->ptBgLayer,
        ptWindow->tPos, 
        pl_add_vec2(ptWindow->tPos, ptWindow->tSize), gptCtx->tColorScheme.tWindowBgColor);

    gptDraw->pop_clip_rect(gptCtx->ptDrawlist);
    gptCtx->ptCurrentWindow = ptWindow->ptParentWindow;
}

plVec2
pl_get_window_pos(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tPos;
}

plVec2
pl_get_window_size(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tSize;
}

plVec2
pl_get_window_scroll(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScroll;
}

plVec2
pl_get_window_scroll_max(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScrollMax;
}

void
pl_set_window_scroll(plVec2 tScroll)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    if(ptWindow->tScrollMax.x >= tScroll.x)
        ptWindow->tScroll.x = tScroll.x;

    if(ptWindow->tScrollMax.y >= tScroll.y)
        ptWindow->tScroll.y = tScroll.y;
}

void
pl_set_next_window_pos(plVec2 tPos, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tPos = tPos;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    gptCtx->tNextWindowData.tPosCondition = tCondition;
}

void
pl_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.bCollapsed = bCollapsed;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED;
    gptCtx->tNextWindowData.tCollapseCondition = tCondition;    
}

bool
pl_step_clipper(plUiClipper* ptClipper)
{
    if(ptClipper->uItemCount == 0)
        return false;
        
    if(ptClipper->uDisplayStart == 0 && ptClipper->uDisplayEnd == 0)
    {
        ptClipper->uDisplayStart = 0;
        ptClipper->uDisplayEnd = 1;
        ptClipper->_fItemHeight = 0.0f;
        ptClipper->_fStartPosY = pl__ui_get_cursor_pos().y;
        return true;
    }
    else if (ptClipper->_fItemHeight == 0.0f)
    {
        ptClipper->_fItemHeight = pl__ui_get_cursor_pos().y - ptClipper->_fStartPosY;
        if(ptClipper->_fStartPosY < pl_get_window_pos().y)
            ptClipper->uDisplayStart = (uint32_t)((pl_get_window_pos().y - ptClipper->_fStartPosY) / ptClipper->_fItemHeight);
        ptClipper->uDisplayEnd = ptClipper->uDisplayStart + (uint32_t)(pl_get_window_size().y / ptClipper->_fItemHeight) + 1;
        ptClipper->uDisplayEnd = pl_minu(ptClipper->uDisplayEnd, ptClipper->uItemCount) + 1;
        if(ptClipper->uDisplayStart > 0)
            ptClipper->uDisplayStart--;

        if(ptClipper->uDisplayEnd > ptClipper->uItemCount)
            ptClipper->uDisplayEnd = ptClipper->uItemCount;
        
        if(ptClipper->uDisplayStart > 0)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.uColumns; i++)
                pl_advance_cursor(0.0f, (float)ptClipper->uDisplayStart * ptClipper->_fItemHeight);
        }
        ptClipper->uDisplayStart++;
        return true;
    }
    else
    {
        if(ptClipper->uDisplayEnd < ptClipper->uItemCount)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.uColumns; i++)
                pl_advance_cursor(0.0f, (float)(ptClipper->uItemCount - ptClipper->uDisplayEnd) * ptClipper->_fItemHeight);
        }

        ptClipper->uDisplayStart = 0;
        ptClipper->uDisplayEnd = 0;
        ptClipper->_fItemHeight = 0.0f;
        ptClipper->_fStartPosY = 0.0f;
        ptClipper->uItemCount = 0;
        return false;
    }
}

void
pl_layout_dynamic(float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
        .uColumns         = uWidgetCount,
        .fWidth           = 1.0f / (float)uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_STATIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
        .uColumns         = uWidgetCount,
        .fWidth           = fWidth
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_row_push(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptCurrentRow->fWidth = fWidth;
}

void
pl_layout_row_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_template_begin(float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_NONE,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
        .uColumns         = 0,
        .uEntryStartIndex = pl_sb_size(ptWindow->sbtRowTemplateEntries)
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_template_push_dynamic(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uDynamicEntryCount++;
    pl_sb_add(ptWindow->sbtRowTemplateEntries);
    pl_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC;
    pl_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = 0.0f;
    ptCurrentRow->uColumns++;
}

void
pl_layout_template_push_variable(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uVariableEntryCount++;
    ptCurrentRow->fWidth += fWidth;
    pl_sb_push(ptWindow->sbuTempLayoutIndexSort, ptCurrentRow->uColumns);
    pl_sb_add(ptWindow->sbtRowTemplateEntries);
    pl_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE;
    pl_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = fWidth;
    ptCurrentRow->uColumns++;
    ptWindow->tTempData.fTempMinWidth += fWidth;
}

void
pl_layout_template_push_static(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fWidth += fWidth;
    ptCurrentRow->uStaticEntryCount++;
    pl_sb_add(ptWindow->sbtRowTemplateEntries);
    pl_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC;
    pl_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = fWidth;
    ptCurrentRow->uColumns++;
    ptWindow->tTempData.fTempStaticWidth += fWidth;
    ptWindow->tTempData.fTempMinWidth += fWidth;
}

void
pl_layout_template_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // total available width minus padding/spacing
    float fWidthAvailable = 0.0f;
    if(ptWindow->bScrollbarY)
        fWidthAvailable = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
    else
        fWidthAvailable = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);

    // simplest cast, not enough room, so nothing left to distribute to dynamic widths
    if(ptWindow->tTempData.fTempMinWidth >= fWidthAvailable)
    {
        for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
        {
            plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + i];
            if(ptEntry->tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                ptEntry->fWidth = 0.0f;
        }
    }
    else if((ptCurrentRow->uDynamicEntryCount + ptCurrentRow->uVariableEntryCount) != 0)
    {

        // sort large to small (slow bubble sort, should replace later)
        bool bSwapOccured = true;
        while(bSwapOccured)
        {
            if(ptCurrentRow->uVariableEntryCount == 0)
                break;
            bSwapOccured = false;
            for(uint32_t i = 0; i < ptCurrentRow->uVariableEntryCount - 1; i++)
            {
                const uint32_t ii = ptWindow->sbuTempLayoutIndexSort[i];
                const uint32_t jj = ptWindow->sbuTempLayoutIndexSort[i + 1];
                
                plUiLayoutRowEntry* ptEntry0 = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ii];
                plUiLayoutRowEntry* ptEntry1 = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + jj];

                if(ptEntry0->fWidth < ptEntry1->fWidth)
                {
                    ptWindow->sbuTempLayoutIndexSort[i] = jj;
                    ptWindow->sbuTempLayoutIndexSort[i + 1] = ii;
                    bSwapOccured = true;
                }
            }
        }

        // add dynamic to the end
        if(ptCurrentRow->uDynamicEntryCount > 0)
        {

            // dynamic entries appended to the end so they will be "sorted" from the get go
            for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
            {
                plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + i];
                if(ptEntry->tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                    pl_sb_push(ptWindow->sbuTempLayoutIndexSort, i);
            }
        }

        // organize into levels
        float fCurrentWidth = -10000.0f;
        for(uint32_t i = 0; i < ptCurrentRow->uVariableEntryCount; i++)
        {
            const uint32_t ii = ptWindow->sbuTempLayoutIndexSort[i];
            plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ii];

            if(ptEntry->fWidth == fCurrentWidth)
            {
                pl_sb_back(ptWindow->sbtTempLayoutSort).uCount++;
            }
            else
            {
                const plUiLayoutSortLevel tNewSortLevel = {
                    .fWidth      = ptEntry->fWidth,
                    .uCount      = 1,
                    .uStartIndex = i
                };
                pl_sb_push(ptWindow->sbtTempLayoutSort, tNewSortLevel);
                fCurrentWidth = ptEntry->fWidth;
            }
        }

        // add dynamic to the end
        if(ptCurrentRow->uDynamicEntryCount > 0)
        {
            const plUiLayoutSortLevel tInitialSortLevel = {
                .fWidth      = 0.0f,
                .uCount      = ptCurrentRow->uDynamicEntryCount,
                .uStartIndex = ptCurrentRow->uVariableEntryCount
            };
            pl_sb_push(ptWindow->sbtTempLayoutSort, tInitialSortLevel);
        }

        // calculate left over width
        float fExtraWidth = fWidthAvailable - ptWindow->tTempData.fTempMinWidth;

        // distribute to levels
        const uint32_t uLevelCount = pl_sb_size(ptWindow->sbtTempLayoutSort);
        if(uLevelCount == 1)
        {
            plUiLayoutSortLevel tCurrentSortLevel = pl_sb_pop(ptWindow->sbtTempLayoutSort);
            const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
            for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uCount; i++)
            {
                plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                ptEntry->fWidth += fDistributableWidth;
            }
        }
        else
        {
            while(fExtraWidth > 0.0f)
            {
                plUiLayoutSortLevel tCurrentSortLevel = pl_sb_pop(ptWindow->sbtTempLayoutSort);

                if(pl_sb_size(ptWindow->sbtTempLayoutSort) == 0) // final
                {
                    const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDistributableWidth;
                    }
                    break;
                }
                    
                const float fDelta = pl_sb_back(ptWindow->sbtTempLayoutSort).fWidth - tCurrentSortLevel.fWidth;
                const float fTotalOwed = fDelta * (float)tCurrentSortLevel.uCount;
                
                if(fTotalOwed < fExtraWidth) // perform operations
                {
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDelta;
                    }
                    pl_sb_back(ptWindow->sbtTempLayoutSort).uCount += tCurrentSortLevel.uCount;
                    fExtraWidth -= fTotalOwed;
                }
                else // do the best we can
                {
                    const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDistributableWidth;
                    }
                    fExtraWidth = 0.0f;
                }
            }
        }

    }

    pl_sb_reset(ptWindow->sbuTempLayoutIndexSort);
    pl_sb_reset(ptWindow->sbtTempLayoutSort);
    ptWindow->tTempData.fTempMinWidth = 0.0f;
    ptWindow->tTempData.fTempStaticWidth = 0.0f;
}

void
pl_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fHeight : 1.0f,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_SPACE,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_layout_space_push(float fX, float fY, float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptCurrentRow->fHorizontalOffset = ptCurrentRow->tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fX * ptWindow->tSize.x : fX;
    ptCurrentRow->fVerticalOffset = fY * ptCurrentRow->fSpecifiedHeight;
    ptCurrentRow->fWidth = fWidth;
    ptCurrentRow->fHeight = fHeight * ptCurrentRow->fSpecifiedHeight;
}

void
pl_layout_space_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

bool
pl_was_last_item_hovered(void)
{
    return gptCtx->tPrevItemData.bHovered;
}

bool
pl_was_last_item_active(void)
{
    return gptCtx->tPrevItemData.bActive;
}

int
pl_get_int(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return iDefaultValue;
    return ptIterator->iValue;
}

float
pl_get_float(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return fDefaultValue;
    return ptIterator->fValue;
}

bool
pl_get_bool(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return pl_get_int(ptStorage, uKey, bDefaultValue ? 1 : 0) != 0;
}

void*
pl_get_ptr(plUiStorage* ptStorage, uint32_t uKey)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return NULL;
    return ptIterator->pValue;    
}

int*
pl_get_int_ptr(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->iValue;
}

float*
pl_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->fValue;
}

bool*
pl_get_bool_ptr(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return (bool*)pl_get_int_ptr(ptStorage, uKey, bDefaultValue ? 1 : 0);
}

void**
pl_get_ptr_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->pValue;
}

void
pl_set_int(plUiStorage* ptStorage, uint32_t uKey, int iValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iValue}));
        return;
    }
    ptIterator->iValue = iValue;
}

void
pl_set_float(plUiStorage* ptStorage, uint32_t uKey, float fValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fValue}));
        return;
    }
    ptIterator->fValue = fValue;
}

void
pl_set_bool(plUiStorage* ptStorage, uint32_t uKey, bool bValue)
{
    pl_set_int(ptStorage, uKey, bValue ? 1 : 0);
}

void
pl_set_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pValue)
{
    plUiStorageEntry* ptIterator = pl_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pValue}));
        return;
    }
    ptIterator->pValue = pValue;    
}

const char*
pl_find_renderered_text_end(const char* pcText, const char* pcTextEnd)
{
    const char* pcTextDisplayEnd = pcText;
    if (!pcTextEnd)
        pcTextEnd = (const char*)-1;

    while (pcTextDisplayEnd < pcTextEnd && *pcTextDisplayEnd != '\0' && (pcTextDisplayEnd[0] != '#' || pcTextDisplayEnd[1] != '#'))
        pcTextDisplayEnd++;
    return pcTextDisplayEnd;
}

bool
pl_is_item_hoverable(const plRect* ptBox, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    if(!gptIOI->is_mouse_hovering_rect(ptBox->tMin, ptBox->tMax))
        return false;

    // check if another item is already hovered
    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    // check if another item is already active
    if(gptCtx->uActiveId != 0 && gptCtx->uActiveId != uHash)
        return false;

    return true;
}

bool
pl_is_item_hoverable_circle(plVec2 tP, float fRadius, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    // hovered & active ID must be some value but not ours
    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    // active ID must be not used or ours
    if(!(gptCtx->uActiveId == uHash || gptCtx->uActiveId == 0))
        return false;
        
    return pl_does_circle_contain_point(tP, fRadius, gptIOI->get_mouse_pos());
}

void
pl_ui_add_text(plDrawLayer2D* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x), roundf(tP.y)}, tColor, pcText, pl_find_renderered_text_end(pcText, pcTextEnd), fWrap);
}

void
pl_add_clipped_text(plDrawLayer2D* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text_clipped_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x + 0.5f), roundf(tP.y + 0.5f)}, tMin, tMax, tColor, pcText, pl_find_renderered_text_end(pcText, pcTextEnd), fWrap);   
}

plVec2
pl_ui_calculate_text_size(plFont* font, float size, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    return gptDraw->calculate_text_size_ex(font, size, text, pl_find_renderered_text_end(text, pcTextEnd), wrap);  
}

bool
pl_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point)
{
    bool b1 = ((point.x - p1.x) * (p0.y - p1.y) - (point.y - p1.y) * (p0.x - p1.x)) < 0.0f;
    bool b2 = ((point.x - p2.x) * (p1.y - p2.y) - (point.y - p2.y) * (p1.x - p2.x)) < 0.0f;
    bool b3 = ((point.x - p0.x) * (p2.y - p0.y) - (point.y - p0.y) * (p2.x - p0.x)) < 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

plUiStorageEntry*
pl_lower_bound(plUiStorageEntry* sbtData, uint32_t uKey)
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

bool
pl_begin_window_ex(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    plUiWindow* ptWindow = NULL;                          // window we are working on
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow; // parent window if there any

    // generate hashed ID
    const uint32_t uWindowID = pl_str_hash(pcName, 0, ptParentWindow ? pl_sb_top(gptCtx->sbuIdStack) : 0);
    pl_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text & title bar sizes
    const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcName, 0.0f);
    float fTitleBarHeight = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist in storage
    ptWindow = pl_get_ptr(&gptCtx->tWindows, uWindowID);

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
        ptWindow->ptBgLayer               = gptDraw->request_2d_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->ptFgLayer               = gptDraw->request_2d_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->tPosAllowableFlags      = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tSizeAllowableFlags     = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tCollapseAllowableFlags = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->ptParentWindow          = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? ptParentWindow : ptWindow;
        ptWindow->uFocusOrder             = pl_sb_size(gptCtx->sbptFocusedWindows);
        ptWindow->tFlags                  = PL_UI_WINDOW_FLAGS_NONE;

        // add to focused windows if not a child
        if(!(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW))
        {
            pl_sb_push(gptCtx->sbptFocusedWindows, ptWindow);
            ptWindow->ptRootWindow = ptWindow;
        }
        else
            ptWindow->ptRootWindow = ptParentWindow->ptRootWindow;

        // add window to storage
        pl_set_ptr(&gptCtx->tWindows, uWindowID, ptWindow);
    }

    // seen this frame (obviously)
    
    ptWindow->bActive = true;
    ptWindow->tFlags = tFlags;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {

        plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
        const plVec2 tStartPos   = pl__ui_get_cursor_pos();

        // set window position to parent window current cursor
        ptWindow->tPos = tStartPos;

        pl_sb_push(ptParentWindow->sbtChildWindows, ptWindow);
    }
    gptCtx->ptCurrentWindow = ptWindow;

    // reset per frame window temporary data
    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));
    pl_sb_reset(ptWindow->sbtChildWindows);
    pl_sb_reset(ptWindow->sbtRowStack);
    pl_sb_reset(ptWindow->sbtRowTemplateEntries);

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

    // updating outer rect here but autosized windows do so again in pl_end_window(..)
    ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
    ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
    ptWindow->tInnerRect = ptWindow->tOuterRect;

    // remove scrollbars from inner rect
    if(ptWindow->bScrollbarX)
        ptWindow->tInnerRect.tMax.y -= gptCtx->tStyle.fScrollbarSize + 2.0f;
    if(ptWindow->bScrollbarY)
        ptWindow->tInnerRect.tMax.x -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    // decorations
    if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR)) // has title bar
    {

        ptWindow->tInnerRect.tMin.y += fTitleBarHeight;

        // draw title bar
        plVec4 tTitleColor;
        if(ptWindow->uId == gptCtx->uActiveWindowId)
            tTitleColor = gptCtx->tColorScheme.tTitleActiveCol;
        else if(ptWindow->bCollapsed)
            tTitleColor = gptCtx->tColorScheme.tTitleBgCollapsedCol;
        else
            tTitleColor = gptCtx->tColorScheme.tTitleBgCol;
        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x, fTitleBarHeight}), tTitleColor);

        // draw title text
        const plVec2 titlePos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, titlePos, gptCtx->tColorScheme.tTextCol, pcName, 0.0f);

        // draw close button
        const float fTitleBarButtonRadius = 8.0f;
        float fTitleButtonStartPos = fTitleBarButtonRadius * 2.0f;
        if(pbOpen)
        {
            const uint32_t uCloseHash = pl_str_hash("PL_CLOSE", 0, pl_sb_top(gptCtx->sbuIdStack));
            plVec2 tCloseCenterPos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            plVec2 tCloseTLPos = {tCloseCenterPos.x - fTitleBarButtonRadius, tCloseCenterPos.y - fTitleBarButtonRadius};
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;
            
            plRect tBoundingBox = pl_calculate_rect(tCloseTLPos, (plVec2){fTitleBarButtonRadius * 2.0f, fTitleBarButtonRadius * 2.0f});
            bool bHovered = false;
            bool bHeld = false;
            bool bPressed = pl_button_behavior(&tBoundingBox, uCloseHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uCloseHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 12);
            else if(gptCtx->uHoveredId == uCloseHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 12);
            else                                      gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.0f, 0.0f, 1.0f}, 12);

            if(bPressed)
                *pbOpen = false;
        }

        // draw collapse button
        if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        {
            const uint32_t uCollapseHash = pl_str_hash("PL_COLLAPSE", 0, pl_sb_top(gptCtx->sbuIdStack));
            plVec2 tCloseCenterPos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            plVec2 tCloseTLPos = {tCloseCenterPos.x - fTitleBarButtonRadius, tCloseCenterPos.y - fTitleBarButtonRadius};
 
            plRect tBoundingBox = pl_calculate_rect(tCloseTLPos, (plVec2){fTitleBarButtonRadius * 2.0f, fTitleBarButtonRadius * 2.0f});
            bool bHovered = false;
            bool bHeld = false;
            bool bPressed = pl_button_behavior(&tBoundingBox, uCollapseHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uCollapseHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 12);
            else if(gptCtx->uHoveredId == uCollapseHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 12);
            else                                         gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.5f, 0.0f, 1.0f}, 12);

            if(bPressed)
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

    }
    else
        fTitleBarHeight = 0.0f;

    // remove padding for inner clip rect
    ptWindow->tInnerClipRect = pl_rect_expand_vec2(&ptWindow->tInnerRect, (plVec2){-gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});

    if(!ptWindow->bCollapsed)
    {
        const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight };

        const plVec2 tInnerClip = { 
            ptWindow->tSize.x - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f),
            ptWindow->tSize.y - fTitleBarHeight - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f)
        };

        const plRect* ptClipRect = gptDraw->get_clip_rect(gptCtx->ptDrawlist);

        if(ptClipRect)
        {
            ptWindow->tInnerClipRect = pl_rect_clip_full(&ptWindow->tInnerClipRect, ptClipRect);
            ptWindow->tOuterRectClipped = pl_rect_clip_full(&ptWindow->tOuterRectClipped, ptClipRect);
        }
        gptDraw->push_clip_rect(gptCtx->ptDrawlist, ptWindow->tInnerClipRect, false);
    }

    // update cursors
    ptWindow->tTempData.tCursorStartPos.x = gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - ptWindow->tScroll.x;
    ptWindow->tTempData.tCursorStartPos.y = gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - ptWindow->tScroll.y;
    ptWindow->tTempData.tRowPos = ptWindow->tTempData.tCursorStartPos;
    ptWindow->tTempData.tCursorStartPos = pl_floor_vec2(ptWindow->tTempData.tCursorStartPos);
    ptWindow->tTempData.fTitleBarHeight = fTitleBarHeight;

    // reset next window flags
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;

    
    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        ptWindow->bVisible = pl_rect_overlaps_rect(&ptWindow->tInnerClipRect, &ptParentWindow->tInnerClipRect);
        return ptWindow->bVisible && !pl_rect_is_inverted(&ptWindow->tInnerClipRect);
    }

    ptWindow->bVisible = true;
    return !ptWindow->bCollapsed;
}

void
pl_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis)
{
    const plRect tParentBgRect = ptWindow->ptParentWindow->tOuterRect;
    if(tAxis == PL_UI_AXIS_X)
    {
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);

        const float fScrollbarHandleSize  = pl_maxf(5.0f, floorf((ptWindow->tSize.x - fRightSidePadding) * ((ptWindow->tSize.x - fRightSidePadding) / (ptWindow->tContentSize.x))));
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

            gptDraw->add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tColorScheme.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);   
            if(gptCtx->uActiveId == uHash)
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash)
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarHoveredCol);
            else
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarHandleCol);
        }
    }
    else if(tAxis == PL_UI_AXIS_Y)
    {
          
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fTopPadding = (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);

        const float fScrollbarHandleSize  = pl_maxf(5.0f, floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding) * ((ptWindow->tSize.y - fTopPadding - fBottomPadding) / (ptWindow->tContentSize.y))));
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
            gptDraw->add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tColorScheme.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);

            // scrollbar handle
            if(gptCtx->uActiveId == uHash) 
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash) 
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarHoveredCol);
            else
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarHandleCol);

        }
    }

    if(gptCtx->uActiveId == uHash && gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        pl__set_active_id(uHash, ptWindow);
}

plVec2
pl_calculate_item_size(float fDefaultHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    float fHeight = ptCurrentRow->fHeight;

    if(fHeight == 0.0f)
        fHeight = fDefaultHeight;

    if(ptCurrentRow->tSystemType ==  PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE)
    {
        const plUiLayoutRowEntry* ptCurrentEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptCurrentRow->uCurrentColumn];
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

void
pl_advance_cursor(float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    ptCurrentRow->uCurrentColumn++;
    
    ptCurrentRow->fMaxWidth = pl_maxf(ptCurrentRow->fHorizontalOffset + fWidth, ptCurrentRow->fMaxWidth);
    ptCurrentRow->fMaxHeight = pl_maxf(ptCurrentRow->fMaxHeight, ptCurrentRow->fVerticalOffset + fHeight);

    // not yet at end of row
    if(ptCurrentRow->uCurrentColumn < ptCurrentRow->uColumns)
        ptCurrentRow->fHorizontalOffset += fWidth + gptCtx->tStyle.tItemSpacing.x;

    // automatic wrap
    if(ptCurrentRow->uCurrentColumn == ptCurrentRow->uColumns && ptCurrentRow->tSystemType != PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x);
        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);   

        // reset
        ptCurrentRow->uCurrentColumn = 0;
        ptCurrentRow->fMaxWidth = 0.0f;
        ptCurrentRow->fMaxHeight = 0.0f;
        ptCurrentRow->fHorizontalOffset = ptCurrentRow->fRowStartX + ptWindow->tTempData.fExtraIndent;
        ptCurrentRow->fVerticalOffset = 0.0f;
    }

    // passed end of row
    if(ptCurrentRow->uCurrentColumn > ptCurrentRow->uColumns && ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        PL_ASSERT(false);
    }
}

void
pl_submit_window(plUiWindow* ptWindow)
{
    ptWindow->bActive = false; // no longer active (for next frame)
    pl_sb_push(gptCtx->sbptWindows, ptWindow);
    for(uint32_t j = 0; j < pl_sb_size(ptWindow->sbtChildWindows); j++)
        pl_submit_window(ptWindow->sbtChildWindows[j]);
}

void
pl__set_active_id(uint32_t uHash, plUiWindow* ptWindow)
{
    gptCtx->bActiveIdJustActivated = gptCtx->uActiveId != uHash;
    gptCtx->uActiveId = uHash;    
    gptCtx->ptActiveWindow = ptWindow;

    if(uHash)
        gptCtx->uActiveIdIsAlive = uHash;

    if(gptCtx->bActiveIdJustActivated && uHash)
        pl__focus_window(ptWindow);
}

void
pl_ui_initialize(void)
{
    gptCtx->ptDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->ptDebugDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->ptBgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist, "ui Background");
    gptCtx->ptFgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist, "ui Foreground");
    gptCtx->ptDebugLayer = gptDraw->request_2d_layer(gptCtx->ptDebugDrawlist, "ui debug");
    gptCtx->tTooltipWindow.ptBgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist, "Tooltip Background");
    gptCtx->tTooltipWindow.ptFgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist, "Tooltip Foreground");

    gptCtx->tFrameBufferScale.x = 1.0f;
    gptCtx->tFrameBufferScale.y = 1.0f;
    pl_set_dark_theme();
}

void
pl_ui_cleanup(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptFocusedWindows[i]->sbtChildWindows); j++)
        {
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->tStorage.sbtData);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbtRowStack);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbtRowTemplateEntries);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbtTempLayoutSort);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbuTempLayoutIndexSort);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->tStorage.sbtData);
            PL_FREE(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]);
        }
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->tStorage.sbtData);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtRowStack);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtRowTemplateEntries);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtTempLayoutSort);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbuTempLayoutIndexSort);
        PL_FREE(gptCtx->sbptFocusedWindows[i]);
    }
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbDrawlists);
    pl_sb_free(gptCtx->sbptFocusedWindows);
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbtColorStack);
    pl_sb_free(gptCtx->sbtTabBars);
    pl_sb_free(gptCtx->sbuIdStack);
    pl_sb_free(gptCtx->tWindows.sbtData);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

#include "pl_ui_widgets.c"
#include "pl_ui_demo.c"

static const plUiI*
pl_load_ui_api(void)
{
    static const plUiI tApi = {
        .initialize                    = pl_ui_initialize,
        .cleanup                       = pl_ui_cleanup,
        .get_draw_list                 = pl_get_draw_list,
        .get_debug_draw_list           = pl_get_debug_draw_list,
        .new_frame                     = pl_new_frame,
        .end_frame                     = pl_end_frame,
        .render                        = pl_render,
        .show_debug_window             = pl_show_debug_window,
        .show_style_editor_window      = pl_show_style_editor_window,
        .show_demo_window              = pl_show_demo_window,
        .set_dark_theme                = pl_set_dark_theme,
        .push_theme_color              = pl_push_theme_color,
        .pop_theme_color               = pl_pop_theme_color,
        .set_default_font              = pl_set_default_font,
        .get_default_font              = pl_get_default_font,
        .begin_window                  = pl_begin_window,
        .end_window                    = pl_end_window,
        .get_window_fg_drawlayer       = pl_get_window_fg_drawlayer,
        .get_window_bg_drawlayer       = pl_get_window_bg_drawlayer,
        .get_cursor_pos                = pl_get_cursor_pos,
        .begin_child                   = pl_begin_child,
        .end_child                     = pl_end_child,
        .begin_tooltip                 = pl_begin_tooltip,
        .end_tooltip                   = pl_end_tooltip,
        .get_window_pos                = pl_get_window_pos,
        .get_window_size               = pl_get_window_size,
        .get_window_scroll             = pl_get_window_scroll,
        .get_window_scroll_max         = pl_get_window_scroll_max,
        .set_window_scroll             = pl_set_window_scroll,
        .set_next_window_pos           = pl_set_next_window_pos,
        .set_next_window_size          = pl_set_next_window_size,
        .set_next_window_collapse      = pl_set_next_window_collapse,
        .button                        = pl_button,
        .selectable                    = pl_selectable,
        .checkbox                      = pl_checkbox,
        .radio_button                  = pl_radio_button,
        .image                         = pl_image,
        .image_ex                      = pl_image_ex,
        .image_button                  = pl_image_button,
        .image_button_ex               = pl_image_button_ex,
        .invisible_button              = pl_invisible_button,
        .dummy                         = pl_dummy,
        .progress_bar                  = pl_progress_bar,
        .text                          = pl_text,
        .text_v                        = pl_text_v,
        .color_text                    = pl_color_text,
        .color_text_v                  = pl_color_text_v,
        .labeled_text                  = pl_labeled_text,
        .labeled_text_v                = pl_labeled_text_v,
        .input_text                    = pl_input_text,
        .input_text_hint               = pl_input_text_hint,
        .input_float                   = pl_input_float,
        .input_int                     = pl_input_int,
        .slider_float                  = pl_slider_float,
        .slider_float_f                = pl_slider_float_f,
        .slider_int                    = pl_slider_int,
        .slider_int_f                  = pl_slider_int_f,
        .drag_float                    = pl_drag_float,
        .drag_float_f                  = pl_drag_float_f,
        .collapsing_header             = pl_collapsing_header,
        .end_collapsing_header         = pl_end_collapsing_header,
        .tree_node                     = pl_tree_node,
        .tree_node_f                   = pl_tree_node_f,
        .tree_node_v                   = pl_tree_node_v,
        .tree_pop                      = pl_tree_pop,
        .begin_tab_bar                 = pl_begin_tab_bar,
        .end_tab_bar                   = pl_end_tab_bar,
        .begin_tab                     = pl_begin_tab,
        .end_tab                       = pl_end_tab,
        .separator                     = pl_separator,
        .vertical_spacing              = pl_vertical_spacing,
        .indent                        = pl_indent,
        .unindent                      = pl_unindent,
        .step_clipper                  = pl_step_clipper,
        .layout_dynamic                = pl_layout_dynamic,
        .layout_static                 = pl_layout_static,
        .layout_row_begin              = pl_layout_row_begin,
        .layout_row_push               = pl_layout_row_push,
        .layout_row_end                = pl_layout_row_end,
        .layout_row                    = pl_layout_row,
        .layout_template_begin         = pl_layout_template_begin,
        .layout_template_push_dynamic  = pl_layout_template_push_dynamic,
        .layout_template_push_variable = pl_layout_template_push_variable,
        .layout_template_push_static   = pl_layout_template_push_static,
        .layout_template_end           = pl_layout_template_end,
        .layout_space_begin            = pl_layout_space_begin,
        .layout_space_push             = pl_layout_space_push,
        .layout_space_end              = pl_layout_space_end,
        .was_last_item_hovered         = pl_was_last_item_hovered,
        .was_last_item_active          = pl_was_last_item_active,
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    gptIOI  = ptApiRegistry->first(PL_API_IO);
    gptDraw = ptApiRegistry->first(PL_API_DRAW);

    gptIO = gptIOI->get_io();
    if(bReload)
    {
        gptCtx = ptDataRegistry->get_data("plUiContext");
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_UI), pl_load_ui_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_UI, pl_load_ui_api());

        static plUiContext tContext = {0};
        gptCtx = &tContext;
        memset(gptCtx, 0, sizeof(plUiContext));

        ptDataRegistry->set_data("plUiContext", gptCtx);
    }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
}
