/*
   pl_material_ext.c
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

#include "pl.h"
#include "pl_material_ext.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_graphics_ext.h"
#include "pl_log_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plEcsI* gptECS = NULL;
    static const plLogI* gptLog = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMaterialContext
{
    plEcsTypeKey tComponentType;
} plMaterialContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plMaterialContext* gptMaterialCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

plEcsTypeKey
pl_get_ecs_type_key_material(void)
{
    return gptMaterialCtx->tComponentType;
}

plEntity
pl_ecs_create_material(plComponentLibrary* ptLibrary, const char* pcName, plMaterialComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed material";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created material: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plMaterialComponent* ptCompOut = gptECS->add_component(ptLibrary, gptMaterialCtx->tComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;  
}

void
pl_material_register_system(void)
{

    const plComponentDesc tMaterialDesc = {
        .pcName = "Material",
        .szSize = sizeof(plMaterialComponent),
    };

    static const plMaterialComponent tMaterialComponentDefault = {
        .tBlendMode            = PL_BLEND_MODE_OPAQUE,
        .tFlags                = PL_MATERIAL_FLAG_CAST_SHADOW | PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW,
        .tShaderType           = PL_SHADER_TYPE_PBR,
        .tBaseColor            = {1.0f, 1.0f, 1.0f, 1.0f},
        .tSheenColor           = {1.0f, 1.0f, 1.0f},
        .tEmissiveColor        = {0.0f, 0.0f, 0.0f, 0.0f},
        .fRoughness            = 1.0f,
        .fClearcoat            = 0.0f,
        .fClearcoatRoughness   = 0.0f,
        .fSheenRoughness       = 0.0f,
        .fMetalness            = 1.0f,
        .fAlphaCutoff          = 0.5f,
        .atTextureMaps         = {0}
    };
    gptMaterialCtx->tComponentType = gptECS->register_type(tMaterialDesc, &tMaterialComponentDefault);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMaterialI tApi = {
        .register_ecs_system = pl_material_register_system,
        .create              = pl_ecs_create_material,
        .get_ecs_type_key    = pl_get_ecs_type_key_material
    };
    pl_set_api(ptApiRegistry, plMaterialI, &tApi);

    gptECS = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptLog = pl_get_api_latest(ptApiRegistry, plLogI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptMaterialCtx = ptDataRegistry->get_data("plMaterialContext");
    }
    else // first load
    {
        static plMaterialContext tCtx = {0};
        gptMaterialCtx = &tCtx;
        ptDataRegistry->set_data("plMaterialContext", gptMaterialCtx);
    }
}

static void
pl_unload_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plMaterialI* ptApi = pl_get_api_latest(ptApiRegistry, plMaterialI);
    ptApiRegistry->remove_api(ptApi);
}