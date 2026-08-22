/*
   pl_transform_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_transform_ext.h"
#include "pl_math.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_profile_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plProfileI* gptProfile = NULL;
    static const plEcsI*     gptECS     = NULL;
#endif


//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTransformContext
{
    plEcsTypeKey tTransformComponentType;
    plEcsTypeKey tHierarchyComponentType;
} plTransformContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plTransformContext* gptTransformCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_transform_register_ecs_system(void)
{
    const plComponentDesc tTransformDesc = {
        .pcName = "Transform",
        .szSize = sizeof(plTransformComponent)
    };

    static plTransformComponent tTransformComponentDefault = {
        
        .tScale    = {1.0f, 1.0f, 1.0f},
        .tRotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .eFlags    = PL_TRANSFORM_FLAGS_DIRTY
    };
    tTransformComponentDefault.tWorld = pl_identity_mat4();
    gptTransformCtx->tTransformComponentType = gptECS->register_type(tTransformDesc, &tTransformComponentDefault);

    const plComponentDesc tHierarchyDesc = {
        .pcName = "Hierarchy",
        .szSize = sizeof(plHierarchyComponent)
    };
    gptTransformCtx->tHierarchyComponentType = gptECS->register_type(tHierarchyDesc, NULL);
}

plEcsTypeKey
pl_transform_get_ecs_type_key_transform(void)
{
    return gptTransformCtx->tTransformComponentType;
}

plEcsTypeKey
pl_transform_get_ecs_type_key_hierarchy(void)
{
    return gptTransformCtx->tHierarchyComponentType;
}

plMat4
pl_transform_compute_parent_transform(plComponentLibrary* ptLibrary, plEntity tChildEntity)
{
    plMat4 tResult = pl_identity_mat4();

    plHierarchyComponent* ptHierarchyComponent = gptECS->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tChildEntity);
    if(ptHierarchyComponent)
    {
        plEntity tParentEntity = ptHierarchyComponent->tParent;
        while(tParentEntity.uIndex != 0)
        {
            plTransformComponent* ptParentTransform = gptECS->get_component(ptLibrary, gptTransformCtx->tTransformComponentType, tParentEntity);
            if(ptParentTransform)
            {
                plMat4 tParentTransform = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);
                tResult = pl_mul_mat4(&tParentTransform, &tResult);
            }

            ptHierarchyComponent = gptECS->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tParentEntity);
            if(ptHierarchyComponent)
            {
                tParentEntity = ptHierarchyComponent->tParent;
            }
            else
            {
                break;
            }
        }
    }

    return tResult;
}

plEntity
pl_transform_create_transform(plComponentLibrary* ptLibrary, const char* pcName, plTransformComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed transform";
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plTransformComponent* ptTransform = gptECS->add_component(ptLibrary, gptTransformCtx->tTransformComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptTransform;

    return tNewEntity;  
}

void
pl_transform_attach_component(plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(gptECS->has_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity))
    {
        ptHierarchyComponent = gptECS->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    else
    {
        ptHierarchyComponent = gptECS->add_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    ptHierarchyComponent->tParent = tParent;
}

void
pl_transform_detach_component(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(gptECS->has_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity))
    {
        ptHierarchyComponent = gptECS->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    else
    {
        ptHierarchyComponent = gptECS->add_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    ptHierarchyComponent->tParent.uIndex = UINT32_MAX;
}

void
pl_transform_run_transform_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plTransformComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptTransformCtx->tTransformComponentType, (void**)&ptComponents, NULL);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plTransformComponent* ptTransform = &ptComponents[i];
        if(ptTransform->eFlags & PL_TRANSFORM_FLAGS_DIRTY)
        {
            ptTransform->tWorld = pl_rotation_translation_scale(ptTransform->tRotation, ptTransform->tTranslation, ptTransform->tScale);
            ptTransform->eFlags &= ~PL_TRANSFORM_FLAGS_DIRTY;
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_transform_run_hierarchy_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plHierarchyComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptTransformCtx->tHierarchyComponentType, (void**)&ptComponents, &ptEntities);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        const plEntity tChildEntity = ptEntities[i];
        plHierarchyComponent* ptHierarchyComponent = gptECS->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tChildEntity);
        plTransformComponent* ptParentTransform = gptECS->get_component(ptLibrary, gptTransformCtx->tTransformComponentType, ptHierarchyComponent->tParent);
        plTransformComponent* ptChildTransform = gptECS->get_component(ptLibrary, gptTransformCtx->tTransformComponentType, tChildEntity);
        if(ptParentTransform && ptChildTransform)
        {
            ptChildTransform->tWorld = pl_mul_mat4(&ptParentTransform->tWorld, &ptChildTransform->tWorld);
            ptChildTransform->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_transform_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTransformI tApi = {
        .register_ecs_system         = pl_transform_register_ecs_system,
        .create_transform            = pl_transform_create_transform,
        .attach_component            = pl_transform_attach_component,
        .detach_component            = pl_transform_detach_component,
        .compute_parent_transform    = pl_transform_compute_parent_transform,
        .run_transform_update_system = pl_transform_run_transform_update_system,
        .run_hierarchy_update_system = pl_transform_run_hierarchy_update_system,
        .get_ecs_type_key_transform  = pl_transform_get_ecs_type_key_transform,
        .get_ecs_type_key_hierarchy  = pl_transform_get_ecs_type_key_hierarchy
    };
    pl_set_api(ptApiRegistry, plTransformI, &tApi);

    gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptECS     = pl_get_api_latest(ptApiRegistry, plEcsI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptTransformCtx = ptDataRegistry->get_data("plTransformContext");
    }
    else // first load
    {

        static plTransformContext tCtx = {0};
        gptTransformCtx = &tCtx;
        ptDataRegistry->set_data("plTransformContext", gptTransformCtx);
    }
}

void
pl_unload_transform_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plTransformI* ptApi = pl_get_api_latest(ptApiRegistry, plTransformI);
    ptApiRegistry->remove_api(ptApi);
}