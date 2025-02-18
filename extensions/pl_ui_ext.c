/*
   pl_ui_ext.c
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
// [SECTION] helper windows
// [SECTION] extension loading
*/

#include "pl_ui_ext.h"
#include "pl_ui_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_set_dark_theme(void)
{
    // styles
    gptCtx->tStyle.fTitlePadding            = 5.0f;
    gptCtx->tStyle.fFontSize                = 13.0f;
    gptCtx->tStyle.fWindowHorizontalPadding = 5.0f;
    gptCtx->tStyle.fWindowVerticalPadding   = 5.0f;
    gptCtx->tStyle.iWindowBorderSize        = 0;
    gptCtx->tStyle.fIndentSize              = 15.0f;
    gptCtx->tStyle.fScrollbarSize           = 10.0f;
    gptCtx->tStyle.fSliderSize              = 12.0f;
    gptCtx->tStyle.fWindowRounding          = 0.0f;
    gptCtx->tStyle.fChildRounding           = 0.0f;
    gptCtx->tStyle.fFrameRounding           = 0.0f;
    gptCtx->tStyle.fScrollbarRounding       = 0.0f;
    gptCtx->tStyle.fGrabRounding            = 0.0f;
    gptCtx->tStyle.fTabRounding             = 4.0f;
    gptCtx->tStyle.tItemSpacing             = (plVec2){8.0f, 4.0f};
    gptCtx->tStyle.tInnerSpacing            = (plVec2){4.0f, 4.0f};
    gptCtx->tStyle.tFramePadding            = (plVec2){4.0f, 4.0f};
    gptCtx->tStyle.tSeparatorTextPadding    = (plVec2){20.0f, 3.0f};
    gptCtx->tStyle.tSeparatorTextAlignment  = (plVec2){0.0f, 0.5f};
    gptCtx->tStyle.fSeparatorTextLineSize   = 3.0f;

    // colors
    gptCtx->tColorScheme.tTitleActiveCol       = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCol           = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCollapsedCol  = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tWindowBgColor        = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tWindowBorderColor    = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tChildBgColor         = (plVec4){0.25f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tButtonCol            = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonHoveredCol     = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonActiveCol      = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTextCol              = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    gptCtx->tColorScheme.tTextDisabledCol      = (plVec4){0.50f, 0.50f, 0.50f, 1.00f};
    gptCtx->tColorScheme.tProgressBarCol       = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    gptCtx->tColorScheme.tCheckmarkCol         = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgCol           = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgHoveredCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    gptCtx->tColorScheme.tFrameBgActiveCol     = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    gptCtx->tColorScheme.tHeaderCol            = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tHeaderHoveredCol     = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    gptCtx->tColorScheme.tHeaderActiveCol      = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    gptCtx->tColorScheme.tScrollbarBgCol       = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    gptCtx->tColorScheme.tScrollbarHandleCol   = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tScrollbarFrameCol    = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    gptCtx->tColorScheme.tScrollbarActiveCol   = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tScrollbarHoveredCol  = gptCtx->tColorScheme.tButtonHoveredCol;
    gptCtx->tColorScheme.tSeparatorCol         = gptCtx->tColorScheme.tScrollbarHandleCol;
    gptCtx->tColorScheme.tTabCol               = gptCtx->tColorScheme.tButtonCol;
    gptCtx->tColorScheme.tTabHoveredCol        = gptCtx->tColorScheme.tButtonHoveredCol;
    gptCtx->tColorScheme.tTabSelectedCol       = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tResizeGripCol        = (plVec4){0.33f, 0.02f, 0.10f, 1.0f};
    gptCtx->tColorScheme.tResizeGripHoveredCol = (plVec4){0.66f, 0.02f, 0.10f, 1.0f};
    gptCtx->tColorScheme.tResizeGripActiveCol  = (plVec4){0.99f, 0.02f, 0.10f, 1.0f};
    gptCtx->tColorScheme.tSliderCol            = gptCtx->tColorScheme.tButtonCol;
    gptCtx->tColorScheme.tSliderHoveredCol     = gptCtx->tColorScheme.tButtonHoveredCol;
    gptCtx->tColorScheme.tSliderActiveCol      = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tPopupBgColor         = (plVec4){0.15f, 0.10f, 0.10f, 0.78f};
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

bool
pl_wants_mouse_capture(void)
{
    return gptCtx->bWantCaptureMouse;
}

bool
pl_wants_keyboard_capture(void)
{
    return gptCtx->bWantCaptureKeyboard;
}

bool
pl_wants_text_input(void)
{
    return gptCtx->bWantTextInput;
}

void
pl_new_frame(void)
{

    pl_sb_reset(gptCtx->sbtBeginPopupStack);

    gptCtx->bWantTextInput = false;
    gptCtx->bWantCaptureMouse = gptCtx->uActiveId != 0 || gptCtx->ptMovingWindow != NULL || gptCtx->ptActiveWindow != NULL;
    gptCtx->bWantCaptureKeyboard = gptCtx->uActiveId != 0;

    for(uint32_t i = 0; i < 5; i++)
    {
        if(gptCtx->abMouseOwned[i])
        {
            gptCtx->bWantCaptureMouse = true;
        }
    }

    // update state id's from previous frame
    gptCtx->uHoveredId = gptCtx->uNextHoveredId;
    gptCtx->uNextHoveredId = 0;

    // null starting state
    gptCtx->bActiveIdJustActivated = false;
    
    // reset previous item data
    gptCtx->tPrevItemData.bHovered = false;

    // reset next window data
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_NONE;

    if(gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false) && gptCtx->uHoveredId == 0)
    {
        // gptCtx->uActiveWindowId = 0;
        pl_sb_reset(gptCtx->sbtOpenPopupStack);
        gptCtx->ptNavWindow = NULL;
    }

    // reset active id if no longer alive
    if(gptCtx->uActiveId != 0 && gptCtx->uActiveIdIsAlive != gptCtx->uActiveId)
    {
        pl__set_active_id(0, NULL);
    }
    gptCtx->uActiveIdIsAlive = 0;
    if(!gptCtx->bNavIdIsAlive)
        gptCtx->uNavId = 0;
    gptCtx->bNavIdIsAlive = false;

    if(gptCtx->uActiveId == 0)
        gptCtx->ptActiveWindow = NULL;
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
    // draw submission
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

    const plVec2 tMousePos = gptIOI->get_mouse_pos();

    // submit windows in display order
    pl_sb_reset(gptCtx->sbptWindows);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        plUiWindow* ptRootWindow = gptCtx->sbptFocusedWindows[i];

        // recursively submits child windows
        if(ptRootWindow->bActive)
            pl__submit_window(ptRootWindow);
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

    // find windows
    //   - we are assuming they will be the last window hovered
    bool bRequestFocus = false;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        plUiWindow* ptWindow = gptCtx->sbptWindows[i];
        if(pl_rect_contains_point(&ptWindow->tOuterRectClipped, gptIO->_tMousePos))
        {
            gptCtx->ptHoveredWindow = ptWindow;

            // scrolling
            if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE) && gptIOI->get_mouse_wheel() != 0.0f)
                gptCtx->ptWheelingWindow = ptWindow;

            float fTitleBarHeight = ptWindow->tTempData.fTitleBarHeight;
            const plRect tTitleBarHitRegion = {
                .tMin = {ptWindow->tPos.x + 2.0f, ptWindow->tPos.y + 2.0f},
                .tMax = {ptWindow->tPos.x + ptWindow->tSize.x - 2.0f, ptWindow->tPos.y + fTitleBarHeight}
            };

            // check if window is activated
            if(gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            {

                bRequestFocus = true;
                gptCtx->ptMovingWindow = NULL;
                gptCtx->ptNavWindow = ptWindow;

                // check if window titlebar is clicked
                if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) && gptIOI->is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                    gptCtx->ptMovingWindow = ptWindow;

            }
        }
    }

    if(bRequestFocus)
        pl__focus_window(gptCtx->ptHoveredWindow->ptRootWindow);

    // scroll window
    if(gptCtx->ptWheelingWindow)
    {
        gptCtx->ptWheelingWindow->tScroll.y -= gptIOI->get_mouse_wheel() * 10.0f;
        gptCtx->ptWheelingWindow->tScroll.y = pl_clampf(0.0f, gptCtx->ptWheelingWindow->tScroll.y, gptCtx->ptWheelingWindow->tScrollMax.y);
    }

    // moving window
    if(gptCtx->ptMovingWindow && gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f) && !(gptCtx->ptMovingWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_MOVE))
    {

        if(tMousePos.x > 0.0f && tMousePos.x < gptIO->tMainViewportSize.x)
            gptCtx->ptMovingWindow->tPos.x = gptCtx->ptMovingWindow->tPos.x + gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;

        if(tMousePos.y > 0.0f && tMousePos.y < gptIO->tMainViewportSize.y)
            gptCtx->ptMovingWindow->tPos.y = gptCtx->ptMovingWindow->tPos.y + gptIOI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;  

        // clamp x
        gptCtx->ptMovingWindow->tPos.x = pl_maxf(gptCtx->ptMovingWindow->tPos.x, -gptCtx->ptMovingWindow->tSize.x / 2.0f);   
        gptCtx->ptMovingWindow->tPos.x = pl_minf(gptCtx->ptMovingWindow->tPos.x, gptIO->tMainViewportSize.x - gptCtx->ptMovingWindow->tSize.x / 2.0f);

        // clamp y
        gptCtx->ptMovingWindow->tPos.y = pl_maxf(gptCtx->ptMovingWindow->tPos.y, 0.0f);   
        gptCtx->ptMovingWindow->tPos.y = pl_minf(gptCtx->ptMovingWindow->tPos.y, gptIO->tMainViewportSize.y - 50.0f);

        gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    gptIO->_fMouseWheel = 0.0f;
    gptIO->_fMouseWheelH = 0.0f;
    pl_sb_reset(gptIO->_sbInputQueueCharacters);

    for(uint32_t i = 0; i < 5; i++)
    {
        if(gptIO->_abMouseClicked[i])
        {
            gptCtx->abMouseOwned[i] = gptCtx->ptHoveredWindow != NULL;
        }
        else if(!gptIO->_abMouseDown[i])
        {
            gptCtx->abMouseOwned[i] = gptCtx->ptHoveredWindow != NULL;
        }
        else if(gptIO->_abMouseReleased[i])
        {
            gptCtx->abMouseOwned[i] = false;
        }
    }
}

