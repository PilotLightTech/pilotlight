/*
   pl_transform_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_transform_ext.h"
#include "pl_math.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_profile_ext.h"
#include "pl_json_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plProfileI* gptProfile = NULL;
    static const plEcsI*     gptEcs     = NULL;
    static const plJsonI*    gptJson    = NULL;
    static const plMemoryI*  gptMemory = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"


//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plHierarchyTraversalEntry
{
    plEntity tEntity;
    plEntity tParent;
} plHierarchyTraversalEntry;

typedef struct _plComponentLibraryHierarchyData
{
    plHierarchyTraversalEntry* sbtTraversalOrder;
    bool                       bDirty;
} plComponentLibraryHierarchyData;

typedef struct _plTransformContext
{
    plEcsTypeKey tTransformComponentType;
    plEcsTypeKey tHierarchyComponentType;
} plTransformContext;

enum
{
    PL_HIERARCHY_VISIT_UNVISITED,
    PL_HIERARCHY_VISIT_VISITING,
    PL_HIERARCHY_VISIT_VISITED
};

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plTransformContext* gptTransformCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool
pl__visit_hierarchy(plComponentLibrary* ptLibrary, plHierarchyComponent* ptComponents, const plEntity* ptEntities,
    uint8_t* auVisitState,uint32_t uIndex, plHierarchyTraversalEntry** psbtTraversalOrder)
{
    // already handled
    if(auVisitState[uIndex] == PL_HIERARCHY_VISIT_VISITED)
        return true;

    // we've reached a node already on our current parent chain:
    //
    // A -> B -> C -> A
    //
    if(auVisitState[uIndex] == PL_HIERARCHY_VISIT_VISITING)
    {
        PL_ASSERT(false && "cycle detected in entity hierarchy");
        return false;
    }

    auVisitState[uIndex] = PL_HIERARCHY_VISIT_VISITING;

    const plEntity tEntity = ptEntities[uIndex];
    const plEntity tParent = ptComponents[uIndex].tParent;

    // entity is actually attached to something
    if(tParent.uIndex != UINT32_MAX)
    {
        if(gptEcs->is_entity_valid(ptLibrary, tParent))
        {
            // Does the parent itself have a hierarchy component?
            //
            // If it does, it must be processed before us.
            // If it doesn't, then it is simply a root as far as the
            // hierarchy traversal is concerned.
            const size_t szParentHierarchyIndex =
                gptEcs->get_index(
                    ptLibrary,
                    gptTransformCtx->tHierarchyComponentType,
                    tParent);

            if(szParentHierarchyIndex != SIZE_MAX)
            {
                if(!pl__visit_hierarchy(
                    ptLibrary,
                    ptComponents,
                    ptEntities,
                    auVisitState,
                    (uint32_t)szParentHierarchyIndex,
                    psbtTraversalOrder))
                {
                    return false;
                }
            }

            // Parent is now guaranteed to have been evaluated before child.
            pl_sb_push(*psbtTraversalOrder, ((plHierarchyTraversalEntry){
                .tEntity = tEntity,
                .tParent = tParent
            }));
        }
        else
        {
            // I'd probably log this eventually:
            // hierarchy references a dead/stale entity.
            PL_ASSERT(false && "hierarchy contains invalid parent entity");
        }
    }

    auVisitState[uIndex] = PL_HIERARCHY_VISIT_VISITED;
    return true;
}

static void
pl__rebuild_hierarchy_order(plComponentLibrary* ptLibrary)
{
    plComponentLibraryHierarchyData* ptData = gptEcs->get_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType);

    pl_sb_reset(ptData->sbtTraversalOrder);

    plHierarchyComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;

    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptTransformCtx->tHierarchyComponentType, (void**)&ptComponents, &ptEntities);

    if(uComponentCount == 0)
        return;

    uint8_t* sbuVisitState = NULL;
    pl_sb_resize(sbuVisitState, uComponentCount);
    memset(sbuVisitState, 0, sizeof(uint8_t) * uComponentCount);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        if(!pl__visit_hierarchy(ptLibrary, ptComponents, ptEntities, sbuVisitState, i, &ptData->sbtTraversalOrder))
        {
            // Invalid hierarchy. Don't leave a partially valid traversal.
            pl_sb_reset(ptData->sbtTraversalOrder);
            break;
        }
    }

    pl_sb_free(sbuVisitState);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl__ecs_hierarchy_init(plComponentLibrary* ptLibrary)
{
    void* pData = PL_ALLOC(sizeof(plComponentLibraryHierarchyData));
    memset(pData, 0, sizeof(plComponentLibraryHierarchyData));
    gptEcs->set_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType, pData);
}

static void
pl__ecs_hierarchy_cleanup(plComponentLibrary* ptLibrary)
{
    plComponentLibraryHierarchyData* ptData = gptEcs->get_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType);
    pl_sb_free(ptData->sbtTraversalOrder);
    PL_FREE(ptData);
    gptEcs->set_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType, NULL);
}

static void
pl__ecs_hierarchy_reset(plComponentLibrary* ptLibrary)
{
    plComponentLibraryHierarchyData* ptData = gptEcs->get_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType);
    pl_sb_reset(ptData->sbtTraversalOrder);
}

static void
pl__ecs_transform_serialize(void* pComponent, plJsonObject* ptJson)
{
    plTransformComponent* ptTransform = pComponent;
    gptJson->add_float_array(ptJson, "translation", ptTransform->tTranslation.d, 3);
    gptJson->add_float_array(ptJson, "rotation", ptTransform->tRotation.d, 4);
    gptJson->add_float_array(ptJson, "scale", ptTransform->tScale.d, 3);
}

static void
pl__ecs_transform_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plTransformComponent* ptTransform = pComponent;
    gptJson->float_array_member(ptJson, "translation", ptTransform->tTranslation.d, NULL);
    gptJson->float_array_member(ptJson, "rotation", ptTransform->tRotation.d, NULL);
    gptJson->float_array_member(ptJson, "scale", ptTransform->tScale.d, NULL);
    ptTransform->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;
}

static void
pl__ecs_hierarchy_serialize(void* pComponent, plJsonObject* ptJson)
{
    plHierarchyComponent* ptComponent = pComponent;
    gptJson->add_uint64_member(ptJson, "parent", ptComponent->tParentId);
}

static void
pl__ecs_hierarchy_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plHierarchyComponent* ptComponent = pComponent;
    char acTempBuffer0[1024] = {0};
    plJsonType tJsonType = gptJson->get_type(gptJson->member(ptJson, "parent"));
    if(tJsonType == PL_JSON_TYPE_NUMBER)
        ptComponent->tParentId = gptJson->uint64_member(ptJson, "parent", 0);
    else if(tJsonType == PL_JSON_TYPE_STRING)
    {
        gptJson->string_member(ptJson, "parent", acTempBuffer0, 1024);
        ptComponent->tParentId = gptEcs->generate_id(NULL, acTempBuffer0, 0);
    }
}

void
pl_transform_register_ecs_components(void)
{
    const plComponentDesc tTransformDesc = {
        .pcDisplayName = "Transform",
        .pcName        = "transform",
        .szSize        = sizeof(plTransformComponent),
        .serialize     = pl__ecs_transform_serialize,
        .deserialize   = pl__ecs_transform_deserialize,
    };

    static plTransformComponent tTransformComponentDefault = {
        .tScale    = {1.0f, 1.0f, 1.0f},
        .tRotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .eFlags    = PL_TRANSFORM_FLAGS_DIRTY
    };
    tTransformComponentDefault.tWorld = pl_identity_mat4();
    gptTransformCtx->tTransformComponentType = gptEcs->register_type(tTransformDesc, &tTransformComponentDefault);

    const plComponentDesc tHierarchyDesc = {
        .pcDisplayName = "Hierarchy",
        .pcName        = "hierarchy",
        .szSize        = sizeof(plHierarchyComponent),
        .init          = pl__ecs_hierarchy_init,
        .cleanup       = pl__ecs_hierarchy_cleanup,
        .reset         = pl__ecs_hierarchy_reset,
        .serialize     = pl__ecs_hierarchy_serialize,
        .deserialize   = pl__ecs_hierarchy_deserialize,
    };
    gptTransformCtx->tHierarchyComponentType = gptEcs->register_type(tHierarchyDesc, NULL);
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

    plHierarchyComponent* ptHierarchyComponent = gptEcs->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tChildEntity);
    if(ptHierarchyComponent)
    {
        plEntity tParentEntity = ptHierarchyComponent->tParent;
        while(tParentEntity.uIndex != 0)
        {
            plTransformComponent* ptParentTransform = gptEcs->get_component(ptLibrary, gptTransformCtx->tTransformComponentType, tParentEntity);
            if(ptParentTransform)
            {
                plMat4 tParentTransform = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);
                tResult = pl_mul_mat4(&tParentTransform, &tResult);
            }

            ptHierarchyComponent = gptEcs->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tParentEntity);
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
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);

    plTransformComponent* ptTransform = gptEcs->add_component(ptLibrary, gptTransformCtx->tTransformComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptTransform;

    return tNewEntity;  
}

void
pl_transform_attach_component(plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(gptEcs->has_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity))
    {
        ptHierarchyComponent = gptEcs->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    else
    {
        ptHierarchyComponent = gptEcs->add_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    ptHierarchyComponent->tParent = tParent;
    ptHierarchyComponent->tParentId = gptEcs->get_entity_id(ptLibrary, tParent);
    plComponentLibraryHierarchyData* ptData = gptEcs->get_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType);
    ptData->bDirty = true;
}

void
pl_transform_detach_component(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(gptEcs->has_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity))
    {
        ptHierarchyComponent = gptEcs->get_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    else
    {
        ptHierarchyComponent = gptEcs->add_component(ptLibrary, gptTransformCtx->tHierarchyComponentType, tEntity);
    }
    ptHierarchyComponent->tParent.uIndex = UINT32_MAX;
    plComponentLibraryHierarchyData* ptData = gptEcs->get_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType);
    ptData->bDirty = true;
}

void
pl_transform_run_transform_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plTransformComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptTransformCtx->tTransformComponentType, (void**)&ptComponents, NULL);

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

    plComponentLibraryHierarchyData* ptData = gptEcs->get_library_type_data(ptLibrary, gptTransformCtx->tHierarchyComponentType);
    if(ptData->bDirty)
    {
        pl__rebuild_hierarchy_order(ptLibrary);
        ptData->bDirty = false;
    }

    const uint32_t uTraversalCount = pl_sb_size(ptData->sbtTraversalOrder);
    for(uint32_t i = 0; i < uTraversalCount; i++)
    {

        const plHierarchyTraversalEntry* ptTraversalEntry = &ptData->sbtTraversalOrder[i];

        plTransformComponent* ptParentTransform = gptEcs->get_component(ptLibrary, gptTransformCtx->tTransformComponentType, ptTraversalEntry->tParent);
        plTransformComponent* ptChildTransform = gptEcs->get_component(ptLibrary, gptTransformCtx->tTransformComponentType, ptTraversalEntry->tEntity);

        if(ptParentTransform && ptChildTransform)
        {
            // ptChildTransform->tWorld = pl_mul_mat4(&ptParentTransform->tWorld, &ptChildTransform->tWorld);
            // ptChildTransform->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;
            const plMat4 tLocal = pl_rotation_translation_scale(ptChildTransform->tRotation, ptChildTransform->tTranslation, ptChildTransform->tScale);
            ptChildTransform->tWorld = pl_mul_mat4(&ptParentTransform->tWorld, &tLocal);
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
        .register_ecs_components     = pl_transform_register_ecs_components,
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

    #ifndef PL_UNITY_BUILD
    gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptEcs     = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptJson    = pl_get_api_latest(ptApiRegistry, plJsonI);
    gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
    #endif

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