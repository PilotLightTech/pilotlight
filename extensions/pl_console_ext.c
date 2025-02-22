/*
   pl_console_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal forward declarations
// [SECTION] enums
// [SECTION] structs
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
// [SECTION] internal forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConsoleContext plConsoleContext;

// enums
typedef int plConsoleVarType;  // -> enum _plConsoleVarType

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plConsoleVarType
{
    PL_CONSOLE_VARIABLE_TYPE_NONE   = 0,
    PL_CONSOLE_VARIABLE_TYPE_TOGGLE,
    PL_CONSOLE_VARIABLE_TYPE_BOOL,
    PL_CONSOLE_VARIABLE_TYPE_STRING,
    PL_CONSOLE_VARIABLE_TYPE_INT,
    PL_CONSOLE_VARIABLE_TYPE_UINT,
    PL_CONSOLE_VARIABLE_TYPE_FLOAT,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plConsoleCommand
{
    plConsoleVarFlags tFlags;
    plConsoleVarType  tType;
    const char*       pcName;
    const char*       pcDescription;
    size_t            szBufferSize; // string var types

    // callbacks
    plConsoleCallback tCallback;
    void*             ptUserData;

    // for direct console vars
    union{
        bool*     pbValue;
        int*      piValue;
        uint32_t* puValue;
        float*    pfValue;
        char*     pcValue;
        void*     pValue;
    };
} plConsoleCommand;

typedef struct _plConsoleContext
{
    // state
    plUiTextFilter tFilter;
    bool bJustOpened;
    bool bOpen;

    // options
    bool bConsoleTransparency;
    bool bShowConsoleBackground;
    bool bMoveable;
    bool bResizable;
    bool bPopup;

    plConsoleCommand* sbtCommands;
} plConsoleContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plConsoleContext* gptConsoleCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void*
pl_console_get_variable(const char* pcName, plConsoleCallback* ptCallbackOut, void** ppUserDataOut)
{
    const uint32_t uCommandCount = pl_sb_size(gptConsoleCtx->sbtCommands);
    for(uint32_t i = 0; i < uCommandCount; i++)
    {
        if(strcmp(gptConsoleCtx->sbtCommands[i].pcName, pcName) == 0)
        {
            if(ptCallbackOut)
                *ptCallbackOut = gptConsoleCtx->sbtCommands[i].tCallback;
            if(ppUserDataOut)
                *ppUserDataOut = gptConsoleCtx->sbtCommands[i].ptUserData;
            return gptConsoleCtx->sbtCommands[i].pValue;
        }
    }

    return NULL;
}

bool
pl_console_remove_variable(const char* pcName)
{
    const uint32_t uCommandCount = pl_sb_size(gptConsoleCtx->sbtCommands);
    for(uint32_t i = 0; i < uCommandCount; i++)
    {
        if(strcmp(gptConsoleCtx->sbtCommands[i].pcName, pcName) == 0)
        {
            pl_sb_del_swap(gptConsoleCtx->sbtCommands, i);
            return true;
        }
    }

    return false;
}

bool
pl_console_add_command(const char* pcName, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    if(pl_console_get_variable(pcName, NULL, NULL))
        return false;

    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_NONE,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };

    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_toggle_variable_ex(const char* pcName, bool* ptToggle, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    if(pl_console_get_variable(pcName, NULL, NULL))
        return false;

    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_TOGGLE,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .pbValue       = ptToggle,
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };

    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_bool_variable_ex(const char* pcName, bool* ptToggle, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    if(pl_console_get_variable(pcName, NULL, NULL))
        return false;

    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_BOOL,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .pbValue       = ptToggle,
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };

    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_string_variable_ex(const char* pcName, char* pcValue, size_t szBufferSize, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    if(pl_console_get_variable(pcName, NULL, NULL))
        return false;

    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_STRING,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .pcValue       = pcValue,
        .szBufferSize  = szBufferSize,
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };
    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_int_variable_ex(const char* pcName, int* piValue, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_INT,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .piValue       = piValue,
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };
    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_uint_variable_ex(const char* pcName, uint32_t* puValue, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    if(pl_console_get_variable(pcName, NULL, NULL))
        return false;

    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_UINT,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .puValue       = puValue,
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };
    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_float_variable_ex(const char* pcName, float* pfValue, const char* pcDescription, plConsoleVarFlags tFlags, plConsoleCallback tCallback, void* pUserData)
{
    if(pl_console_get_variable(pcName, NULL, NULL))
        return false;

    plConsoleCommand tCommand = {
        .tFlags        = tFlags,
        .tType         = PL_CONSOLE_VARIABLE_TYPE_FLOAT,
        .pcName        = pcName,
        .pcDescription = pcDescription ? pcDescription : "",
        .pfValue       = pfValue,
        .tCallback     = tCallback,
        .ptUserData    = pUserData
    };
    pl_sb_push(gptConsoleCtx->sbtCommands, tCommand);
    return true;
}

bool
pl_console_add_toggle_variable(const char* pcName, bool* ptToggle, const char* pcDescription, plConsoleVarFlags tFlags)
{
    return pl_console_add_toggle_variable_ex(pcName, ptToggle, pcDescription, tFlags, NULL, NULL);
}

bool
pl_console_add_bool_variable(const char* pcName, bool* ptToggle, const char* pcDescription, plConsoleVarFlags tFlags)
{
    return pl_console_add_bool_variable_ex(pcName, ptToggle, pcDescription, tFlags, NULL, NULL);
}

bool
pl_console_add_string_variable(const char* pcName, char* pcValue, size_t szBufferSize, const char* pcDescription, plConsoleVarFlags tFlags)
{
    return pl_console_add_string_variable_ex(pcName, pcValue, szBufferSize, pcDescription, tFlags, NULL, NULL);
}

bool
pl_console_add_int_variable(const char* pcName, int* piValue, const char* pcDescription, plConsoleVarFlags tFlags)
{
    return pl_console_add_int_variable_ex(pcName, piValue, pcDescription, tFlags, NULL, NULL);
}

bool
pl_console_add_uint_variable(const char* pcName, uint32_t* puValue, const char* pcDescription, plConsoleVarFlags tFlags)
{
    return pl_console_add_uint_variable_ex(pcName, puValue, pcDescription, tFlags, NULL, NULL);
}

bool
pl_console_add_float_variable(const char* pcName, float* pfValue, const char* pcDescription, plConsoleVarFlags tFlags)
{
    return pl_console_add_float_variable_ex(pcName, pfValue, pcDescription, tFlags, NULL, NULL);
}

void
pl_console_initialize(plConsoleSettings tSettings)
{
    gptConsoleCtx->bConsoleTransparency = true;
    gptConsoleCtx->bShowConsoleBackground = true;

    if(tSettings.tFlags & PL_CONSOLE_FLAGS_POPUP)
        gptConsoleCtx->bPopup = true;
    if(tSettings.tFlags & PL_CONSOLE_FLAGS_MOVABLE)
        gptConsoleCtx->bMoveable = true;
    if(tSettings.tFlags & PL_CONSOLE_FLAGS_RESIZABLE)
        gptConsoleCtx->bResizable = true;

    // pilot light
    static plVersion tVersion = PILOT_LIGHT_VERSION;
    pl_console_add_uint_variable("VersionMajor", &tVersion.uMajor, "Pilot Light Major Version", PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY);
    pl_console_add_uint_variable("VersionMinor", &tVersion.uMinor, "Pilot Light Minor Version", PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY);
    pl_console_add_uint_variable("VersionPatch", &tVersion.uPatch, "Pilot Light Patch Version", PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY);

    // io
    pl_console_add_float_variable("i.MouseDragThreshold", &gptIO->fMouseDragThreshold, "mouse drag threshold", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_float_variable("i.MouseDoubleClickTime", &gptIO->fMouseDoubleClickTime, "mouse drag threshold", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_float_variable("i.MouseDoubleClickMaxDist", &gptIO->fMouseDoubleClickMaxDist, "mouse double click max distance", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_float_variable("i.KeyRepeatDelay", &gptIO->fKeyRepeatDelay, "key repeat delay", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_float_variable("i.KeyRepeatRate", &gptIO->fKeyRepeatRate, "key repeat rate", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_float_variable("i.HeadlessUpdateRate", &gptIO->fHeadlessUpdateRate, "frame rate when headless (FPS)", PL_CONSOLE_VARIABLE_FLAGS_NONE);

    // console
    pl_console_add_toggle_variable("c.Alpha", &gptConsoleCtx->bConsoleTransparency, "toggles command console transparency", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_toggle_variable("c.Background", &gptConsoleCtx->bShowConsoleBackground, "toggles command console background", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_toggle_variable("c.Moveable", &gptConsoleCtx->bMoveable, "allow console moving", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_toggle_variable("c.Resizeable", &gptConsoleCtx->bResizable, "allow console resizing", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    pl_console_add_toggle_variable("c.Popup", &gptConsoleCtx->bPopup, "make console popup", PL_CONSOLE_VARIABLE_FLAGS_NONE);
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
    if(gptConsoleCtx->bPopup)
        gptUI->open_popup("Console Window##0", 0);
    gptConsoleCtx->bOpen = true;
    gptConsoleCtx->bJustOpened = true;
    memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
}

void
pl_console_close(void)
{
    gptConsoleCtx->bOpen = false;
    gptConsoleCtx->bJustOpened = false;
    memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
}

void
pl_console_update(void)
{
    bool bInitialOpen = true;

    bool bPopup = gptConsoleCtx->bPopup;

    if(bPopup)
        bInitialOpen = gptUI->is_popup_open("Console Window##0");

    if(bInitialOpen)
    {
        const uint32_t uCommandCount = pl_sb_size(gptConsoleCtx->sbtCommands);

        plUiWindowFlags tConsoleFlags = PL_CONSOLE_FLAGS_NONE;

        // center window
        if(!(gptConsoleCtx->bMoveable))
        {
            plVec2 tWindowPosition = {gptIO->tMainViewportSize.x * 0.25f, gptIO->tMainViewportSize.y * 0.10f};
            gptUI->set_next_window_pos(tWindowPosition, PL_UI_COND_ALWAYS);
            tConsoleFlags |= PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_TITLE_BAR;
        }
        else
        {
            plVec2 tWindowPosition = {gptIO->tMainViewportSize.x * 0.25f, gptIO->tMainViewportSize.y * 0.10f};
            gptUI->set_next_window_pos(tWindowPosition, PL_UI_COND_ONCE);
        }

        if(!(gptConsoleCtx->bResizable))
        {
            plVec2 tWindowSize = {gptIO->tMainViewportSize.x * 0.5f, gptIO->tMainViewportSize.y * 0.80f};
            gptUI->set_next_window_size(tWindowSize, PL_UI_COND_ALWAYS);
            tConsoleFlags |= PL_UI_WINDOW_FLAGS_NO_RESIZE;
        }

        // modify theme
        gptUI->push_theme_color(PL_UI_COLOR_TITLE_ACTIVE, (plVec4){0.10f, 0.10f, 0.45f, gptConsoleCtx->bConsoleTransparency ? 0.90f : 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_TITLE_BG_COLLAPSED, (plVec4){0.10f, 0.10f, 0.45f, gptConsoleCtx->bConsoleTransparency ? 0.90f : 1.0f});

        gptUI->push_theme_color(PL_UI_COLOR_WINDOW_BG, (plVec4){0.10f, 0.10f, 0.25f, gptConsoleCtx->bConsoleTransparency ? 0.90f : 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_POPUP_BG, (plVec4){0.10f, 0.10f, 0.25f, gptConsoleCtx->bConsoleTransparency ? 0.90f : 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_CHILD_BG, (plVec4){0.10f, 0.15f, 0.10f, 0.78f});
        gptUI->push_theme_color(PL_UI_COLOR_FRAME_BG, (plVec4){0.10f, 0.10f, 0.45f, 0.78f});
        gptUI->push_theme_color(PL_UI_COLOR_SCROLLBAR_HANDLE, (plVec4){0.10f, 0.10f, 0.55f, 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_SCROLLBAR_HOVERED, (plVec4){0.10f, 0.10f, 0.85f, 1.0f});
        gptUI->push_theme_color(PL_UI_COLOR_SCROLLBAR_ACTIVE, (plVec4){0.10f, 0.10f, 0.95f, 1.0f});

        // window flags
        if(!gptConsoleCtx->bShowConsoleBackground)
            tConsoleFlags |= PL_UI_WINDOW_FLAGS_NO_BACKGROUND;

        static plConsoleCommand* ptLastCommand = NULL;
        static int iTempIntVar = 0;
        static uint32_t uTempIntVar = 0;
        static float fTempFloatVar = 0;
        static char acTempStringVar[256] = {0};

        bool bOpened = false;
        if(bPopup)
            bOpened = gptUI->begin_popup("Console Window##0", tConsoleFlags);
        else if(gptConsoleCtx->bOpen)
            bOpened = gptUI->begin_window("Console Window##1", &gptConsoleCtx->bOpen, tConsoleFlags);

        if(bOpened)
        {

            bool bClosePopup = false;
            plConsoleCommand* ptActiveCommand = NULL; // execute if this is valid

            // just opened, reset things
            static char acValueBuffer[256] = {0};
            if(gptConsoleCtx->bJustOpened)
            {
                memset(acValueBuffer, 0, 256);
                memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
                
                // let this be set after the first widget so we can
                // set focus on new open
                // gptConsoleCtx->bJustOpened = false;
            }

            // custom layout
            gptUI->layout_template_begin(0.0f);
            gptUI->layout_template_push_static(65.0f);
            gptUI->layout_template_push_dynamic();
            gptUI->layout_template_end();

            // command line
            gptUI->text("Command");
            if(gptUI->input_text("##Run Command", gptConsoleCtx->tFilter.acInputBuffer, 256, 0))
            {
                gptUI->text_filter_build(&gptConsoleCtx->tFilter);
            }

            // focus when just opened so user can immediately begin typing
            if(gptConsoleCtx->bJustOpened)
            {
                gptUI->set_keyboard_focus_last_item();
                gptConsoleCtx->bJustOpened = false;
            }

            // go ahead & check if any items match
            uint32_t uFirstItem = UINT32_MAX;
            for(uint32_t i = 0; i < uCommandCount; i++)
            {
                if(gptUI->text_filter_pass(&gptConsoleCtx->tFilter, gptConsoleCtx->sbtCommands[i].pcName, NULL))
                {
                    uFirstItem = i;
                    break;
                }
            }

            // grab first command
            plConsoleCommand* ptFirstCommand = NULL;
            if(uFirstItem != UINT32_MAX)
            {
                if(!(gptConsoleCtx->sbtCommands[uFirstItem].tFlags & PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY))
                    ptFirstCommand = &gptConsoleCtx->sbtCommands[uFirstItem];
            }

            // if command is a value type, show appropriate widget
            if(ptFirstCommand)
            {

                plUiInputTextFlags tInputFlags = PL_UI_INPUT_TEXT_FLAGS_NONE;

                if(ptFirstCommand->tFlags & PL_CONSOLE_VARIABLE_FLAGS_UPDATE_AFTER_ENTER)
                    tInputFlags |= PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE;

                switch (ptFirstCommand->tType)
                {
                    case PL_CONSOLE_VARIABLE_TYPE_STRING:
                    {
                        gptUI->text("Value");
                        if(ptFirstCommand->tFlags & PL_CONSOLE_VARIABLE_FLAGS_UPDATE_AFTER_ENTER)
                        {
                            if(ptFirstCommand != ptLastCommand)
                            {
                                strncpy(acTempStringVar, ptFirstCommand->pcValue, ptFirstCommand->szBufferSize);
                            }

                            if(gptUI->input_text("##commandvalue0", acTempStringVar, ptFirstCommand->szBufferSize, tInputFlags))
                            {
                                strncpy(ptFirstCommand->pcValue, acTempStringVar, ptFirstCommand->szBufferSize);
                            }
                        }
                        else if(gptUI->input_text("##commandvalue0", ptFirstCommand->pcValue, ptFirstCommand->szBufferSize, tInputFlags))
                        {
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_INT:
                    {
                        if(ptFirstCommand != ptLastCommand)
                            iTempIntVar = *ptFirstCommand->piValue;

                        gptUI->text("Value");
                        if(gptUI->input_int("##commandvalue1", &iTempIntVar, tInputFlags))
                        {
                            *ptFirstCommand->piValue = iTempIntVar;
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_UINT:
                    {
                        if(ptFirstCommand != ptLastCommand)
                            uTempIntVar = *ptFirstCommand->puValue;

                        gptUI->text("Value");
                        if(gptUI->input_uint("##commandvalue1", &uTempIntVar, tInputFlags))
                        {
                            *ptFirstCommand->puValue = uTempIntVar;
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_BOOL:
                    {
                        gptUI->text("Value");
                        if(gptUI->checkbox("##commandvalue3", ptFirstCommand->pbValue))
                        {
                            ptActiveCommand = ptFirstCommand;
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_FLOAT:
                    {

                        if(ptFirstCommand != ptLastCommand)
                            fTempFloatVar = *ptFirstCommand->pfValue;

                        gptUI->text("Value");
                        if(gptUI->input_float("##commandvalue2", &fTempFloatVar, NULL, tInputFlags))
                        {
                            *ptFirstCommand->pfValue = fTempFloatVar;
                        }
                        break;
                    }
                    
                    default:
                        break;
                }

                ptLastCommand = ptFirstCommand;
            }

            // show available commands (after filtering)
            gptUI->layout_dynamic(gptUI->get_window_size().y - (gptUI->get_cursor_pos().y - gptUI->get_window_pos().y) - 15.0f, 1);
            if(gptUI->begin_child("Command Child", 0, PL_UI_WINDOW_FLAGS_NO_BACKGROUND))
            {

                // layout
                float acRatios[] = {0.30f, 0.25f, 0.45f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, acRatios);

                // custom theming
                gptUI->push_theme_color(PL_UI_COLOR_HEADER_HOVERED, (plVec4){0.10f, 0.40f, 0.10f, 0.78f});
                gptUI->push_theme_color(PL_UI_COLOR_HEADER, (plVec4){0.10f, 0.55f, 0.10f, 0.78f});
                gptUI->push_theme_color(PL_UI_COLOR_HEADER_ACTIVE, (plVec4){0.10f, 0.75f, 0.10f, 0.78f});
                gptUI->push_theme_color(PL_UI_COLOR_TEXT, (plVec4){0.10f, 1.0f, 0.10f, 1.0f});

                bool bDummyValue = false;
                if(gptUI->text_filter_active(&gptConsoleCtx->tFilter))
                {

                    for(uint32_t i = uFirstItem; i < uCommandCount; i++)
                    {
                        if(gptUI->text_filter_pass(&gptConsoleCtx->tFilter, gptConsoleCtx->sbtCommands[i].pcName, NULL))
                        {

                            if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_NONE)
                                gptUI->color_text((plVec4){0.0f, 1.0f, 1.0f, 1.0f}, gptConsoleCtx->sbtCommands[i].pcName);
                            else if(gptConsoleCtx->sbtCommands[i].tFlags & PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY)
                                gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, gptConsoleCtx->sbtCommands[i].pcName);
                            else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_TOGGLE)
                            {
                                if(gptUI->selectable(gptConsoleCtx->sbtCommands[i].pcName, &bDummyValue, 0))
                                {
                                    ptActiveCommand = &gptConsoleCtx->sbtCommands[i];
                                    // gptConsoleCtx->bJustOpened = true;
                                }
                            }
                            else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_BOOL)
                            {
                                if(gptUI->selectable(gptConsoleCtx->sbtCommands[i].pcName, gptConsoleCtx->sbtCommands[i].pbValue, 0))
                                {
                                    ptActiveCommand = &gptConsoleCtx->sbtCommands[i];
                                }
                            }
                            else
                                gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, gptConsoleCtx->sbtCommands[i].pcName);

                            if(gptUI->was_last_item_hovered())
                            {
                                gptUI->begin_tooltip();
                                gptUI->layout_static(0.0f, 300, 1);
                                gptUI->text(gptConsoleCtx->sbtCommands[i].pcName);
                                gptUI->text(gptConsoleCtx->sbtCommands[i].pcDescription);
                                gptUI->end_tooltip();
                            }
                            
                            if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_STRING)
                                gptUI->color_text((plVec4){0.3f, 1.0f, 0.3f, 1.0f}, "\"%s\"", gptConsoleCtx->sbtCommands[i].pcValue);
                            else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_INT)
                                gptUI->color_text((plVec4){0.0f, 1.0f, 1.0f, 1.0f}, "%d", *gptConsoleCtx->sbtCommands[i].piValue);
                                else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_UINT)
                                gptUI->color_text((plVec4){1.0f, 0.0f, 1.0f, 1.0f}, "%u", *gptConsoleCtx->sbtCommands[i].puValue);
                            else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_FLOAT)
                                gptUI->color_text((plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "%g", *gptConsoleCtx->sbtCommands[i].pfValue);
                            else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_TOGGLE)
                            {
                                if(*gptConsoleCtx->sbtCommands[i].pbValue)
                                    gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "%s", "ON");
                                else
                                    gptUI->color_text((plVec4){0.6f, 0.4f, 0.4f, 1.0f}, "%s", "OFF");
                            }
                            else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_BOOL)
                            {
                                if(*gptConsoleCtx->sbtCommands[i].pbValue)
                                    gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "%s", "TRUE");
                                else
                                    gptUI->color_text((plVec4){0.6f, 0.4f, 0.4f, 1.0f}, "%s", "FALSE");
                            }
                            else
                            {
                                gptUI->color_text((plVec4){0.0f, 1.0f, 0.0f, 1.0f}, "%s", "->");
                            }

                            gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, gptConsoleCtx->sbtCommands[i].pcDescription);
                        }
                    }
                }
                else
                {
                    plUiClipper tClipper = {uCommandCount};
                    while(gptUI->step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            if(gptUI->text_filter_pass(&gptConsoleCtx->tFilter, gptConsoleCtx->sbtCommands[i].pcName, NULL))
                            {

                                if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_TOGGLE)
                                {
                                    if(gptUI->selectable(gptConsoleCtx->sbtCommands[i].pcName, &bDummyValue, 0))
                                    {
                                        ptActiveCommand = &gptConsoleCtx->sbtCommands[i];
                                        // gptConsoleCtx->bJustOpened = true;
                                    }
                                }
                                else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_BOOL)
                                {
                                    if(gptUI->selectable(gptConsoleCtx->sbtCommands[i].pcName, gptConsoleCtx->sbtCommands[i].pbValue, 0))
                                    {
                                        ptActiveCommand = &gptConsoleCtx->sbtCommands[i];
                                    }
                                }
                                else
                                    gptUI->selectable(gptConsoleCtx->sbtCommands[i].pcName, &bDummyValue, 0);
                                

                                if(gptUI->was_last_item_hovered())
                                {
                                    gptUI->begin_tooltip();
                                    gptUI->layout_static(0.0f, 300, 1);
                                    gptUI->text(gptConsoleCtx->sbtCommands[i].pcName);
                                    gptUI->text(gptConsoleCtx->sbtCommands[i].pcDescription);
                                    gptUI->end_tooltip();
                                }
                                
                                if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_STRING)
                                    gptUI->color_text((plVec4){0.3f, 1.0f, 0.3f, 1.0f}, "\"%s\"", gptConsoleCtx->sbtCommands[i].pcValue);
                                else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_INT)
                                    gptUI->color_text((plVec4){0.0f, 1.0f, 1.0f, 1.0f}, "%d", *gptConsoleCtx->sbtCommands[i].piValue);
                                    else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_UINT)
                                    gptUI->color_text((plVec4){1.0f, 0.0f, 1.0f, 1.0f}, "%u", *gptConsoleCtx->sbtCommands[i].puValue);
                                else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_FLOAT)
                                    gptUI->color_text((plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "%g", *gptConsoleCtx->sbtCommands[i].pfValue);
                                else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_TOGGLE)
                                {
                                    if(*gptConsoleCtx->sbtCommands[i].pbValue)
                                        gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "%s", "ON");
                                    else
                                        gptUI->color_text((plVec4){0.6f, 0.4f, 0.4f, 1.0f}, "%s", "OFF");
                                }
                                else if(gptConsoleCtx->sbtCommands[i].tType == PL_CONSOLE_VARIABLE_TYPE_BOOL)
                                {
                                    if(*gptConsoleCtx->sbtCommands[i].pbValue)
                                        gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "%s", "TRUE");
                                    else
                                        gptUI->color_text((plVec4){0.6f, 0.4f, 0.4f, 1.0f}, "%s", "FALSE");
                                }
                                else
                                {
                                    gptUI->color_text((plVec4){0.0f, 1.0f, 0.0f, 1.0f}, "%s", "->");
                                }

                                gptUI->color_text((plVec4){1.0f, 1.0f, 1.0f, 1.0f}, gptConsoleCtx->sbtCommands[i].pcDescription);
                            }
                        }
                    }
                }

                gptUI->pop_theme_color(4);
                gptUI->end_child();
            }

            if(gptIOI->is_key_pressed(PL_KEY_ENTER, false))
            {
                ptActiveCommand = ptFirstCommand;
                gptConsoleCtx->bJustOpened = true;
            }

            if(ptActiveCommand)
            {
                switch (ptActiveCommand->tType)
                {

                    case PL_CONSOLE_VARIABLE_TYPE_NONE:
                    {
                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData);
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_TOGGLE:
                    {
                        *ptActiveCommand->pbValue = !*ptActiveCommand->pbValue;
                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData ? ptActiveCommand->ptUserData : ptActiveCommand->pfValue);
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_BOOL:
                    {
                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData ? ptActiveCommand->ptUserData : ptActiveCommand->pfValue);
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_STRING:
                    {
                        if(ptActiveCommand->tFlags & PL_CONSOLE_VARIABLE_FLAGS_UPDATE_AFTER_ENTER)
                            strncpy(ptActiveCommand->pcValue, acTempStringVar, ptActiveCommand->szBufferSize);

                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData ? ptActiveCommand->ptUserData : ptActiveCommand->pcValue);
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_INT:
                    {
                        *ptActiveCommand->piValue = iTempIntVar;
                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData ? ptActiveCommand->ptUserData : ptActiveCommand->piValue);
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_UINT:
                    {
                        *ptActiveCommand->puValue = uTempIntVar;
                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData ? ptActiveCommand->ptUserData : ptActiveCommand->puValue);
                        }
                        break;
                    }

                    case PL_CONSOLE_VARIABLE_TYPE_FLOAT:
                    {
                        *ptActiveCommand->pfValue = fTempFloatVar;
                        if(ptActiveCommand->tCallback)
                        {
                            ptActiveCommand->tCallback(ptActiveCommand->pcName, ptActiveCommand->ptUserData ? ptActiveCommand->ptUserData : ptActiveCommand->pfValue);
                        }
                        break;
                    }
                    
                    default:
                        break;
                }

                // close popup if requested
                if(ptActiveCommand->tFlags & PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE)
                    bClosePopup = true;
            }

            if(bClosePopup || gptIOI->is_key_pressed(PL_KEY_ESCAPE, false))
            {
                if(bPopup)
                    gptUI->close_current_popup();
                else
                    gptConsoleCtx->bOpen = false;
                memset(gptConsoleCtx->tFilter.acInputBuffer, 0, 256);
            }

            if(bPopup)
                gptUI->end_popup();
            else
                gptUI->end_window();
        }
        gptUI->pop_theme_color(9);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_console_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plConsoleI tApi = {
        .initialize             = pl_console_initialize,
        .cleanup                = pl_console_cleanup,
        .open                   = pl_console_open,
        .close                  = pl_console_close,
        .update                 = pl_console_update,
        .add_command            = pl_console_add_command,
        .add_toggle_variable    = pl_console_add_toggle_variable,
        .add_bool_variable      = pl_console_add_bool_variable,
        .add_string_variable    = pl_console_add_string_variable,
        .add_int_variable       = pl_console_add_int_variable,
        .add_uint_variable      = pl_console_add_uint_variable,
        .add_float_variable     = pl_console_add_float_variable,
        .add_toggle_variable_ex = pl_console_add_toggle_variable_ex,
        .add_bool_variable_ex   = pl_console_add_bool_variable_ex,
        .add_string_variable_ex = pl_console_add_string_variable_ex,
        .add_int_variable_ex    = pl_console_add_int_variable_ex,
        .add_uint_variable_ex   = pl_console_add_uint_variable_ex,
        .add_float_variable_ex  = pl_console_add_float_variable_ex,
        .get_variable           = pl_console_get_variable,
        .remove_variable        = pl_console_remove_variable
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
#endif