void
pl_push_theme_color(plUiColor tColorCode, plVec4 tColor)
{
    const plUiColorStackItem tPrevItem = {
        .tIndex = tColorCode,
        .tColor = gptCtx->tColorScheme.atColors[tColorCode]
    };
    gptCtx->tColorScheme.atColors[tColorCode] = tColor;
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
    gptCtx->tFont = ptFont;
    gptCtx->tStyle.fFontSize = ptFont->fSize;
}

plFont*
pl_get_default_font(void)
{
    return gptCtx->tFont;
}

void
pl_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    
    plUiLayoutRow tNewRow = {
        .fWidgetHeight    = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
        .uColumns         = uWidgetCount,
        .pfSizesOrRatios  = pfSizesOrRatios,
    };
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_end_window(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_MENU)
    {
        gptCtx->uMenuDepth--;
    }

    if(!ptWindow->bCollapsed)
        gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

    if(pl_sb_size(gptCtx->sbptWindowStack) == 0)
    {
        gptCtx->ptCurrentWindow = NULL;
    }
    else
    {
        gptCtx->ptCurrentWindow = pl_sb_pop(gptCtx->sbptWindowStack);
    }
    pl_sb_pop(gptCtx->sbuIdStack);

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW)
    {
        if(gptCtx->ptCurrentWindow && !gptCtx->ptCurrentWindow->bCollapsed)
            gptDraw->push_clip_rect(gptCtx->ptDrawlist, gptCtx->ptCurrentWindow->tInnerClipRect, false);
    }
}

bool
pl_begin_window(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    bool bResult = pl__begin_window_ex(pcName, pbOpen, tFlags);

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
    return pl__get_cursor_pos();
}

void
pl_set_next_window_size(plVec2 tSize, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tSize = tSize;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
    gptCtx->tNextWindowData.tSizeCondition = tCondition;
}

void
pl_set_next_window_pos(plVec2 tPos, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tPos = tPos;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    gptCtx->tNextWindowData.tPosCondition = tCondition;
}

void
pl_end_child(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_end_window();
    pl__smart_advance_cursor(ptWindow->tSize.x, ptWindow->tSize.y);
}

bool
pl_begin_child(const char* pcName, plUiChildFlags tChildFlags, plUiWindowFlags tWindowFlags)
{
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tLayoutRow;
    const plVec2 tStartPos   = pl__get_cursor_pos();
    const plVec2 tWidgetSize = pl__calculate_item_size(200.0f);

    const plUiWindowFlags tFlags = 
        PL_UI_WINDOW_FLAGS_CHILD_WINDOW |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    pl_set_next_window_pos(tStartPos, PL_UI_COND_ALWAYS);
    pl_set_next_window_size(tWidgetSize, PL_UI_COND_ALWAYS);
    bool bValue =  pl__begin_window_ex(pcName, NULL, tFlags);

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
pl_open_popup(const char* pcName, plUiPopupFlags tFlags)
{
    const uint32_t uHash = pl_str_hash(pcName, 0, pl_sb_top(gptCtx->sbuIdStack));
    plUiPopupData tPopupData = {
        .uId = uHash,
        .ulOpenFrameCount = gptIO->ulFrameCount
    };
    pl_sb_push(gptCtx->sbtOpenPopupStack, tPopupData);
}

void
pl_close_current_popup(void)
{
    if(pl_sb_size(gptCtx->sbtBeginPopupStack) == 0)
        return;

    uint32_t uCurrentPopupIndex = pl_sb_size(gptCtx->sbtBeginPopupStack) - 1;

    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtOpenPopupStack); i++)
    {
        if(gptCtx->sbtOpenPopupStack[i].uId == gptCtx->sbtBeginPopupStack[uCurrentPopupIndex].uId)
        {
            // pl__focus_window(gptCtx->ptCurrentWindow->ptRestoreWindow);
            pl_sb_del(gptCtx->sbtOpenPopupStack, i);
            break;
        }
    }
}

void
pl_end_popup(void)
{
    pl_end_window();
}

bool
pl_is_popup_open(const char* pcName)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcName, 0, pl_sb_top(gptCtx->sbuIdStack));

    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtOpenPopupStack); i++)
    {
        if(gptCtx->sbtOpenPopupStack[i].uId == uHash)
        {
            return true;
        }
    }
    return false;
}

