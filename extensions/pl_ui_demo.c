#include "pl_ui_internal.h"

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
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerRect.tMin, ptWindow->tInnerRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f, 0.0f, 0);

                if(bShowWindowInnerClipRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerClipRect.tMin, ptWindow->tInnerClipRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f, 0.0f, 0);

                if(bShowWindowOuterRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f, 0.0f, 0);

                if(bShowWindowOuterClippedRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRectClipped.tMin, ptWindow->tOuterRectClipped.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f, 0.0f, 0);
            }
        }
        
        pl_separator();

        if(pl_tree_node("Windows"))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptFocusedWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = gptCtx->sbptFocusedWindows[uWindowIndex];

                if(pl_tree_node(ptWindow->pcName))
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
        if(pl_tree_node("Internal State"))
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

        if(pl_begin_tab_bar("Tabs"))
        {
            if(pl_begin_tab("Colors"))
            { 
                pl_end_tab();
            }
            
            if(pl_begin_tab("Sizes"))
            {
                pl_vertical_spacing();
                pl_separator_text("Title");
                pl_slider_float("Title Padding", &ptStyle->fTitlePadding, 0.0f, 32.0f);

                pl_vertical_spacing();
                pl_separator_text("Window");
                pl_slider_float("Horizontal Padding## window", &ptStyle->fWindowHorizontalPadding, 0.0f, 32.0f);
                pl_slider_float("Vertical Padding## window", &ptStyle->fWindowVerticalPadding, 0.0f, 32.0f);

                pl_vertical_spacing();
                pl_separator_text("Scrollbar");
                pl_slider_float("Size##scrollbar", &ptStyle->fScrollbarSize, 0.0f, 32.0f);

                pl_vertical_spacing();
                pl_separator_text("Rounding");
                pl_slider_float("Window Rounding", &ptStyle->fWindowRounding, 0.0f, 12.0f);
                pl_slider_float("Child Rounding", &ptStyle->fChildRounding, 0.0f, 12.0f);
                pl_slider_float("Frame Rounding", &ptStyle->fFrameRounding, 0.0f, 12.0f);
                pl_slider_float("Scrollbar Rounding", &ptStyle->fScrollbarRounding, 0.0f, 12.0f);
                pl_slider_float("Grab Rounding", &ptStyle->fGrabRounding, 0.0f, 12.0f);
                pl_slider_float("Tab Rounding", &ptStyle->fTabRounding, 0.0f, 12.0f);
                
                pl_vertical_spacing();
                pl_separator_text("Misc");
                pl_slider_float("Indent", &ptStyle->fIndentSize, 0.0f, 32.0f); 
                pl_slider_float("Slider Size", &ptStyle->fSliderSize, 3.0f, 32.0f); 
                pl_slider_float("Font Size", &ptStyle->fFontSize, 13.0f, 48.0f); 

                pl_vertical_spacing();
                pl_separator_text("Widgets");
                pl_slider_float("Separator Text Size", &ptStyle->fSeparatorTextLineSize, 0.0f, 10.0f); 
                pl_slider_float("Separator Text Alignment x", &ptStyle->tSeparatorTextAlignment.x, 0.0f, 1.0f); 
                pl_slider_float("Separator Text Alignment y", &ptStyle->tSeparatorTextAlignment.y, 0.0f, 1.0f); 
                pl_slider_float("Separator Text Pad x", &ptStyle->tSeparatorTextPadding.x, 0.0f, 40.0f); 
                pl_slider_float("Separator Text Pad y", &ptStyle->tSeparatorTextPadding.y, 0.0f, 40.0f); 
                pl_end_tab();
            }
            pl_end_tab_bar();
        }     
        pl_end_window();
    }  
}

