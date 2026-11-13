/*
   pl_ui_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] docking
// [SECTION] settings
// [SECTION] public api implementations
// [SECTION] helper windows
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ui_ext.h"
#include "pl_ui_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] docking
//-----------------------------------------------------------------------------

static plUiDockNode*
pl__dock_node(uint32_t uId)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes); i++)
    {
        if(gptCtx->sbtDockNodes[i].uId == uId)
            return &gptCtx->sbtDockNodes[i];
    }
    return NULL;
}

static uint32_t
pl__dock_new_node(void)
{
    do
    {
        gptCtx->uNextDockId++;
    }
    while(!gptCtx->uNextDockId || pl__dock_node(gptCtx->uNextDockId));
    plUiDockNode tNode = {
        .uId = gptCtx->uNextDockId,
        .fRatio = 0.5f
    };
    pl_sb_push(gptCtx->sbtDockNodes, tNode);
    return tNode.uId;
}

static plUiDockNode*
pl__dock_root(uint32_t uId)
{
    plUiDockNode* ptNode = pl__dock_node(uId);
    while(ptNode && ptNode->uParent)
        ptNode = pl__dock_node(ptNode->uParent);
    return ptNode;
}

static plUiWindow*
pl__dock_host(plUiWindow* ptWindow)
{
    plUiDockNode* ptRoot = pl__dock_root(ptWindow->uDockId);
    if(ptRoot && ptRoot->bFloating)
        return pl__get_ptr(&gptCtx->tWindows, ptRoot->uHostWindow);
    return NULL;
}

static void
pl__draw_tab(plDrawLayer2D* ptLayer, plRect tBox, const char* pcLabel, plVec4 tColor)
{
    gptDraw->add_rect_rounded_filled(ptLayer, tBox.tMin, tBox.tMax, gptCtx->tStyle.fTabRounding, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tColor)});
    plRect tTextBox = gptDraw->calculate_text_bb(tBox.tMin, pcLabel, (plDrawTextOptions){.ptFont = gptCtx->tFont, .fSize = gptCtx->tStyle.fFontSize, .pcTextEnd = pl__find_renderered_text_end(pcLabel, NULL), .fWrap = -1.0f});
    const plVec2 tTextPos = pl_add_vec2(tBox.tMin, pl_sub_vec2(pl_rect_center(&tBox), pl_rect_center(&tTextBox)));
    pl__add_clipped_text(ptLayer, gptCtx->tFont, gptCtx->tStyle.fFontSize, tTextPos, tBox.tMin, tBox.tMax, PL_COLOR_32_VEC4(gptCtx->tColorScheme.tTextCol), pcLabel, -1.0f);
}

static void
pl__dock_layout(uint32_t uId, plRect tRect)
{
    plUiDockNode* ptNode = pl__dock_node(uId);
    ptNode->tRect = tRect;
    ptNode->ulLastFrame = gptIO->ulFrameCount;
    if(!ptNode->auChildren[0])
        return;
    plRect atRects[2] = {tRect, tRect};
    const int iAxis = ptNode->iAxis;
    const float fExtent = pl_maxf(0.0f, tRect.tMax.d[iAxis] - tRect.tMin.d[iAxis] - 4.0f);
    const float fSplit = tRect.tMin.d[iAxis] + fExtent * ptNode->fRatio;
    atRects[0].tMax.d[iAxis] = fSplit;
    atRects[1].tMin.d[iAxis] = pl_minf(tRect.tMax.d[iAxis], fSplit + 4.0f);
    pl__dock_layout(ptNode->auChildren[0], atRects[0]);
    pl__dock_layout(ptNode->auChildren[1], atRects[1]);
}

uint32_t
pl_ui_dock_split(uint32_t uNode, int iDirection, float fRatio, uint32_t* puOpposite)
{
    if(puOpposite)
        *puOpposite = 0;
    plUiDockNode* ptNode = pl__dock_node(uNode);
    if(!ptNode || ptNode->auChildren[0] || iDirection < 0 || iDirection > 3)
        return 0;
    const plRect tRect = ptNode->tRect;
    const uint32_t uSelected = ptNode->uSelectedWindow;
    const uint32_t auChildren[2] = {pl__dock_new_node(), pl__dock_new_node()};
    ptNode = pl__dock_node(uNode);
    ptNode->auChildren[0] = auChildren[0];
    ptNode->auChildren[1] = auChildren[1];
    const int iAt = (iDirection == 1 || iDirection == 3) ? 1 : 0;
    ptNode->iAxis = iDirection >= 2 ? 1 : 0;
    fRatio = pl_clampf(0.1f, fRatio, 0.9f);
    ptNode->fRatio = iAt ? 1.0f - fRatio : fRatio;
    ptNode->uSelectedWindow = 0;
    pl__dock_node(auChildren[0])->uParent = uNode;
    pl__dock_node(auChildren[1])->uParent = uNode;
    pl__dock_node(auChildren[1-iAt])->uSelectedWindow = uSelected;
    pl__dock_node(auChildren[1-iAt])->bHadWindows = ptNode->bHadWindows;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptCreatedWindows); i++)
        if(gptCtx->sbptCreatedWindows[i]->uDockId == uNode)
            gptCtx->sbptCreatedWindows[i]->uDockId = auChildren[1-iAt];
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtWindowSettings); i++)
    {
        if(gptCtx->sbtWindowSettings[i].uDockId == uNode)
            gptCtx->sbtWindowSettings[i].uDockId = auChildren[1-iAt];
    }
    pl__dock_layout(uNode, tRect);
    if(puOpposite)
        *puOpposite = auChildren[1-iAt];
    return auChildren[iAt];
}

static void
pl__dock_assign(plUiWindow* ptWindow, uint32_t uNode)
{
    if(uNode == ptWindow->uDockId)
        return;
    plUiDockNode* ptNode = pl__dock_node(uNode);
    if(uNode && (!ptNode || ptNode->auChildren[0]))
        return;
    plUiDockNode* ptOld = pl__dock_node(ptWindow->uDockId);
    if(ptOld && ptOld->uSelectedWindow == ptWindow->uId)
        ptOld->uSelectedWindow = 0;
    if(!ptWindow->uDockId)
    {
        ptWindow->tUndockedPos = ptWindow->tPos;
        ptWindow->tUndockedSize = ptWindow->bCollapsed ? ptWindow->tFullSize : ptWindow->tSize;
        ptWindow->bUndockedCollapsed = ptWindow->bCollapsed;
    }
    ptWindow->uDockId = uNode;
    ptWindow->bCollapsed = false;
    if(ptNode)
    {
        ptNode->uSelectedWindow = ptWindow->uId;
        ptNode->bHadWindows = true;
    }
    else
    {
        ptWindow->tPos = ptWindow->tUndockedPos;
        ptWindow->tSize = ptWindow->tUndockedSize;
        ptWindow->tFullSize = ptWindow->tUndockedSize;
        ptWindow->bCollapsed = ptWindow->bUndockedCollapsed;
    }
}

static void
pl__dock_prune_empty(void)
{
    // Defer compaction until all windows have been submitted. Inactive tabs are
    // still submitted; windows the application closes or hides are not.
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes);)
    {
        plUiDockNode* ptEmpty = &gptCtx->sbtDockNodes[i];
        if(ptEmpty->auChildren[0] || !ptEmpty->uParent || !ptEmpty->bHadWindows || pl__dock_root(ptEmpty->uId)->ulLastFrame != gptIO->ulFrameCount)
        {
            i++;
            continue;
        }
        bool bOccupied = false;
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptCreatedWindows); j++)
        {
            plUiWindow* ptMember = gptCtx->sbptCreatedWindows[j];
            if(ptMember->uDockId == ptEmpty->uId && ptMember->uLastFrameActive == gptIO->ulFrameCount)
                bOccupied = true;
        }
        if(bOccupied)
        {
            i++;
            continue;
        }
        const uint32_t uEmpty = ptEmpty->uId;
        plUiDockNode* ptParent = pl__dock_node(ptEmpty->uParent);
        const uint32_t uParent = ptParent->uId;
        const uint32_t uSibling = ptParent->auChildren[ptParent->auChildren[0] == uEmpty ? 1 : 0];
        const plUiDockNode tSibling = *pl__dock_node(uSibling);
        ptParent->auChildren[0] = tSibling.auChildren[0];
        ptParent->auChildren[1] = tSibling.auChildren[1];
        ptParent->uSelectedWindow = tSibling.uSelectedWindow;
        ptParent->fRatio = tSibling.fRatio;
        ptParent->iAxis = tSibling.iAxis;
        ptParent->bHadWindows = true;
        if(ptParent->auChildren[0])
        {
            pl__dock_node(ptParent->auChildren[0])->uParent = uParent;
            pl__dock_node(ptParent->auChildren[1])->uParent = uParent;
        }
        uint32_t uLeaf = uParent;
        while(pl__dock_node(uLeaf)->auChildren[0])
            uLeaf = pl__dock_node(uLeaf)->auChildren[0];
        // Reopened windows join a surviving pane; saved references never dangle.
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptCreatedWindows); j++)
        {
            plUiWindow* ptMember = gptCtx->sbptCreatedWindows[j];
            if(ptMember->uDockId == uEmpty || ptMember->uDockId == uSibling)
                ptMember->uDockId = uLeaf;
        }
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbtWindowSettings); j++)
        {
            plUiWindowSettings* ptSettings = &gptCtx->sbtWindowSettings[j];
            if(ptSettings->uDockId == uEmpty || ptSettings->uDockId == uSibling)
                ptSettings->uDockId = uLeaf;
        }
        if(gptCtx->uDockSplitterActive == uParent || gptCtx->uDockSplitterActive == uSibling || gptCtx->uDockSplitterActive == uEmpty)
            gptCtx->uDockSplitterActive = 0;
        const plRect tRect = ptParent->tRect;
        for(uint32_t j = pl_sb_size(gptCtx->sbtDockNodes); j > 0; j--)
        {
            const uint32_t uId = gptCtx->sbtDockNodes[j - 1].uId;
            if(uId == uEmpty || uId == uSibling)
                pl_sb_del(gptCtx->sbtDockNodes, j - 1);
        }
        pl__dock_layout(uParent, tRect);
        i = 0;
    }
}

static void
pl__dock_restore_single_windows(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes);)
    {
        plUiDockNode* ptRoot = &gptCtx->sbtDockNodes[i];
        if(!ptRoot->bFloating || ptRoot->uParent || ptRoot->auChildren[0] || ptRoot->ulLastFrame != gptIO->ulFrameCount)
        {
            i++;
            continue;
        }
        plUiWindow* ptRemaining = NULL;
        uint32_t uCount = 0;
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptCreatedWindows); j++)
        {
            plUiWindow* ptMember = gptCtx->sbptCreatedWindows[j];
            if(ptMember->uDockId == ptRoot->uId && ptMember->uLastFrameActive == gptIO->ulFrameCount)
            {
                ptRemaining = ptMember;
                uCount++;
            }
        }
        plUiWindow* ptHost = pl__get_ptr(&gptCtx->tWindows, ptRoot->uHostWindow);
        if(uCount != 1 || (ptHost && (gptCtx->ptMovingWindow == ptHost || gptCtx->ptSizingWindow == ptHost)))
        {
            i++;
            continue;
        }
        const plVec2 tPos = ptHost ? ptHost->tPos : ptRoot->tHostPos;
        const plVec2 tSize = ptHost ? ptHost->tSize : ptRoot->tHostSize;
        if(gptCtx->ptNavWindow && (gptCtx->ptNavWindow == ptHost || gptCtx->ptNavWindow->uDockId == ptRoot->uId))
            gptCtx->ptNavWindow = ptRemaining;
        if(gptCtx->ptTitleFocusWindow && (gptCtx->ptTitleFocusWindow == ptHost || gptCtx->ptTitleFocusWindow->uDockId == ptRoot->uId))
            gptCtx->ptTitleFocusWindow = ptRemaining;
        // Keep the group's stacking position without raising it over other windows.
        if(ptHost)
        {
            const uint32_t uOrder = ptRemaining->uFocusOrder;
            ptRemaining->uFocusOrder = ptHost->uFocusOrder;
            ptHost->uFocusOrder = uOrder;
            gptCtx->sbptFocusedWindows[ptRemaining->uFocusOrder] = ptRemaining;
            gptCtx->sbptFocusedWindows[ptHost->uFocusOrder] = ptHost;
            ptHost->bActive = false;
        }
        // Closed members recover their floating placement when reopened.
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptCreatedWindows); j++)
        {
            plUiWindow* ptMember = gptCtx->sbptCreatedWindows[j];
            if(ptMember->uDockId == ptRoot->uId)
                pl__dock_assign(ptMember, 0);
        }
        for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbtWindowSettings); j++)
        {
            if(gptCtx->sbtWindowSettings[j].uDockId == ptRoot->uId)
                gptCtx->sbtWindowSettings[j].uDockId = 0;
        }
        ptRemaining->tPos = ptRemaining->tUndockedPos = tPos;
        ptRemaining->tSize = ptRemaining->tFullSize = ptRemaining->tUndockedSize = tSize;
        ptRemaining->bCollapsed = ptRemaining->bUndockedCollapsed = false;
        pl_sb_del(gptCtx->sbtDockNodes, i);
    }
}

void
pl_ui_set_next_window_dock(uint32_t uNode, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.uDockId = uNode;
    gptCtx->tNextWindowData.tDockCondition = tCondition ? tCondition : PL_UI_COND_ALWAYS;
    gptCtx->tNextWindowData.bHasDock = true;
}

uint32_t
pl_ui_get_window_dock(void)
{
    return gptCtx->ptCurrentWindow ? gptCtx->ptCurrentWindow->uDockId : 0;
}

static void
pl__dock_space(uint32_t uId, plVec2 tPos, plVec2 tSize, plDrawLayer2D* ptLayer)
{
    tSize = pl_max_vec2(tSize, (plVec2){1,1});
    pl__dock_layout(uId, pl_calculate_rect(tPos, tSize));
    gptDraw->add_rect_filled(ptLayer, tPos, pl_add_vec2(tPos, tSize),
        (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tWindowBgColor)});
    // Splitter geometry is shared by layout, hit testing, and drawing.
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes); i++)
    {
        plUiDockNode* ptNode = &gptCtx->sbtDockNodes[i];
        uint32_t uRoot = ptNode->uId;
        while(pl__dock_node(uRoot)->uParent) uRoot = pl__dock_node(uRoot)->uParent;
        if(uRoot != uId || !ptNode->auChildren[0])
            continue;
        const int iAxis = ptNode->iAxis;
        plRect tDivider = ptNode->tRect;
        tDivider.tMin.d[iAxis] = pl__dock_node(ptNode->auChildren[0])->tRect.tMax.d[iAxis];
        tDivider.tMax.d[iAxis] = pl__dock_node(ptNode->auChildren[1])->tRect.tMin.d[iAxis];
        const bool bAvailable = !pl_sb_size(gptCtx->sbtOpenPopupStack) && !gptCtx->bPopupMouseDismissed &&
            !gptCtx->ptMovingWindow && (!gptCtx->ptHoveredWindow || pl__dock_root(gptCtx->ptHoveredWindow->uDockId) == pl__dock_node(uId) || gptCtx->ptHoveredWindow->uId == pl__dock_node(uId)->uHostWindow);
        const bool bHover = bAvailable && pl_rect_contains_point(&tDivider, gptIOI->get_mouse_pos());
        if(bHover && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            gptCtx->uDockSplitterActive = ptNode->uId;
        if(gptCtx->uDockSplitterActive == ptNode->uId)
        {
            gptCtx->bWantCaptureMouse = true;
            gptCtx->abMouseOwned[PL_MOUSE_BUTTON_LEFT] = true;
            if(gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
            {
                const float fExtent = pl_maxf(1.0f, ptNode->tRect.tMax.d[iAxis] - ptNode->tRect.tMin.d[iAxis] - 4.0f);
                const plVec2 tMouse = gptIOI->get_mouse_pos();
                ptNode->fRatio = pl_clampf(0.1f, (tMouse.d[iAxis] - ptNode->tRect.tMin.d[iAxis]) / fExtent, 0.9f);
                pl__dock_layout(uId, pl_calculate_rect(tPos, tSize));
            }
            else
                gptCtx->uDockSplitterActive = 0;
        }
        gptDraw->add_rect_filled(ptLayer, tDivider.tMin, tDivider.tMax,
            (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(bHover ? gptCtx->tColorScheme.tSeparatorCol : gptCtx->tColorScheme.tTitleBgCol)});
    }
}

uint32_t
pl_ui_dock_space(const char* pcName, plVec2 tPos, plVec2 tSize)
{
    const uint32_t uId = pl_str_hash(pcName, 0, pl_str_hash("PL_DOCKSPACE", 0, 0));
    if(!pl__dock_node(uId))
    {
        plUiDockNode tRoot = {.uId = uId, .fRatio = 0.5f};
        pl_sb_push(gptCtx->sbtDockNodes, tRoot);
    }
    pl__dock_space(uId, tPos, tSize, gptCtx->ptDockLayer);
    return uId;
}

static void
pl__dock_tabs(plUiWindow* ptWindow, bool* pbOpen, float fHeight)
{
    plUiDockNode* ptNode = pl__dock_node(ptWindow->uDockId);
    uint32_t uCount = 0;
    float fTotalWidth = 0.0f;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptCreatedWindows); i++)
    {
        plUiWindow* ptTab = gptCtx->sbptCreatedWindows[i];
        if(ptTab->uDockId == ptNode->uId && ptTab->uLastFrameActive + 1 >= gptIO->ulFrameCount)
        {
            uCount++;
            fTotalWidth += pl__calculate_text_size(gptCtx->tFont, gptCtx->tStyle.fFontSize, ptTab->pcName, -1.0f).x + 2.0f * gptCtx->tStyle.tFramePadding.x;
        }
    }
    const float fAvailable = pl_maxf(1.0f, ptWindow->tSize.x - (pbOpen ? pl_maxf(32.0f, fHeight) : 0.0f));
    const float fSpacing = pl_minf(gptCtx->tStyle.tInnerSpacing.x, fAvailable / pl_max(1u, uCount * 2));
    const float fScale = pl_minf(1.0f, pl_maxf(1.0f, fAvailable - fSpacing * (uCount ? uCount - 1 : 0)) / pl_maxf(1.0f, fTotalWidth));
    const float fTabHeight = pl__get_frame_height();
    float fX = ptWindow->tPos.x;
    const bool bFocused = pl__title_highlight_root(gptCtx->ptTitleFocusWindow) == ptWindow;
    gptDraw->add_rect_filled(ptWindow->ptFgLayer, ptWindow->tPos, pl_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x, fHeight}), (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(bFocused ? gptCtx->tColorScheme.tTitleActiveCol : gptCtx->tColorScheme.tTitleBgCol)});
    gptDraw->add_line(ptWindow->ptFgLayer, (plVec2){fX, ptWindow->tPos.y + fTabHeight}, (plVec2){fX + fAvailable, ptWindow->tPos.y + fTabHeight}, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tButtonActiveCol), .fThickness = 1.0f});
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptCreatedWindows); i++)
    {
        plUiWindow* ptTab = gptCtx->sbptCreatedWindows[i];
        if(ptTab->uDockId != ptNode->uId || ptTab->uLastFrameActive + 1 < gptIO->ulFrameCount)
            continue;
        const float fNaturalWidth = pl__calculate_text_size(gptCtx->tFont, gptCtx->tStyle.fFontSize, ptTab->pcName, -1.0f).x + 2.0f * gptCtx->tStyle.tFramePadding.x;
        const float fWidth = fNaturalWidth * fScale;
        plRect tBox = pl_calculate_rect((plVec2){fX, ptWindow->tPos.y}, (plVec2){fWidth, fTabHeight});
        const bool bHovered = pl__popup_allows_window(ptWindow) && gptCtx->ptHoveredWindow == ptWindow &&
            pl_rect_contains_point(&tBox, gptIOI->get_mouse_pos());
        if(bHovered && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            ptNode->uSelectedWindow = ptTab->uId;
            gptCtx->ptDockDragWindow = ptTab;
            pl__focus_window(ptTab);
        }
        const plVec4 tColor = gptCtx->ptDockDragWindow == ptTab ? gptCtx->tColorScheme.tTabSelectedCol : bHovered ? gptCtx->tColorScheme.tTabHoveredCol :
            (ptNode->uSelectedWindow == ptTab->uId ? gptCtx->tColorScheme.tTabSelectedCol : gptCtx->tColorScheme.tTabCol);
        pl__draw_tab(ptWindow->ptFgLayer, tBox, ptTab->pcName, tColor);
        fX += fWidth + fSpacing;
    }
    ptWindow->tTempData.fDockTabsEndX = fX;
}

static void
pl__dock_target_rect(plRect tPane, int iDirection, plRect* ptTarget)
{
    const plVec2 tPaneSize = pl_sub_vec2(tPane.tMax, tPane.tMin);
    const float fSize = pl_maxf(1.0f, pl_minf(gptCtx->tStyle.fFontSize * 2.2f, pl_minf(tPaneSize.x, tPaneSize.y) / 4.0f));
    plVec2 tCenter = pl_rect_center(&tPane);
    if(iDirection >= 0)
        tCenter.d[iDirection >= 2 ? 1 : 0] += (iDirection == 0 || iDirection == 2 ? -1.0f : 1.0f) * fSize * 1.2f;
    *ptTarget = pl_calculate_rect(pl_sub_vec2(tCenter, (plVec2){fSize * 0.5f, fSize * 0.5f}), (plVec2){fSize, fSize});
}

static int
pl__dock_target_direction(plRect tPane, plVec2 tMouse)
{
    for(int i = -1; i < 4; i++)
    {
        plRect tTarget;
        pl__dock_target_rect(tPane, i, &tTarget);
        if(pl_rect_contains_point(&tTarget, tMouse))
            return i;
    }
    return -2; // no target; -1 is the center/tab target
}

static void
pl__dock_draw_reticle(plRect tPane, int iSelected)
{
    const plVec2 tCenter = pl_rect_center(&tPane);
    // Draw the compass connectors behind all five targets.
    for(int i = 0; i < 4; i++)
    {
        plRect tBox;
        pl__dock_target_rect(tPane, i, &tBox);
        gptDraw->add_line(gptCtx->ptDebugLayer, tCenter, pl_rect_center(&tBox), (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tTabSelectedCol), .fThickness = 2.0f});
    }
    for(int i = -1; i < 4; i++)
    {
        plRect tBox;
        pl__dock_target_rect(tPane, i, &tBox);
        const float fSize = pl_rect_width(&tBox);
        const plVec2 tSlotCenter = pl_rect_center(&tBox);
        plVec4 tFill = i == iSelected ? gptCtx->tColorScheme.tHeaderActiveCol : gptCtx->tColorScheme.tTitleBgCol;
        tFill.a = 0.95f;
        const uint32_t uOutline = PL_COLOR_32_VEC4(i == iSelected ? gptCtx->tColorScheme.tTextCol : gptCtx->tColorScheme.tTabSelectedCol);
        // Compass arms and a diamond center distinguish PL's target from tabs.
        gptDraw->add_rect_rounded_filled(gptCtx->ptDebugLayer, tBox.tMin, tBox.tMax, fSize * 0.18f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tFill)});
        gptDraw->add_rect_rounded(gptCtx->ptDebugLayer, tBox.tMin, tBox.tMax, fSize * 0.18f, 0, 0, (plDrawLineOptions){.uColor = uOutline, .fThickness = i == iSelected ? 2.0f : 1.0f});
        const plDrawSolidOptions tGlyph = {.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tTextCol)};
        if(i == -1)
        {
            const float fRadius = fSize * 0.23f;
            plVec2 tTop = {tSlotCenter.x, tSlotCenter.y - fRadius};
            plVec2 tBottom = {tSlotCenter.x, tSlotCenter.y + fRadius};
            gptDraw->add_triangle_filled(gptCtx->ptDebugLayer, tTop, (plVec2){tSlotCenter.x + fRadius, tSlotCenter.y}, tBottom, tGlyph);
            gptDraw->add_triangle_filled(gptCtx->ptDebugLayer, tTop, tBottom, (plVec2){tSlotCenter.x - fRadius, tSlotCenter.y}, tGlyph);
        }
        else
        {
            plRect tGlyphBox = pl_rect_expand_vec2(&tBox, (plVec2){-fSize * 0.25f, -fSize * 0.25f});
            const int iAxis = i >= 2 ? 1 : 0;
            if(i == 0 || i == 2)
                tGlyphBox.tMax.d[iAxis] = tSlotCenter.d[iAxis];
            else
                tGlyphBox.tMin.d[iAxis] = tSlotCenter.d[iAxis];
            gptDraw->add_rect_filled(gptCtx->ptDebugLayer, tGlyphBox.tMin, tGlyphBox.tMax, tGlyph);
        }
    }
}

static void
pl__dock_drag(void)
{
    if(gptCtx->ptDockDragWindow && gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 8.0f))
    {
        plUiWindow* ptWindow = gptCtx->ptDockDragWindow;
        pl__dock_assign(ptWindow, 0);
        ptWindow->tPos = pl_sub_vec2(gptIOI->get_mouse_pos(), (plVec2){ptWindow->tSize.x * 0.5f, 10.0f});
        ptWindow->tFlags &= ~(PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_RESIZE);
        gptCtx->ptMovingWindow = ptWindow;
        gptCtx->ptDockDragWindow = NULL;
    }
    if(!gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        gptCtx->ptDockDragWindow = NULL;
    plUiWindow* ptWindow = gptCtx->ptMovingWindow;
    if(!ptWindow || ptWindow->uDockId || (ptWindow->tFlags & (PL_UI_WINDOW_FLAGS_CHILD_WINDOW | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_NO_MOVE)))
        return;
    // Avoid docking on a click without a drag. The moving window is cleared after this call.
    const bool bReleased = gptIOI->is_mouse_released(PL_MOUSE_BUTTON_LEFT);
    if(bReleased && gptIO->_afMouseDragMaxDistSqr[PL_MOUSE_BUTTON_LEFT] < 4.0f)
        return;
    if(!bReleased && !gptIOI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
        return;
    const plVec2 tMouse = gptIOI->get_mouse_pos();
    if(pl_sb_size(gptCtx->sbtOpenPopupStack) || gptCtx->bPopupMouseDismissed || (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_DOCKING))
        return;

    uint32_t uTarget = 0;
    plUiWindow* ptTargetWindow = NULL;
    plRect tTargetRect = {0};
    bool bBlocked = false;
    // Find the frontmost window below the dragged window. A window that cannot
    // dock still occludes targets behind it.
    for(uint32_t i = pl_sb_size(gptCtx->sbptFocusedWindows); i > 0; i--)
    {
        plUiWindow* ptCandidate = gptCtx->sbptFocusedWindows[i - 1];
        if(ptCandidate == ptWindow || !ptCandidate->bActive || ptCandidate->uHideFrames || pl__dock_host(ptCandidate))
            continue;
        if(!pl_rect_contains_point(&ptCandidate->tOuterRectClipped, tMouse))
            continue;
        bBlocked = true;
        if(ptCandidate->tFlags & PL_UI_WINDOW_FLAGS_DOCK_HOST)
        {
            for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbtDockNodes); j++)
            {
                plUiDockNode* ptNode = &gptCtx->sbtDockNodes[j];
                if(!ptNode->auChildren[0] && ptNode->ulLastFrame == gptIO->ulFrameCount && pl__dock_root(ptNode->uId)->uHostWindow == ptCandidate->uId && !ptCandidate->bCollapsed && pl_rect_contains_point(&ptNode->tRect, tMouse))
                {
                    uTarget = ptNode->uId;
                    tTargetRect = ptNode->tRect;
                    break;
                }
            }
        }
        else if(!(ptCandidate->tFlags & (PL_UI_WINDOW_FLAGS_CHILD_WINDOW | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_NO_DOCKING | PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_TITLE_BAR)))
        {
            uTarget = ptCandidate->uDockId;
            ptTargetWindow = ptCandidate;
            tTargetRect = ptCandidate->tOuterRect;
        }
        break;
    }
    if(!bBlocked)
    {
        for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes); i++)
        {
            plUiDockNode* ptNode = &gptCtx->sbtDockNodes[i];
            if(!ptNode->auChildren[0] && !pl__dock_root(ptNode->uId)->bFloating && ptNode->ulLastFrame == gptIO->ulFrameCount && pl_rect_contains_point(&ptNode->tRect, tMouse))
            {
                uTarget = ptNode->uId;
                tTargetRect = ptNode->tRect;
                break;
            }
        }
    }
    if(uTarget || ptTargetWindow)
    {
        plRect tPreview = tTargetRect;
        const int iDirection = pl__dock_target_direction(tTargetRect, tMouse);
        if(iDirection == -2)
        {
            pl__dock_draw_reticle(tTargetRect, iDirection);
            return;
        }
        if(iDirection >= 0)
        {
            int iAxis = iDirection >= 2 ? 1 : 0;
            float fMid = (tPreview.tMin.d[iAxis] + tPreview.tMax.d[iAxis]) * 0.5f;
            if(iDirection == 0 || iDirection == 2)
                tPreview.tMax.d[iAxis] = fMid;
            else
                tPreview.tMin.d[iAxis] = fMid;
        }
        plVec4 tColor = gptCtx->tColorScheme.tHeaderActiveCol;
        tColor.a = 0.45f;
        gptDraw->add_rect_filled(gptCtx->ptDebugLayer, tPreview.tMin, tPreview.tMax, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tColor)});
        pl__dock_draw_reticle(tTargetRect, iDirection);
        if(bReleased)
        {
            if(!uTarget)
            {
                uTarget = pl__dock_new_node();
                plUiDockNode* ptRoot = pl__dock_node(uTarget);
                ptRoot->bFloating = true;
                snprintf(ptRoot->acHostTitle, sizeof(ptRoot->acHostTitle), "%.*s", (int)(pl__find_renderered_text_end(ptTargetWindow->pcName, NULL) - ptTargetWindow->pcName), ptTargetWindow->pcName);
                ptRoot->tHostPos = ptTargetWindow->tPos;
                ptRoot->tHostSize = ptTargetWindow->bCollapsed ? ptTargetWindow->tFullSize : ptTargetWindow->tSize;
                pl__dock_layout(uTarget, tTargetRect);
                pl__dock_assign(ptTargetWindow, uTarget);
            }
            if(iDirection >= 0)
                uTarget = pl_ui_dock_split(uTarget, iDirection, 0.5f, NULL);
            pl__dock_assign(ptWindow, uTarget);
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] settings
//-----------------------------------------------------------------------------

static plUiWindowSettings*
pl__settings_window(uint32_t uId)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtWindowSettings); i++)
    {
        if(gptCtx->sbtWindowSettings[i].uId == uId)
            return &gptCtx->sbtWindowSettings[i];
    }
    return NULL;
}

void
pl_ui_set_settings_file(const char* pcPath)
{
    PL_ASSERT(!gptCtx->bSettingsLoaded && "Set the settings path before the first UI frame");
    if(gptCtx->bSettingsLoaded)
        return;
    char* pcCopy = NULL;
    if(pcPath && *pcPath)
    {
        pcCopy = PL_ALLOC(strlen(pcPath) + 1);
        strcpy(pcCopy, pcPath);
    }
    PL_FREE(gptCtx->pcSettingsFile);
    gptCtx->pcSettingsFile = pcCopy;
}

static bool
pl__settings_geometry(plVec2 tPos, plVec2 tSize)
{
    return fabsf(tPos.x) <= 10000000.0f && fabsf(tPos.y) <= 10000000.0f && tSize.x > 0.0f && tSize.y > 0.0f && tSize.x <= 1000000.0f && tSize.y <= 1000000.0f;
}

static plUiDockNode*
pl__settings_node(plUiDockNode* sbtNodes, uint32_t uId)
{
    for(uint32_t i = 0; i < pl_sb_size(sbtNodes); i++)
    {
        if(sbtNodes[i].uId == uId)
            return &sbtNodes[i];
    }
    return NULL;
}

static bool
pl__settings_parse(char* pcText)
{
    plUiDockNode* sbtNodes = NULL;
    plUiWindowSettings* sbtWindows = NULL;
    bool bValid = strncmp(pcText, "PL_UI_LAYOUT 1\n", 15) == 0;
    bool bEnd = false;
    char* pcLine = bValid ? pcText + 15 : NULL;
    while(bValid && pcLine && *pcLine)
    {
        char* pcNext = strchr(pcLine, '\n');
        if(pcNext)
            *pcNext++ = 0;
        int iRead = 0;
        int iCollapsed = 0;
        if(strcmp(pcLine, "End") == 0)
        {
            bEnd = true;
            bValid = !pcNext || !*pcNext;
            break;
        }
        if(*pcLine == 'N')
        {
            plUiDockNode tNode = {0};
            int iFloating = 0;
            bValid = sscanf(pcLine, "N %u %u %u %u %u %d %f %d %f %f %f %f %d %n", &tNode.uId, &tNode.uParent, &tNode.auChildren[0], &tNode.auChildren[1], &tNode.uSelectedWindow, &tNode.iAxis, &tNode.fRatio, &iFloating, &tNode.tHostPos.x, &tNode.tHostPos.y, &tNode.tHostSize.x, &tNode.tHostSize.y, &iCollapsed, &iRead) == 13;
            bValid = bValid && !pcLine[iRead] && tNode.uId && !pl__settings_node(sbtNodes, tNode.uId) && (tNode.iAxis == 0 || tNode.iAxis == 1) && tNode.fRatio >= 0.1f && tNode.fRatio <= 0.9f && (iFloating == 0 || iFloating == 1) && (iCollapsed == 0 || iCollapsed == 1) && pl__settings_geometry(tNode.tHostPos, tNode.tHostSize) && pl_sb_size(sbtNodes) < 4096;
            tNode.bFloating = iFloating != 0;
            tNode.bHadWindows = tNode.uSelectedWindow != 0;
            tNode.bHostCollapsed = iCollapsed != 0;
            tNode.tRect = pl_calculate_rect(tNode.tHostPos, tNode.tHostSize);
            if(bValid)
                pl_sb_push(sbtNodes, tNode);
        }
        else if(*pcLine == 'T')
        {
            uint32_t uId = 0;
            char acHex[511] = {0};
            bValid = sscanf(pcLine, "T %u %510s %n", &uId, acHex, &iRead) == 2 && !pcLine[iRead];
            plUiDockNode* ptNode = pl__settings_node(sbtNodes, uId);
            const size_t szLength = strlen(acHex);
            bValid = bValid && ptNode && ptNode->bFloating && !ptNode->acHostTitle[0] && szLength % 2 == 0;
            for(size_t i = 0; bValid && i < szLength; i += 2)
            {
                const char* pcDigits = "0123456789abcdef";
                const char* pcHigh = strchr(pcDigits, acHex[i]);
                const char* pcLow = strchr(pcDigits, acHex[i + 1]);
                bValid = pcHigh && pcLow;
                if(bValid)
                {
                    const unsigned char uChar = (unsigned char)((pcHigh - pcDigits) * 16 + pcLow - pcDigits);
                    bValid = uChar != 0;
                    ptNode->acHostTitle[i / 2] = (char)uChar;
                }
            }
        }
        else if(*pcLine == 'W')
        {
            plUiWindowSettings tWindow = {0};
            bValid = sscanf(pcLine, "W %u %u %f %f %f %f %d %n", &tWindow.uId, &tWindow.uDockId, &tWindow.tPos.x, &tWindow.tPos.y, &tWindow.tSize.x, &tWindow.tSize.y, &iCollapsed, &iRead) == 7;
            bValid = bValid && !pcLine[iRead] && tWindow.uId && (iCollapsed == 0 || iCollapsed == 1) && pl__settings_geometry(tWindow.tPos, tWindow.tSize) && pl_sb_size(sbtWindows) < 8192;
            for(uint32_t i = 0; bValid && i < pl_sb_size(sbtWindows); i++)
                bValid = sbtWindows[i].uId != tWindow.uId;
            tWindow.bCollapsed = iCollapsed != 0;
            if(bValid)
            {
                pl_sb_push(sbtWindows, tWindow);
            }
        }
        else
            bValid = false;
        pcLine = pcNext;
    }
    bValid = bValid && bEnd;
    // Validate the complete graph before installing it: no dangling links,
    // duplicate children, cycles, or unbounded recursive layout depth.
    for(uint32_t i = 0; bValid && i < pl_sb_size(sbtNodes); i++)
    {
        plUiDockNode* ptNode = &sbtNodes[i];
        if(ptNode->bFloating && ptNode->uParent)
            bValid = false;
        if(!!ptNode->auChildren[0] != !!ptNode->auChildren[1])
            bValid = false;
        if(ptNode->auChildren[0])
        {
            plUiDockNode* ptA = pl__settings_node(sbtNodes, ptNode->auChildren[0]);
            plUiDockNode* ptB = pl__settings_node(sbtNodes, ptNode->auChildren[1]);
            bValid = bValid && ptA && ptB && ptA != ptB && ptA->uParent == ptNode->uId && ptB->uParent == ptNode->uId;
        }
        uint32_t uDepth = 0;
        while(bValid && ptNode->uParent)
        {
            plUiDockNode* ptParent = pl__settings_node(sbtNodes, ptNode->uParent);
            bValid = ptParent && (ptParent->auChildren[0] == ptNode->uId || ptParent->auChildren[1] == ptNode->uId) && ++uDepth <= 64;
            ptNode = ptParent;
        }
    }
    for(uint32_t i = 0; bValid && i < pl_sb_size(sbtWindows); i++)
    {
        if(sbtWindows[i].uDockId)
        {
            plUiDockNode* ptNode = pl__settings_node(sbtNodes, sbtWindows[i].uDockId);
            bValid = ptNode && !ptNode->auChildren[0];
        }
    }
    if(bValid)
    {
        pl_sb_free(gptCtx->sbtDockNodes);
        pl_sb_free(gptCtx->sbtWindowSettings);
        gptCtx->sbtDockNodes = sbtNodes;
        gptCtx->sbtWindowSettings = sbtWindows;
    }
    else
    {
        pl_sb_free(sbtNodes);
        pl_sb_free(sbtWindows);
    }
    return bValid;
}

static void
pl__settings_load(void)
{
    if(gptCtx->bSettingsLoaded)
        return;
    gptCtx->bSettingsLoaded = true;
    if(!gptCtx->pcSettingsFile || !gptFile)
        return;
    plFileStream* ptFile = gptFile->open_stream(gptCtx->pcSettingsFile, PL_FILE_MODE_READ);
    if(!ptFile)
    {
        gptCtx->bSettingsReadOnly = gptFile->exists(gptCtx->pcSettingsFile);
        return;
    }
    uint64_t ulSize = 0;
    bool bValid = gptFile->size_stream(ptFile, &ulSize) == PL_FILE_RESULT_SUCCESS && ulSize > 0 && ulSize <= 1024 * 1024;
    char* pcText = NULL;
    if(bValid)
    {
        pcText = PL_ALLOC((size_t)ulSize + 1);
        bValid = gptFile->read_stream(ptFile, pcText, (size_t)ulSize) == (size_t)ulSize;
        pcText[ulSize] = 0;
        bValid = bValid && strlen(pcText) == (size_t)ulSize && pl__settings_parse(pcText);
    }
    gptFile->close_stream(ptFile);
    PL_FREE(pcText);
    gptCtx->bSettingsReadOnly = !bValid;
    if(!bValid)
        fprintf(stderr, "UI: invalid layout in %s; leaving the file unchanged.\n", gptCtx->pcSettingsFile);
}

static void
pl__settings_snapshot(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptCreatedWindows); i++)
    {
        plUiWindow* ptWindow = gptCtx->sbptCreatedWindows[i];
        if(ptWindow->tFlags & (PL_UI_WINDOW_FLAGS_CHILD_WINDOW | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_NO_SAVED_SETTINGS | PL_UI_WINDOW_FLAGS_DOCK_HOST))
            continue;
        plUiWindowSettings* ptSettings = pl__settings_window(ptWindow->uId);
        if(!ptSettings)
        {
            plUiWindowSettings tSettings = {.uId = ptWindow->uId};
            pl_sb_push(gptCtx->sbtWindowSettings, tSettings);
            ptSettings = &pl_sb_last(gptCtx->sbtWindowSettings);
        }
        ptSettings->uDockId = ptWindow->uDockId;
        ptSettings->tPos = ptWindow->uDockId ? ptWindow->tUndockedPos : ptWindow->tPos;
        ptSettings->tSize = ptWindow->uDockId ? ptWindow->tUndockedSize : (ptWindow->bCollapsed ? ptWindow->tFullSize : ptWindow->tSize);
        ptSettings->bCollapsed = ptWindow->uDockId ? ptWindow->bUndockedCollapsed : ptWindow->bCollapsed;
    }
    pl_sb_reset(gptCtx->sbcSettingsSnapshot);
    pl_sb_sprintf(gptCtx->sbcSettingsSnapshot, "%s", "PL_UI_LAYOUT 1\n");
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes); i++)
    {
        plUiDockNode* ptNode = &gptCtx->sbtDockNodes[i];
        plUiWindow* ptHost = pl__get_ptr(&gptCtx->tWindows, ptNode->uHostWindow);
        if(ptNode->bFloating && ptHost)
        {
            ptNode->tHostPos = ptHost->tPos;
            ptNode->tHostSize = ptHost->bCollapsed ? ptHost->tFullSize : ptHost->tSize;
            ptNode->bHostCollapsed = ptHost->bCollapsed;
        }
        plVec2 tPos = ptNode->bFloating ? ptNode->tHostPos : ptNode->tRect.tMin;
        plVec2 tSize = ptNode->bFloating ? ptNode->tHostSize : pl_sub_vec2(ptNode->tRect.tMax, ptNode->tRect.tMin);
        tSize = pl_max_vec2(tSize, (plVec2){1, 1});
        pl_sb_pop(gptCtx->sbcSettingsSnapshot);
        pl_sb_sprintf(gptCtx->sbcSettingsSnapshot, "N %u %u %u %u %u %d %.9g %d %.9g %.9g %.9g %.9g %d\n", ptNode->uId, ptNode->uParent, ptNode->auChildren[0], ptNode->auChildren[1], ptNode->uSelectedWindow, ptNode->iAxis, ptNode->fRatio, ptNode->bFloating, tPos.x, tPos.y, tSize.x, tSize.y, ptNode->bHostCollapsed);
        if(ptNode->bFloating && ptNode->acHostTitle[0])
        {
            char acHex[511] = {0};
            const char* pcDigits = "0123456789abcdef";
            const size_t szLength = strlen(ptNode->acHostTitle);
            for(size_t j = 0; j < szLength; j++)
            {
                const unsigned char uChar = (unsigned char)ptNode->acHostTitle[j];
                acHex[j * 2] = pcDigits[uChar >> 4];
                acHex[j * 2 + 1] = pcDigits[uChar & 15];
            }
            pl_sb_pop(gptCtx->sbcSettingsSnapshot);
            pl_sb_sprintf(gptCtx->sbcSettingsSnapshot, "T %u %s\n", ptNode->uId, acHex);
        }
    }
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtWindowSettings); i++)
    {
        plUiWindowSettings* ptWindow = &gptCtx->sbtWindowSettings[i];
        pl_sb_pop(gptCtx->sbcSettingsSnapshot);
        pl_sb_sprintf(gptCtx->sbcSettingsSnapshot, "W %u %u %.9g %.9g %.9g %.9g %d\n", ptWindow->uId, ptWindow->uDockId, ptWindow->tPos.x, ptWindow->tPos.y, ptWindow->tSize.x, ptWindow->tSize.y, ptWindow->bCollapsed);
    }
    pl_sb_pop(gptCtx->sbcSettingsSnapshot);
    pl_sb_sprintf(gptCtx->sbcSettingsSnapshot, "%s", "End\n");
}

bool
pl_ui_save_settings(void)
{
    if(!gptCtx->bSettingsLoaded || !gptCtx->pcSettingsFile || gptCtx->bSettingsReadOnly || !gptFile)
        return false;
    pl__settings_snapshot();
    if(gptCtx->sbcSettingsSaved && strcmp(gptCtx->sbcSettingsSaved, gptCtx->sbcSettingsSnapshot) == 0)
        return true;
    char acTempPath[4096] = {0};
    plFileStream* ptFile = gptFile->open_temp_stream(gptCtx->pcSettingsFile, acTempPath, sizeof(acTempPath));
    bool bSaved = false;
    if(ptFile)
    {
        size_t szBytes = strlen(gptCtx->sbcSettingsSnapshot);
        bSaved = gptFile->write_stream(ptFile, gptCtx->sbcSettingsSnapshot, szBytes) == szBytes;
        if(gptFile->flush_stream(ptFile) != PL_FILE_RESULT_SUCCESS)
            bSaved = false;
        if(gptFile->close_stream(ptFile) != PL_FILE_RESULT_SUCCESS)
            bSaved = false;
        if(bSaved)
            bSaved = gptFile->replace_file(acTempPath, gptCtx->pcSettingsFile) == PL_FILE_RESULT_SUCCESS;
        if(!bSaved)
            gptFile->remove(acTempPath);
    }
    if(bSaved)
    {
        pl_sb_reset(gptCtx->sbcSettingsSaved);
        pl_sb_sprintf(gptCtx->sbcSettingsSaved, "%s", gptCtx->sbcSettingsSnapshot);
    }
    else if(!gptCtx->bSettingsWriteFailed)
        fprintf(stderr, "UI: could not save layout to %s; will retry.\n", gptCtx->pcSettingsFile);
    gptCtx->bSettingsWriteFailed = !bSaved;
    return bSaved;
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_ui_set_light_theme(void)
{
    // styles
    gptCtx->tStyle.fTitlePadding            = 5.0f;
    gptCtx->tStyle.fFontSize                = 13.0f;
    gptCtx->tStyle.fWindowHorizontalPadding = 5.0f;
    gptCtx->tStyle.fWindowVerticalPadding   = 5.0f;
    gptCtx->tStyle.iWindowBorderSize        = 0;
    gptCtx->tStyle.fIndentSize              = 20.0f;
    gptCtx->tStyle.fScrollbarSize           = 10.0f;
    gptCtx->tStyle.fSliderSize              = 12.0f;
    gptCtx->tStyle.fTreeLineSize            = 1.0f;
    gptCtx->tStyle.fColorMarkerSize         = 8.0f;
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
    gptCtx->tColorScheme.tTitleActiveCol       = (plVec4){0.89f, 0.75f, 0.80f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCol           = (plVec4){0.91f, 0.91f, 0.92f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCollapsedCol  = (plVec4){0.94f, 0.94f, 0.95f, 1.00f};
    gptCtx->tColorScheme.tWindowBgColor        = (plVec4){0.95f, 0.95f, 0.96f, 0.98f};
    gptCtx->tColorScheme.tWindowBorderColor    = (plVec4){0.68f, 0.44f, 0.51f, 1.00f};
    gptCtx->tColorScheme.tChildBgColor         = (plVec4){0.98f, 0.98f, 0.99f, 1.00f};

    gptCtx->tColorScheme.tButtonCol            = (plVec4){0.86f, 0.66f, 0.72f, 1.00f};
    gptCtx->tColorScheme.tButtonHoveredCol     = (plVec4){0.80f, 0.55f, 0.63f, 1.00f};
    gptCtx->tColorScheme.tButtonActiveCol      = (plVec4){0.72f, 0.40f, 0.51f, 1.00f};

    gptCtx->tColorScheme.tTextCol              = (plVec4){0.12f, 0.12f, 0.14f, 1.00f};
    gptCtx->tColorScheme.tTextDisabledCol      = (plVec4){0.52f, 0.52f, 0.54f, 1.00f};

    gptCtx->tColorScheme.tProgressBarCol       = (plVec4){0.85f, 0.60f, 0.12f, 1.00f};
    gptCtx->tColorScheme.tCheckmarkCol         = (plVec4){0.55f, 0.08f, 0.20f, 1.00f};

    gptCtx->tColorScheme.tFrameBgCol           = (plVec4){0.89f, 0.89f, 0.91f, 1.00f};
    gptCtx->tColorScheme.tFrameBgHoveredCol    = (plVec4){0.95f, 0.82f, 0.85f, 1.00f};
    gptCtx->tColorScheme.tFrameBgActiveCol     = (plVec4){0.90f, 0.72f, 0.77f, 1.00f};

    gptCtx->tColorScheme.tHeaderCol            = (plVec4){0.91f, 0.79f, 0.83f, 1.00f};
    gptCtx->tColorScheme.tHeaderHoveredCol     = (plVec4){0.85f, 0.67f, 0.73f, 1.00f};
    gptCtx->tColorScheme.tHeaderActiveCol      = (plVec4){0.79f, 0.55f, 0.64f, 1.00f};

    gptCtx->tColorScheme.tScrollbarBgCol       = (plVec4){0.88f, 0.88f, 0.89f, 1.00f};
    gptCtx->tColorScheme.tScrollbarHandleCol   = (plVec4){0.70f, 0.65f, 0.68f, 1.00f};
    gptCtx->tColorScheme.tScrollbarFrameCol    = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    gptCtx->tColorScheme.tScrollbarActiveCol   = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tScrollbarHoveredCol  = gptCtx->tColorScheme.tButtonHoveredCol;

    gptCtx->tColorScheme.tSeparatorCol         = (plVec4){0.73f, 0.71f, 0.73f, 1.00f};

    gptCtx->tColorScheme.tTabCol               = (plVec4){0.88f, 0.82f, 0.84f, 1.00f};
    gptCtx->tColorScheme.tTabHoveredCol        = gptCtx->tColorScheme.tButtonHoveredCol;
    gptCtx->tColorScheme.tTabSelectedCol       = (plVec4){0.86f, 0.68f, 0.73f, 1.00f};

    gptCtx->tColorScheme.tResizeGripCol        = (plVec4){0.70f, 0.50f, 0.56f, 1.00f};
    gptCtx->tColorScheme.tResizeGripHoveredCol = (plVec4){0.65f, 0.32f, 0.43f, 1.00f};
    gptCtx->tColorScheme.tResizeGripActiveCol  = (plVec4){0.55f, 0.08f, 0.20f, 1.00f};

    gptCtx->tColorScheme.tSliderCol            = gptCtx->tColorScheme.tButtonCol;
    gptCtx->tColorScheme.tSliderHoveredCol     = gptCtx->tColorScheme.tButtonHoveredCol;
    gptCtx->tColorScheme.tSliderActiveCol      = gptCtx->tColorScheme.tButtonActiveCol;

    gptCtx->tColorScheme.tPopupBgColor         = (plVec4){0.98f, 0.97f, 0.98f, 0.99f};
    gptCtx->tColorScheme.tTreeLineCol          = (plVec4){0.52f, 0.54f, 0.57f, 0.65f};
}

void
pl_ui_set_dark_theme(void)
{
    // styles
    gptCtx->tStyle.fTitlePadding            = 5.0f;
    gptCtx->tStyle.fFontSize                = 13.0f;
    gptCtx->tStyle.fWindowHorizontalPadding = 5.0f;
    gptCtx->tStyle.fWindowVerticalPadding   = 5.0f;
    gptCtx->tStyle.iWindowBorderSize        = 0;
    gptCtx->tStyle.fIndentSize              = 20.0f;
    gptCtx->tStyle.fScrollbarSize           = 10.0f;
    gptCtx->tStyle.fSliderSize              = 12.0f;
    gptCtx->tStyle.fTreeLineSize            = 1.0f;
    gptCtx->tStyle.fColorMarkerSize         = 8.0f;
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
    gptCtx->tColorScheme.tTitleBgCollapsedCol  = (plVec4){0.00f, 0.00f, 0.00f, 1.00f};
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
    gptCtx->tColorScheme.tTreeLineCol          = (plVec4){0.422f, 0.445f, 0.491f, 0.460f};
}

plDrawList2D**
pl_ui_get_draw_lists(uint32_t* puListCountOut)
{
    *puListCountOut = pl_sb_size(gptCtx->sbDrawlists);
    return gptCtx->sbDrawlists;
}

plDrawList2D*
pl_ui_get_debug_draw_list(void)
{
    return gptCtx->ptDebugDrawlist;
}

plDrawLayer2D*
pl_ui_get_debug_draw_layer(void)
{
    return gptCtx->ptDebugLayer;
}

bool
pl_ui_wants_mouse_capture(void)
{
    return gptCtx->bWantCaptureMouse;
}

bool
pl_ui_wants_keyboard_capture(void)
{
    return gptCtx->bWantCaptureKeyboard;
}

void
pl_ui_set_wants_mouse_capture_next_frame(bool bValue)
{
    gptCtx->iWantCaptureMouseNextFrame = (int)bValue;
}

void
pl_ui_set_wants_keyboard_capture_next_frame(bool bValue)
{
    gptCtx->iWantCaptureKeyboardNextFrame = (int)bValue;
}

bool
pl_ui_wants_text_input(void)
{
    return gptCtx->bWantTextInput;
}

void
pl_ui_begin_frame(void)
{
    pl__settings_load();
    gptCtx->fMainMenuBarHeight = 0.0f;

    PL_ASSERT(pl_sb_size(gptCtx->sbtBeginPopupStack) == 0 && "Missing end_popup()");
    pl_sb_reset(gptCtx->sbtBeginPopupStack);
    pl_sb_reset(gptCtx->sbDrawlists);

    gptCtx->bWantTextInput = false;
    const bool bHadOpenPopup = pl_sb_size(gptCtx->sbtOpenPopupStack) != 0;

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

    // A dock space may disappear while a divider is held; release must not depend
    // on submitting that space again.
    if(!gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        gptCtx->uDockSplitterActive = 0;
    gptCtx->bPopupMouseDismissed = false;
    if(gptIO->_tMousePos.x != gptIO->_tMousePosPrev.x || gptIO->_tMousePos.y != gptIO->_tMousePosPrev.y || gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptCtx->bMenuHoverSuppressed = false;
    }
    // Hit-test this frame's mouse against the latest submitted geometry. A move and
    // click may arrive together; the previous frame's cached hover is not sufficient.
    gptCtx->ptHoveredWindow = NULL;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        plUiWindow* ptWindow = gptCtx->sbptWindows[i];
        if(ptWindow->ptRootWindow->tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW)
        {
            bool bOpen = false;
            for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbtOpenPopupStack); j++)
            {
                if(gptCtx->sbtOpenPopupStack[j].ptWindow == ptWindow->ptRootWindow)
                    bOpen = true;
            }
            if(!bOpen)
                continue;
        }
        if(ptWindow->uHideFrames == 0 && pl_rect_contains_point(&ptWindow->tOuterRectClipped, gptIOI->get_mouse_pos()))
            gptCtx->ptHoveredWindow = ptWindow;
    }
    gptCtx->tNextWindowData.bHasDock = false;
    if(pl_sb_size(gptCtx->sbtOpenPopupStack))
    {
        if(gptIOI->is_key_pressed(PL_KEY_ESCAPE, false))
        {
            gptCtx->bMenuHoverSuppressed = true;
            pl__close_popup_to_level(pl_sb_size(gptCtx->sbtOpenPopupStack) - 1, true);
        }
        else if(gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false) || gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
        {
            uint32_t uKeep = 0;
            for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtOpenPopupStack); i++)
            {
                if(pl__window_in_popup(gptCtx->ptHoveredWindow, gptCtx->sbtOpenPopupStack[i].ptWindow))
                    uKeep = i + 1;
            }
            if(uKeep < pl_sb_size(gptCtx->sbtOpenPopupStack))
            {
                // An outside click dismisses the stack without activating a widget behind it.
                gptCtx->bPopupMouseDismissed = uKeep == 0;
                pl__close_popup_to_level(uKeep, true);
            }
        }
    }

    // Clicking the scene releases UI focus. Popup dismissal still consumes this
    // frame's input, but must not leave the restored host capturing the camera.
    if(!gptCtx->ptHoveredWindow && (gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false) || gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false)))
        pl__set_nav_window(NULL);
    else if(!bHadOpenPopup && gptIOI->is_key_pressed(PL_KEY_ESCAPE, false))
        pl__set_nav_window(NULL);

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

    // Resolve capture after focus changes so the initial camera click is usable.
    gptCtx->bWantCaptureMouse = gptCtx->uActiveId != 0 || gptCtx->ptMovingWindow != NULL || gptCtx->ptActiveWindow != NULL || gptCtx->ptNavWindow != NULL;
    gptCtx->bWantCaptureKeyboard = gptCtx->uActiveId != 0 || bHadOpenPopup;

    if(gptCtx->iWantCaptureMouseNextFrame != -1)
        gptCtx->bWantCaptureMouse = (gptCtx->iWantCaptureMouseNextFrame != 0);
    if(gptCtx->iWantCaptureKeyboardNextFrame != -1)
        gptCtx->bWantCaptureKeyboard = (gptCtx->iWantCaptureKeyboardNextFrame != 0);
    gptCtx->iWantCaptureKeyboardNextFrame = -1;
    gptCtx->iWantCaptureMouseNextFrame = -1;

    for(uint32_t i = 0; i < 5; i++)
    {
        if(gptCtx->abMouseOwned[i])
            gptCtx->bWantCaptureMouse = true;
    }
    if(pl_sb_size(gptCtx->sbtOpenPopupStack) || gptCtx->bPopupMouseDismissed)
        gptCtx->bWantCaptureMouse = true;
}

static inline void
pl__focus_window(plUiWindow* ptWindow)
{

    if(ptWindow != gptCtx->ptNavWindow)
    {
        pl__set_nav_window(ptWindow);
    }

    if(ptWindow == NULL)
    {
        return;
    }

    plUiWindow* ptHost = pl__dock_host(ptWindow->ptRootWindow);
    if(ptHost)
        ptWindow = ptHost;
    
    pl_sb_del(gptCtx->sbptFocusedWindows, ptWindow->ptRootWindow->uFocusOrder);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        gptCtx->sbptFocusedWindows[i]->uFocusOrder = i;
    }
    ptWindow->ptRootWindow->uFocusOrder = pl_sb_size(gptCtx->sbptFocusedWindows);
    pl_sb_push(gptCtx->sbptFocusedWindows, ptWindow->ptRootWindow);
    
}

void
pl_ui_push_theme_color(plUiColor tColorCode, plVec4 tColor)
{
    const plUiColorStackItem tPrevItem = {
        .tIndex = tColorCode,
        .tColor = gptCtx->tColorScheme.atColors[tColorCode]
    };
    gptCtx->tColorScheme.atColors[tColorCode] = tColor;
    pl_sb_push(gptCtx->sbtColorStack, tPrevItem);
}

void
pl_ui_pop_theme_color(uint32_t uCount)
{
    for(uint32_t i = 0; i < uCount; i++)
    {
        const plUiColorStackItem tPrevItem = pl_sb_last(gptCtx->sbtColorStack);
        gptCtx->tColorScheme.atColors[tPrevItem.tIndex] = tPrevItem.tColor;
        pl_sb_pop(gptCtx->sbtColorStack);
    }
}

void
pl_ui_set_default_font(plFont* ptFont)
{
    gptCtx->tFont = ptFont;
    gptCtx->tStyle.fFontSize = ptFont->fSize;
}

plFont*
pl_ui_get_default_font(void)
{
    return gptCtx->tFont;
}

void
pl_ui_set_keyboard_focus_last_item(void)
{
    const uint32_t uHash = gptCtx->tPrevItemData.uHash;
    pl__set_nav_id(uHash, gptCtx->ptCurrentWindow);
    pl__set_active_id(uHash, gptCtx->ptCurrentWindow);
    gptCtx->uHoveredId = uHash;
}

void
pl_ui_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios)
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
pl_ui_end_window(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_MENU)
    {
        gptCtx->uMenuDepth--;
    }

    if(!ptWindow->bCollapsed)
        gptDraw->pop_clip_rect(ptWindow->ptDrawlist);

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
        // if(gptCtx->ptCurrentWindow && !gptCtx->ptCurrentWindow->bCollapsed)
        //     gptDraw->push_clip_rect(gptCtx->ptDrawlist, gptCtx->ptCurrentWindow->tInnerClipRect, false);
    }

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        if(!ptWindow->bCollapsed)
            gptDraw->pop_clip_rect(ptWindow->ptDrawlist);
    }
}

bool
pl_ui_begin_window(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    bool bResult = pl__begin_window_ex(pcName, pbOpen, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bResult)
    {
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
        pl_ui_end_window();
    return bResult;
}

plDrawLayer2D*
pl_ui_get_window_fg_drawlayer(void)
{
    PL_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptFgLayer;
}

plDrawLayer2D*
pl_ui_get_window_bg_drawlayer(void)
{
    PL_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptBgLayer; 
}

plVec2
pl_ui_get_cursor_pos(void)
{
    return pl__get_cursor_pos();
}

void
pl_ui_set_next_window_size(plVec2 tSize, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tSize = tSize;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
    gptCtx->tNextWindowData.tSizeCondition = tCondition;
}

void
pl_ui_set_next_window_pos(plVec2 tPos, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tPos = tPos;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    gptCtx->tNextWindowData.tPosCondition = tCondition;
}

void
pl_ui_end_child(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_ui_end_window();
    pl__smart_advance_cursor(ptWindow->tSize.x, ptWindow->tSize.y);
}

bool
pl_ui_begin_child(const char* pcName, plUiChildFlags tChildFlags, plUiWindowFlags tWindowFlags)
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
        PL_UI_WINDOW_FLAGS_NO_MOVE | tWindowFlags;

    pl_ui_set_next_window_pos(tStartPos, PL_UI_COND_ALWAYS);
    pl_ui_set_next_window_size(tWidgetSize, PL_UI_COND_ALWAYS);
    bool bValue =  pl__begin_window_ex(pcName, NULL, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bValue)
    {
        plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
        ptWindow->tMinSize = pl_min_vec2(ptWindow->tMinSize, tWidgetSize);
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
    {
        pl_ui_end_child();
    }
    return bValue;
}

static void
pl__close_popup_to_level(uint32_t uRemaining, bool bRestoreFocus)
{
    if(uRemaining >= pl_sb_size(gptCtx->sbtOpenPopupStack))
        return;
    plUiWindow* ptRestore = gptCtx->sbtOpenPopupStack[uRemaining].ptRestoreWindow;
    for(uint32_t i = uRemaining; i < pl_sb_size(gptCtx->sbtOpenPopupStack); i++)
    {
        plUiWindow* ptWindow = gptCtx->sbtOpenPopupStack[i].ptWindow;
        if(ptWindow && pl__window_in_popup(gptCtx->ptActiveWindow, ptWindow))
            pl__set_active_id(0, NULL);
    }
    pl_sb_resize(gptCtx->sbtOpenPopupStack, uRemaining);
    if(bRestoreFocus)
        pl__focus_window(ptRestore);
}

void
pl_ui_open_popup_id(uint32_t uHash, plUiPopupFlags tFlags)
{
    (void)tFlags; // reserved
    const uint32_t uDepth = pl_sb_size(gptCtx->sbtBeginPopupStack);

    // Closing a parent while still submitting it must not create orphan popups.
    if(uDepth > pl_sb_size(gptCtx->sbtOpenPopupStack))
        return;
    if(uDepth < pl_sb_size(gptCtx->sbtOpenPopupStack) && gptCtx->sbtOpenPopupStack[uDepth].uId == uHash)
        return;
    pl__close_popup_to_level(uDepth, false);
    plUiPopupData tPopupData = {
        .uId = uHash,
        .ulOpenFrameCount = gptIO->ulFrameCount,
        .ptRestoreWindow = gptCtx->ptCurrentWindow ? gptCtx->ptCurrentWindow : gptCtx->ptNavWindow,
        .ptParentWindow = gptCtx->ptCurrentWindow
    };
    pl_sb_push(gptCtx->sbtOpenPopupStack, tPopupData);
}

void
pl_ui_open_popup(const char* pcName, plUiPopupFlags tFlags)
{
    uint32_t uSeed = pl_sb_size(gptCtx->sbuIdStack) ? pl_sb_top(gptCtx->sbuIdStack) : 0;
    pl_ui_open_popup_id(pl_str_hash(pcName, 0, uSeed), tFlags);
}

void
pl_ui_close_current_popup(void)
{
    uint32_t uDepth = pl_sb_size(gptCtx->sbtBeginPopupStack);
    if(!uDepth || uDepth > pl_sb_size(gptCtx->sbtOpenPopupStack))
        return;
    uDepth--;
    if(gptCtx->sbtOpenPopupStack[uDepth].uId != gptCtx->sbtBeginPopupStack[uDepth].uId)
        return;
    // Selecting a command in a submenu closes its menu chain, including a context popup.
    while(uDepth && gptCtx->sbtOpenPopupStack[uDepth].ptWindow && (gptCtx->sbtOpenPopupStack[uDepth].ptWindow->tFlags & PL_UI_WINDOW_FLAGS_MENU))
        uDepth--;
    pl__close_popup_to_level(uDepth, true);
}

void
pl_ui_end_popup(void)
{
    PL_ASSERT(pl_sb_size(gptCtx->sbtBeginPopupStack) > 0);
    PL_ASSERT(gptCtx->ptCurrentWindow && (gptCtx->ptCurrentWindow->tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW));
    pl_ui_end_window();
    pl_sb_pop(gptCtx->sbtBeginPopupStack);
}

uint32_t
pl_ui_get_id(const char* pcName)
{
    if(pcName)
        return pl__ui_get_id(pcName);
    return pl_sb_size(gptCtx->sbuIdStack) > 0 ? pl_sb_top(gptCtx->sbuIdStack) : 0;
}

bool
pl_ui_is_popup_open_id(uint32_t uHash)
{
    uint32_t uDepth = pl_sb_size(gptCtx->sbtBeginPopupStack);
    return uDepth < pl_sb_size(gptCtx->sbtOpenPopupStack) && gptCtx->sbtOpenPopupStack[uDepth].uId == uHash;
}

bool
pl_ui_is_popup_open(const char* pcName)
{
    uint32_t uSeed = pl_sb_size(gptCtx->sbuIdStack) ? pl_sb_top(gptCtx->sbuIdStack) : 0;
    return pl_ui_is_popup_open_id(pl_str_hash(pcName, 0, uSeed));
}

bool
pl_ui_begin_popup_id(uint32_t uHash, plUiWindowFlags tFlags)
{
    if(!pl_ui_is_popup_open_id(uHash))
    {
        gptCtx->tNextWindowData.tFlags = 0;
        gptCtx->tNextWindowData.bHasDock = false;
        return false;
    }
    const uint32_t uDepth = pl_sb_size(gptCtx->sbtBeginPopupStack);
    const bool bNew = gptCtx->sbtOpenPopupStack[uDepth].ptWindow == NULL;
    if(bNew && !(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS))
    {
        gptCtx->tNextWindowData.tPos = gptIOI->get_mouse_pos();
        gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_ALWAYS;
        gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    }
    pl_sb_push(gptCtx->sbtBeginPopupStack, gptCtx->sbtOpenPopupStack[uDepth]);
    char acName[48] = {0};
    snprintf(acName, sizeof(acName), "Popup###PL_POPUP_%08x", uHash);
    bool bResult = pl__begin_window_ex(acName, NULL, tFlags | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_NO_COLLAPSE);
    gptCtx->sbtOpenPopupStack[uDepth].ptWindow = gptCtx->ptCurrentWindow;
    gptCtx->sbtBeginPopupStack[uDepth].ptWindow = gptCtx->ptCurrentWindow;
    if(bResult)
    {
        static const float afWidth[] = {300.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, afWidth);
        if(bNew)
            pl__focus_window(gptCtx->ptCurrentWindow);
    }
    else pl_ui_end_popup();
    return bResult;
}

bool
pl_ui_begin_popup(const char* pcName, plUiWindowFlags tFlags)
{
    uint32_t uSeed = pl_sb_size(gptCtx->sbuIdStack) ? pl_sb_top(gptCtx->sbuIdStack) : 0;
    return pl_ui_begin_popup_id(pl_str_hash(pcName, 0, uSeed), tFlags);
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
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_AUTO_SIZE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    // place window at mouse position
    const plVec2 tMousePos = gptIOI->get_mouse_pos();
    ptWindow->tTempData.tCursorStartPos = pl_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    ptWindow->tPos = tMousePos;
    ptWindow->tTempData.tRowCursorLastPos = ptWindow->tTempData.tRowCursorPos;
    ptWindow->tTempData.tRowCursorPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    ptWindow->tTempData.tRowCursorPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);

    const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y };
    const plVec2 tEndClip = { ptWindow->tSize.x, ptWindow->tSize.y };
    gptDraw->push_clip_rect(ptWindow->ptDrawlist, pl_calculate_rect(tStartClip, tEndClip), false);

    ptWindow->ptParentWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = ptWindow;

    static const float pfRatios[] = {300.0f};
    pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
}

void
pl_ui_end_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    ptWindow->tSize.y = ptWindow->tContentSize.y;

    gptDraw->add_rect_rounded_filled(ptWindow->ptBgLayer,
        ptWindow->tPos, 
        pl_add_vec2(ptWindow->tPos, ptWindow->tSize), 0.0f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tWindowBgColor)});

    gptDraw->pop_clip_rect(ptWindow->ptDrawlist);
    gptCtx->ptCurrentWindow = ptWindow->ptParentWindow;
}

bool
pl_ui_is_window_appearing(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->bAppearing;
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

plVec2
pl_ui_get_window_scroll(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScroll;
}

plVec2
pl_ui_get_window_scroll_max(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScrollMax;
}

void
pl_ui_set_window_scroll(plVec2 tScroll)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    if(ptWindow->tScrollMax.x >= tScroll.x)
        ptWindow->tScroll.x = tScroll.x;

    if(ptWindow->tScrollMax.y >= tScroll.y)
        ptWindow->tScroll.y = tScroll.y;
}

void
pl_ui_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.bCollapsed = bCollapsed;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED;
    gptCtx->tNextWindowData.tCollapseCondition = tCondition;    
}

bool
pl_ui_step_clipper(plUiClipper* ptClipper)
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
        if(ptClipper->_fStartPosY < pl_ui_get_window_pos().y)
            ptClipper->uDisplayStart = (uint32_t)((pl_ui_get_window_pos().y - ptClipper->_fStartPosY) / ptClipper->_fItemHeight);
        ptClipper->uDisplayEnd = ptClipper->uDisplayStart + (uint32_t)(pl_ui_get_window_size().y / ptClipper->_fItemHeight) + 1;
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
pl_ui_layout_dynamic(float fHeight, uint32_t uWidgetCount)
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
pl_ui_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount)
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
pl_ui_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
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
pl_ui_layout_row_push(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptCurrentRow->fWidgetWidth = fWidth;
}

void
pl_ui_layout_row_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowCursorLastPos = ptWindow->tTempData.tRowCursorPos;
    ptWindow->tTempData.tRowCursorPos.y = ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_ui_layout_template_begin(float fHeight)
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
pl_ui_layout_template_push_dynamic(void)
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
pl_ui_layout_template_push_variable(float fWidth)
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
pl_ui_layout_template_push_static(float fWidth)
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
pl_ui_layout_template_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE);
    // ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    // ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    // ptWindow->tTempData.tRowCursorPos.y += ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

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
        - (float)pl_sb_size(gptCtx->ptCurrentWindow->sbtTreeNodeStack) * gptCtx->tStyle.fIndentSize; // remove indent

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
        while(true)
        {
            if(ptCurrentRow->uVariableEntryCount == 0)
                break;
            bool bSwapOccured = false;
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
            if(!bSwapOccured)
                break;
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
pl_ui_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
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
pl_ui_layout_space_push(float fX, float fY, float fWidth, float fHeight)
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
pl_ui_layout_space_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowCursorPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);

    ptWindow->tTempData.tRowCursorLastPos = ptWindow->tTempData.tRowCursorPos;
    ptWindow->tTempData.tRowCursorPos.y = ptWindow->tTempData.tRowCursorPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tLayoutRow = tNewRow;
}

void
pl_ui_stash_layout(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow == NULL)
        return;

    // push cursor info onto stack
    plUiCursorStackItem tCursorStackItem = {
        .tPos = ptWindow->tTempData.tRowCursorLastPos
        // .tPos = pl__get_cursor_pos()
    };
    pl_sb_push(ptWindow->sbtCursorStack, tCursorStackItem);

    // push current layout onto stack
    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tLayoutRow);
    pl_ui_layout_dynamic(0.0f, 1);

    if(pl_sb_size(ptWindow->sbfMaxCursorYStack) > 0)
    {
        float fLastMax = pl_sb_top(ptWindow->sbfMaxCursorYStack);
        pl_sb_push(ptWindow->sbfMaxCursorYStack, fLastMax);
    }
    else
    {
        pl_sb_push(ptWindow->sbfMaxCursorYStack, 0.0f);
    }
}

void
pl_ui_apply_layout(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(ptWindow == NULL)
        return;

    pl_sb_pop(ptWindow->sbfMaxCursorYStack);
    ptWindow->tTempData.tLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
    plVec2 tNewCursor = ptWindow->tTempData.tRowCursorPos;

    plUiCursorStackItem tOldCursorInfo = pl_sb_pop(ptWindow->sbtCursorStack);

    if(ptWindow->tTempData.tLayoutRow.uCurrentColumn == 0)
    {
        pl__set_cursor((plVec2){tOldCursorInfo.tPos.x, ptWindow->tTempData.tCursorMaxPos.y});
    }
    else
    {
        pl__set_cursor((plVec2){tOldCursorInfo.tPos.x, tOldCursorInfo.tPos.y});
        ptWindow->tTempData.tLayoutRow.fMaxHeight = pl_maxf(ptWindow->tTempData.tLayoutRow.fMaxHeight - gptCtx->tStyle.tItemSpacing.y, tNewCursor.y - tOldCursorInfo.tPos.y);
    }
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

static void
pl__render_checker_board(plDrawLayer2D* draw_list, plVec2 p_min, plVec2 p_max, plVec4 col, float grid_step, plVec2 grid_off, float rounding, plDrawRectFlags flags)
{
    if(col.a < 1.0f)
    {
        uint32_t col_bg1 = PL_COLOR_32_VEC4(pl__ui_alpha_blend_colors((plVec4){128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f}, col));
        uint32_t col_bg2 = PL_COLOR_32_VEC4(pl__ui_alpha_blend_colors((plVec4){204.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f}, col));
        gptDraw->add_rect_rounded_filled(draw_list, p_min, p_max, rounding, 0, flags, (plDrawSolidOptions){.uColor = col_bg1});

        int yi = 0;
        for (float y = p_min.y + grid_off.y; y < p_max.y; y += grid_step, yi++)
        {
            float y1 = pl_clampf(p_min.y, y, p_max.y), y2 = pl_minf(y + grid_step, p_max.y);
            if (y2 <= y1)
                continue;
            for (float x = p_min.x + grid_off.x + ((yi ^ 1)  & 1) * grid_step; x < p_max.x; x += grid_step * 2.0f)
            {
                float x1 = pl_clampf(p_min.x, x, p_max.x), x2 = pl_minf(x + grid_step, p_max.x);
                if (x2 <= x1)
                    continue;
                plDrawRectFlags cell_flags = PL_DRAW_RECT_FLAG_ROUND_CORNERS_NONE; // FIXME: Could use CalcRoundingFlagsForRectInRect()
                if (y1 <= p_min.y) { if (x1 <= p_min.x) cell_flags |= PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT; if (x2 >= p_max.x) cell_flags |= PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT; }
                if (y2 >= p_max.y) { if (x1 <= p_min.x) cell_flags |= PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT; if (x2 >= p_max.x) cell_flags |= PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT; }

                // Combine flags
                cell_flags = (flags == PL_DRAW_RECT_FLAG_ROUND_CORNERS_NONE || cell_flags == PL_DRAW_RECT_FLAG_ROUND_CORNERS_NONE) ? PL_DRAW_RECT_FLAG_ROUND_CORNERS_NONE : (cell_flags & flags);
                gptDraw->add_rect_rounded_filled(draw_list, (plVec2){x1, y1}, (plVec2){x2, y2}, rounding, 0, cell_flags, (plDrawSolidOptions){.uColor = col_bg2});
            }
        }
    }
    else
    {
        gptDraw->add_rect_rounded_filled(draw_list, p_min, p_max, rounding, 0, flags, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(col)});
    }
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

    if(!pl__popup_allows_window(ptWindow))
        return false;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    if(!gptIOI->is_mouse_hovering_rect(ptBox->tMin, ptBox->tMax))
        return false;

    // check if another item is already hovered
    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    // check if another item is already active
    if(gptCtx->uActiveId != 0 && gptCtx->uActiveId != uHash)
    {
        if(!gptCtx->bActiveIdAllowsOverlap)
        {
            return false;
        }
    }

    return true;
}

bool
pl_is_item_hoverable_circle(plVec2 tP, float fRadius, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(!pl__popup_allows_window(ptWindow))
        return false;

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
    gptDraw->add_text(ptLayer, (plVec2){roundf(tP.x), roundf(tP.y)}, (plDrawTextOptions){
        .fSize = fSize,
        .fWrap = fWrap,
        .pcTextEnd = NULL,
        .ptFont = ptFont,
        .uColor = uColor
    },
        "%.*s", (int)(pl__find_renderered_text_end(pcText, pcTextEnd) - pcText), pcText
    );
}

static void
pl__add_clipped_text(plDrawLayer2D* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, uint32_t uColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text(ptLayer, (plVec2){roundf(tP.x), roundf(tP.y)},
        (plDrawTextOptions){
            .fSize = fSize,
            .fWrap = fWrap,
            .pcTextEnd = NULL,
            .ptFont = ptFont,
            .uColor = uColor,
            .tClipMin = tMin,
            .tClipMax = tMax
    },
        "%.*s", (int)(pl__find_renderered_text_end(pcText, pcTextEnd) - pcText), pcText);
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

static void
pl__dock_begin_floating(plUiDockNode* ptRoot)
{
    if(ptRoot->ulLastFrame == gptIO->ulFrameCount)
        return;

    // The host is submitted lazily by the first member, including an inactive tab.
    // The host only supplies geometry and resizing; each pane draws its own header.
    const plUiNextWindowData tNext = gptCtx->tNextWindowData;
    plUiWindow* ptCurrent = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = NULL;
    memset(&gptCtx->tNextWindowData, 0, sizeof(gptCtx->tNextWindowData));
    char acName[64];
    snprintf(acName, sizeof(acName), "Dock group###PL_DOCK_HOST_%08x", ptRoot->uId);
    ptRoot->uHostWindow = pl_str_hash(acName, 0, 0);
    if(!ptRoot->acHostTitle[0] && ptCurrent)
        snprintf(ptRoot->acHostTitle, sizeof(ptRoot->acHostTitle), "%.*s", (int)(pl__find_renderered_text_end(ptCurrent->pcName, NULL) - ptCurrent->pcName), ptCurrent->pcName);
    pl_ui_set_next_window_pos(ptRoot->tHostPos, PL_UI_COND_ONCE);
    pl_ui_set_next_window_size(ptRoot->tHostSize, PL_UI_COND_ONCE);
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_ALWAYS;
    gptCtx->tNextWindowData.bCollapsed = false;
    const bool bVisible = pl__begin_window_ex(acName, NULL, PL_UI_WINDOW_FLAGS_DOCK_HOST | PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | PL_UI_WINDOW_FLAGS_NO_COLLAPSE | PL_UI_WINDOW_FLAGS_NO_DOCKING | PL_UI_WINDOW_FLAGS_NO_SAVED_SETTINGS | PL_UI_WINDOW_FLAGS_NO_SCROLLBAR);
    plUiWindow* ptHost = gptCtx->ptCurrentWindow;
    ptRoot->uHostWindow = ptHost->uId;
    ptRoot->tHostPos = ptHost->tPos;
    ptRoot->tHostSize = ptHost->bCollapsed ? ptHost->tFullSize : ptHost->tSize;
    ptRoot->bHostCollapsed = ptHost->bCollapsed;
    if(bVisible)
    {
        const plVec2 tPos = pl_add_vec2(ptHost->tPos, (plVec2){2.0f, 2.0f});
        const plVec2 tSize = pl_sub_vec2(ptHost->tSize, (plVec2){4.0f, 4.0f});
        pl__dock_space(ptRoot->uId, tPos, tSize, ptHost->ptFgLayer);
    }
    else
        ptRoot->ulLastFrame = gptIO->ulFrameCount;
    pl_ui_end_window();
    gptCtx->ptCurrentWindow = ptCurrent;
    gptCtx->tNextWindowData = tNext;
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
    uint32_t uWindowID = pl_str_hash(pcName, 0, ptParentWindow ? pl_sb_top(gptCtx->sbuIdStack) : 0);
    if(tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW)
    {
        const uint32_t uPopupID = pl_sb_top(gptCtx->sbtBeginPopupStack).uId;
        const uint32_t uDepth = pl_sb_size(gptCtx->sbtBeginPopupStack);
        // Separate popup-window storage from normal windows and from other popup depths.
        const uint32_t uSeed = pl_str_hash_data(&uDepth, sizeof(uDepth), pl_str_hash("PL_POPUP_WINDOW", 0, 0));
        uWindowID = pl_str_hash_data(&uPopupID, sizeof(uPopupID), uSeed);
    }
    pl_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text & title bar sizes
    const char* pcTitle = pcName;
    if(tFlags & PL_UI_WINDOW_FLAGS_DOCK_HOST)
    {
        for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes); i++)
        {
            if(gptCtx->sbtDockNodes[i].uHostWindow == uWindowID)
            {
                pcTitle = gptCtx->sbtDockNodes[i].acHostTitle;
                break;
            }
        }
    }
    const plVec2 tTextSize = pl__calculate_text_size(gptCtx->tFont, gptCtx->tStyle.fFontSize, pcTitle, 0.0f);
    float fTitleBarHeight = (tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist in storage
    ptWindow = pl__get_ptr(&gptCtx->tWindows, uWindowID);

    // new window needs to be created
    if(ptWindow == NULL)
    {
        
        // allocate new window
        ptWindow = PL_ALLOC(sizeof(plUiWindow));
        memset(ptWindow, 0, sizeof(plUiWindow));
        ptWindow->uFocusOrder                   = pl_sb_size(gptCtx->sbptFocusedWindows);
        ptWindow->uId                           = uWindowID;
        ptWindow->szNameBufferLength            = strlen(pcName) + 1;
        ptWindow->pcName                        = PL_ALLOC(ptWindow->szNameBufferLength);
        ptWindow->tPos                          = (plVec2){ gptIO->tMainViewportSize.x * 0.25f, gptIO->tMainViewportSize.y * 0.25f};
        ptWindow->tMinSize                      = (plVec2){ 200.0f, 200.0f };
        ptWindow->tMaxSize                      = (plVec2){ 10000.0f, 10000.0f };
        ptWindow->tSize                         = (plVec2){ gptIO->tMainViewportSize.x * 0.50f, gptIO->tMainViewportSize.y * 0.50f};
        ptWindow->ptDrawlist                    = gptDraw->request_2d_drawlist();
        ptWindow->ptBgLayer                     = gptDraw->request_2d_layer(ptWindow->ptDrawlist);
        ptWindow->ptFgLayer                     = gptDraw->request_2d_layer(ptWindow->ptDrawlist);
        ptWindow->tPosAllowableFlags            = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE | PL_UI_COND_APPEARING;
        ptWindow->tSizeAllowableFlags           = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE | PL_UI_COND_APPEARING;
        ptWindow->tCollapseAllowableFlags       = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE | PL_UI_COND_APPEARING;
        ptWindow->ptParentWindow                = ptParentWindow;
        ptWindow->ptRootWindow                  = NULL;
        ptWindow->ptRootWindowPopupTree         = NULL;
        ptWindow->ptRootWindowTitleBarHighlight = NULL;
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
            ptWindow->ptRootWindowPopupTree = ptParentWindow ? ptParentWindow : ptWindow;
            ptWindow->ptRootWindowTitleBarHighlight = ptParentWindow ? ptParentWindow->ptRootWindow : ptWindow;
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
        pl_sb_push(gptCtx->sbptCreatedWindows, ptWindow);

        plUiWindowSettings* ptSettings = pl__settings_window(uWindowID);
        if(ptSettings && !(tFlags & (PL_UI_WINDOW_FLAGS_CHILD_WINDOW | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_NO_SAVED_SETTINGS)))
        {
            ptWindow->tPos = ptSettings->tPos;
            ptWindow->tSize = ptSettings->tSize;
            ptWindow->tFullSize = ptSettings->tSize;
            ptWindow->bCollapsed = ptSettings->bCollapsed;
            ptWindow->tUndockedPos = ptSettings->tPos;
            ptWindow->tUndockedSize = ptSettings->tSize;
            ptWindow->bUndockedCollapsed = ptSettings->bCollapsed;
            ptWindow->uDockId = (tFlags & PL_UI_WINDOW_FLAGS_NO_DOCKING) ? 0 : ptSettings->uDockId;
            ptWindow->bDockConditionOnce = true;
            ptWindow->tPosAllowableFlags &= ~PL_UI_COND_ONCE;
            ptWindow->tSizeAllowableFlags &= ~PL_UI_COND_ONCE;
            ptWindow->tCollapseAllowableFlags &= ~PL_UI_COND_ONCE;
        }
        if(tFlags & PL_UI_WINDOW_FLAGS_DOCK_HOST)
            ptWindow->tFullSize = gptCtx->tNextWindowData.tSize;
    }
    else
    {
        ptWindow->bAppearing = ptWindow->uLastFrameActive < gptIO->ulFrameCount - 1;

        if(ptWindow->bAppearing)
        {
            ptWindow->tPosAllowableFlags      |= PL_UI_COND_APPEARING;
            ptWindow->tSizeAllowableFlags     |= PL_UI_COND_APPEARING;
            ptWindow->tCollapseAllowableFlags |= PL_UI_COND_APPEARING;
        }
        else
        {
            ptWindow->tPosAllowableFlags      &= ~PL_UI_COND_APPEARING;
            ptWindow->tSizeAllowableFlags     &= ~PL_UI_COND_APPEARING;
            ptWindow->tCollapseAllowableFlags &= ~PL_UI_COND_APPEARING;
        }
    }

    ptWindow->uLastFrameActive = gptIO->ulFrameCount;

    ptWindow->ptParentWindow = ptParentWindow;

    // seen this frame (obviously)
    ptWindow->bActive = true;
    ptWindow->tFlags = tFlags;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        pl_sb_push(ptParentWindow->sbtChildWindows, ptWindow);
    }
    gptCtx->ptCurrentWindow = ptWindow;

    // Dock assignment is independent of position/size conditions; ONCE preserves user undocking.
    if((tFlags & PL_UI_WINDOW_FLAGS_NO_DOCKING) && ptWindow->uDockId)
        pl__dock_assign(ptWindow, 0);
    if(gptCtx->tNextWindowData.bHasDock && !(tFlags & (PL_UI_WINDOW_FLAGS_CHILD_WINDOW | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_NO_DOCKING)))
    {
        plUiConditionFlags tCondition = gptCtx->tNextWindowData.tDockCondition;
        if((tCondition & PL_UI_COND_ALWAYS) || ((tCondition & PL_UI_COND_ONCE) && !ptWindow->bDockConditionOnce) || ((tCondition & PL_UI_COND_APPEARING) && ptWindow->bAppearing))
        {
            pl__dock_assign(ptWindow, gptCtx->tNextWindowData.uDockId);
            ptWindow->bDockConditionOnce = true;
        }
    }
    gptCtx->tNextWindowData.bHasDock = false;
    plUiDockNode* ptDock = pl__dock_node(ptWindow->uDockId);
    if(ptDock)
    {
        plUiDockNode* ptRoot = pl__dock_root(ptDock->uId);
        if(ptRoot->bFloating)
            pl__dock_begin_floating(ptRoot);
        if(!ptDock->uSelectedWindow)
            ptDock->uSelectedWindow = ptWindow->uId;
        ptDock->bHadWindows = true;
        ptWindow->bCollapsed = false;
        if(ptDock->ulLastFrame != gptIO->ulFrameCount || ptDock->uSelectedWindow != ptWindow->uId || (ptRoot->bFloating && ptRoot->bHostCollapsed))
        {
            // No clip rect was pushed; end_window() observes collapsed solely for stack balance.
            ptWindow->bCollapsed = true;
            ptWindow->bActive = false;
            ptWindow->bVisible = false;
            gptCtx->tNextWindowData.tFlags = 0;
            return false;
        }
        tFlags &= ~(PL_UI_WINDOW_FLAGS_AUTO_SIZE | PL_UI_WINDOW_FLAGS_NO_TITLE_BAR);
        tFlags |= PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE;
        ptWindow->tFlags = tFlags;
        fTitleBarHeight = pl__get_frame_height() + gptCtx->tStyle.tItemSpacing.y;
        gptCtx->tNextWindowData.tPos = ptDock->tRect.tMin;
        gptCtx->tNextWindowData.tSize = pl_sub_vec2(ptDock->tRect.tMax, ptDock->tRect.tMin);
        gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS | PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
        gptCtx->tNextWindowData.tPosCondition = gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_ALWAYS;
    }

    // use last frame cursors for sizing
    ptWindow->tContentSize = pl_add_vec2((plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
        pl_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos));

    // reset per frame window temporary data
    plVec2 tOldCursorStart = ptWindow->tTempData.tCursorStartPos;
    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));
    pl_sb_reset(ptWindow->sbfAvailableSizeStack);
    pl_sb_reset(ptWindow->sbfMaxCursorYStack);
    pl_sb_reset(ptWindow->sbtChildWindows);
    pl_sb_reset(ptWindow->sbtRowStack);
    pl_sb_reset(ptWindow->sbtCursorStack);
    pl_sb_reset(ptWindow->sbtRowTemplateEntries);
    ptWindow->tTempData.tCursorMaxPos = tOldCursorStart;

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // should window collapse
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED)
    {
        if(ptWindow->tCollapseAllowableFlags & gptCtx->tNextWindowData.tCollapseCondition)
        {
            ptWindow->bCollapsed = gptCtx->tNextWindowData.bCollapsed;
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

    if(bWindowSizeSet || !ptWindow->bCollapsed)
        ptWindow->tFullSize = ptWindow->tSize;
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
    if(!ptWindow->uDockId && !(tFlags & (PL_UI_WINDOW_FLAGS_CHILD_WINDOW | PL_UI_WINDOW_FLAGS_POPUP_WINDOW | PL_UI_WINDOW_FLAGS_MAIN_MENU_BAR)))
    {
        ptWindow->tPos.y = pl_maxf(ptWindow->tPos.y, gptCtx->fMainMenuBarHeight);
        tStartPos = ptWindow->tPos;
    }
    ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
    ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
    ptWindow->tInnerRect = ptWindow->tOuterRect;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        const plRect* ptParentRect = gptDraw->get_clip_rect(ptParentWindow->ptDrawlist);
        ptWindow->tOuterRectClipped = pl_rect_clip_full(&ptWindow->tOuterRectClipped, ptParentRect);
    }

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

    if(ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }
    else
        ptWindow->tScroll.y = 0;

    if(ptWindow->bScrollbarX)
    {
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }
    else
        ptWindow->tScroll.x = 0;

    // remove scrollbars from inner rect
    if(ptWindow->bScrollbarY)
        ptWindow->tInnerRect.tMax.x -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    if(ptWindow->bScrollbarX)
        ptWindow->tInnerRect.tMax.y -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    // decorations
    bool bHasTitleBar = !(tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR);
    if(bHasTitleBar) // has title bar
    {

        ptWindow->tInnerRect.tMin.y += fTitleBarHeight;

        const bool bHighlightWindow = gptCtx->ptTitleFocusWindow && pl__title_highlight_root(ptWindow) == pl__title_highlight_root(gptCtx->ptTitleFocusWindow);

        // draw title bar
        plVec4 tTitleColor;
        if(bHighlightWindow)
            tTitleColor = gptCtx->tColorScheme.tTitleActiveCol;
        else if(ptWindow->bCollapsed)
            tTitleColor = gptCtx->tColorScheme.tTitleBgCollapsedCol;
        else
            tTitleColor = gptCtx->tColorScheme.tTitleBgCol;
        if(!ptDock)
            gptDraw->add_rect_rounded_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x, fTitleBarHeight}), gptCtx->tStyle.fWindowRounding, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tTitleColor)});

        // draw title text
        const plVec2 titlePos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
        if(ptDock) pl__dock_tabs(ptWindow, pbOpen, fTitleBarHeight);
        else pl__add_text(ptWindow->ptFgLayer, gptCtx->tFont, gptCtx->tStyle.fFontSize, titlePos, PL_COLOR_32_VEC4(gptCtx->tColorScheme.tTextCol), pcTitle, 0.0f);

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

            if(gptCtx->uActiveId == uCloseHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 1.0f)});
            else if(gptCtx->uHoveredId == uCloseHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 1.0f)});
            else                                      gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 1.0f)});

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

            if(gptCtx->uActiveId == uCollapseHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 1.0f)});
            else if(gptCtx->uHoveredId == uCollapseHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 1.0f)});
            else                                         gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.5f, 0.0f, 1.0f)});

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

    // update layout cursors
    ptWindow->tTempData.tCursorStartPos.x = gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - ptWindow->tScroll.x;
    ptWindow->tTempData.tCursorStartPos.y = gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - ptWindow->tScroll.y;

    ptWindow->tTempData.tRowCursorLastPos = ptWindow->tTempData.tCursorStartPos;
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
            gptCtx->tStyle.fWindowRounding, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM | (bHasTitleBar ? 0 : PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP),
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
        if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_BACKGROUND) &&  !pl_rect_is_inverted(&tBgRect))
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
                0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM | (bHasTitleBar ? 0 : PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP),
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

    // Build the content clip after excluding the title bar and scrollbars.
    ptWindow->tInnerClipRect = pl_rect_expand_vec2(&ptWindow->tInnerRect, (plVec2){-gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        const plRect* ptParentRect = gptDraw->get_clip_rect(ptParentWindow->ptDrawlist);
        ptWindow->tInnerClipRect = pl_rect_clip_full(&ptWindow->tInnerClipRect, ptParentRect);
    }

    if(!ptWindow->bCollapsed)
    {
        // const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight };

        // const plVec2 tInnerClip = { 
        //     ptWindow->tSize.x - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f),
        //     ptWindow->tSize.y - fTitleBarHeight - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f)
        // };

        // const plRect* ptClipRect = gptDraw->get_clip_rect(ptWindow->ptDrawlist);

        // if(ptClipRect)
        // {
        //     ptWindow->tInnerClipRect = pl_rect_clip_full(&ptWindow->tInnerClipRect, ptClipRect);
            
        // }

        if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        {
            const plRect* ptParentRect = gptDraw->get_clip_rect(ptParentWindow->ptDrawlist);
            // ptWindow->tOuterRectClipped = pl_rect_clip_full(&ptWindow->tOuterRectClipped, ptParentRect);
            gptDraw->push_clip_rect(ptWindow->ptDrawlist, *ptParentRect, false);
        }

        gptDraw->push_clip_rect(ptWindow->ptDrawlist, ptWindow->tInnerClipRect, true);
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
                - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) // remove spacing between widgets
                - (float)pl_sb_size(gptCtx->ptCurrentWindow->sbtTreeNodeStack) * gptCtx->tStyle.fIndentSize; // remove indent

            fWidth *= fTotalWidthAvailable;
        }
        else
        {
            fWidth -= (float)pl_sb_size(gptCtx->ptCurrentWindow->sbtTreeNodeStack) * gptCtx->tStyle.fIndentSize;
        }

        const plVec2 tWidgetSize = { fWidth, fHeight};
        return tWidgetSize;
    }
}

static void
pl__set_cursor(plVec2 tPos)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tRowCursorLastPos = ptWindow->tTempData.tRowCursorPos;
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
        
        ptWindow->tTempData.tRowCursorLastPos = ptWindow->tTempData.tRowCursorPos;
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
    plUiDockNode* ptNode = pl__dock_node(ptWindow->uDockId);
    if(ptNode && ptNode->uSelectedWindow != ptWindow->uId)
        return;
    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW)
    {
        bool bOpen = false;
        for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtOpenPopupStack); i++)
        {
            if(gptCtx->sbtOpenPopupStack[i].ptWindow == ptWindow)
                bOpen = true;
        }
        if(!bOpen)
            return;
    }
    pl_sb_push(gptCtx->sbptWindows, ptWindow);
    for(uint32_t j = 0; j < pl_sb_size(ptWindow->sbtChildWindows); j++)
        pl__submit_window(ptWindow->sbtChildWindows[j]);
}

static void
pl__set_active_id(uint32_t uHash, plUiWindow* ptWindow)
{
    if(gptCtx->uScalarInputId != uHash)
        gptCtx->uScalarInputId = 0;
    gptCtx->bActiveIdJustActivated = gptCtx->uActiveId != uHash;
    gptCtx->uActiveId = uHash;

    gptCtx->ptActiveWindow = ptWindow;
    gptCtx->bActiveIdAllowsOverlap = false;
    if(uHash)
    {
        gptCtx->uActiveIdIsAlive = uHash;
    }
}

static void
pl__set_nav_id(uint32_t uHash, plUiWindow* ptWindow)
{
    gptCtx->uNavId = uHash;
    
    if(uHash)
        gptCtx->bNavIdIsAlive = true;

    pl__set_nav_window(ptWindow);
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

        // if(gptCtx->uActiveId == uHash)
        //     gptCtx->uActiveIdIsAlive = uHash;
    }

    gptCtx->tPrevItemData.uHash = uHash;
}

void
pl_ui_initialize(void)
{
    pl_ui_set_settings_file("pl_ui.ini");
    // gptCtx->ptDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->ptDockDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->ptDockLayer = gptDraw->request_2d_layer(gptCtx->ptDockDrawlist);
    gptCtx->ptDebugDrawlist = gptDraw->request_2d_drawlist();
    gptCtx->tTooltipWindow.ptDrawlist = gptDraw->request_2d_drawlist();
    // gptCtx->ptBgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
    // gptCtx->ptFgLayer = gptDraw->request_2d_layer(gptCtx->ptDrawlist);
    gptCtx->ptDebugLayer = gptDraw->request_2d_layer(gptCtx->ptDebugDrawlist);
    gptCtx->tTooltipWindow.ptBgLayer = gptDraw->request_2d_layer(gptCtx->tTooltipWindow.ptDrawlist);
    gptCtx->tTooltipWindow.ptFgLayer = gptDraw->request_2d_layer(gptCtx->tTooltipWindow.ptDrawlist);

    gptCtx->tFrameBufferScale.x = 1.0f;
    gptCtx->tFrameBufferScale.y = 1.0f;
    gptCtx->iWantCaptureKeyboardNextFrame = -1;
    gptCtx->iWantCaptureMouseNextFrame = -1;
    pl_ui_set_dark_theme();
    gptCtx->ulMainMenuBarFrame = UINT64_MAX;

    if(gptConsole->add_bool_variable)
    {
        gptConsole->add_toggle_variable("t.UiDebug", &gptCtx->bShowDebugTool, "shows ui debug tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
        gptConsole->add_toggle_variable("t.UiStyle", &gptCtx->bShowStyleTool, "shows ui style tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    }
}

void
pl_ui_cleanup(void)
{
    pl_ui_save_settings();
    PL_FREE(gptCtx->pcSettingsFile);
    pl_sb_free(gptCtx->sbtWindowSettings);
    pl_sb_free(gptCtx->sbcSettingsSnapshot);
    pl_sb_free(gptCtx->sbcSettingsSaved);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptCreatedWindows); i++)
    {
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->tStorage.sbtData);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbtChildWindows);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbfAvailableSizeStack);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbfMaxCursorYStack);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbtRowStack);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbtCursorStack);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbtTreeNodeStack);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbtRowTemplateEntries);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbtTempLayoutSort);
        pl_sb_free(gptCtx->sbptCreatedWindows[i]->sbuTempLayoutIndexSort);
        PL_FREE(gptCtx->sbptCreatedWindows[i]->pcName);
        PL_FREE(gptCtx->sbptCreatedWindows[i]);
    }
    pl_sb_free(gptCtx->sbptWindowStack);
    pl_sb_free(gptCtx->sbcTempBuffer);
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbDrawlists);
    pl_sb_free(gptCtx->sbptFocusedWindows);
    pl_sb_free(gptCtx->sbtColorStack);
    pl_sb_free(gptCtx->sbtTabBars);
    pl_sb_free(gptCtx->sbuIdStack);
    pl_sb_free(gptCtx->sbtBeginPopupStack);
    pl_sb_free(gptCtx->sbtOpenPopupStack);
    pl_sb_free(gptCtx->sbtDockNodes);
    gptDraw->return_2d_layer(gptCtx->ptDockLayer);
    gptDraw->return_2d_drawlist(gptCtx->ptDockDrawlist);
    pl_sb_free(gptCtx->sbptCreatedWindows);
    pl_sb_free(gptCtx->tWindows.sbtData);
    pl_sb_free(gptCtx->tInputTextState.sbInitialTextA);
    pl_sb_free(gptCtx->tInputTextState.sbTextA);
    pl_sb_free(gptCtx->tInputTextState.sbTextW);
}

static void
pl__ui_text_filter_split(plUiTextFilterRange* ptFilter, char separator, plUiTextFilterRange** psbtFilters)
{
    plUiTextFilterRange* sbtFilters = *psbtFilters;

    pl_sb_reset(sbtFilters);
    const char* wb = ptFilter->b;
    const char* we = wb;

    while (we < ptFilter->e)
    {
        if (*we == separator)
        {
            pl_sb_push(sbtFilters, ((plUiTextFilterRange){wb, we}));
            wb = we + 1;
        }
        we++;
    }
    if (wb != we)
    {
        pl_sb_push(sbtFilters, ((plUiTextFilterRange){wb, we}));
    }

    *psbtFilters = sbtFilters;
}

static const char*
pl__str_is_tr(const char* haystack, const char* haystack_end, const char* needle, const char* needle_end)
{
    if (!needle_end)
        needle_end = needle + strlen(needle);

    const char un0 = (char)pl_str_to_upper(*needle);
    while ((!haystack_end && *haystack) || (haystack_end && haystack < haystack_end))
    {
        if (pl_str_to_upper(*haystack) == un0)
        {
            const char* b = needle + 1;
            for (const char* a = haystack + 1; b < needle_end; a++, b++)
                if (pl_str_to_upper(*a) != pl_str_to_upper(*b))
                    break;
            if (b == needle_end)
                return haystack;
        }
        haystack++;
    }
    return NULL;
}

void
pl_ui_text_filter_cleanup(plUiTextFilter* ptFilter)
{
    pl_sb_free(ptFilter->sbtFilters);
}

bool
pl_ui_text_filter_active(plUiTextFilter* ptFilter)
{
    return pl_sb_size(ptFilter->sbtFilters) > 0;
}

void
pl_ui_text_filter_build(plUiTextFilter* ptFilter)
{
    pl_sb_reset(ptFilter->sbtFilters);
    plUiTextFilterRange tInputRange = {
        .b = ptFilter->acInputBuffer,
        .e = ptFilter->acInputBuffer + strlen(ptFilter->acInputBuffer)
    };
    pl__ui_text_filter_split(&tInputRange, ',', &ptFilter->sbtFilters);

    ptFilter->uCountGrep = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptFilter->sbtFilters); i++)
    {
        while (ptFilter->sbtFilters[i].b < ptFilter->sbtFilters[i].e && (ptFilter->sbtFilters[i].b[0] == ' ' || ptFilter->sbtFilters[i].b[0] == '\t'))
            ptFilter->sbtFilters[i].b++;
        while (ptFilter->sbtFilters[i].e > ptFilter->sbtFilters[i].b && (ptFilter->sbtFilters[i].e[-1] == ' ' || ptFilter->sbtFilters[i].e[-1] == '\t'))
            ptFilter->sbtFilters[i].e--;
        if(ptFilter->sbtFilters[i].b == ptFilter->sbtFilters[i].e) // empty
            continue;
        if(ptFilter->sbtFilters[i].b[0] != '-')
            ptFilter->uCountGrep += 1;
    }

}

bool
pl_ui_text_filter_pass(plUiTextFilter* ptFilter, const char* text, const char* text_end)
{
    if (pl_sb_size(ptFilter->sbtFilters) == 0)
        return true;

    if (text == NULL)
        text = text_end = "";

    for(uint32_t i = 0; i < pl_sb_size(ptFilter->sbtFilters); i++)
    {
        if (ptFilter->sbtFilters[i].b == ptFilter->sbtFilters[i].e)
            continue;
        if (ptFilter->sbtFilters[i].b[0] == '-')
        {
            // Subtract
            if (pl__str_is_tr(text, text_end, ptFilter->sbtFilters[i].b + 1, ptFilter->sbtFilters[i].e) != NULL)
                return false;
        }
        else
        {
            // Grep
            if (pl__str_is_tr(text, text_end, ptFilter->sbtFilters[i].b, ptFilter->sbtFilters[i].e) != NULL)
                return true;
        }
    }

    // Implicit * grep
    if (ptFilter->uCountGrep == 0)
        return true;

    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] helper windows
//-----------------------------------------------------------------------------

#include "pl_ui_widgets.c"

void
pl_ui_show_debug_window(bool* pbOpen)
{
    // tools
    static bool bShowWindowOuterRect = false;
    static bool bShowWindowOuterClippedRect = false;
    static bool bShowWindowInnerRect = false;
    static bool bShowWindowInnerClipRect = false;

    if(pl_ui_begin_window("Pilot Light UI Metrics/Debugger", pbOpen, 0))
    {

        const float pfRatios[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        pl_ui_separator();

        pl_ui_checkbox("Show Window Inner Rect", &bShowWindowInnerRect);
        pl_ui_checkbox("Show Window Inner Clip Rect", &bShowWindowInnerClipRect);
        pl_ui_checkbox("Show Window Outer Rect", &bShowWindowOuterRect);
        pl_ui_checkbox("Show Window Outer Rect Clipped", &bShowWindowOuterClippedRect);
        pl_ui_separator();

        if(pl_ui_tree_node("Windows", 0))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptFocusedWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = gptCtx->sbptFocusedWindows[uWindowIndex];

                if(pl_ui_tree_node(ptWindow->pcName, 0))
                {
                    pl_ui_text(" - Pos:          (%0.1f, %0.1f)", ptWindow->tPos.x, ptWindow->tPos.y);
                    pl_ui_text(" - Size:         (%0.1f, %0.1f)", ptWindow->tSize.x, ptWindow->tSize.y);
                    pl_ui_text(" - Content Size: (%0.1f, %0.1f)", ptWindow->tContentSize.x, ptWindow->tContentSize.y);
                    pl_ui_text(" - Min Size:     (%0.1f, %0.1f)", ptWindow->tMinSize.x, ptWindow->tMinSize.y);
                    pl_ui_text(" - Scroll:       (%0.1f/%0.1f, %0.1f/%0.1f)", ptWindow->tScroll.x, ptWindow->tScrollMax.x, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_ui_text(" - Focused:      %s", ptWindow == gptCtx->ptNavWindow ? "1" : "0");
                    pl_ui_text(" - Active:       %s", ptWindow == gptCtx->ptActiveWindow ? "1" : "0");
                    pl_ui_text(" - Hovered:      %s", ptWindow == gptCtx->ptHoveredWindow ? "1" : "0");
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
        if(pl_ui_tree_node("Internal State", 0))
        {
            pl_ui_separator_text("Windowing");
            pl_ui_indent(0.0f);
            
            pl_ui_text("Hovered Window: %s", gptCtx->ptHoveredWindow ? gptCtx->ptHoveredWindow->pcName : "NULL");
            pl_ui_text("Moving Window:  %s", gptCtx->ptMovingWindow ? gptCtx->ptMovingWindow->pcName : "NULL");
            pl_ui_text("Sizing Window:  %s", gptCtx->ptSizingWindow ? gptCtx->ptSizingWindow->pcName : "NULL");
            pl_ui_text("Scrolling Window:  %s", gptCtx->ptScrollingWindow ? gptCtx->ptScrollingWindow->pcName : "NULL");
            pl_ui_text("Wheeling Window:  %s", gptCtx->ptWheelingWindow ? gptCtx->ptWheelingWindow->pcName : "NULL");
            pl_ui_unindent(0.0f);
            
            pl_ui_separator_text("Items");
            pl_ui_indent(0.0f);
            pl_ui_text("Active Window: %s", gptCtx->ptActiveWindow ? gptCtx->ptActiveWindow->pcName : "NULL");
            pl_ui_text("Active ID:        %u", gptCtx->uActiveId);
            pl_ui_text("Hovered ID:       %u", gptCtx->uHoveredId);
            pl_ui_unindent(0.0f);

            pl_ui_separator_text("Navigation");
            pl_ui_indent(0.0f);
            pl_ui_text("Nav Window: %s", gptCtx->ptNavWindow ? gptCtx->ptNavWindow->pcName : "NULL"); 
            pl_ui_text("Nav ID:     %u", gptCtx->uNavId);
            pl_ui_unindent(0.0f);
            pl_ui_tree_pop();
        }
        if(pl_ui_tree_node("Input", 0))
        {
            pl_ui_separator_text("Keyboard");
            pl_ui_indent(0.0f);
            pl_ui_text("Keys mods: %s%s%s%s", gptIO->bKeyCtrl ? "CTRL " : "", gptIO->bKeyShift ? "SHIFT " : "", gptIO->bKeyAlt ? "ALT " : "", gptIO->bKeySuper ? "SUPER " : "");
            pl_ui_unindent(0.0f);
            
            pl_ui_separator_text("Mouse State");
            pl_ui_indent(0.0f);
            if (gptIOI->is_mouse_pos_valid(gptIO->_tMousePos))
                pl_ui_text("Mouse pos: (%g, %g)", gptIO->_tMousePos.x, gptIO->_tMousePos.y);
            else
                pl_ui_text("Mouse pos: <INVALID>");
            pl_ui_text("Mouse delta: (%g, %g)", gptIO->_tMouseDelta.x, gptIO->_tMouseDelta.y);
            // int count = IM_ARRAYSIZE(io.MouseDown);
            // pl_text("Mouse down:");     for (int i = 0; i < count; i++) if (IsMouseDown(i)) { SameLine(); Text("b%d (%.02f secs)", i, io.MouseDownDuration[i]); }
            // pl_text("Mouse clicked:");  for (int i = 0; i < count; i++) if (IsMouseClicked(i)) { SameLine(); Text("b%d (%d)", i, io.MouseClickedCount[i]); }
            // pl_text("Mouse released:"); for (int i = 0; i < count; i++) if (IsMouseReleased(i)) { SameLine(); Text("b%d", i); }
            pl_ui_text("Mouse wheel: %.1f", gptIO->_fMouseWheel);
            // Text("MouseStationaryTimer: %.2f", g.MouseStationaryTimer);
            // pl_text("Mouse source: %s", GetMouseSourceName(io.MouseSource));
            pl_ui_unindent(0.0f);

            pl_ui_separator_text("Mouse Wheeling");
            pl_ui_text("Wheeling Window: '%s'", gptCtx->ptWheelingWindow ? gptCtx->ptWheelingWindow->pcName : "NULL");
            // Text("WheelingWindowReleaseTimer: %.2f", gptCtx->WheelingWindowReleaseTimer);
            // Text("WheelingAxisAvg[] = { %.3f, %.3f }, Main Axis: %s", g.WheelingAxisAvg.x, g.WheelingAxisAvg.y, (g.WheelingAxisAvg.x > g.WheelingAxisAvg.y) ? "X" : (g.WheelingAxisAvg.x < g.WheelingAxisAvg.y) ? "Y" : "<none>");
            pl_ui_indent(0.0f);

            pl_ui_unindent(0.0f);

            pl_ui_tree_pop();
        }

        pl_ui_end_window();
    } 

    for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
    {
        const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

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
}

void
pl_ui_show_style_editor_window(bool* pbOpen)
{

    if(pl_ui_begin_window("Pilot Light UI Style", pbOpen, 0))
    {

        pl_ui_layout_static(0.0f, 100.0f, 2);
        if(pl_ui_button("Light"))
        {
            pl_ui_set_light_theme();
            gptCtx->tStyle.fFontSize = gptCtx->tFont->fSize;
        }

        if(pl_ui_button("Dark"))
        {
            pl_ui_set_dark_theme();
            gptCtx->tStyle.fFontSize = gptCtx->tFont->fSize;
        }

        const float pfRatios[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        plUiStyle* ptStyle = &gptCtx->tStyle;
        plUiColorScheme* ptColorScheme = &gptCtx->tColorScheme;

        if(pl_ui_begin_tab_bar("Tabs", 0))
        {
            if(pl_ui_begin_tab("Colors", 0))
            {
                pl_ui_layout_dynamic(0.0f, 1);
                static int iEditingColor = 0;
                static const char* apcColorNames[PL_UI_COLOR_COUNT] = {
                    "TitleActive",
                    "TitleBg",
                    "TitleBgCollapsed",
                    "WindowBg",
                    "WindowBorder",
                    "ChildBg",
                    "Button",
                    "ButtonHovered",
                    "ButtonActive",
                    "Text",
                    "TextDisabled",
                    "ProgressBar",
                    "Checkmark",
                    "FrameBg",
                    "FrameBgHovered",
                    "FrameBgActive",
                    "Header",
                    "HeaderHovered",
                    "HeaderActive",
                    "ScrollbarBg",
                    "ScrollbarHandle",
                    "ScrollbarFrame",
                    "ScrollbarActive",
                    "ScrollbarHovered",
                    "Separator",
                    "Tab",
                    "TabHovered",
                    "TabSelected",
                    "ResizeGrip",
                    "ResizeGripHovered",
                    "ResizeGripActive",
                    "Slider",
                    "SliderHovered",
                    "SliderActive",
                    "PopupBg",
                    "TreeLine"
                };

                for(uint32_t i = 0; i < PL_UI_COLOR_COUNT; i++)
                {
                    // pl_ui_radio_button(apcColorNames[i], &iEditingColor, i);
                    pl_ui_color_edit4(apcColorNames[i], ptColorScheme->atColors[i].d, PL_UI_COLOR_EDIT_FLAGS_NO_LABEL, ptColorScheme->atColors[i].d);
                }

                // pl_ui_layout_dynamic(0.0f, 1);

                // pl_ui_color_editor4("Color", ptColorScheme->atColors[iEditingColor].d, PL_UI_COLOR_EDIT_FLAGS_NO_LABEL, ptColorScheme->atColors[iEditingColor].d);
                pl_ui_end_tab();
            }
            
            if(pl_ui_begin_tab("Sizes", 0))
            {
                pl_ui_vertical_spacing();
                pl_ui_separator_text("Title");

                uint32_t uTempValue = (uint32_t)ptStyle->fTitlePadding;
                if(pl_ui_slider_uint("Title Padding", &uTempValue, 0, 32, 0)) ptStyle->fTitlePadding = (float)uTempValue;

                pl_ui_vertical_spacing();
                pl_ui_separator_text("Window");

                uTempValue = (uint32_t)ptStyle->fWindowHorizontalPadding;
                if(pl_ui_slider_uint("Horizontal Padding## window", &uTempValue, 0, 32, 0)) ptStyle->fWindowHorizontalPadding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fWindowVerticalPadding;
                if(pl_ui_slider_uint("Vertical Padding## window", &uTempValue, 0, 32, 0)) ptStyle->fWindowVerticalPadding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->iWindowBorderSize;
                if(pl_ui_slider_uint("Border Size## window", &uTempValue, 0, 1, 0)) ptStyle->iWindowBorderSize = (int)uTempValue;

                pl_ui_vertical_spacing();
                pl_ui_separator_text("Scrollbar");

                uTempValue = (uint32_t)ptStyle->fScrollbarSize;
                if(pl_ui_slider_uint("Size##scrollbar", &uTempValue, 0, 32, 0)) ptStyle->fScrollbarSize = (float)uTempValue;

                pl_ui_vertical_spacing();
                pl_ui_separator_text("Rounding");
                uTempValue = (uint32_t)ptStyle->fWindowRounding;
                if(pl_ui_slider_uint("Window Rounding", &uTempValue, 0, 12, 0)) ptStyle->fWindowRounding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fChildRounding;
                if(pl_ui_slider_uint("Child Rounding", &uTempValue, 0, 12, 0)) ptStyle->fChildRounding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fFrameRounding;
                if(pl_ui_slider_uint("Frame Rounding", &uTempValue, 0, 12, 0)) ptStyle->fFrameRounding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fScrollbarRounding;
                if(pl_ui_slider_uint("Scrollbar Rounding", &uTempValue, 0, 12, 0)) ptStyle->fScrollbarRounding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fGrabRounding;
                if(pl_ui_slider_uint("Grab Rounding", &uTempValue, 0, 12, 0)) ptStyle->fGrabRounding = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fTabRounding;
                if(pl_ui_slider_uint("Tab Rounding", &uTempValue, 0, 12, 0)) ptStyle->fTabRounding = (float)uTempValue;
                
                pl_ui_vertical_spacing();
                pl_ui_separator_text("Misc");
                uTempValue = (uint32_t)ptStyle->fIndentSize;
                if(pl_ui_slider_uint("Indent", &uTempValue, 0, 32, 0)) ptStyle->fIndentSize = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fSliderSize;
                if(pl_ui_slider_uint("Slider Size", &uTempValue, 3, 32, 0)) ptStyle->fSliderSize = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fTreeLineSize;
                if(pl_ui_slider_uint("Treeline Size", &uTempValue, 0, 10, 0)) ptStyle->fTreeLineSize = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fColorMarkerSize;
                if(pl_ui_slider_uint("Color Marker Size", &uTempValue, 0, 10, 0)) ptStyle->fColorMarkerSize = (float)uTempValue;
                uTempValue = (uint32_t)ptStyle->fFontSize;
                if(pl_ui_slider_uint("Font Size", &uTempValue, 13, 48, 0)) ptStyle->fFontSize = (float)uTempValue;

                pl_ui_vertical_spacing();
                pl_ui_separator_text("Widgets");
                pl_ui_slider_float("Separator Text Size", &ptStyle->fSeparatorTextLineSize, 0.0f, 10.0f, 0); 
                pl_ui_slider_float("Separator Text Alignment x", &ptStyle->tSeparatorTextAlignment.x, 0.0f, 1.0f, 0); 
                pl_ui_slider_float("Separator Text Alignment y", &ptStyle->tSeparatorTextAlignment.y, 0.0f, 1.0f, 0); 
                pl_ui_slider_float("Separator Text Pad x", &ptStyle->tSeparatorTextPadding.x, 0.0f, 40.0f, 0); 
                pl_ui_slider_float("Separator Text Pad y", &ptStyle->tSeparatorTextPadding.y, 0.0f, 40.0f, 0); 
                pl_ui_end_tab();
            }
            pl_ui_end_tab_bar();
        }     
        pl_ui_end_window();
    }  
}

void
pl_ui_end_frame(void)
{
    if(gptCtx->bShowStyleTool)
    {
        pl_ui_show_style_editor_window(&gptCtx->bShowStyleTool);
    }

    if(gptCtx->bShowDebugTool)
    {
        pl_ui_show_debug_window(&gptCtx->bShowDebugTool);
    }

    pl__dock_drag();

    // submit windows in display order
    pl_sb_reset(gptCtx->sbptWindows);
    for(uint32_t uPass = 0; uPass < 3; uPass++)
    {
        for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
        {
            plUiWindow* ptRootWindow = gptCtx->sbptFocusedWindows[i];
            const uint32_t uWindowPass = (ptRootWindow->tFlags & PL_UI_WINDOW_FLAGS_POPUP_WINDOW) ? 2 : ((ptRootWindow->tFlags & PL_UI_WINDOW_FLAGS_MAIN_MENU_BAR) ? 1 : 0);
            // Main menu bar overlays ordinary windows; popups overlay the bar.
            if(uWindowPass != uPass)
                continue;
            if(ptRootWindow->bActive && !pl__dock_host(ptRootWindow))
            {
                pl__submit_window(ptRootWindow);
                if(ptRootWindow->tFlags & PL_UI_WINDOW_FLAGS_DOCK_HOST)
                {
                    for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptCreatedWindows); j++)
                    {
                        plUiWindow* ptMember = gptCtx->sbptCreatedWindows[j];
                        if(ptMember->bActive && pl__dock_host(ptMember) == ptRootWindow)
                            pl__submit_window(ptMember);
                    }
                }
            }
        }
    }


    // draw submission
    gptDraw->submit_2d_layer(gptCtx->ptDockLayer);
    pl_sb_push(gptCtx->sbDrawlists, gptCtx->ptDockDrawlist);
    // gptDraw->submit_2d_layer(gptCtx->ptBgLayer);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        if(gptCtx->sbptWindows[i]->uHideFrames == 0)
        {
            gptDraw->submit_2d_layer(gptCtx->sbptWindows[i]->ptBgLayer);
            gptDraw->submit_2d_layer(gptCtx->sbptWindows[i]->ptFgLayer);
            pl_sb_push(gptCtx->sbDrawlists, gptCtx->sbptWindows[i]->ptDrawlist);
        }
        else
        {
            gptCtx->sbptWindows[i]->uHideFrames--;
        }
    }
    gptDraw->submit_2d_layer(gptCtx->tTooltipWindow.ptBgLayer);
    gptDraw->submit_2d_layer(gptCtx->tTooltipWindow.ptFgLayer);
    // gptDraw->submit_2d_layer(gptCtx->ptFgLayer);
    gptDraw->submit_2d_layer(gptCtx->ptDebugLayer);
    pl_sb_push(gptCtx->sbDrawlists, gptCtx->tTooltipWindow.ptDrawlist);
    pl_sb_push(gptCtx->sbDrawlists, gptCtx->ptDebugDrawlist);

    const plVec2 tMousePos = gptIOI->get_mouse_pos();

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
        if(ptWindow->uHideFrames == 0 && pl_rect_contains_point(&ptWindow->tOuterRectClipped, gptIO->_tMousePos))
        {
            gptCtx->ptHoveredWindow = ptWindow;

            // scrolling
            if(pl__popup_allows_window(ptWindow) && !(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE) && gptIOI->get_mouse_wheel() != 0.0f)
                gptCtx->ptWheelingWindow = ptWindow;

            float fTitleBarHeight = ptWindow->tTempData.fTitleBarHeight;
            const plRect tTitleBarHitRegion = {
                .tMin = {ptWindow->tPos.x + 2.0f, ptWindow->tPos.y + 2.0f},
                .tMax = {ptWindow->tPos.x + ptWindow->tSize.x - 2.0f, ptWindow->tPos.y + fTitleBarHeight}
            };

            // check if window is activated
            if(pl__popup_allows_window(ptWindow) && !gptCtx->uDockSplitterActive && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            {

                bRequestFocus = true;
                gptCtx->ptMovingWindow = NULL;
                pl__set_nav_window(ptWindow);

                // check if window titlebar is clicked
                if(!ptWindow->uDockId && !(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) && gptIOI->is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                    gptCtx->ptMovingWindow = ptWindow;
                else if(ptWindow->uDockId && !gptCtx->ptDockDragWindow && !gptCtx->uActiveId && tMousePos.x >= ptWindow->tTempData.fDockTabsEndX && tMousePos.x < ptWindow->tPos.x + ptWindow->tSize.x - 32.0f && gptIOI->is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                    gptCtx->ptMovingWindow = pl__dock_host(ptWindow);

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
        gptCtx->ptMovingWindow->tPos.y = pl_maxf(gptCtx->ptMovingWindow->tPos.y, gptCtx->fMainMenuBarHeight);
        gptCtx->ptMovingWindow->tPos.y = pl_minf(gptCtx->ptMovingWindow->tPos.y, gptIO->tMainViewportSize.y - 50.0f);

        gptIOI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    pl__dock_prune_empty();
    pl__dock_restore_single_windows();

    // A selected tab may not be submitted by the application this frame. Select
    // another live member for the next frame, without depending on begin order.
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtDockNodes); i++)
    {
        plUiDockNode* ptNode = &gptCtx->sbtDockNodes[i];
        plUiWindow* ptSelected = pl__get_ptr(&gptCtx->tWindows, ptNode->uSelectedWindow);
        if(!ptNode->auChildren[0] && (!ptSelected || ptSelected->uDockId != ptNode->uId || ptSelected->uLastFrameActive != gptIO->ulFrameCount))
        {
            for(uint32_t j = 0; j < pl_sb_size(gptCtx->sbptCreatedWindows); j++)
            {
                plUiWindow* ptMember = gptCtx->sbptCreatedWindows[j];
                if(ptMember->uDockId == ptNode->uId && ptMember->uLastFrameActive == gptIO->ulFrameCount)
                {
                    ptNode->uSelectedWindow = ptMember->uId;
                    break;
                }
            }
        }
    }
    gptCtx->fSettingsSaveTimer += gptIO->fDeltaTime;
    if(gptCtx->fSettingsSaveTimer >= 1.0f)
    {
        pl_ui_save_settings();
        gptCtx->fSettingsSaveTimer = 0.0f;
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
        // else if(!gptIO->_abMouseDown[i])
        // {
        //     gptCtx->abMouseOwned[i] = gptCtx->ptHoveredWindow != NULL;
        // }
        else if(gptIO->_abMouseReleased[i])
        {
            gptCtx->abMouseOwned[i] = false;
        }
    }
}

static void
pl__ui_color_tooltip(const char* pcLabel, float* afColRGBA, plUiColorEditFlags tFlags)
{
    int cr = PL_F32_TO_INT8_SAT(afColRGBA[0]);
    int cg = PL_F32_TO_INT8_SAT(afColRGBA[1]);
    int cb = PL_F32_TO_INT8_SAT(afColRGBA[2]);
    int ca = PL_F32_TO_INT8_SAT(tFlags & PL_UI_COLOR_EDIT_FLAGS_NO_ALPHA ? 1.0f : afColRGBA[3]);

    pl_ui_begin_tooltip();
    pl_ui_layout_static(0.0f, 300, 1);
    pl_ui_text(pcLabel);
    pl_ui_separator();
    if(tFlags & PL_UI_COLOR_EDIT_FLAGS_NO_ALPHA)
    {
        pl_ui_text("#%02X%02X%02X", cr, cg, cb);
        pl_ui_text("#R:%d, G:%d, B:%d", cr, cg, cb);
        pl_ui_text("(%.3f, %.3f, %.3f)", afColRGBA[0], afColRGBA[1], afColRGBA[2]);
    }
    else
    {
        pl_ui_text("#%02X%02X%02X%02X", cr, cg, cb, ca);
        pl_ui_text("#R:%d, G:%d, B:%d, A:%d", cr, cg, cb, ca);
        pl_ui_text("(%.3f, %.3f, %.3f, %.3f)", afColRGBA[0], afColRGBA[1], afColRGBA[2], afColRGBA[3]);
    }
    pl_ui_end_tooltip();
}

static bool
pl__ui_button_ex(const char* pcText, const plVec2* ptSize, const plVec2* ptPos)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    bool bPressed = false;
    if(pl__ui_should_render(ptPos, ptSize))
    {
        const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
        plRect tBoundingBox = pl_calculate_rect(*ptPos, *ptSize);
        const plRect* ptClipRect = gptDraw->get_clip_rect(ptWindow->ptDrawlist);
        tBoundingBox = pl_rect_clip_full(&tBoundingBox, ptClipRect);

        bool bHovered = false;
        bool bHeld = false;
        bPressed = pl__button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_rounded_filled(ptWindow->ptFgLayer, *ptPos, tBoundingBox.tMax, gptCtx->tStyle.fFrameRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tButtonActiveCol)});
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_rounded_filled(ptWindow->ptFgLayer, *ptPos, tBoundingBox.tMax, gptCtx->tStyle.fFrameRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tButtonHoveredCol)});
        else                                 gptDraw->add_rect_rounded_filled(ptWindow->ptFgLayer, *ptPos, tBoundingBox.tMax, gptCtx->tStyle.fFrameRounding, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(gptCtx->tColorScheme.tButtonCol)});

        const plVec2 tTextSize = pl__calculate_text_size(gptCtx->tFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
        const plRect tTextBounding = gptDraw->calculate_text_bb(*ptPos, pcText,
            (plDrawTextOptions){
                .ptFont = gptCtx->tFont,
                .fSize = gptCtx->tStyle.fFontSize,
                .pcTextEnd = pl__find_renderered_text_end(pcText, NULL),
                .fWrap = -1.0f
        });
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        plVec2 tTextStartPos = {
            .x = ptPos->x,
            .y = ptPos->y + ptPos->y + ptSize->y / 2.0f - tTextActualCenter.y
        };

        if((pl_rect_width(&tBoundingBox) < tTextSize.x)) // clipping, so start at beginning of widget
            tTextStartPos.x += gptCtx->tStyle.tFramePadding.x;
        else // not clipping, so center on widget
            tTextStartPos.x += ptPos->x + ptSize->x / 2.0f - tTextActualCenter.x;

        pl__add_clipped_text(ptWindow->ptFgLayer, gptCtx->tFont, gptCtx->tStyle.fFontSize, tTextStartPos, tBoundingBox.tMin, tBoundingBox.tMax, PL_COLOR_32_VEC4(gptCtx->tColorScheme.tTextCol), pcText, 0.0f);
        pl__add_widget(uHash);
    }
    return bPressed;
}


//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_ui_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plUiI tApi = {
        .initialize                              = pl_ui_initialize,
        .cleanup                                 = pl_ui_cleanup,
        .get_draw_lists                          = pl_ui_get_draw_lists,
        .get_debug_draw_list                     = pl_ui_get_debug_draw_list,
        .get_debug_draw_layer                    = pl_ui_get_debug_draw_layer,
        .begin_frame                             = pl_ui_begin_frame,
        .end_frame                               = pl_ui_end_frame,
        .wants_keyboard_capture                  = pl_ui_wants_keyboard_capture,
        .wants_text_input                        = pl_ui_wants_text_input,
        .wants_mouse_capture                     = pl_ui_wants_mouse_capture,
        .show_debug_window                       = pl_ui_show_debug_window,
        .show_style_editor_window                = pl_ui_show_style_editor_window,
        .set_dark_theme                          = pl_ui_set_dark_theme,
        .set_light_theme                         = pl_ui_set_light_theme,
        .push_theme_color                        = pl_ui_push_theme_color,
        .pop_theme_color                         = pl_ui_pop_theme_color,
        .set_default_font                        = pl_ui_set_default_font,
        .get_default_font                        = pl_ui_get_default_font,
        .begin_window                            = pl_ui_begin_window,
        .end_window                              = pl_ui_end_window,
        .get_window_fg_drawlayer                 = pl_ui_get_window_fg_drawlayer,
        .get_window_bg_drawlayer                 = pl_ui_get_window_bg_drawlayer,
        .get_cursor_pos                          = pl_ui_get_cursor_pos,
        .begin_child                             = pl_ui_begin_child,
        .end_child                               = pl_ui_end_child,
        .is_popup_open                           = pl_ui_is_popup_open,
        .is_popup_open_id                        = pl_ui_is_popup_open_id,
        .get_id                                  = pl_ui_get_id,
        .close_current_popup                     = pl_ui_close_current_popup,
        .open_popup                              = pl_ui_open_popup,
        .open_popup_id                           = pl_ui_open_popup_id,
        .begin_popup                             = pl_ui_begin_popup,
        .end_popup                               = pl_ui_end_popup,
        .begin_popup_id                          = pl_ui_begin_popup_id,
        .begin_menu_bar                          = pl_ui_begin_menu_bar,
        .end_menu_bar                            = pl_ui_end_menu_bar,
        .dock_space                              = pl_ui_dock_space,
        .dock_split                              = pl_ui_dock_split,
        .set_next_window_dock                    = pl_ui_set_next_window_dock,
        .get_window_dock                         = pl_ui_get_window_dock,
        .begin_tooltip                           = pl_ui_begin_tooltip,
        .end_tooltip                             = pl_ui_end_tooltip,
        .is_window_appearing                     = pl_ui_is_window_appearing,
        .get_window_pos                          = pl_ui_get_window_pos,
        .get_window_size                         = pl_ui_get_window_size,
        .get_window_scroll                       = pl_ui_get_window_scroll,
        .get_window_scroll_max                   = pl_ui_get_window_scroll_max,
        .set_window_scroll                       = pl_ui_set_window_scroll,
        .set_next_window_pos                     = pl_ui_set_next_window_pos,
        .set_next_window_size                    = pl_ui_set_next_window_size,
        .set_next_window_collapse                = pl_ui_set_next_window_collapse,
        .button                                  = pl_ui_button,
        .selectable                              = pl_ui_selectable,
        .checkbox                                = pl_ui_checkbox,
        .checkbox_flags                          = pl_ui_checkbox_flags,
        .radio_button                            = pl_ui_radio_button,
        .image                                   = pl_ui_image,
        .image_ex                                = pl_ui_image_ex,
        .image_button                            = pl_ui_image_button,
        .image_button_ex                         = pl_ui_image_button_ex,
        .invisible_button                        = pl_ui_invisible_button,
        .dummy                                   = pl_ui_dummy,
        .progress_bar                            = pl_ui_progress_bar,
        .text                                    = pl_ui_text,
        .text_v                                  = pl_ui_text_v,
        .color_text                              = pl_ui_color_text,
        .color_text_v                            = pl_ui_color_text_v,
        .labeled_text                            = pl_ui_labeled_text,
        .labeled_text_v                          = pl_ui_labeled_text_v,
        .begin_combo                             = pl_ui_begin_combo,
        .end_combo                               = pl_ui_end_combo,
        .input_text                              = pl_ui_input_text,
        .input_text_hint                         = pl_ui_input_text_hint,
        .input_float                             = pl_ui_input_float,
        .input_float_ex                          = pl_ui_input_float_ex,
        .input_float2                            = pl_ui_input_float2,
        .input_float3                            = pl_ui_input_float3,
        .input_float4                            = pl_ui_input_float4,
        .input_int                               = pl_ui_input_int,
        .input_int_ex                            = pl_ui_input_int_ex,
        .input_int2                              = pl_ui_input_int2,
        .input_int3                              = pl_ui_input_int3,
        .input_int4                              = pl_ui_input_int4,
        .input_uint                              = pl_ui_input_uint,
        .input_uint_ex                           = pl_ui_input_uint_ex,
        .input_uint2                             = pl_ui_input_uint2,
        .input_uint3                             = pl_ui_input_uint3,
        .input_uint4                             = pl_ui_input_uint4,
        .slider_float                            = pl_ui_slider_float,
        .slider_float_f                          = pl_ui_slider_float_f,
        .slider_int                              = pl_ui_slider_int,
        .slider_int_f                            = pl_ui_slider_int_f,
        .slider_uint                             = pl_ui_slider_uint,
        .slider_uint_f                           = pl_ui_slider_uint_f,
        .slider_angle                            = pl_ui_slider_angle,
        .drag_float                              = pl_ui_drag_float,
        .drag_float_f                            = pl_ui_drag_float_f,
        .drag_int                                = pl_ui_drag_int,
        .drag_int_f                              = pl_ui_drag_int_f,
        .drag_int2                               = pl_ui_drag_int2,
        .drag_int2_f                             = pl_ui_drag_int2_f,
        .drag_int3                               = pl_ui_drag_int3,
        .drag_int3_f                             = pl_ui_drag_int3_f,
        .drag_int4                               = pl_ui_drag_int4,
        .drag_int4_f                             = pl_ui_drag_int4_f,
        .drag_uint                               = pl_ui_drag_uint,
        .drag_uint_f                             = pl_ui_drag_uint_f,
        .drag_uint2                              = pl_ui_drag_uint2,
        .drag_uint2_f                            = pl_ui_drag_uint2_f,
        .drag_uint3                              = pl_ui_drag_uint3,
        .drag_uint3_f                            = pl_ui_drag_uint3_f,
        .drag_uint4                              = pl_ui_drag_uint4,
        .drag_uint4_f                            = pl_ui_drag_uint4_f,
        .slider_float2_f                         = pl_ui_slider_float2_f,
        .slider_float2                           = pl_ui_slider_float2,
        .slider_float3_f                         = pl_ui_slider_float3_f,
        .slider_float3                           = pl_ui_slider_float3,
        .slider_float4_f                         = pl_ui_slider_float4_f,
        .slider_float4                           = pl_ui_slider_float4,
        .slider_int2_f                           = pl_ui_slider_int2_f,
        .slider_int2                             = pl_ui_slider_int2,
        .slider_int3_f                           = pl_ui_slider_int3_f,
        .slider_int3                             = pl_ui_slider_int3,
        .slider_int4_f                           = pl_ui_slider_int4_f,
        .slider_int4                             = pl_ui_slider_int4,
        .slider_uint2_f                          = pl_ui_slider_uint2_f,
        .slider_uint2                            = pl_ui_slider_uint2,
        .slider_uint3_f                          = pl_ui_slider_uint3_f,
        .slider_uint3                            = pl_ui_slider_uint3,
        .slider_uint4_f                          = pl_ui_slider_uint4_f,
        .slider_uint4                            = pl_ui_slider_uint4,
        .drag_float2_f                           = pl_ui_drag_float2_f,
        .drag_float2                             = pl_ui_drag_float2,
        .drag_float3_f                           = pl_ui_drag_float3_f,
        .drag_float3                             = pl_ui_drag_float3,
        .drag_float4_f                           = pl_ui_drag_float4_f,
        .drag_float4                             = pl_ui_drag_float4,
        .color_picker3                           = pl_ui_color_picker3,
        .color_picker4                           = pl_ui_color_picker4,
        .color_edit3                             = pl_ui_color_edit3,
        .color_edit4                             = pl_ui_color_edit4,
        .color_button                            = pl_ui_color_button,
        .collapsing_header                       = pl_ui_collapsing_header,
        .tree_node                               = pl_ui_tree_node,
        .tree_node_f                             = pl_ui_tree_node_f,
        .tree_node_v                             = pl_ui_tree_node_v,
        .tree_pop                                = pl_ui_tree_pop,
        .begin_menu                              = pl_ui_begin_menu,
        .end_menu                                = pl_ui_end_menu,
        .menu_item                               = pl_ui_menu_item,
        .menu_item_toggle                        = pl_ui_menu_item_toggle,
        .begin_tab_bar                           = pl_ui_begin_tab_bar,
        .end_tab_bar                             = pl_ui_end_tab_bar,
        .begin_tab                               = pl_ui_begin_tab,
        .end_tab                                 = pl_ui_end_tab,
        .separator                               = pl_ui_separator,
        .separator_text                          = pl_ui_separator_text,
        .vertical_spacing                        = pl_ui_vertical_spacing,
        .indent                                  = pl_ui_indent,
        .unindent                                = pl_ui_unindent,
        .step_clipper                            = pl_ui_step_clipper,
        .stash_layout                            = pl_ui_stash_layout,
        .apply_layout                            = pl_ui_apply_layout,
        .layout_dynamic                          = pl_ui_layout_dynamic,
        .layout_static                           = pl_ui_layout_static,
        .layout_row_begin                        = pl_ui_layout_row_begin,
        .layout_row_push                         = pl_ui_layout_row_push,
        .layout_row_end                          = pl_ui_layout_row_end,
        .layout_row                              = pl_ui_layout_row,
        .layout_template_begin                   = pl_ui_layout_template_begin,
        .layout_template_push_dynamic            = pl_ui_layout_template_push_dynamic,
        .layout_template_push_variable           = pl_ui_layout_template_push_variable,
        .layout_template_push_static             = pl_ui_layout_template_push_static,
        .layout_template_end                     = pl_ui_layout_template_end,
        .layout_space_begin                      = pl_ui_layout_space_begin,
        .layout_space_push                       = pl_ui_layout_space_push,
        .layout_space_end                        = pl_ui_layout_space_end,
        .was_last_item_hovered                   = pl_ui_was_last_item_hovered,
        .was_last_item_active                    = pl_ui_was_last_item_active,
        .push_id_string                          = pl_ui_push_id_string,
        .push_id_pointer                         = pl_ui_push_id_pointer,
        .push_id_uint                            = pl_ui_push_id_uint,
        .pop_id                                  = pl_ui_pop_id,
        .text_filter_build                       = pl_ui_text_filter_build,
        .text_filter_pass                        = pl_ui_text_filter_pass,
        .text_filter_cleanup                     = pl_ui_text_filter_cleanup,
        .text_filter_active                      = pl_ui_text_filter_active,
        .set_keyboard_focus_last_item            = pl_ui_set_keyboard_focus_last_item,
        .set_wants_mouse_capture_next_frame      = pl_ui_set_wants_mouse_capture_next_frame,
        .set_wants_keyboard_capture_next_frame   = pl_ui_set_wants_keyboard_capture_next_frame,
        .set_settings_file                       = pl_ui_set_settings_file,
        .save_settings                           = pl_ui_save_settings,
        .begin_main_menu_bar                     = pl_ui_begin_main_menu_bar,
        .end_main_menu_bar                       = pl_ui_end_main_menu_bar,
    };
    pl_set_api(ptApiRegistry, plUiI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptDraw = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptIOI = pl_get_api_latest(ptApiRegistry, plIOI);
    gptConsole = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptFile = pl_get_api_latest(ptApiRegistry, plFileI);
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

void
pl_unload_ui_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plUiI* ptApi = pl_get_api_latest(ptApiRegistry, plUiI);
    ptApiRegistry->remove_api(ptApi);
    gptCtx = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

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