bool
pl_begin_popup(const char* pcName, plUiWindowFlags tFlags)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const uint32_t uHash = pl_str_hash(pcName, 0, pl_sb_top(gptCtx->sbuIdStack));

    bool bIsOpen = false;
    bool bNewOpen = false;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtOpenPopupStack); i++)
    {
        if(gptCtx->sbtOpenPopupStack[i].uId == uHash)
        {
            bNewOpen = gptCtx->sbtOpenPopupStack[i].ulOpenFrameCount <= gptIO->ulFrameCount + 1;
            bIsOpen = true;
            break;
        }
    }

    plUiPopupData tPopupData = {
        .uId = uHash,
        .ulOpenFrameCount = gptIO->ulFrameCount + 1
    };
    pl_sb_push(gptCtx->sbtBeginPopupStack, tPopupData);

    if(bIsOpen)
    {
        gptDraw->pop_clip_rect(gptCtx->ptDrawlist);
        bool bResult = pl__begin_window_ex(pcName, NULL, tFlags | PL_UI_WINDOW_FLAGS_POPUP_WINDOW);

        static const float pfRatios[] = {300.0f};
        if(bResult)
        {
            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
            if(bNewOpen)
            {
                pl__focus_window(gptCtx->ptCurrentWindow);
            }
            gptCtx->ptNavWindow = gptCtx->ptCurrentWindow;
        }
        else
            pl_end_popup();
        return bResult;
    }

    return false;
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
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_AUTO_SIZE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    // place window at mouse position
    const plVec2 tMousePos = gptIOI->get_mouse_pos();
    ptWindow->tTempData.tCursorStartPos = pl_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    ptWindow->tPos = tMousePos;
    ptWindow->tTempData.tRowCursorPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    ptWindow->tTempData.tRowCursorPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);

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

    gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer,
        ptWindow->tPos, 
        pl_add_vec2(ptWindow->tPos, ptWindow->tSize), 0.0f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tWindowBgColor)});

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
        ptClipper->_fStartPosY = pl__get_cursor_pos().y;
        return true;
    }
    else if (ptClipper->_fItemHeight == 0.0f)
    {
        ptClipper->_fItemHeight = pl__get_cursor_pos().y - ptClipper->_fStartPosY;
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
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tLayoutRow.uColumns; i++)
                pl__smart_advance_cursor(0.0f, (float)ptClipper->uDisplayStart * ptClipper->_fItemHeight);
        }
        ptClipper->uDisplayStart++;
        return true;
    }
    else
    {
        if(ptClipper->uDisplayEnd < ptClipper->uItemCount)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tLayoutRow.uColumns; i++)
                pl__smart_advance_cursor(0.0f, (float)(ptClipper->uItemCount - ptClipper->uDisplayEnd) * ptClipper->_fItemHeight);
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
        .fWidgetHeight    = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
        .uColumns         = uWidgetCount,
        .fWidgetWidth     = 1.0f / (float)uWidgetCount
    };
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    plUiLayoutRow tNewRow = {
        .fWidgetHeight    = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_STATIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
        .uColumns         = uWidgetCount,
        .fWidgetWidth     = fWidth
    };
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    plUiLayoutRow tNewRow = {
        .fWidgetHeight    = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_layout_row_push(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptCurrentRow->fWidgetWidth = fWidth;
}

void
pl_layout_row_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowCursorPos.y = ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_layout_template_begin(float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    plUiLayoutRow tNewRow = {
        .fWidgetHeight    = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_NONE,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
        .uColumns         = 0,
        .uEntryStartIndex = pl_sb_size(ptWindow->sbtRowTemplateEntries)
    };
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_layout_template_push_dynamic(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
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
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    ptCurrentRow->uVariableEntryCount++;
    ptCurrentRow->fWidgetWidth += fWidth;
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
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    ptCurrentRow->fWidgetWidth += fWidth;
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
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowCursorPos.y = ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // total available width minus padding/spacing

    float fAvailableSize = 0.0f;
    if(pl_sb_size(ptWindow->sbfAvailableSizeStack) > 0)
    {
        fAvailableSize = pl_sb_top(ptWindow->sbfAvailableSizeStack);
    }
    else
    {
        fAvailableSize = ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;

        // remove scrollbar if present
        if(ptWindow->bScrollbarY)
            fAvailableSize -= (2.0f + gptCtx->tStyle.fScrollbarSize);
    }

    float fWidthAvailable = fAvailableSize
        - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1)                // remove spacing between widgets
        - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize; // remove indent

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
        .fWidgetHeight    = fHeight,
        .fSpecifiedHeight = tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fHeight : 1.0f,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_SPACE,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_layout_space_push(float fX, float fY, float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;

    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptCurrentRow->fWidgetXOffset = ptCurrentRow->tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fX * ptWindow->tSize.x : fX;
    ptCurrentRow->fWidgetYOffset = fY * ptCurrentRow->fSpecifiedHeight;
    ptCurrentRow->fWidgetWidth = fWidth;
    ptCurrentRow->fWidgetHeight = fHeight * ptCurrentRow->fSpecifiedHeight;
}

void
pl_layout_space_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowCursorPos.y = ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tLayoutRow = tNewRow;
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

static int
pl__get_int(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return iDefaultValue;
    return ptIterator->iValue;
}

static float
pl__get_float(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return fDefaultValue;
    return ptIterator->fValue;
}

static bool
pl__get_bool(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return pl__get_int(ptStorage, uKey, bDefaultValue ? 1 : 0) != 0;
}

static void*
pl__get_ptr(plUiStorage* ptStorage, uint32_t uKey)
{
    plUiStorageEntry* ptIterator = pl__lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return NULL;
    return ptIterator->pValue;    
}

static int*
pl__get_int_ptr(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
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

static float*
pl__get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
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

static bool*
pl__get_bool_ptr(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return (bool*)pl__get_int_ptr(ptStorage, uKey, bDefaultValue ? 1 : 0);
}

static void**
pl__get_ptr_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue)
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

static void
pl__set_int(plUiStorage* ptStorage, uint32_t uKey, int iValue)
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

static void
pl__set_float(plUiStorage* ptStorage, uint32_t uKey, float fValue)
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

static void
pl__set_bool(plUiStorage* ptStorage, uint32_t uKey, bool bValue)
{
    pl__set_int(ptStorage, uKey, bValue ? 1 : 0);
}

static void
pl__set_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pValue)
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

static bool
pl__is_item_hoverable(const plRect* ptBox, uint32_t uHash)
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
        
    return pl__does_circle_contain_point(tP, fRadius, gptIOI->get_mouse_pos());
}

static void
pl__add_text(plDrawLayer2D* ptLayer, plFont* ptFont, float fSize, plVec2 tP, uint32_t uColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text(ptLayer, (plVec2){roundf(tP.x), roundf(tP.y)}, pcText, (plDrawTextOptions){
        .fSize = fSize,
        .fWrap = fWrap,
        .pcTextEnd = pl__find_renderered_text_end(pcText, pcTextEnd),
        .ptFont = ptFont,
        .uColor = uColor
    });
}

static void
pl__add_clipped_text(plDrawLayer2D* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, uint32_t uColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text_clipped(ptLayer, (plVec2){roundf(tP.x), roundf(tP.y)}, pcText, tMin, tMax,
        (plDrawTextOptions){
            .fSize = fSize,
            .fWrap = fWrap,
            .pcTextEnd = pl__find_renderered_text_end(pcText, pcTextEnd),
            .ptFont = ptFont,
            .uColor = uColor
    });
}