void
pl_show_demo_window(bool* pbOpen)
{
    if(pl_begin_window("UI Demo", pbOpen, PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR))
    {

        static const float pfRatios0[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios0);

        pl_text("Pilot Light UI v%s", PL_UI_EXT_VERSION);

        if(pl_collapsing_header("Help"))
        {
            pl_text("Under construction");
            pl_end_collapsing_header();
        }
    
        if(pl_collapsing_header("Window Options"))
        {
            pl_text("Under construction");
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("Widgets"))
        {
            if(pl_tree_node("Basic"))
            {

                pl_layout_static(0.0f, 100, 2);
                pl_button("Button");
                pl_checkbox("Checkbox", NULL);

                pl_layout_dynamic(0.0f, 2);
                pl_button("Button");
                pl_checkbox("Checkbox", NULL);

                pl_layout_dynamic(0.0f, 1);
                static char buff[64] = {'c', 'a', 'a'};
                pl_input_text("label 0", buff, 64);
                static char buff2[64] = {'c', 'c', 'c'};
                pl_input_text_hint("label 1", "hint", buff2, 64);

                static float fValue = 3.14f;
                static int iValue117 = 117;

                pl_input_float("label 2", &fValue, "%0.3f");
                pl_input_int("label 3", &iValue117);

                static int iValue = 0;
                pl_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

                pl_layout_row_push(0.33f);
                pl_radio_button("Option 1", &iValue, 0);

                pl_layout_row_push(0.33f);
                pl_radio_button("Option 2", &iValue, 1);

                pl_layout_row_push(0.34f);
                pl_radio_button("Option 3", &iValue, 2);

                pl_layout_row_end();

                const float pfRatios[] = {1.0f};
                pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                pl_separator();
                pl_labeled_text("Label", "Value");
                static int iValue1 = 0;
                static float fValue1 = 23.0f;
                static float fValue2 = 100.0f;
                static int iValue2 = 3;
                pl_slider_float("float slider 1", &fValue1, 0.0f, 100.0f);
                pl_slider_float("float slider 2", &fValue2, -50.0f, 100.0f);
                pl_slider_int("int slider 1", &iValue1, 0, 10);
                pl_slider_int("int slider 2", &iValue2, -5, 10);
                pl_drag_float("float drag", &fValue2, 1.0f, -100.0f, 100.0f);
                static int aiIntArray[4] = {0};
                pl_input_int2("input int 2", aiIntArray);
                pl_input_int3("input int 3", aiIntArray);
                pl_input_int4("input int 4", aiIntArray);

                static float afFloatArray[4] = {0};
                pl_input_float2("input float 2", afFloatArray, "%0.3f");
                pl_input_float3("input float 3", afFloatArray, "%0.3f");
                pl_input_float4("input float 4", afFloatArray, "%0.3f");

                if(pl_menu_item("Menu item 0", NULL, false, true))
                {
                    printf("menu item 0\n");
                }

                if(pl_menu_item("Menu item selected", "CTRL+M", true, true))
                {
                    printf("menu item selected\n");
                }

                if(pl_menu_item("Menu item disabled", NULL, false, false))
                {
                    printf("menu item disabled\n");
                }

                static bool bMenuSelection = false;
                if(pl_menu_item_toggle("Menu item toggle", NULL, &bMenuSelection, true))
                {
                    printf("menu item toggle\n");
                }

                if(pl_begin_menu("menu (not ready)", true))
                {

                    if(pl_menu_item("Menu item 0", NULL, false, true))
                    {
                        printf("menu item 0\n");
                    }

                    if(pl_menu_item("Menu item selected", "CTRL+M", true, true))
                    {
                        printf("menu item selected\n");
                    }

                    if(pl_menu_item("Menu item disabled", NULL, false, false))
                    {
                        printf("menu item disabled\n");
                    }
                    if(pl_begin_menu("sub menu", true))
                    {

                        if(pl_menu_item("Menu item 0", NULL, false, true))
                        {
                            printf("menu item 0\n");
                        }
                        pl_end_menu();
                    }
                    pl_end_menu();
                }


                static uint32_t uComboSelect = 0;
                static const char* apcCombo[] = {
                    "Tomato",
                    "Onion",
                    "Carrot",
                    "Lettuce",
                    "Fish"
                };
                bool abCombo[5] = {0};
                abCombo[uComboSelect] = true;
                if(pl_begin_combo("Combo", apcCombo[uComboSelect], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < 5; i++)
                    {
                        if(pl_selectable(apcCombo[i], &abCombo[i]))
                        {
                            uComboSelect = i;
                            pl_close_current_popup();
                        }
                    }
                    pl_end_combo();
                }

                const float pfRatios22[] = {200.0f, 120.0f};
                pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios22);
                pl_button("Hover me!");
                if(pl_was_last_item_hovered())
                {
                    pl_begin_tooltip();
                    pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios22);
                    pl_text("I'm a tooltip!");
                    pl_end_tooltip();
                }
                pl_button("Just a button");

                pl_tree_pop();
            }

            if(pl_tree_node("Selectables"))
            {
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                pl_selectable("Selectable 1", &bSelectable0);
                pl_selectable("Selectable 2", &bSelectable1);
                pl_selectable("Selectable 3", &bSelectable2);
                pl_tree_pop();
            }

            if(pl_tree_node("Combo"))
            {
                plUiComboFlags tComboFlags = PL_UI_COMBO_FLAGS_NONE;

                static bool bComboHeightSmall = false;
                static bool bComboHeightRegular = false;
                static bool bComboHeightLarge = false;
                static bool bComboNoArrow = false;

                pl_checkbox("PL_UI_COMBO_FLAGS_HEIGHT_SMALL", &bComboHeightSmall);
                pl_checkbox("PL_UI_COMBO_FLAGS_HEIGHT_REGULAR", &bComboHeightRegular);
                pl_checkbox("PL_UI_COMBO_FLAGS_HEIGHT_LARGE", &bComboHeightLarge);
                pl_checkbox("PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON", &bComboNoArrow);

                if(bComboHeightSmall)   tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_SMALL;
                if(bComboHeightRegular) tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_REGULAR;
                if(bComboHeightLarge)   tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_LARGE;
                if(bComboNoArrow)       tComboFlags |= PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON;

                static uint32_t uComboSelect = 0;
                static const char* apcCombo[] = {
                    "Tomato",
                    "Onion",
                    "Carrot",
                    "Lettuce",
                    "Fish",
                    "Beef",
                    "Chicken",
                    "Cereal",
                    "Wheat",
                    "Cane",
                };
                bool abCombo[10] = {0};
                abCombo[uComboSelect] = true;
                if(pl_begin_combo("Combo", apcCombo[uComboSelect], tComboFlags))
                {
                    for(uint32_t i = 0; i < 10; i++)
                    {
                        if(pl_selectable(apcCombo[i], &abCombo[i]))
                        {
                            uComboSelect = i;
                            pl_close_current_popup();
                        }
                    }
                    pl_end_combo();
                }
                pl_tree_pop();
            }

            if(pl_tree_node("Plotting"))
            {
                pl_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
                pl_tree_pop();
            }

            if(pl_tree_node("Trees"))
            {
                
                if(pl_tree_node("Root Node"))
                {
                    if(pl_tree_node("Child 1"))
                    {
                        pl_button("Press me");
                        pl_tree_pop();
                    }
                    if(pl_tree_node("Child 2"))
                    {
                        pl_button("Press me");
                        pl_tree_pop();
                    }
                    pl_tree_pop();
                }
                pl_tree_pop();
            }

            if(pl_tree_node("Tabs"))
            {
                if(pl_begin_tab_bar("Tabs1"))
                {
                    if(pl_begin_tab("Tab 0"))
                    {
                        static bool bSelectable0 = false;
                        static bool bSelectable1 = false;
                        static bool bSelectable2 = false;
                        pl_selectable("Selectable 1", &bSelectable0);
                        pl_selectable("Selectable 2", &bSelectable1);
                        pl_selectable("Selectable 3", &bSelectable2);
                        pl_end_tab();
                    }

                    if(pl_begin_tab("Tab 1"))
                    {
                        static int iValue = 0;
                        pl_radio_button("Option 1", &iValue, 0);
                        pl_radio_button("Option 2", &iValue, 1);
                        pl_radio_button("Option 3", &iValue, 2);
                        pl_end_tab();
                    }

                    if(pl_begin_tab("Tab 2"))
                    {
                        if(pl_begin_child("CHILD2"))
                        {
                            const float pfRatios3[] = {600.0f};
                            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                            for(uint32_t i = 0; i < 25; i++)
                                pl_text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
                            pl_end_child();
                        }
                        
                        pl_end_tab();
                    }
                    pl_end_tab_bar();
                }
                pl_tree_pop();
            }
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("Scrolling"))
        {
            const float pfRatios2[] = {0.5f, 0.50f};
            const float pfRatios3[] = {600.0f};

            pl_layout_static(0.0f, 100, 1);
            static bool bUseClipper = true;
            pl_checkbox("Use Clipper", &bUseClipper);
            
            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2, pfRatios2);
            if(pl_begin_child("CHILD"))
            {

                pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);


                if(bUseClipper)
                {
                    plUiClipper tClipper = {1000000};
                    while(pl_step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            pl_text("%u Label", i);
                            pl_text("%u Value", i);
                        } 
                    }
                }
                else
                {
                    for(uint32_t i = 0; i < 1000000; i++)
                    {
                            pl_text("%u Label", i);
                            pl_text("%u Value", i);
                    }
                }


                pl_end_child();
            }
            

            if(pl_begin_child("CHILD2"))
            {
                pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                for(uint32_t i = 0; i < 25; i++)
                    pl_text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");

                pl_end_child();
            }
            

            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("Layout Systems"))
        {
            pl_text("General Notes");
            pl_text("  - systems ordered by increasing flexibility");
            pl_separator();

            if(pl_tree_node("System 1 - simple dynamic"))
            {
                static int iWidgetCount = 5;
                static float fWidgetHeight = 0.0f;
                pl_separator_text("Notes");
                pl_text("  - wraps (i.e. will add rows)");
                pl_text("  - evenly spaces widgets based on available space");
                pl_text("  - height of 0.0f sets row height equal to minimum height");
                pl_text("    of maximum height widget");
                pl_vertical_spacing();

                pl_separator_text("Options");
                pl_slider_int("Widget Count", &iWidgetCount, 1, 10);
                pl_slider_float("Height", &fWidgetHeight, 0.0f, 100.0f);
                pl_vertical_spacing();

                pl_separator_text("Example");
                pl_layout_dynamic(fWidgetHeight, (uint32_t)iWidgetCount);
                pl_vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2 - 1; i++)
                {
                    pl_sb_sprintf(gptCtx->sbcTempBuffer, "Button %d", i);
                    pl_button(gptCtx->sbcTempBuffer);
                    pl_sb_reset(gptCtx->sbcTempBuffer);
                }
                pl_tree_pop();
            }

            if(pl_tree_node("System 2 - simple static"))
            {
                static int iWidgetCount = 5;
                static float fWidgetWidth = 100.0f;
                static float fWidgetHeight = 0.0f;
                pl_separator_text("Notes");
                pl_text("  - wraps (i.e. will add rows)");
                pl_text("  - provides each widget with the same specified width");
                pl_text("  - height of 0.0f sets row height equal to minimum height");
                pl_text("    of maximum height widget");
                pl_vertical_spacing();

                pl_separator_text("Options");
                pl_slider_int("Widget Count", &iWidgetCount, 1, 10);
                pl_slider_float("Width", &fWidgetWidth, 50.0f, 500.0f);
                pl_slider_float("Height", &fWidgetHeight, 0.0f, 100.0f);
                pl_vertical_spacing();

                pl_separator_text("Example");
                pl_layout_static(fWidgetHeight, fWidgetWidth, (uint32_t)iWidgetCount);
                pl_vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2 - 1; i++)
                {
                    pl_sb_sprintf(gptCtx->sbcTempBuffer, "Button %d", i);
                    pl_button(gptCtx->sbcTempBuffer);
                    pl_sb_reset(gptCtx->sbcTempBuffer);
                }
                pl_tree_pop();
            }

            if(pl_tree_node("System 3 - single system row"))
            {
                static bool bDynamicRow = false;
                static int iWidgetCount = 2;
                static float afWidgetStaticWidths[4] = {
                    100.0f, 100.0f, 100.0f, 100.0f
                };
                static float afWidgetDynamicWidths[4] = {
                    0.25f, 0.25f, 0.25f, 0.25f
                };

                static float fWidgetHeight = 0.0f;

                pl_separator_text("Notes");
                pl_text("  - does not wrap (i.e. will not add rows)");
                pl_text("  - allows user to change widget widths individually");
                pl_text("  - widths interpreted as ratios of available width when");
                pl_text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                pl_text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                pl_text("  - height of 0.0f sets row height equal to minimum height");
                pl_text("    of maximum height widget");
                pl_vertical_spacing();

                pl_separator_text("Options");
                pl_checkbox("Dynamic", &bDynamicRow);
                pl_slider_int("Widget Count", &iWidgetCount, 1, 4);
                pl_slider_float("Height", &fWidgetHeight, 0.0f, 100.0f);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        pl_push_id_uint((uint32_t)i);
                        pl_slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f);
                        pl_pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        pl_push_id_uint((uint32_t)i);
                        pl_slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f);
                        pl_pop_id();
                    }
                }
                pl_vertical_spacing();

                pl_separator_text("Example");
                pl_layout_row_begin(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount);
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                for(int i = 0; i < iWidgetCount; i++)
                {
                    pl_layout_row_push(afWidgetWidths[i]);
                    pl_sb_sprintf(gptCtx->sbcTempBuffer, "Button %d", i);
                    pl_button(gptCtx->sbcTempBuffer);
                    pl_sb_reset(gptCtx->sbcTempBuffer);
                }
                pl_layout_row_end();
                pl_vertical_spacing();
                pl_tree_pop();
            }

            if(pl_tree_node("System 4 - single system row (array form)"))
            {
                static bool bDynamicRow = false;
                static int iWidgetCount = 2;
                static float afWidgetStaticWidths[4] = {
                    100.0f, 100.0f, 100.0f, 100.0f
                };
                static float afWidgetDynamicWidths[4] = {
                    0.25f, 0.25f, 0.25f, 0.25f
                };

                static float fWidgetHeight = 0.0f;

                pl_separator_text("Notes");
                pl_text("  - same as System 3 but array form");
                pl_text("  - wraps (i.e. will add rows)");
                pl_text("  - allows user to change widget widths individually");
                pl_text("  - widths interpreted as ratios of available width when");
                pl_text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                pl_text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                pl_text("  - height of 0.0f sets row height equal to minimum height");
                pl_text("    of maximum height widget");
                pl_vertical_spacing();

                pl_separator_text("Options");
                pl_checkbox("Dynamic", &bDynamicRow);
                pl_slider_int("Widget Count", &iWidgetCount, 1, 4);
                pl_slider_float("Height", &fWidgetHeight, 0.0f, 100.0f);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        pl_push_id_uint((uint32_t)i);
                        pl_slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f);
                        pl_pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        pl_push_id_uint((uint32_t)i);
                        pl_slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f);
                        pl_pop_id();
                    }
                }
                pl_vertical_spacing();

                pl_separator_text("Example");
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                pl_layout_row(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount, afWidgetWidths);
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(gptCtx->sbcTempBuffer, "Button %d", i);
                    pl_button(gptCtx->sbcTempBuffer);
                    pl_sb_reset(gptCtx->sbcTempBuffer);
                }
                pl_vertical_spacing();
                pl_tree_pop();
            }

            if(pl_tree_node("System 5 - template"))
            {
                static int iWidgetCount = 6;
                static float fWidgetHeight = 0.0f;

                pl_separator_text("Notes");
                pl_text("  - most complex and second most flexible system");
                pl_text("  - wraps (i.e. will add rows)");
                pl_text("  - allows user to change widget systems individually");
                pl_text("    - dynamic: changes based on available space");
                pl_text("    - variable: same as dynamic but minimum width specified by user");
                pl_text("    - static: pixel width explicitely specified by user");
                pl_text("  - height of 0.0f sets row height equal to minimum height");
                pl_text("    of maximum height widget");
                pl_vertical_spacing();

                pl_separator_text("Options");
                pl_slider_float("Height", &fWidgetHeight, 0.0f, 100.0f);
                pl_vertical_spacing();

                pl_separator_text("Example 0");

                pl_layout_template_begin(fWidgetHeight);
                pl_layout_template_push_dynamic();
                pl_layout_template_push_variable(150.0f);
                pl_layout_template_push_static(150.0f);
                pl_layout_template_end();
                pl_button("dynamic##0");
                pl_button("variable 150.0f##0");
                pl_button("static 150.0f##0");
                pl_checkbox("dynamic##1", NULL);
                pl_checkbox("variable 150.0f##1", NULL);
                pl_checkbox("static 150.0f##1", NULL);
                pl_vertical_spacing();

                pl_layout_dynamic(0.0f, 1);
                pl_separator_text("Example 1");
                pl_layout_template_begin(fWidgetHeight);
                pl_layout_template_push_static(150.0f);
                pl_layout_template_push_variable(150.0f);
                pl_layout_template_push_dynamic();
                pl_layout_template_end();
                pl_button("static 150.0f##2");
                pl_button("variable 150.0f##2");
                pl_button("dynamic##2");
                pl_checkbox("static 150.0f##3", NULL);
                pl_checkbox("variable 150.0f##3", NULL);
                pl_checkbox("dynamic##3", NULL);

                pl_layout_dynamic(0.0f, 1);
                pl_separator_text("Example 2");
                pl_layout_template_begin(fWidgetHeight);
                pl_layout_template_push_variable(150.0f);
                pl_layout_template_push_variable(300.0f);
                pl_layout_template_push_dynamic();
                pl_layout_template_end();
                pl_button("variable 150.0f##4");
                pl_button("variable 300.0f##4");
                pl_button("dynamic##4");
                pl_checkbox("static 150.0f##5", NULL);
                pl_button("variable 300.0f##5");
                pl_checkbox("dynamic##5", NULL);
                
                pl_vertical_spacing();
                pl_tree_pop();
            }

            if(pl_tree_node("System 6 - space"))
            {
                pl_separator_text("Notes");
                pl_text("  - most flexible system");
                pl_vertical_spacing();

                pl_separator_text("Example - static");

                pl_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

                pl_layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
                pl_button("w100 h100");

                pl_layout_space_push(105.0f, 105.0f, 300.0f, 100.0f);
                pl_button("x105 y105 w300 h100");

                pl_layout_space_end();

                pl_layout_dynamic(0.0f, 1);
                pl_separator_text("Example - dynamic");

                pl_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2);

                pl_layout_space_push(0.0f, 0.0f, 0.5f, 0.5f);
                pl_button("x0 y0 w0.5 h0.5");

                pl_layout_space_push(0.5f, 0.5f, 0.5f, 0.5f);
                pl_button("x0.5 y0.5 w0.5 h0.5");

                pl_layout_space_end();

                pl_tree_pop();
            }
            pl_end_collapsing_header();
        }
    }
    pl_end_window();
}