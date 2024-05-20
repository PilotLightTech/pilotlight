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

        for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
        {
            const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

            if(ptWindow->bActive)
            {
                if(bShowWindowInnerRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerRect.tMin, ptWindow->tInnerRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowInnerClipRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerClipRect.tMin, ptWindow->tInnerClipRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowOuterRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowOuterClippedRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRectClipped.tMin, ptWindow->tOuterRectClipped.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
            }
        }
        
        pl_separator();

        if(pl_tree_node("Windows"))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

                if(pl_tree_node(ptWindow->pcName))
                {
                    pl_text(" - Pos:          (%0.1f, %0.1f)", ptWindow->tPos.x, ptWindow->tPos.y);
                    pl_text(" - Size:         (%0.1f, %0.1f)", ptWindow->tSize.x, ptWindow->tSize.y);
                    pl_text(" - Content Size: (%0.1f, %0.1f)", ptWindow->tContentSize.x, ptWindow->tContentSize.y);
                    pl_text(" - Min Size:     (%0.1f, %0.1f)", ptWindow->tMinSize.x, ptWindow->tMinSize.y);
                    pl_text(" - Scroll:       (%0.1f/%0.1f, %0.1f/%0.1f)", ptWindow->tScroll.x, ptWindow->tScrollMax.x, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_text(" - Active:       %s", ptWindow->uId == gptCtx->uActiveWindowId ? "1" : "0");
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
            pl_text("Windows");
            pl_indent(0.0f);
            pl_text("Active Window: %s", gptCtx->ptActiveWindow ? gptCtx->ptActiveWindow->pcName : "NULL");
            pl_text("Hovered Window: %s", gptCtx->ptHoveredWindow ? gptCtx->ptHoveredWindow->pcName : "NULL");
            pl_text("Moving Window:  %s", gptCtx->ptMovingWindow ? gptCtx->ptMovingWindow->pcName : "NULL");
            pl_text("Sizing Window:  %s", gptCtx->ptSizingWindow ? gptCtx->ptSizingWindow->pcName : "NULL");
            pl_text("Scrolling Window:  %s", gptCtx->ptScrollingWindow ? gptCtx->ptScrollingWindow->pcName : "NULL");
            pl_text("Wheeling Window:  %s", gptCtx->ptWheelingWindow ? gptCtx->ptWheelingWindow->pcName : "NULL");
            pl_unindent(0.0f);
            pl_text("Items");
            pl_indent(0.0f);
            pl_text("Active Window ID: %u", gptCtx->uActiveWindowId);
            pl_text("Active ID:        %u", gptCtx->uActiveId);
            pl_text("Hovered ID:       %u", gptCtx->uHoveredId);
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
                pl_text("Title");
                pl_slider_float("Title Padding", &ptStyle->fTitlePadding, 0.0f, 32.0f);

                pl_vertical_spacing();
                pl_text("Window");
                pl_slider_float("Horizontal Padding## window", &ptStyle->fWindowHorizontalPadding, 0.0f, 32.0f);
                pl_slider_float("Vertical Padding## window", &ptStyle->fWindowVerticalPadding, 0.0f, 32.0f);

                pl_vertical_spacing();
                pl_text("Scrollbar");
                pl_slider_float("Size##scrollbar", &ptStyle->fScrollbarSize, 0.0f, 32.0f);
                
                pl_vertical_spacing();
                pl_text("Misc");
                pl_slider_float("Indent", &ptStyle->fIndentSize, 0.0f, 32.0f); 
                pl_slider_float("Slider Size", &ptStyle->fSliderSize, 3.0f, 32.0f); 
                pl_slider_float("Font Size", &ptStyle->fFontSize, 13.0f, 48.0f); 
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
    if(pl_begin_window("UI Demo", pbOpen, 0))
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

                const float pfRatios22[] = {100.0f, 120.0f};
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

        if(pl_collapsing_header("Layout & Scrolling"))
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


        if(pl_collapsing_header("Testing 0"))
        {
            // first row
            pl_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 60.0f, 2);

            pl_layout_row_push(200.0f);
            pl_button("Hover me 60!");

            pl_layout_row_push(200.0f);
            pl_button("Hover me 40");

            pl_layout_row_end();

            // space
            pl_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

            pl_layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
            pl_button("Hover me A!");

            pl_layout_space_push(105.0f, 105.0f, 100.0f, 100.0f);
            pl_button("Hover me B!");

            pl_layout_space_end();

            // space
            pl_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 500.0f, 2);

            pl_layout_space_push(0.0f, 0.0f, 0.5f, 0.1f);
            pl_button("Hover me AA!");

            pl_layout_space_push(0.25f, 0.50f, 0.5f, 0.1f);
            pl_button("Hover me BB!");

            pl_layout_space_end();

            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("Testing 1"))
        {
            // first row
            pl_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 60.0f, 2);

            pl_layout_row_push(200.0f);
            pl_button("Hover me 60!");

            pl_layout_row_push(200.0f);
            pl_button("Hover me 40");

            pl_layout_row_end();

            // second row
            pl_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 100.0f, 3);

            pl_layout_row_push(100.0f);
            pl_button("Hover me 100!");

            pl_layout_row_push(200.0f);
            pl_button("Hover me 200");

            pl_layout_row_push(300.0f);
            pl_button("Hover me 300");

            pl_layout_row_end();


            // third row
            pl_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

            pl_layout_row_push(0.33f);
            pl_button("Hover me 100!");

            pl_layout_row_push(0.33f);
            pl_button("Hover me 200");

            pl_layout_row_push(0.34f);
            pl_button("Hover me 300");

            pl_layout_row_end();

            // fourth & fifth row
            const float pfRatios33[] = {0.25f, 0.25f, 0.50f};
            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, pfRatios33);

            // row 4
            pl_button("Hover me 100a!");
            pl_button("Hover me 200a");
            pl_button("Hover me 300a");

            // row 5
            pl_button("Hover me 100b!");
            pl_button("Hover me 200b");
            pl_button("Hover me 300b");

            // sixth
            const float pfRatios2[] = {100.0f, 100.0f};
            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios2);

            // row 6
            pl_button("Hover me 100c!");
            pl_button("Hover me 200c");

            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("Testing 2"))
        {
            pl_layout_template_begin(60.0f);
            pl_layout_template_push_static(100.0f);
            pl_layout_template_push_variable(500.0f);
            pl_layout_template_push_dynamic();
            pl_layout_template_end();

            pl_button("static");
            pl_button("variable");
            pl_button("dynamic");

            pl_button("static");
            pl_button("variable");
            pl_button("dynamic");

            pl_layout_template_begin(30.0f);
            pl_layout_template_push_static(100.0f);
            pl_layout_template_push_dynamic();
            pl_layout_template_push_variable(500.0f);
            pl_layout_template_end();

            pl_button("static");
            pl_button("dynamic");
            pl_button("variable");
            
            pl_button("static##11");
            pl_button("dynamic##11");
            pl_button("variable##11");

            pl_layout_template_begin(0.0f);
            pl_layout_template_push_variable(500.0f);
            pl_layout_template_push_dynamic();
            pl_layout_template_push_static(100.0f);
            pl_layout_template_end();

            
            pl_button("variable##11");
            pl_button("dynamic##11");
            pl_button("static##11");

            pl_button("variable##11");
            pl_button("dynamic##11");
            pl_button("static##11");

            pl_end_collapsing_header();
        }
        pl_end_window();
    }
}