static plVec2
pl__calculate_text_size(plFont* ptFont, float size, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    return gptDraw->calculate_text_size(text, (plDrawTextOptions){.ptFont = ptFont, .fSize = size, .pcTextEnd = pl__find_renderered_text_end(text, pcTextEnd), .fWrap = wrap});  
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
pl__begin_window_ex(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    plUiWindow* ptWindow = NULL;                          // window we are working on
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow; // parent window if there any

    if(tFlags & PL_UI_WINDOW_FLAGS_MENU)
    {
        gptCtx->uMenuDepth++;
    }

    if(ptParentWindow)
    {
        pl_sb_push(gptCtx->sbptWindowStack, ptParentWindow);
    }

    // generate hashed ID
    const uint32_t uWindowID = pl_str_hash(pcName, 0, ptParentWindow ? pl_sb_top(gptCtx->sbuIdStack) : 0);
    pl_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text & title bar sizes
    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->tFont, gptCtx->tStyle.fFontSize, pcName, 0.0f);
    float fTitleBarHeight = (tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist in storage
    ptWindow = pl__get_ptr(&gptCtx->tWindows, uWindowID);

    // new window needs to be created
    if(ptWindow == NULL)
    {
        // allocate new window
        ptWindow = PL_ALLOC(sizeof(plUiWindow));
        memset(ptWindow, 0, sizeof(plUiWindow));
        ptWindow->uId                           = uWindowID;
        ptWindow->szNameBufferLength            = strlen(pcName) + 1;
        ptWindow->pcName                        = PL_ALLOC(ptWindow->szNameBufferLength);
        ptWindow->tPos                          = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMinSize                      = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMaxSize                      = (plVec2){ 10000.0f, 10000.0f};
        ptWindow->tSize                         = (plVec2){ 500.0f, 500.0f};
        ptWindow->ptBgLayer                     = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
        ptWindow->ptFgLayer                     = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
        ptWindow->tPosAllowableFlags            = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tSizeAllowableFlags           = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tCollapseAllowableFlags       = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->ptParentWindow                = ptParentWindow;
        ptWindow->ptRootWindow                  = NULL;
        ptWindow->ptRootWindowPopupTree         = NULL;
        ptWindow->ptRootWindowTitleBarHighlight = NULL;
        ptWindow->uFocusOrder                   = pl_sb_size(gptCtx->sbptFocusedWindows);
        ptWindow->tFlags                        = PL_UI_WINDOW_FLAGS_NONE;
        ptWindow->bAppearing                    = true;

        memset(ptWindow->pcName, 0, ptWindow->szNameBufferLength);
        strcpy(ptWindow->pcName, pcName);

        // add to focused windows if not a child
        if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        {
            ptWindow->ptRootWindow = ptParentWindow->ptRootWindow;
            ptWindow->ptRootWindowPopupTree = ptWindow->ptRootWindow;
            ptWindow->ptRootWindowTitleBarHighlight = ptWindow->ptRootWindow;
        }
        else if(tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW)
        {
            pl_sb_push(gptCtx->sbptFocusedWindows, ptWindow);
            ptWindow->ptRootWindow = ptWindow;
            ptWindow->ptRootWindowPopupTree = ptParentWindow;
            ptWindow->ptRootWindowTitleBarHighlight = ptParentWindow->ptRootWindow;
        }
        else // normal window
        {
            pl_sb_push(gptCtx->sbptFocusedWindows, ptWindow);
            ptWindow->ptRootWindow = ptWindow;
            ptWindow->ptRootWindowPopupTree = ptWindow;
            ptWindow->ptRootWindowTitleBarHighlight = ptWindow;
        }

        // add window to storage
        pl__set_ptr(&gptCtx->tWindows, uWindowID, ptWindow);
    }
    else
    {
        ptWindow->bAppearing = false;
    }

    // seen this frame (obviously)
    ptWindow->bActive = true;
    ptWindow->tFlags = tFlags;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        pl_sb_push(ptParentWindow->sbtChildWindows, ptWindow);
    }
    gptCtx->ptCurrentWindow = ptWindow;

    // use last frame cursors for sizing
    ptWindow->tContentSize = pl_add_vec2((plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
        pl_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos));

    // reset per frame window temporary data
    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));
    pl_sb_reset(ptWindow->sbfAvailableSizeStack);
    pl_sb_reset(ptWindow->sbfMaxCursorYStack);
    pl_sb_reset(ptWindow->sbtChildWindows);
    pl_sb_reset(ptWindow->sbtRowStack);
    pl_sb_reset(ptWindow->sbtCursorStack);
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

            ptWindow->tMinSize = pl_min_vec2(ptWindow->tSize, ptWindow->tMinSize);
        }   
    }

    if(ptWindow->bCollapsed)
        ptWindow->tSize = (plVec2){ptWindow->tSize.x, fTitleBarHeight};

    // adjust window position if outside viewport
    if((ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) == 0)
    {
        if(ptWindow->tPos.x > gptIO->tMainViewportSize.x)
            ptWindow->tPos.x = gptIO->tMainViewportSize.x - ptWindow->tSize.x / 2.0f;

        if(ptWindow->tPos.y > gptIO->tMainViewportSize.y)
        {
            ptWindow->tPos.y = gptIO->tMainViewportSize.y - ptWindow->tSize.y / 2.0f;
            ptWindow->tPos.y = pl_maxf(ptWindow->tPos.y, 0.0f);
        }
    }

    // updating outer rect here but autosized windows do so again in pl_end_window(..)
    ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
    ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
    ptWindow->tInnerRect = ptWindow->tOuterRect;

    ptWindow->tScrollMax = pl_sub_vec2(ptWindow->tContentSize, (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});
    
    // clamp scrolling max
    ptWindow->tScrollMax = pl_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = (ptWindow->tScrollMax.x > 0.0f) && (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR);
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_SCROLLBAR)
    {
        ptWindow->bScrollbarX = false;
        ptWindow->bScrollbarY = false;
    }

    if(ptWindow->bScrollbarX && ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }
    else if(!ptWindow->bScrollbarY)
        ptWindow->tScroll.y = 0;
    else if(!ptWindow->bScrollbarX)
        ptWindow->tScroll.x = 0;

    // remove scrollbars from inner rect
    if(ptWindow->bScrollbarY)
        ptWindow->tInnerRect.tMax.x -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    if(ptWindow->bScrollbarX)
        ptWindow->tInnerRect.tMax.y -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    // decorations
    if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR)) // has title bar
    {

        ptWindow->tInnerRect.tMin.y += fTitleBarHeight;

        const bool bHighlightWindow = (gptCtx->ptNavWindow && ptWindow->ptRootWindowTitleBarHighlight == gptCtx->ptNavWindow->ptRootWindowTitleBarHighlight);

        // draw title bar
        plVec4 tTitleColor;
        if(bHighlightWindow)
            tTitleColor = gptCtx->tColorScheme.tTitleActiveCol;
        else if(ptWindow->bCollapsed)
            tTitleColor = gptCtx->tColorScheme.tTitleBgCollapsedCol;
        else
            tTitleColor = gptCtx->tColorScheme.tTitleBgCol;
        gptDraw->add_rect_rounded_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x, fTitleBarHeight}), gptCtx->tStyle.fWindowRounding, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tTitleColor)});

        // draw title text
        const plVec2 titlePos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
        pl__add_text(ptWindow->ptFgLayer, gptCtx->tFont, gptCtx->tStyle.fFontSize, titlePos, PL_COLOR_32_VEC4(gptCtx->tColorScheme.tTextCol), pcName, 0.0f);

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
            bool bPressed = pl__button_behavior(&tBoundingBox, uCloseHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uCloseHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 12, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 1.0f)});
            else if(gptCtx->uHoveredId == uCloseHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 12, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 1.0f)});
            else                                      gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 12, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 1.0f)});

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
            bool bPressed = pl__button_behavior(&tBoundingBox, uCollapseHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uCollapseHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 12, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 1.0f)});
            else if(gptCtx->uHoveredId == uCollapseHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 12, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 1.0f)});
            else                                         gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 12, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.5f, 0.0f, 1.0f)});

            if(bPressed)
            {
                ptWindow->bCollapsed = !ptWindow->bCollapsed;
                if(!ptWindow->bCollapsed)
                {
                    ptWindow->tSize = ptWindow->tFullSize;
                    if(tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE)
                        ptWindow->bAppearing = true;
                }
            }
        }

    }
    else
        fTitleBarHeight = 0.0f;

    if(ptWindow->bAppearing)
        ptWindow->uHideFrames = 1;

    // remove padding for inner clip rect
    ptWindow->tInnerClipRect = pl_rect_expand_vec2(&ptWindow->tInnerRect, (plVec2){-gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});

    // update layout cursors
    ptWindow->tTempData.tCursorStartPos.x = gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - ptWindow->tScroll.x;
    ptWindow->tTempData.tCursorStartPos.y = gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - ptWindow->tScroll.y;
    ptWindow->tTempData.tRowCursorPos = ptWindow->tTempData.tCursorStartPos;
    ptWindow->tTempData.tCursorStartPos = pl_floor_vec2(ptWindow->tTempData.tCursorStartPos);
    ptWindow->tTempData.fTitleBarHeight = fTitleBarHeight;

    // reset next window flags
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;

    ptWindow->bVisible = true;

    const bool bScrollBarsPresent = ptWindow->bScrollbarX || ptWindow->bScrollbarY;

    uint32_t uBackgroundColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tWindowBgColor);
    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        uBackgroundColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tChildBgColor);
    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW)
        uBackgroundColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tPopupBgColor);


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
        
        // draw background
        gptDraw->add_rect_rounded_filled(
            ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax,
            gptCtx->tStyle.fWindowRounding, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM,
            (plDrawSolidOptions){.uColor = uBackgroundColor});

        ptWindow->tFullSize = ptWindow->tSize;
    }

    // regular window non collapsed
    else if(!ptWindow->bCollapsed)
    {
        plRect tBgRect = pl_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        {
            plRect tParentBgRect = ptParentWindow->tOuterRect;
            tBgRect = pl_rect_clip(&tBgRect, &tParentBgRect);
        }

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

        // draw border
        if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_BACKGROUND))
        {
            if(gptCtx->tStyle.iWindowBorderSize != 0)
            {
                gptDraw->add_rect_rounded(
                    ptWindow->ptFgLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax,
                    gptCtx->tStyle.fWindowRounding, 0, 0,
                    (plDrawLineOptions){.fThickness = (float)gptCtx->tStyle.iWindowBorderSize, .uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tWindowBorderColor)});
            }
            gptDraw->add_rect_rounded_filled(
                ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tStyle.fWindowRounding,
                0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM,
                (plDrawSolidOptions){.uColor = uBackgroundColor});
        }

        // vertical scroll bar
        if(ptWindow->bScrollbarY)
            pl__render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

        if(ptWindow->bScrollbarX)
            pl__render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

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
                const bool bPressed = pl__button_behavior(&tBoundingBox, uResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uResizeHash)
                {
                    gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tResizeGripActiveCol)});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
                }
                else if(gptCtx->uHoveredId == uResizeHash)
                {
                    gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tResizeGripHoveredCol)});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
                }
                else
                {
                    gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tResizeGripCol)});
                }
            }

            // east border
            {

                plRect tBoundingBox = pl_calculate_rect(tTopRight, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, -gptCtx->tStyle.fWindowRounding});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl__button_behavior(&tBoundingBox, uEastResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uEastResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tTopRight.x, tTopRight.y + gptCtx->tStyle.fWindowRounding}, (plVec2){tBottomRight.x, tBottomRight.y - gptCtx->tStyle.fWindowRounding}, (plDrawLineOptions){.fThickness = 2.0f, .uColor = PL_COLOR_32_RGB(0.99f, 0.02f, 0.10f)});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
                else if(gptCtx->uHoveredId == uEastResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tTopRight.x, tTopRight.y + gptCtx->tStyle.fWindowRounding}, (plVec2){tBottomRight.x, tBottomRight.y - gptCtx->tStyle.fWindowRounding}, (plDrawLineOptions){.fThickness = 2.0f, .uColor = PL_COLOR_32_RGB(0.66f, 0.02f, 0.10f)});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
            }

            // west border
            {
                plRect tBoundingBox = pl_calculate_rect(tTopLeft, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, -gptCtx->tStyle.fWindowRounding});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl__button_behavior(&tBoundingBox, uWestResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uWestResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tTopLeft.x, tTopLeft.y + gptCtx->tStyle.fWindowRounding}, (plVec2){tBottomLeft.x, tBottomLeft.y - gptCtx->tStyle.fWindowRounding}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.99f, 0.02f, 0.10f), .fThickness = 2.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
                else if(gptCtx->uHoveredId == uWestResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tTopLeft.x, tTopLeft.y + gptCtx->tStyle.fWindowRounding}, (plVec2){tBottomLeft.x, tBottomLeft.y - gptCtx->tStyle.fWindowRounding}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.66f, 0.02f, 0.10f), .fThickness = 2.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
                }
            }

            // north border
            {
                plRect tBoundingBox = {tTopLeft, (plVec2){tTopRight.x - 15.0f, tTopRight.y}};
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){-gptCtx->tStyle.fWindowRounding, fHoverPadding / 2.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl__button_behavior(&tBoundingBox, uNorthResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uNorthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tTopLeft.x + gptCtx->tStyle.fWindowRounding, tTopLeft.y}, (plVec2){tTopRight.x - gptCtx->tStyle.fWindowRounding, tTopRight.y}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.99f, 0.02f, 0.10f), .fThickness = 2.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
                else if(gptCtx->uHoveredId == uNorthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tTopLeft.x + gptCtx->tStyle.fWindowRounding, tTopLeft.y}, (plVec2){tTopRight.x - gptCtx->tStyle.fWindowRounding, tTopRight.y}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.66f, 0.02f, 0.10f), .fThickness = 2.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
            }

            // south border
            {
                plRect tBoundingBox = {tBottomLeft, (plVec2){tBottomRight.x - 15.0f, tBottomRight.y}};
                tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){-gptCtx->tStyle.fWindowRounding, fHoverPadding / 2.0f});

                bool bHovered = false;
                bool bHeld = false;
                const bool bPressed = pl__button_behavior(&tBoundingBox, uSouthResizeHash, &bHovered, &bHeld);

                if(gptCtx->uActiveId == uSouthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tBottomLeft.x + gptCtx->tStyle.fWindowRounding, tBottomLeft.y}, (plVec2){tBottomRight.x - gptCtx->tStyle.fWindowRounding, tBottomRight.y}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.99f, 0.02f, 0.10f), .fThickness = 2.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
                else if(gptCtx->uHoveredId == uSouthResizeHash)
                {
                    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){tBottomLeft.x + gptCtx->tStyle.fWindowRounding, tBottomLeft.y}, (plVec2){tBottomRight.x - gptCtx->tStyle.fWindowRounding, tBottomRight.y}, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.66f, 0.02f, 0.10f), .fThickness = 2.0f});
                    gptIOI->set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
                }
            }
        }

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
                
                if(tMousePos.y < ptWindow->tPos.y)
                {
                    ptWindow->tScroll.y = 0.0f;
                    gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
                else if(tMousePos.y > ptWindow->tPos.y + ptWindow->tSize.y)
                {
                    ptWindow->tScroll.y = ptWindow->tScrollMax.y;
                    gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
                else
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

                if(tMousePos.x < ptWindow->tPos.x)
                {
                    ptWindow->tScroll.x = 0.0f;
                    gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
                else if(tMousePos.x > ptWindow->tPos.x + ptWindow->tSize.x)
                {
                    ptWindow->tScroll.x = ptWindow->tScrollMax.x;
                    gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
                else
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

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        ptWindow->bVisible = pl_rect_overlaps_rect(&ptWindow->tInnerClipRect, &ptParentWindow->tInnerClipRect);
        return ptWindow->bVisible && !pl_rect_is_inverted(&ptWindow->tInnerClipRect);
    }
    return !ptWindow->bCollapsed;
}

static void
pl__render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis)
{
    const plRect tParentBgRect = ptWindow->ptParentWindow ? ptWindow->ptParentWindow->tOuterRect : ptWindow->tOuterRect;
    if(tAxis == PL_UI_AXIS_X)
    {
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);

        const float fScrollbarHandleSize  = pl_maxf(12.0f, floorf((ptWindow->tSize.x - fRightSidePadding) * ((ptWindow->tSize.x - fRightSidePadding) / (ptWindow->tContentSize.x))));
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

            gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarBgCol)});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);   
            if(gptCtx->uActiveId == uHash)
                gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarActiveCol)});
            else if(gptCtx->uHoveredId == uHash)
                gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarHoveredCol)});
            else
                gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarHandleCol)});
        }
    }
    else if(tAxis == PL_UI_AXIS_Y)
    {
          
        const float fBottomPadding = ptWindow->bScrollbarX && (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR) ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fTopPadding = (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);

        const float fScrollbarHandleSize  = pl_maxf(12.0f, floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding) * ((ptWindow->tSize.y - fTopPadding - fBottomPadding) / (ptWindow->tContentSize.y))));
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
            gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarBgCol)});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl__button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);

            // scrollbar handle
            if(gptCtx->uActiveId == uHash) 
                gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarActiveCol)});
            else if(gptCtx->uHoveredId == uHash) 
                gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarHoveredCol)});
            else
                gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tStyle.fScrollbarRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tScrollbarHandleCol)});

        }
    }

    if(gptCtx->uActiveId == uHash && gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        pl__set_active_id(uHash, ptWindow);
}

