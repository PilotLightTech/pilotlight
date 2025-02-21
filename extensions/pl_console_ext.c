/*
   pl_console_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_console_ext.h"
#include "pl_ui_ext.h"
#include "pl_draw_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

// apis
static const plMemoryI* gptMemory = NULL;
static const plUiI*     gptUI     = NULL;
static const plDrawI*   gptDraw   = NULL;
static const plIOI*     gptIOI    = NULL;

static plIO* gptIO = NULL;

#ifndef PL_DS_ALLOC
    
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plConsoleCommand
{
    const char* pcName;
    const char* pcDescription;
    bool*       pbValue;
    bool        bClosesWindow;
    bool        bValueType;
} plConsoleCommand;

typedef struct _plConsoleContext
{
    // state
    plUiTextFilter tFilter;
    bool bJustOpened;

    // options
    bool bConsoleTransparency;
    bool bShowConsoleBackground;

    plConsoleCommand* sbtCommands;
} plConsoleContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plConsoleContext* gptConsoleCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_console_add_toggle_option(const char* pcName, bool* ptToggle, const char* pcDescription)
{
    plConsoleCommand tCommand = {
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .pbValue       = ptToggle,
        .bClosesWindow = true
    };
    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

void
pl_console_initialize(plConsoleSettings tSettings)
{
    gptConsoleCtx->bConsoleTransparency = true;
    gptConsoleCtx->bShowConsoleBackground = true;

    pl_console_add_toggle_option("STYLE_CONSOLE_ALPHA", &gptConsoleCtx->bConsoleTransparency, "toggles command console transparency");
    pl_sb_top(gptConsoleCtx->sbtCommands).bClosesWindow = false;
    pl_console_add_toggle_option("STYLE_CONSOLE_BACKGROUND", &gptConsoleCtx->bShowConsoleBackground, "toggles command console background");
    pl_sb_top(gptConsoleCtx->sbtCommands).bClosesWindow = false;
}

void
pl_console_cleanup(void)
{
    gptUI->text_filter_cleanup(&gptConsoleCtx->tFilter);
    pl_sb_free(gptConsoleCtx->sbtCommands);
}

void
pl_console_open(void)
{
    gptUI->open_popup("Console Window", 0);
    gptConsoleCtx->bJustOpened = true;
    memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
}

void
pl_console_update(void)
{
    if(gptUI->is_popup_open("Console Window"))
    {
        const uint32_t uCommandCount = pl_sb_size(gptConsoleCtx->sbtCommands);

        plVec2 tWindowPosition = {gptIO->tMainViewportSize.x * 0.25f, gptIO->tMainViewportSize.y * 0.10f};
        plVec2 tWindowSize = {gptIO->tMainViewportSize.x * 0.5f, gptIO->tMainViewportSize.y * 0.80f};
        gptUI->set_next_window_pos(tWindowPosition, PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size(tWindowSize, PL_UI_COND_ALWAYS);
        gptUI->push_theme_color(PL_UI_COLOR_POPUP_BG, (plVec4){0.10f, 0.10f, 0.25f, gptConsoleCtx->bConsoleTransparency ? 0.90f : 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_CHILD_BG, (plVec4){0.10f, 0.15f, 0.10f, 0.78f});
        gptUI->push_theme_color(PL_UI_COLOR_FRAME_BG, (plVec4){0.10f, 0.10f, 0.45f, 0.78f});
        gptUI->push_theme_color(PL_UI_COLOR_SCROLLBAR_HANDLE, (plVec4){0.10f, 0.10f, 0.55f, 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_SCROLLBAR_HOVERED, (plVec4){0.10f, 0.10f, 0.85f, 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_SCROLLBAR_ACTIVE, (plVec4){0.10f, 0.10f, 0.95f, 1.0f});
        plUiWindowFlags tConsoleFlags = PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_NO_MOVE;

        if(!gptConsoleCtx->bShowConsoleBackground) tConsoleFlags |= PL_UI_WINDOW_FLAGS_NO_BACKGROUND;

        if(gptUI->begin_popup("Console Window", tConsoleFlags))
        {

            if(gptIOI->is_key_pressed(PL_KEY_ESCAPE, false))
            {
                gptUI->close_current_popup();
            }

            static char acValueBuffer[256] = {0};
            if(gptConsoleCtx->bJustOpened)
            {
                memset(acValueBuffer, 0, 256);
                memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
            }

            gptUI->layout_template_begin(0.0f);
            gptUI->layout_template_push_static(65.0f);
            gptUI->layout_template_push_dynamic();
            gptUI->layout_template_end();
            gptUI->text("Command");
            if(gptUI->input_text("##Run Command", gptConsoleCtx->tFilter.acInputBuffer, 256, 0))
            {
                gptUI->text_filter_build(&gptConsoleCtx->tFilter);
            }
            if(gptConsoleCtx->bJustOpened)
            {
                gptUI->set_keyboard_focus_last_item();
                gptConsoleCtx->bJustOpened = false;
            }

            uint32_t uFirstItem = UINT32_MAX;

            for(uint32_t i = 0; i < uCommandCount; i++)
            {
                if(gptUI->text_filter_pass(&gptConsoleCtx->tFilter, gptConsoleCtx->sbtCommands[i].pcName, NULL))
                {
                    uFirstItem = i;
                    break;
                }
            } 

            if(uFirstItem != UINT32_MAX && gptConsoleCtx->sbtCommands[uFirstItem].bValueType)
            {
                gptUI->text("Value");
                if(gptUI->input_text("##commandvalue", acValueBuffer, 256, 0))
                {
                }
            }

            if(gptIOI->is_key_pressed(PL_KEY_ENTER, false) && uFirstItem != UINT32_MAX)
            {
                
                for(uint32_t i = 0; i < uCommandCount; i++)
                {
                    if(gptUI->text_filter_pass(&gptConsoleCtx->tFilter, gptConsoleCtx->sbtCommands[i].pcName, NULL))
                    {
                        *gptConsoleCtx->sbtCommands[i].pbValue = !*gptConsoleCtx->sbtCommands[i].pbValue;
                        if(gptConsoleCtx->sbtCommands[i].bClosesWindow)
                        {
                            gptUI->close_current_popup();
                            memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
                        }
                        else
                        {
                            gptConsoleCtx->bJustOpened = true;
                        }
                        break;
                    }
                }  
            }

            gptUI->layout_dynamic(gptUI->get_window_size().y - (gptUI->get_cursor_pos().y - gptUI->get_window_pos().y) - 15.0f, 1);
            if(gptUI->begin_child("Command Child", 0, PL_UI_WINDOW_FLAGS_NO_BACKGROUND))
            {

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_static(100.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_end();

                float acRatios[2] = {0.25f, 0.75};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, acRatios);

                gptUI->push_theme_color(PL_UI_COLOR_HEADER_HOVERED, (plVec4){0.10f, 0.55f, 0.10f, 0.78f});
                gptUI->push_theme_color(PL_UI_COLOR_HEADER_ACTIVE, (plVec4){0.10f, 0.75f, 0.10f, 0.78f});
                gptUI->push_theme_color(PL_UI_COLOR_TEXT, (plVec4){0.10f, 1.0f, 0.10f, 1.0f});


                bool bDummyValue = false;
                for(uint32_t i = uFirstItem; i < uCommandCount; i++)
                {
                    if(gptUI->text_filter_pass(&gptConsoleCtx->tFilter, gptConsoleCtx->sbtCommands[i].pcName, NULL))
                    {
                        bool bToggled = false;
                        if(gptUI->selectable(gptConsoleCtx->sbtCommands[i].pcName, &bDummyValue, 0))
                            bToggled = true;

                        
                        gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, gptConsoleCtx->sbtCommands[i].pcDescription);

                        if(bToggled)
                        {
                            *gptConsoleCtx->sbtCommands[i].pbValue = !*gptConsoleCtx->sbtCommands[i].pbValue;
                            if(gptConsoleCtx->sbtCommands[i].bClosesWindow)
                                gptUI->close_current_popup();
                            else
                            {
                                gptConsoleCtx->bJustOpened = true;
                            }
                        }
                    }
                }
                gptUI->pop_theme_color(3);
                gptUI->end_child();
            }

            gptUI->end_popup();
        }
        gptUI->pop_theme_color(6);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_console_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plConsoleI tApi = {
        .initialize        = pl_console_initialize,
        .cleanup           = pl_console_cleanup,
        .open              = pl_console_open,
        .update            = pl_console_update,
        .add_toggle_option = pl_console_add_toggle_option
    };
    pl_set_api(ptApiRegistry, plConsoleI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptUI     = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIOI    = pl_get_api_latest(ptApiRegistry, plIOI);
    gptDraw   = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptIO     = gptIOI->get_io();

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptConsoleCtx = ptDataRegistry->get_data("plConsoleContext");
    }
    else
    {
        static plConsoleContext gtConsoleCtx = {0};
        gptConsoleCtx = &gtConsoleCtx;
        ptDataRegistry->set_data("plConsoleContext", gptConsoleCtx);
    }
}

PL_EXPORT void
pl_unload_console_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plConsoleI* ptApi = pl_get_api_latest(ptApiRegistry, plConsoleI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD
    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
#endif