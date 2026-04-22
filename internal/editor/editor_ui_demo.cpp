#include "editor.h"

void
pl__show_ui_demo_window(plAppData* ptAppData)
{
    if(gptUI->begin_window("UI Demo", &ptAppData->bShowUiDemo, PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR))
    {

        static const float pfRatios0[] = {1.0f};
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios0);

        if(gptUI->begin_collapsing_header("Help", 0))
        {
            gptUI->text("Under construction");
            gptUI->end_collapsing_header();
        }
    
        if(gptUI->begin_collapsing_header("Window Options", 0))
        {
            gptUI->text("Under construction");
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header("Widgets", 0))
        {
            if(gptUI->tree_node("Basic", 0))
            {

                gptUI->layout_static(0.0f, 100, 2);
                gptUI->button("Button");
                gptUI->checkbox("Checkbox", NULL);

                gptUI->layout_dynamic(0.0f, 2);
                gptUI->button("Button");
                gptUI->checkbox("Checkbox", NULL);

                gptUI->layout_dynamic(0.0f, 1);
                static char buff[64] = {'c', 'a', 'a'};
                gptUI->input_text("label 0", buff, 64, 0);
                static char buff2[64] = {'c', 'c', 'c'};
                gptUI->input_text_hint("label 1", "hint", buff2, 64, 0);

                static float fValue = 3.14f;
                static int iValue117 = 117;

                gptUI->input_float("label 2", &fValue, "%0.3f", 0);
                gptUI->input_int("label 3", &iValue117, 0);

                static int iValue = 0;
                gptUI->layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

                gptUI->layout_row_push(0.33f);
                gptUI->radio_button("Option 1", &iValue, 0);

                gptUI->layout_row_push(0.33f);
                gptUI->radio_button("Option 2", &iValue, 1);

                gptUI->layout_row_push(0.34f);
                gptUI->radio_button("Option 3", &iValue, 2);

                gptUI->layout_row_end();

                const float pfRatios[] = {1.0f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                gptUI->separator();
                gptUI->labeled_text("Label", "Value");
                static int iValue1 = 0;
                static float fValue1 = 23.0f;
                static float fValue2 = 100.0f;
                static int iValue2 = 3;
                gptUI->slider_float("float slider 1", &fValue1, 0.0f, 100.0f, 0);
                gptUI->slider_float("float slider 2", &fValue2, -50.0f, 100.0f, 0);
                gptUI->slider_int("int slider 1", &iValue1, 0, 10, 0);
                gptUI->slider_int("int slider 2", &iValue2, -5, 10, 0);
                gptUI->drag_float("float drag", &fValue2, 1.0f, -100.0f, 100.0f, 0);
                static int aiIntArray[4] = {0};
                gptUI->input_int2("input int 2", aiIntArray, 0);
                gptUI->input_int3("input int 3", aiIntArray, 0);
                gptUI->input_int4("input int 4", aiIntArray, 0);

                static float afFloatArray[4] = {0};
                gptUI->input_float2("input float 2", afFloatArray, "%0.3f", 0);
                gptUI->input_float3("input float 3", afFloatArray, "%0.3f", 0);
                gptUI->input_float4("input float 4", afFloatArray, "%0.3f", 0);

                if(gptUI->menu_item("Menu item 0", NULL, false, true))
                {
                    printf("menu item 0\n");
                }

                if(gptUI->menu_item("Menu item selected", "CTRL+M", true, true))
                {
                    printf("menu item selected\n");
                }

                if(gptUI->menu_item("Menu item disabled", NULL, false, false))
                {
                    printf("menu item disabled\n");
                }

                static bool bMenuSelection = false;
                if(gptUI->menu_item_toggle("Menu item toggle", NULL, &bMenuSelection, true))
                {
                    printf("menu item toggle\n");
                }

                if(gptUI->begin_menu("menu (not ready)", true))
                {

                    if(gptUI->menu_item("Menu item 0", NULL, false, true))
                    {
                        printf("menu item 0\n");
                    }

                    if(gptUI->menu_item("Menu item selected", "CTRL+M", true, true))
                    {
                        printf("menu item selected\n");
                    }

                    if(gptUI->menu_item("Menu item disabled", NULL, false, false))
                    {
                        printf("menu item disabled\n");
                    }
                    if(gptUI->begin_menu("sub menu", true))
                    {

                        if(gptUI->menu_item("Menu item 0", NULL, false, true))
                        {
                            printf("menu item 0\n");
                        }
                        gptUI->end_menu();
                    }
                    gptUI->end_menu();
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
                if(gptUI->begin_combo("Combo", apcCombo[uComboSelect], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < 5; i++)
                    {
                        if(gptUI->selectable(apcCombo[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUI->close_current_popup();
                        }
                    }
                    gptUI->end_combo();
                }

                const float pfRatios22[] = {200.0f, 120.0f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios22);
                gptUI->button("Hover me!");
                if(gptUI->was_last_item_hovered())
                {
                    gptUI->begin_tooltip();
                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios22);
                    gptUI->text("I'm a tooltip!");
                    gptUI->end_tooltip();
                }
                gptUI->button("Just a button");

                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Selectables", 0))
            {
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                gptUI->selectable("Selectable 1", &bSelectable0, 0);
                gptUI->selectable("Selectable 2", &bSelectable1, 0);
                gptUI->selectable("Selectable 3", &bSelectable2, 0);
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Combo", 0))
            {
                plUiComboFlags tComboFlags = PL_UI_COMBO_FLAGS_NONE;

                static bool bComboHeightSmall = false;
                static bool bComboHeightRegular = false;
                static bool bComboHeightLarge = false;
                static bool bComboNoArrow = false;

                gptUI->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_SMALL", &bComboHeightSmall);
                gptUI->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_REGULAR", &bComboHeightRegular);
                gptUI->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_LARGE", &bComboHeightLarge);
                gptUI->checkbox("PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON", &bComboNoArrow);

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
                if(gptUI->begin_combo("Combo", apcCombo[uComboSelect], tComboFlags))
                {
                    for(uint32_t i = 0; i < 10; i++)
                    {
                        if(gptUI->selectable(apcCombo[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUI->close_current_popup();
                        }
                    }
                    gptUI->end_combo();
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Plotting", 0))
            {
                gptUI->progress_bar(0.75f, pl_create_vec2(-1.0f, 0.0f), NULL);
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Trees", 0))
            {
                
                if(gptUI->tree_node("Root Node", 0))
                {
                    if(gptUI->tree_node("Child 1", 0))
                    {
                        gptUI->button("Press me");
                        gptUI->tree_pop();
                    }
                    if(gptUI->tree_node("Child 2", 0))
                    {
                        gptUI->button("Press me");
                        gptUI->tree_pop();
                    }
                    gptUI->tree_pop();
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Tabs", 0))
            {
                if(gptUI->begin_tab_bar("Tabs1", 0))
                {
                    if(gptUI->begin_tab("Tab 0", 0))
                    {
                        static bool bSelectable0 = false;
                        static bool bSelectable1 = false;
                        static bool bSelectable2 = false;
                        gptUI->selectable("Selectable 1", &bSelectable0, 0);
                        gptUI->selectable("Selectable 2", &bSelectable1, 0);
                        gptUI->selectable("Selectable 3", &bSelectable2, 0);
                        gptUI->end_tab();
                    }

                    if(gptUI->begin_tab("Tab 1", 0))
                    {
                        static int iValue = 0;
                        gptUI->radio_button("Option 1", &iValue, 0);
                        gptUI->radio_button("Option 2", &iValue, 1);
                        gptUI->radio_button("Option 3", &iValue, 2);
                        gptUI->end_tab();
                    }

                    if(gptUI->begin_tab("Tab 2", 0))
                    {
                        if(gptUI->begin_child("CHILD2", 0, 0))
                        {
                            const float pfRatios3[] = {600.0f};
                            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                            for(uint32_t i = 0; i < 25; i++)
                                gptUI->text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
                            gptUI->end_child();
                        }
                        
                        gptUI->end_tab();
                    }
                    gptUI->end_tab_bar();
                }
                gptUI->tree_pop();
            }
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header("Scrolling", 0))
        {
            const float pfRatios2[] = {0.5f, 0.50f};
            const float pfRatios3[] = {600.0f};

            gptUI->layout_static(0.0f, 200, 1);
            static bool bUseClipper = true;
            gptUI->checkbox("Use Clipper", &bUseClipper);
            
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2, pfRatios2);
            if(gptUI->begin_child("CHILD", 0, 0))
            {

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);


                if(bUseClipper)
                {
                    plUiClipper tClipper = {1000000};
                    while(gptUI->step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            gptUI->text("%u Label", i);
                            gptUI->text("%u Value", i);
                        } 
                    }
                }
                else
                {
                    for(uint32_t i = 0; i < 1000000; i++)
                    {
                            gptUI->text("%u Label", i);
                            gptUI->text("%u Value", i);
                    }
                }


                gptUI->end_child();
            }
            

            if(gptUI->begin_child("CHILD2", 0, 0))
            {
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                for(uint32_t i = 0; i < 25; i++)
                    gptUI->text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");

                gptUI->end_child();
            }
            

            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header("Layout Systems", 0))
        {
            gptUI->text("General Notes");
            gptUI->text("  - systems ordered by increasing flexibility");
            gptUI->separator();

            if(gptUI->tree_node("System 1 - simple dynamic", 0))
            {
                static int iWidgetCount = 5;
                static float fWidgetHeight = 0.0f;
                gptUI->separator_text("Notes");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - evenly spaces widgets based on available space");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 10, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                gptUI->layout_dynamic(fWidgetHeight, (uint32_t)iWidgetCount);
                gptUI->vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 2 - simple static", 0))
            {
                static int iWidgetCount = 5;
                static float fWidgetWidth = 100.0f;
                static float fWidgetHeight = 0.0f;
                gptUI->separator_text("Notes");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - provides each widget with the same specified width");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 10, 0);
                gptUI->slider_float("Width", &fWidgetWidth, 50.0f, 500.0f, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                gptUI->layout_static(fWidgetHeight, fWidgetWidth, (uint32_t)iWidgetCount);
                gptUI->vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 3 - single system row", 0))
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

                gptUI->separator_text("Notes");
                gptUI->text("  - does not wrap (i.e. will not add rows)");
                gptUI->text("  - allows user to change widget widths individually");
                gptUI->text("  - widths interpreted as ratios of available width when");
                gptUI->text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                gptUI->text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->checkbox("Dynamic", &bDynamicRow);
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 4, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f, 0);
                        gptUI->pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f, 0);
                        gptUI->pop_id();
                    }
                }
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                gptUI->layout_row_begin(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount);
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                for(int i = 0; i < iWidgetCount; i++)
                {
                    gptUI->layout_row_push(afWidgetWidths[i]);
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->layout_row_end();
                gptUI->vertical_spacing();
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 4 - single system row (array form)", 0))
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

                gptUI->separator_text("Notes");
                gptUI->text("  - same as System 3 but array form");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - allows user to change widget widths individually");
                gptUI->text("  - widths interpreted as ratios of available width when");
                gptUI->text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                gptUI->text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->checkbox("Dynamic", &bDynamicRow);
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 4, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f, 0);
                        gptUI->pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f, 0);
                        gptUI->pop_id();
                    }
                }
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                gptUI->layout_row(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount, afWidgetWidths);
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->vertical_spacing();
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 5 - template", 0))
            {
                static int iWidgetCount = 6;
                static float fWidgetHeight = 0.0f;

                gptUI->separator_text("Notes");
                gptUI->text("  - most complex and second most flexible system");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - allows user to change widget systems individually");
                gptUI->text("    - dynamic: changes based on available space");
                gptUI->text("    - variable: same as dynamic but minimum width specified by user");
                gptUI->text("    - static: pixel width explicitely specified by user");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUI->vertical_spacing();

                gptUI->separator_text("Example 0");

                gptUI->layout_template_begin(fWidgetHeight);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_push_variable(150.0f);
                gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_end();
                gptUI->button("dynamic##0");
                gptUI->button("variable 150.0f##0");
                gptUI->button("static 150.0f##0");
                gptUI->checkbox("dynamic##1", NULL);
                gptUI->checkbox("variable 150.0f##1", NULL);
                gptUI->checkbox("static 150.0f##1", NULL);
                gptUI->vertical_spacing();

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Example 1");
                gptUI->layout_template_begin(fWidgetHeight);
                gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_push_variable(150.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_end();
                gptUI->button("static 150.0f##2");
                gptUI->button("variable 150.0f##2");
                gptUI->button("dynamic##2");
                gptUI->checkbox("static 150.0f##3", NULL);
                gptUI->checkbox("variable 150.0f##3", NULL);
                gptUI->checkbox("dynamic##3", NULL);

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Example 2");
                gptUI->layout_template_begin(fWidgetHeight);
                gptUI->layout_template_push_variable(150.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_end();
                gptUI->button("variable 150.0f##4");
                gptUI->button("variable 300.0f##4");
                gptUI->button("dynamic##4");
                gptUI->checkbox("static 150.0f##5", NULL);
                gptUI->button("variable 300.0f##5");
                gptUI->checkbox("dynamic##5", NULL);
                
                gptUI->vertical_spacing();
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 6 - space", 0))
            {
                gptUI->separator_text("Notes");
                gptUI->text("  - most flexible system");
                gptUI->vertical_spacing();

                gptUI->separator_text("Example - static");

                gptUI->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

                gptUI->layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
                gptUI->button("w100 h100");

                gptUI->layout_space_push(105.0f, 105.0f, 300.0f, 100.0f);
                gptUI->button("x105 y105 w300 h100");

                gptUI->layout_space_end();

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Example - dynamic");

                gptUI->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2);

                gptUI->layout_space_push(0.0f, 0.0f, 0.5f, 0.5f);
                gptUI->button("x0 y0 w0.5 h0.5");

                gptUI->layout_space_push(0.5f, 0.5f, 0.5f, 0.5f);
                gptUI->button("x0.5 y0.5 w0.5 h0.5");

                gptUI->layout_space_end();

                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Misc. Testing", 0))
            {
                const float pfRatios[] = {1.0f};
                const float pfRatios2[] = {0.5f, 0.5f};
                const float pfRatios3[] = {0.5f * 0.5f, 0.25f * 0.5f, 0.25f * 0.5f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);
                if(gptUI->begin_collapsing_header("Information", 0))
                {
                    gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                    gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());

                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, pfRatios3);
                    if(gptUI->begin_collapsing_header("sub0", 0))
                    {
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->end_collapsing_header();
                    }
                    if(gptUI->begin_collapsing_header("sub1", 0))
                    {
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->end_collapsing_header();
                    }
                    if(gptUI->begin_collapsing_header("sub2", 0))
                    {
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->end_collapsing_header();
                    }

                    gptUI->end_collapsing_header();
                }
                if(gptUI->begin_collapsing_header("App Options", 0))
                {
                    gptUI->checkbox("Freeze Culling Camera", NULL);
                    int iCascadeCount  = 2;
                    gptUI->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0);

                    gptUI->end_collapsing_header();
                }
                
                if(gptUI->begin_collapsing_header("Graphics", 0))
                {
                    gptUI->checkbox("Freeze Culling Camera", NULL);
                    int iCascadeCount  = 2;
                    gptUI->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0);

                    gptUI->end_collapsing_header();
                }
                if(gptUI->begin_tab_bar("tab bar2", 0))
                {
                    if(gptUI->begin_tab("tab0000000000", 0))
                    {
                        gptUI->checkbox("Entities", NULL);
                        gptUI->end_tab();
                    }
                    if(gptUI->begin_tab("tab1", 0))
                    {
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->end_tab();
                    }
                    gptUI->end_tab_bar();
                }

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                if(gptUI->begin_collapsing_header("Tools", 0))
                {
                    gptUI->checkbox("Device Memory Analyzer", NULL);
                    gptUI->checkbox("Device Memory Analyzer", NULL);
                    gptUI->end_collapsing_header();
                }

                if(gptUI->begin_collapsing_header("Debug", 0))
                {
                    gptUI->button("resize");
                    gptUI->checkbox("Always Resize", NULL);
                    gptUI->end_collapsing_header();
                }

                gptUI->tree_pop();
            }
            gptUI->end_collapsing_header();
        }
        gptUI->end_window();
    }
}