static plVec2
pl__calculate_item_size(float fDefaultHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;

    float fHeight = ptCurrentRow->fWidgetHeight;
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
            ptCurrentRow->fWidgetWidth = ptCurrentRow->pfSizesOrRatios[ptCurrentRow->uCurrentColumn];

        float fWidth = ptCurrentRow->fWidgetWidth;

        if(ptCurrentRow->tType ==  PL_UI_LAYOUT_ROW_TYPE_DYNAMIC) // width was a ratio
        {
            float fAvailableSize = 0.0f;
            if(pl_sb_size(ptWindow->sbfAvailableSizeStack) > 0)
            {
                fAvailableSize = pl_sb_top(ptWindow->sbfAvailableSizeStack);
            }
            else
            {
                fAvailableSize = ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;
                if(ptWindow->bScrollbarY)
                    fAvailableSize -= (2.0f + gptCtx->tStyle.fScrollbarSize);
            }

            float fTotalWidthAvailable = fAvailableSize
                - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1)                // remove spacing between widgets
                - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize; // remove indent

            fWidth *= fTotalWidthAvailable;
        }

        const plVec2 tWidgetSize = { fWidth, fHeight};
        return tWidgetSize;
    }
}

static void
pl__advance_cursor(plVec2 tOffset)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tRowCursorPos = pl_add_vec2(ptWindow->tTempData.tRowCursorPos, tOffset);
}

