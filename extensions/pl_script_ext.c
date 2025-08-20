/*
   pl_script_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>
#include "pl.h"
#include "pl_script_ext.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static plApiRegistryI*             gptApiRegistry       = NULL;
    static const plExtensionRegistryI* gptExtensionRegistry = NULL;
    static const plEcsI*               gptECS               = NULL;
    static const plProfileI*           gptProfile           = NULL;
    static const plLogI*               gptLog               = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plScriptContext
{
    plEcsTypeKey tComponentType;
} plScriptContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plScriptContext* gptScriptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

plEcsTypeKey
pl_get_ecs_type_key_script(void)
{
    return gptScriptCtx->tComponentType;
}

plEntity
pl_ecs_create_script(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plScriptComponent** pptCompOut)
{

    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created script: '%s'", pcFile);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcFile);
    plScriptComponent* ptScript =  gptECS->add_component(ptLibrary, gptScriptCtx->tComponentType, tNewEntity);
    ptScript->tFlags = tFlags;
    strncpy(ptScript->acFile, pcFile, PL_MAX_PATH_LENGTH);

    gptExtensionRegistry->load(pcFile, "pl_load_script", "pl_unload_script", tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptInterface* ptScriptApi = gptApiRegistry->get_api(pcFile, (plVersion)plScriptInterface_version);
    ptScript->_ptApi = ptScriptApi;
    PL_ASSERT(ptScriptApi->run);

    if(ptScriptApi->setup)
        ptScriptApi->setup(ptLibrary, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptScript;
    return tNewEntity;
}

void
pl_ecs_attach_script(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plEntity tEntity, plScriptComponent** pptCompOut)
{
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "attach script: '%s'", pcFile);
    plScriptComponent* ptScript = gptECS->add_component(ptLibrary, gptScriptCtx->tComponentType, tEntity);
    ptScript->tFlags = tFlags;
    strncpy(ptScript->acFile, pcFile, PL_MAX_NAME_LENGTH);

    gptExtensionRegistry->load(pcFile, "pl_load_script", "pl_unload_script", tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptInterface* ptScriptApi = gptApiRegistry->get_api(pcFile, (plVersion)plScriptInterface_version);
    ptScript->_ptApi = ptScriptApi;
    PL_ASSERT(ptScriptApi->run);

    if(ptScriptApi->setup)
        ptScriptApi->setup(ptLibrary, tEntity);

    if(pptCompOut)
        *pptCompOut = ptScript;
}

void
pl_script_register_system(void)
{

    const plComponentDesc tScriptDesc = {
        .pcName = "Script",
        .szSize = sizeof(plScriptComponent)
    };
    gptScriptCtx->tComponentType = gptECS->register_type(tScriptDesc, NULL);
}

void
pl_run_script_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plScriptComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptScriptCtx->tComponentType, (void**)&ptComponents, &ptEntities);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        const plEntity tEnitity = ptEntities[i];
        if(ptComponents[i].tFlags == 0)
            continue;

        if(ptComponents[i].tFlags & PL_SCRIPT_FLAG_PLAYING)
            ptComponents[i]._ptApi->run(ptLibrary, tEnitity);
        if(ptComponents[i].tFlags & PL_SCRIPT_FLAG_PLAY_ONCE)
            ptComponents[i].tFlags = PL_SCRIPT_FLAG_NONE;
    }
    pl_end_cpu_sample(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_script_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plScriptI tApi = {
        .attach              = pl_ecs_attach_script,
        .create              = pl_ecs_create_script,
        .run_update_system   = pl_run_script_update_system,
        .register_ecs_system = pl_script_register_system,
        .get_ecs_type_key    = pl_get_ecs_type_key_script
    };
    pl_set_api(ptApiRegistry, plScriptI, &tApi);

    gptApiRegistry       = ptApiRegistry;
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptECS               = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptProfile           = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog               = pl_get_api_latest(ptApiRegistry, plLogI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptScriptCtx = ptDataRegistry->get_data("plScriptContext");
    }
    else // first load
    {
        static plScriptContext tCtx = {0};
        gptScriptCtx = &tCtx;
        ptDataRegistry->set_data("plScriptContext", gptScriptCtx);
    }
}

static void
pl_unload_script_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plScriptI* ptApi = pl_get_api_latest(ptApiRegistry, plScriptI);
    ptApiRegistry->remove_api(ptApi);
}