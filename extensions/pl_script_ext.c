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
#include "pl_json_ext.h"
#include "pl_string_intern_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static plApiRegistryI*             gptApiRegistry       = NULL;
    static const plExtensionRegistryI* gptExtensionRegistry = NULL;
    static const plEcsI*               gptEcs               = NULL;
    static const plProfileI*           gptProfile           = NULL;
    static const plLogI*               gptLog               = NULL;
    static const plJsonI*              gptJson              = NULL;
    static const plStringInternI*      gptString            = NULL;
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
pl_script_get_ecs_type_key(void)
{
    return gptScriptCtx->tComponentType;
}

plEntity
pl_script_create(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plScriptComponent** pptCompOut)
{

    PL_LOG_DEBUG_API_F(gptLog, gptEcs->get_log_channel(), "created script: '%s'", pcFile);
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcFile);
    plScriptComponent* ptScript =  gptEcs->add_component(ptLibrary, gptScriptCtx->tComponentType, tNewEntity);
    ptScript->tFlags = tFlags;
    ptScript->pcPath = gptString->intern(pcFile);

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
pl_script_attach(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plEntity tEntity, plScriptComponent** pptCompOut)
{
    PL_LOG_DEBUG_API_F(gptLog, gptEcs->get_log_channel(), "attach script: '%s'", pcFile);
    plScriptComponent* ptScript = gptEcs->add_component(ptLibrary, gptScriptCtx->tComponentType, tEntity);
    ptScript->tFlags = tFlags;
    ptScript->pcPath = gptString->intern(pcFile);

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
pl_script_load(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plScriptComponent* ptScript = gptEcs->get_component(ptLibrary, gptScriptCtx->tComponentType, tEntity);
    PL_LOG_DEBUG_API_F(gptLog, gptEcs->get_log_channel(), "load script: '%s'", ptScript->pcPath);

    gptExtensionRegistry->load(ptScript->pcPath, "pl_load_script", "pl_unload_script", ptScript->tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptInterface* ptScriptApi = gptApiRegistry->get_api(ptScript->pcPath, (plVersion)plScriptInterface_version);
    ptScript->_ptApi = ptScriptApi;
    PL_ASSERT(ptScriptApi->run);

    if(ptScriptApi->setup)
        ptScriptApi->setup(ptLibrary, tEntity);
}

static void
pl__ecs_script_serialize(void* pComponent, plJsonObject* ptJson)
{
    plScriptComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "file", ptComponent->pcPath);
    gptJson->add_bool_member(ptJson, "playing", ptComponent->tFlags & PL_SCRIPT_FLAG_PLAYING);
    gptJson->add_bool_member(ptJson, "play_once", ptComponent->tFlags & PL_SCRIPT_FLAG_PLAY_ONCE);
    gptJson->add_bool_member(ptJson, "reloadable", ptComponent->tFlags & PL_SCRIPT_FLAG_RELOADABLE);
}

static void
pl__ecs_script_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plScriptComponent* ptComponent = pComponent;
    char acTempBuffer0[1024] = {0};
    gptJson->string_member(ptJson, "file", acTempBuffer0, 1024);
    ptComponent->pcPath = gptString->intern(acTempBuffer0);
    if(gptJson->bool_member(ptJson, "playing", false)) ptComponent->tFlags |= PL_SCRIPT_FLAG_PLAYING;
    if(gptJson->bool_member(ptJson, "play_once", false)) ptComponent->tFlags |= PL_SCRIPT_FLAG_PLAY_ONCE;
    if(gptJson->bool_member(ptJson, "reloadable", false)) ptComponent->tFlags |= PL_SCRIPT_FLAG_RELOADABLE;
}

void
pl_script_register_ecs_components(void)
{

    const plComponentDesc tScriptDesc = {
        .pcDisplayName = "Script",
        .pcName = "script",
        .szSize = sizeof(plScriptComponent),
        .serialize = pl__ecs_script_serialize,
        .deserialize = pl__ecs_script_deserialize,
    };
    gptScriptCtx->tComponentType = gptEcs->register_type(tScriptDesc, NULL);
}

void
pl_script_run_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plScriptComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptScriptCtx->tComponentType, (void**)&ptComponents, &ptEntities);

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
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_script_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plScriptI tApi = {
        .attach              = pl_script_attach,
        .create              = pl_script_create,
        .load                = pl_script_load,
        .run_update_system   = pl_script_run_update_system,
        .register_ecs_components = pl_script_register_ecs_components,
        .get_ecs_type_key    = pl_script_get_ecs_type_key
    };
    pl_set_api(ptApiRegistry, plScriptI, &tApi);

    gptApiRegistry       = ptApiRegistry;
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptEcs               = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptProfile           = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog               = pl_get_api_latest(ptApiRegistry, plLogI);
    gptJson              = pl_get_api_latest(ptApiRegistry, plJsonI);
    gptString            = pl_get_api_latest(ptApiRegistry, plStringInternI);

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

void
pl_unload_script_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plScriptI* ptApi = pl_get_api_latest(ptApiRegistry, plScriptI);
    ptApiRegistry->remove_api(ptApi);
}