static void
pl__set_cursor(plVec2 tPos)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tRowCursorPos = tPos;
}

static void
pl__smart_advance_cursor(float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;

    ptCurrentRow->uCurrentColumn++;
    
    ptCurrentRow->fMaxWidth = pl_maxf(ptCurrentRow->fWidgetXOffset + fWidth, ptCurrentRow->fMaxWidth);
    ptCurrentRow->fMaxHeight = pl_maxf(ptCurrentRow->fMaxHeight, ptCurrentRow->fWidgetYOffset + fHeight);

    // not yet at end of row
    if(ptCurrentRow->uCurrentColumn < ptCurrentRow->uColumns)
        ptCurrentRow->fWidgetXOffset += fWidth + gptCtx->tStyle.tItemSpacing.x;

    // automatic wrap
    if(ptCurrentRow->uCurrentColumn == ptCurrentRow->uColumns && ptCurrentRow->tSystemType != PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        ptWindow->tTempData.tRowCursorPos.y = ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x);
        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);   

        // reset
        ptCurrentRow->uCurrentColumn = 0;
        ptCurrentRow->fMaxWidth = 0.0f;
        ptCurrentRow->fMaxHeight = 0.0f;
        ptCurrentRow->fWidgetXOffset = ptWindow->tTempData.fExtraIndent;
        ptCurrentRow->fWidgetYOffset = 0.0f;
    }

    if(pl_sb_size(ptWindow->sbfMaxCursorYStack) > 0)
    {
        pl_sb_top(ptWindow->sbfMaxCursorYStack) = pl_maxf(pl_sb_top(ptWindow->sbfMaxCursorYStack), ptWindow->tTempData.tRowCursorPos.y);
    }

    // passed end of row
    if(ptCurrentRow->uCurrentColumn > ptCurrentRow->uColumns && ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        PL_ASSERT(false);
    }
}

static void
pl__submit_window(plUiWindow* ptWindow)
{
    ptWindow->bActive = false; // no longer active (for next frame)
    pl_sb_push(gptCtx->sbptWindows, ptWindow);
    for(uint32_t j = 0; j < pl_sb_size(ptWindow->sbtChildWindows); j++)
        pl__submit_window(ptWindow->sbtChildWindows[j]);
}

static void
pl__set_active_id(uint32_t uHash, plUiWindow* ptWindow)
{
    gptCtx->bActiveIdJustActivated = gptCtx->uActiveId != uHash;
    gptCtx->uActiveId = uHash;
    gptCtx->ptActiveWindow = ptWindow;
    if(uHash)
        gptCtx->uActiveIdIsAlive = uHash;
}

static void
pl__set_nav_id(uint32_t uHash, plUiWindow* ptWindow)
{
    gptCtx->uNavId = uHash;
    
    if(uHash)
        gptCtx->bNavIdIsAlive = true;

    if(gptCtx->ptNavWindow != ptWindow)
    {
        pl_sb_reset(gptCtx->sbtOpenPopupStack);
    }

    gptCtx->ptNavWindow = ptWindow;
}

