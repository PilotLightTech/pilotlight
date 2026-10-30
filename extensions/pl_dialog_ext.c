/*
   pl_dialog_ext.c
*/

/*
Index of this file:

// [SECTION] includes
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_dialog_ext.h"

#include "pl_ui_ext.h"
#include "pl_platform_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    const plUiI*   gptUI   = NULL;
    const plFileI* gptFile = NULL;
#endif

#include "pl_ds.h"

typedef struct _plDialogsContext
{
    plDirectoryInfo* sbtDirectoryInfo;
    plUiTextFilter tTextFilter;
    char acCurrentBuffer[256];
    char* sbcSelectedPath;
    uint32_t uPopupId;

    plFileDialogDesc tCurrentDesc;

} plDialogsContext;

static plDialogsContext* gptDialogCtx = NULL;

bool pl__dialog_render_entry(const char*, plDirectoryEntry*);
bool pl__dialog_render_entry(const char* pcParentPath, plDirectoryEntry* ptEntry)
{
    bool bResult = false;

    char acPath[1024];
    size_t szLength = strlen(pcParentPath);
    bool bHasSeparator = szLength > 0 &&
        (pcParentPath[szLength - 1] == '/' ||
         pcParentPath[szLength - 1] == '\\');

    pl_snprintf(acPath, sizeof(acPath), "%s%s%s",
        pcParentPath,
        szLength > 0 && !bHasSeparator ? "/" : "",
        ptEntry->acName);

    if(ptEntry->eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
    {
        bool bSelected = false;
        if(gptUI->selectable(ptEntry->acName, &bSelected, 0))
        {
            pl_sb_reset(gptDialogCtx->sbcSelectedPath);
            pl_sb_sprintf(gptDialogCtx->sbcSelectedPath, "%s", acPath);
            bResult = true;
            gptUI->close_current_popup();
        }
    }
    else if(ptEntry->eType == PL_DIRECTORY_ENTRY_TYPE_DIRECTORY)
    {
        // Avoid recursively traversing the current/parent directories.
        if(strcmp(ptEntry->acName, ".") == 0 ||
           strcmp(ptEntry->acName, "..") == 0)
            return false;

        if(gptUI->tree_node(ptEntry->acName, 0))
        {
            if(pl_sb_size(gptDialogCtx->sbtDirectoryInfo) == 0)
            {
                plDirectoryInfo tEmpty = {0};
                pl_sb_push(gptDialogCtx->sbtDirectoryInfo, tEmpty);
            }

            plDirectoryInfo tDirectoryInfo = pl_sb_pop(gptDialogCtx->sbtDirectoryInfo);

            gptFile->get_directory_info(acPath, &tDirectoryInfo);

            for(uint32_t i = 0; i < tDirectoryInfo.uEntryCount; i++)
            {
                if(pl__dialog_render_entry(acPath, &tDirectoryInfo.sbtEntries[i]))
                    bResult = true;
            }

            gptFile->reset_directory_info(&tDirectoryInfo);
            pl_sb_push(gptDialogCtx->sbtDirectoryInfo, tDirectoryInfo);
            gptUI->tree_pop();
        }
    }

    return bResult;
}

bool
pl_dialog_open_file_dialog(const plFileDialogDesc* ptDesc)
{
    if(ptDesc)
        gptDialogCtx->tCurrentDesc = *ptDesc;

    gptUI->open_popup_id(gptDialogCtx->uPopupId, 0);
    return true;
}

plDialogResult
pl_dialog_draw_file_dialog(void)
{
    plFileDialogMode tResult = PL_DIALOG_RESULT_NONE;

    gptDialogCtx->uPopupId = gptUI->get_id(gptDialogCtx->tCurrentDesc.pcTitle);

    if(gptUI->is_popup_open_id(gptDialogCtx->uPopupId))
    {
        if(gptUI->begin_popup(gptDialogCtx->tCurrentDesc.pcTitle, 0))
        {

            if(gptUI->is_window_appearing())
            {
                strncpy(gptDialogCtx->tTextFilter.acInputBuffer, gptDialogCtx->tCurrentDesc.pcStartDirectory, 256);
                strncpy(gptDialogCtx->acCurrentBuffer, gptDialogCtx->tCurrentDesc.pcStartDirectory, 256);
                gptUI->text_filter_build(&gptDialogCtx->tTextFilter);
            }

            gptUI->layout_template_begin(0.0f);
            gptUI->layout_template_push_static(50.0f);
            gptUI->layout_template_push_dynamic();
            gptUI->layout_template_end();

            gptUI->text("Open:");
            if(gptUI->input_text_hint("##filter", "Filter (inc,-exc)", gptDialogCtx->tTextFilter.acInputBuffer, 256, 0))
            {
                gptUI->text_filter_build(&gptDialogCtx->tTextFilter);

                if(gptDialogCtx->tTextFilter.acInputBuffer[strnlen(gptDialogCtx->tTextFilter.acInputBuffer, 256) - 1] == '/')
                {
                    strncpy(gptDialogCtx->acCurrentBuffer, gptDialogCtx->tTextFilter.acInputBuffer, 256);
                }
            }

            if(gptUI->is_window_appearing())
            {
                gptUI->set_keyboard_focus_last_item();
            }

            const plVec2 tWindowSize = gptUI->get_window_size();

            gptUI->layout_dynamic(tWindowSize.y - 80.0f, 1);

            if(gptUI->begin_child("File Child", 0, 0))
            {
                if(pl_sb_size(gptDialogCtx->sbtDirectoryInfo) == 0)
                {
                    pl_sb_add(gptDialogCtx->sbtDirectoryInfo);
                    pl_sb_add(gptDialogCtx->sbtDirectoryInfo);
                }
                plDirectoryInfo tDirectoryInfo = pl_sb_pop(gptDialogCtx->sbtDirectoryInfo);
                gptFile->get_directory_info(gptDialogCtx->acCurrentBuffer, &tDirectoryInfo);

                gptUI->layout_dynamic(0.0f, 1);
                for(uint32_t i = 0; i < tDirectoryInfo.uEntryCount; i++)
                {
                    if(pl__dialog_render_entry(gptDialogCtx->acCurrentBuffer, &tDirectoryInfo.sbtEntries[i]))
                        tResult = PL_DIALOG_RESULT_ACCEPTED;
                }

                gptFile->reset_directory_info(&tDirectoryInfo);
                pl_sb_push(gptDialogCtx->sbtDirectoryInfo, tDirectoryInfo);
                gptUI->end_child();
            }
            gptUI->end_popup();
        }
    }
    return tResult;
}

const char*
pl_dialog_get_file_dialog_path(void)
{
    return gptDialogCtx->sbcSelectedPath;
}

void
pl_dialog_close_file_dialog(void)
{
    gptUI->close_current_popup();
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_dialog_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDialogI tApi = {
        .open_file_dialog = pl_dialog_open_file_dialog,
        .close_file_dialog = pl_dialog_close_file_dialog,
        .draw_file_dialog = pl_dialog_draw_file_dialog,
        .get_file_dialog_path = pl_dialog_get_file_dialog_path,
    };
    pl_set_api(ptApiRegistry, plDialogI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptFile   = pl_get_api_latest(ptApiRegistry, plFileI);
    gptUI     = pl_get_api_latest(ptApiRegistry, plUiI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptDialogCtx = ptDataRegistry->get_data("plDialogsContext");
    }
    else
    {
        static plDialogsContext gtDialogsCtx = {0};
        gptDialogCtx = &gtDialogsCtx;
        gptDialogCtx->tCurrentDesc.pcTitle = "Select File";
        gptDialogCtx->tCurrentDesc.pcStartDirectory = "../assets/";
        ptDataRegistry->set_data("plDialogsContext", gptDialogCtx);
    }
}

PL_EXPORT void
pl_unload_dialog_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    for(uint32_t i = 0; i < pl_sb_size(gptDialogCtx->sbtDirectoryInfo); i++)
    {
        gptFile->cleanup_directory_info(&gptDialogCtx->sbtDirectoryInfo[i]);
    }
    pl_sb_free(gptDialogCtx->sbtDirectoryInfo);
    pl_sb_free(gptDialogCtx->sbcSelectedPath);

    const plDialogI* ptApi = pl_get_api_latest(ptApiRegistry, plDialogI);
    ptApiRegistry->remove_api(ptApi);
}