static void
pl__add_widget(uint32_t uHash)
{
    if(uHash != 0)
    {
        if(gptCtx->uNavId == uHash)
        {
            if(gptCtx->ptCurrentWindow == gptCtx->ptNavWindow)
            {
                gptCtx->bNavIdIsAlive = true;
            }
        }
    }
}

void
pl_ui_initialize(void)
{
    gptCtx->ptDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->ptDebugDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->ptBgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
    gptCtx->ptFgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
    gptCtx->ptDebugLayer = gptDraw->request_2d_layer(gptCtx->ptDebugDrawlist);
    gptCtx->tTooltipWindow.ptBgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
    gptCtx->tTooltipWindow.ptFgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist);

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
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbfAvailableSizeStack);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbfMaxCursorYStack);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbtCursorStack);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbtRowTemplateEntries);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbtTempLayoutSort);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->sbuTempLayoutIndexSort);
            pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->tStorage.sbtData);
            PL_FREE(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]->pcName);
            PL_FREE(gptCtx->sbptFocusedWindows[i]->sbtChildWindows[j]);
        }
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->tStorage.sbtData);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtChildWindows);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbfAvailableSizeStack);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbfMaxCursorYStack);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtRowStack);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtCursorStack);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtRowTemplateEntries);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbtTempLayoutSort);
        pl_sb_free(gptCtx->sbptFocusedWindows[i]->sbuTempLayoutIndexSort);
        PL_FREE(gptCtx->sbptFocusedWindows[i]->pcName);
        PL_FREE(gptCtx->sbptFocusedWindows[i]);
    }
    pl_sb_free(gptCtx->sbptWindowStack);
    pl_sb_free(gptCtx->sbcTempBuffer);
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbDrawlists);
    pl_sb_free(gptCtx->sbptFocusedWindows);
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbtColorStack);
    pl_sb_free(gptCtx->sbtTabBars);
    pl_sb_free(gptCtx->sbuIdStack);
    pl_sb_free(gptCtx->sbtBeginPopupStack);
    pl_sb_free(gptCtx->sbtOpenPopupStack);
    pl_sb_free(gptCtx->tWindows.sbtData);
}

//-----------------------------------------------------------------------------
// [SECTION] helper windows
//-----------------------------------------------------------------------------

#include "pl_ui_widgets.c"


void
pl_show_debug_window(bool* pbOpen)
{
    // tools
    static bool bShowWindowOuterRect = false;
    static bool bShowWindowOuterClippedRect = false;
    static bool bShowWindowInnerRect = false;
    static bool bShowWindowInnerClipRect = false;

    if(pl_begin_window("Pilot Light UI Metrics/Debugger", pbOpen, 0))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        pl_separator();

        pl_checkbox("Show Window Inner Rect", &bShowWindowInnerRect);
        pl_checkbox("Show Window Inner Clip Rect", &bShowWindowInnerClipRect);
        pl_checkbox("Show Window Outer Rect", &bShowWindowOuterRect);
        pl_checkbox("Show Window Outer Rect Clipped", &bShowWindowOuterClippedRect);

        for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptFocusedWindows); uWindowIndex++)
        {
            const plUiWindow* ptWindow = gptCtx->sbptFocusedWindows[uWindowIndex];

            if(ptWindow->bActive)
            {
                if(bShowWindowInnerRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerRect.tMin, ptWindow->tInnerRect.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 1.0f, 0.0f), .fThickness = 1.0f});

                if(bShowWindowInnerClipRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerClipRect.tMin, ptWindow->tInnerClipRect.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 1.0f, 0.0f), .fThickness = 1.0f});

                if(bShowWindowOuterRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 1.0f, 0.0f), .fThickness = 1.0f});

                if(bShowWindowOuterClippedRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRectClipped.tMin, ptWindow->tOuterRectClipped.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 1.0f, 0.0f), .fThickness = 1.0f});
            }
        }
        
        pl_separator();

        if(pl_tree_node("Windows", 0))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptFocusedWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = gptCtx->sbptFocusedWindows[uWindowIndex];

                if(pl_tree_node(ptWindow->pcName, 0))
                {
                    pl_text(" - Pos:          (%0.1f, %0.1f)", ptWindow->tPos.x, ptWindow->tPos.y);
                    pl_text(" - Size:         (%0.1f, %0.1f)", ptWindow->tSize.x, ptWindow->tSize.y);
                    pl_text(" - Content Size: (%0.1f, %0.1f)", ptWindow->tContentSize.x, ptWindow->tContentSize.y);
                    pl_text(" - Min Size:     (%0.1f, %0.1f)", ptWindow->tMinSize.x, ptWindow->tMinSize.y);
                    pl_text(" - Scroll:       (%0.1f/%0.1f, %0.1f/%0.1f)", ptWindow->tScroll.x, ptWindow->tScrollMax.x, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_text(" - Focused:      %s", ptWindow == gptCtx->ptNavWindow ? "1" : "0");
                    pl_text(" - Active:       %s", ptWindow == gptCtx->ptActiveWindow ? "1" : "0");
                    pl_text(" - Hovered:      %s", ptWindow == gptCtx->ptHoveredWindow ? "1" : "0");
                    pl_text(" - Dragging:     %s", ptWindow == gptCtx->ptMovingWindow ? "1" : "0");
                    pl_text(" - Scrolling:    %s", ptWindow == gptCtx->ptWheelingWindow ? "1" : "0");
                    pl_text(" - Resizing:     %s", ptWindow == gptCtx->ptSizingWindow ? "1" : "0");
                    pl_text(" - Collapsed:    %s", ptWindow->bCollapsed ? "1" : "0");
                    pl_text(" - Auto Sized:   %s", ptWindow->tFlags &  PL_UI_WINDOW_FLAGS_AUTO_SIZE ? "1" : "0");

                    pl_tree_pop();
                }  
            }
            pl_tree_pop();
        }
        if(pl_tree_node("Internal State", 0))
        {
            pl_separator_text("Windowing");
            pl_indent(0.0f);
            
            pl_text("Hovered Window: %s", gptCtx->ptHoveredWindow ? gptCtx->ptHoveredWindow->pcName : "NULL");
            pl_text("Moving Window:  %s", gptCtx->ptMovingWindow ? gptCtx->ptMovingWindow->pcName : "NULL");
            pl_text("Sizing Window:  %s", gptCtx->ptSizingWindow ? gptCtx->ptSizingWindow->pcName : "NULL");
            pl_text("Scrolling Window:  %s", gptCtx->ptScrollingWindow ? gptCtx->ptScrollingWindow->pcName : "NULL");
            pl_text("Wheeling Window:  %s", gptCtx->ptWheelingWindow ? gptCtx->ptWheelingWindow->pcName : "NULL");
            pl_unindent(0.0f);
            
            pl_separator_text("Items");
            pl_indent(0.0f);
            pl_text("Active Window: %s", gptCtx->ptActiveWindow ? gptCtx->ptActiveWindow->pcName : "NULL");
            pl_text("Active ID:        %u", gptCtx->uActiveId);
            pl_text("Hovered ID:       %u", gptCtx->uHoveredId);
            pl_unindent(0.0f);

            pl_separator_text("Navigation");
            pl_indent(0.0f);
            pl_text("Nav Window: %s", gptCtx->ptNavWindow ? gptCtx->ptNavWindow->pcName : "NULL"); 
            pl_text("Nav ID:     %u", gptCtx->uNavId);
            pl_unindent(0.0f);
            pl_tree_pop();
        }
        pl_end_window();
    } 
}

void
pl_show_style_editor_window(bool* pbOpen)
{

    if(pl_begin_window("Pilot Light UI Style", pbOpen, 0))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        plUiStyle* ptStyle = &gptCtx->tStyle;

        if(pl_begin_tab_bar("Tabs", 0))
        {
            if(pl_begin_tab("Colors", 0))
            { 
                pl_end_tab();
            }
            
            if(pl_begin_tab("Sizes", 0))
            {
                pl_vertical_spacing();
                pl_separator_text("Title");
                pl_slider_float("Title Padding", &ptStyle->fTitlePadding, 0.0f, 32.0f, 0);

                pl_vertical_spacing();
                pl_separator_text("Window");
                pl_slider_float("Horizontal Padding## window", &ptStyle->fWindowHorizontalPadding, 0.0f, 32.0f, 0);
                pl_slider_float("Vertical Padding## window", &ptStyle->fWindowVerticalPadding, 0.0f, 32.0f, 0);
                pl_slider_int("Border Size## window", &ptStyle->iWindowBorderSize, 0, 1, 0);

                pl_vertical_spacing();
                pl_separator_text("Scrollbar");
                pl_slider_float("Size##scrollbar", &ptStyle->fScrollbarSize, 0.0f, 32.0f, 0);

                pl_vertical_spacing();
                pl_separator_text("Rounding");
                pl_slider_float("Window Rounding", &ptStyle->fWindowRounding, 0.0f, 12.0f, 0);
                pl_slider_float("Child Rounding", &ptStyle->fChildRounding, 0.0f, 12.0f, 0);
                pl_slider_float("Frame Rounding", &ptStyle->fFrameRounding, 0.0f, 12.0f, 0);
                pl_slider_float("Scrollbar Rounding", &ptStyle->fScrollbarRounding, 0.0f, 12.0f, 0);
                pl_slider_float("Grab Rounding", &ptStyle->fGrabRounding, 0.0f, 12.0f, 0);
                pl_slider_float("Tab Rounding", &ptStyle->fTabRounding, 0.0f, 12.0f, 0);
                
                pl_vertical_spacing();
                pl_separator_text("Misc");
                pl_slider_float("Indent", &ptStyle->fIndentSize, 0.0f, 32.0f, 0); 
                pl_slider_float("Slider Size", &ptStyle->fSliderSize, 3.0f, 32.0f, 0); 
                pl_slider_float("Font Size", &ptStyle->fFontSize, 13.0f, 48.0f, 0); 

                pl_vertical_spacing();
                pl_separator_text("Widgets");
                pl_slider_float("Separator Text Size", &ptStyle->fSeparatorTextLineSize, 0.0f, 10.0f, 0); 
                pl_slider_float("Separator Text Alignment x", &ptStyle->tSeparatorTextAlignment.x, 0.0f, 1.0f, 0); 
                pl_slider_float("Separator Text Alignment y", &ptStyle->tSeparatorTextAlignment.y, 0.0f, 1.0f, 0); 
                pl_slider_float("Separator Text Pad x", &ptStyle->tSeparatorTextPadding.x, 0.0f, 40.0f, 0); 
                pl_slider_float("Separator Text Pad y", &ptStyle->tSeparatorTextPadding.y, 0.0f, 40.0f, 0); 
                pl_end_tab();
            }
            pl_end_tab_bar();
        }     
        pl_end_window();
    }  
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ui_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plUiI tApi = {
        .initialize                    = pl_ui_initialize,
        .cleanup                       = pl_ui_cleanup,
        .get_draw_list                 = pl_get_draw_list,
        .get_debug_draw_list           = pl_get_debug_draw_list,
        .new_frame                     = pl_new_frame,
        .end_frame                     = pl_end_frame,
        .wants_keyboard_capture        = pl_wants_keyboard_capture,
        .wants_text_input              = pl_wants_text_input,
        .wants_mouse_capture           = pl_wants_mouse_capture,
        .show_debug_window             = pl_show_debug_window,
        .show_style_editor_window      = pl_show_style_editor_window,
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
        .is_popup_open                 = pl_is_popup_open,
        .close_current_popup           = pl_close_current_popup,
        .open_popup                    = pl_open_popup,
        .begin_popup                   = pl_begin_popup,
        .end_popup                     = pl_end_popup,
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
        .begin_combo                   = pl_begin_combo,
        .end_combo                     = pl_end_combo,
        .input_text                    = pl_input_text,
        .input_text_hint               = pl_input_text_hint,
        .input_float                   = pl_input_float,
        .input_float2                  = pl_input_float2,
        .input_float3                  = pl_input_float3,
        .input_float4                  = pl_input_float4,
        .input_int                     = pl_input_int,
        .input_int2                    = pl_input_int2,
        .input_int3                    = pl_input_int3,
        .input_int4                    = pl_input_int4,
        .input_uint                    = pl_input_uint,
        .input_uint2                   = pl_input_uint2,
        .input_uint3                   = pl_input_uint3,
        .input_uint4                   = pl_input_uint4,
        .slider_float                  = pl_slider_float,
        .slider_float_f                = pl_slider_float_f,
        .slider_int                    = pl_slider_int,
        .slider_int_f                  = pl_slider_int_f,
        .slider_uint                   = pl_slider_uint,
        .slider_uint_f                 = pl_slider_uint_f,
        .drag_float                    = pl_drag_float,
        .drag_float_f                  = pl_drag_float_f,
        .begin_collapsing_header       = pl_begin_collapsing_header,
        .end_collapsing_header         = pl_end_collapsing_header,
        .tree_node                     = pl_tree_node,
        .tree_node_f                   = pl_tree_node_f,
        .tree_node_v                   = pl_tree_node_v,
        .tree_pop                      = pl_tree_pop,
        .begin_menu                    = pl_begin_menu,
        .end_menu                      = pl_end_menu,
        .menu_item                     = pl_menu_item,
        .menu_item_toggle              = pl_menu_item_toggle,
        .begin_tab_bar                 = pl_begin_tab_bar,
        .end_tab_bar                   = pl_end_tab_bar,
        .begin_tab                     = pl_begin_tab,
        .end_tab                       = pl_end_tab,
        .separator                     = pl_separator,
        .separator_text                = pl_separator_text,
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
        .push_id_string                = pl_push_id_string,
        .push_id_pointer               = pl_push_id_pointer,
        .push_id_uint                  = pl_push_id_uint,
        .pop_id                        = pl_pop_id,
    };
    pl_set_api(ptApiRegistry, plUiI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptDraw = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptIOI = pl_get_api_latest(ptApiRegistry, plIOI);
    gptIO = gptIOI->get_io();

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptCtx = ptDataRegistry->get_data("plUiContext");
    }
    else // first load
    {
        static plUiContext tContext = {0};
        gptCtx = &tContext;
        ptDataRegistry->set_data("plUiContext", gptCtx);
    }
}

PL_EXPORT void
pl_unload_ui_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plUiI* ptApi = pl_get_api_latest(ptApiRegistry, plUiI);
    ptApiRegistry->remove_api(ptApi);

    gptCtx = NULL;
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif