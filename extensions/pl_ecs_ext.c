/*
   pl_ecs_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_ecs_ext.h"
#include "pl_math.h"

// extensions
#include "pl_job_ext.h"
#include "pl_script_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"

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

    static plApiRegistryI*             gptApiRegistry       = NULL;
    static const plExtensionRegistryI* gptExtensionRegistry = NULL;
    static const plJobI*               gptJob               = NULL;
    static const plProfileI*           gptProfile           = NULL;
    static const plLogI*               gptLog               = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plComponentManager
{
    plComponentLibrary* ptParentLibrary;
    plEntity*           sbtEntities; // aligned with pComponents
    uint32_t            uCount;
    uint32_t            uCapacity;
    size_t              szSize;
    void*               pComponents; // aligned with sbtEntites
    void*               pInternal;
} plComponentManager;

typedef struct _plComponentLibrary
{
    // [INTERNAL]
    uint32_t*           _sbtEntityGenerations;
    uint32_t*           _sbtEntityFreeIndices;
    plHashMap*          _atHashmaps; // map entity -> index in sbtEntities/pComponents
    plComponentManager* _sbtManagers; // just for internal convenience
} plComponentLibrary;

typedef struct _plEcsContext
{
    bool                bFinalized;
    uint64_t            uLogChannel;
    plComponentDesc*    sbtComponentDescriptions;
    plEcsTypeKey        tTagComponentType;
    plEcsTypeKey        tLayerComponentType;
    plEcsTypeKey        tScriptComponentType;
    plEcsTypeKey        tTransformComponentType;
    plEcsTypeKey        tHierarchyComponentType;
    plComponentLibrary* ptDefaultLibrary;
} plEcsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plEcsContext* gptEcsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline bool
pl_ecs_has_entity(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;
    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    return pl_hm_has_key(&ptLibrary->_atHashmaps[tType], tEntity.uIndex);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

plEcsTypeKey
pl_ecs_register_type(plComponentDesc tDesc, const void* pTemplate)
{
    PL_ASSERT(!gptEcsCtx->bFinalized && "ECS setup already finalized!");
    
    if(gptEcsCtx->bFinalized)
        return UINT32_MAX;
        
    tDesc._pTemplate = pTemplate;
    pl_sb_push(gptEcsCtx->sbtComponentDescriptions, tDesc);
    return pl_sb_size(gptEcsCtx->sbtComponentDescriptions) - 1;
}

void
pl_ecs_initialize(plEcsInit tInit)
{
    gptEcsCtx->tTagComponentType = pl_ecs_register_type((plComponentDesc){
        .pcName = "Tag",
        .szSize = sizeof(plTagComponent)
    }, NULL);

    plLayerComponent tLayerComponentDefault = {
        .uLayerMask = ~0u,
        ._uPropagationMask = ~0u
    };
    gptEcsCtx->tLayerComponentType = pl_ecs_register_type((plComponentDesc){
        .pcName = "Layer",
        .szSize = sizeof(plLayerComponent)
    }, &tLayerComponentDefault);

    const plComponentDesc tTransformDesc = {
        .pcName = "Transform",
        .szSize = sizeof(plTransformComponent)
    };

    static plTransformComponent tTransformComponentDefault = {
        
        .tScale    = {1.0f, 1.0f, 1.0f},
        .tRotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .tFlags    = PL_TRANSFORM_FLAGS_DIRTY
    };
    tTransformComponentDefault.tWorld = pl_identity_mat4();
    gptEcsCtx->tTransformComponentType = pl_ecs_register_type(tTransformDesc, &tTransformComponentDefault);

    const plComponentDesc tHierarchyDesc = {
        .pcName = "Hierarchy",
        .szSize = sizeof(plHierarchyComponent)
    };
    gptEcsCtx->tHierarchyComponentType = pl_ecs_register_type(tHierarchyDesc, NULL);

    const plComponentDesc tScriptDesc = {
        .pcName = "Script",
        .szSize = sizeof(plScriptComponent)
    };
    gptEcsCtx->tScriptComponentType = pl_ecs_register_type(tScriptDesc, NULL);

}

plEcsTypeKey
pl_ecs_get_ecs_type_key_tag(void)
{
    return gptEcsCtx->tTagComponentType;
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_layer(void)
{
    return gptEcsCtx->tLayerComponentType;
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_transform(void)
{
    return gptEcsCtx->tTransformComponentType;
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_hierarchy(void)
{
    return gptEcsCtx->tHierarchyComponentType;
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_script(void)
{
    return gptEcsCtx->tScriptComponentType;
}

plComponentLibrary*
pl_ecs_get_default_library(void)
{
    return gptEcsCtx->ptDefaultLibrary;
}

bool
pl_ecs_create_library(plComponentLibrary** pptLibrary)
{

    PL_ASSERT(gptEcsCtx->bFinalized && "ECS not finalized");
    if(!gptEcsCtx->bFinalized)
        return false;

    plComponentLibrary* ptLibrary = PL_ALLOC(sizeof(plComponentLibrary));
    memset(ptLibrary, 0, sizeof(plComponentLibrary));
    *pptLibrary = ptLibrary;

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);

    ptLibrary->_atHashmaps = PL_ALLOC(sizeof(plHashMap64) * (uComponentTypeCount + 1));
    memset(ptLibrary->_atHashmaps, 0, sizeof(plHashMap64) * (uComponentTypeCount + 1));

    pl_sb_resize(ptLibrary->_sbtManagers, pl_sb_size(gptEcsCtx->sbtComponentDescriptions));

    // initialize component managers

    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        ptLibrary->_sbtManagers[i].szSize = gptEcsCtx->sbtComponentDescriptions[i].szSize;
        ptLibrary->_sbtManagers[i].ptParentLibrary = ptLibrary;
        if(gptEcsCtx->sbtComponentDescriptions[i].init)
            gptEcsCtx->sbtComponentDescriptions[i].init(ptLibrary);
    }

    pl_sb_push(ptLibrary->_sbtEntityGenerations, UINT32_MAX-1);

    pl_log_info(gptLog, gptEcsCtx->uLogChannel, "initialized component library");

    return true;
}

void
pl_ecs_finalize(void)
{
    gptEcsCtx->bFinalized = true;
    pl_ecs_create_library(&gptEcsCtx->ptDefaultLibrary);
}


void
pl_ecs_set_internal_data(plComponentLibrary* ptLibrary, plEcsTypeKey tType, void* pData)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    ptLibrary->_sbtManagers[tType].pInternal = pData;
}

void*
pl_ecs_get_internal_data(plComponentLibrary* ptLibrary, plEcsTypeKey tType)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;
    return ptLibrary->_sbtManagers[tType].pInternal;
}

void
pl_ecs_reset_library(plComponentLibrary* ptLibrary)
{

    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(gptEcsCtx->sbtComponentDescriptions[i].reset)
            gptEcsCtx->sbtComponentDescriptions[i].reset(ptLibrary);

        ptLibrary->_sbtManagers[i].uCount = 0;
        pl_sb_reset(ptLibrary->_sbtManagers[i].sbtEntities);
        pl_hm_free(&ptLibrary->_atHashmaps[i]);
    }
    pl_hm_free(&ptLibrary->_atHashmaps[uComponentTypeCount]);
    // general
    pl_sb_reset(ptLibrary->_sbtEntityFreeIndices);
    pl_sb_reset(ptLibrary->_sbtEntityGenerations);
    pl_sb_push(ptLibrary->_sbtEntityGenerations, UINT32_MAX-1);
}

void
pl_ecs_cleanup_library(plComponentLibrary** pptLibrary)
{
    plComponentLibrary* ptLibrary = *pptLibrary;
    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(gptEcsCtx->sbtComponentDescriptions[i].cleanup)
            gptEcsCtx->sbtComponentDescriptions[i].cleanup(ptLibrary);

        ptLibrary->_sbtManagers[i].uCount = 0;
        ptLibrary->_sbtManagers[i].uCapacity = 0;
        if(ptLibrary->_sbtManagers[i].pComponents)
        {
            PL_FREE(ptLibrary->_sbtManagers[i].pComponents);
            ptLibrary->_sbtManagers[i].pComponents = NULL;
        }
        pl_sb_free(ptLibrary->_sbtManagers[i].sbtEntities);
        pl_hm_free(&ptLibrary->_atHashmaps[i]);
    }
    pl_hm_free(&ptLibrary->_atHashmaps[uComponentTypeCount]);

    // general
    pl_sb_free(ptLibrary->_sbtManagers);
    pl_sb_free(ptLibrary->_sbtEntityFreeIndices);
    pl_sb_free(ptLibrary->_sbtEntityGenerations);
    PL_FREE(ptLibrary->_atHashmaps);

    PL_FREE(ptLibrary);
    *pptLibrary = NULL;
}

void
pl_ecs_cleanup(void)
{
    pl_ecs_cleanup_library(&gptEcsCtx->ptDefaultLibrary);
    pl_sb_free(gptEcsCtx->sbtComponentDescriptions);
}

bool
pl_ecs_is_entity_valid(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    if(tEntity.uIndex == UINT32_MAX)
        return false;
    return ptLibrary->_sbtEntityGenerations[tEntity.uIndex] == tEntity.uGeneration;
}

plEntity
pl_ecs_get_entity(plComponentLibrary* ptLibrary, const char* pcName)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    const uint64_t ulHash = pl_hm_hash_str(pcName, 0);
    uint64_t uIndex = 0;
    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    if(pl_hm_has_key_ex(&ptLibrary->_atHashmaps[uComponentTypeCount], ulHash, &uIndex))
    {
        return (plEntity){.uIndex = (uint32_t)uIndex, .uGeneration = ptLibrary->_sbtEntityGenerations[uIndex]};
    }
    return (plEntity){UINT32_MAX, UINT32_MAX};
}

plEntity
pl_ecs_get_current_entity(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;
    tEntity.uGeneration = ptLibrary->_sbtEntityGenerations[tEntity.uIndex];
    return tEntity;
}

size_t
pl_ecs_get_index(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{ 
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    size_t szIndex = pl_hm_lookup(&ptLibrary->_atHashmaps[tType], (uint64_t)tEntity.uIndex);
    return szIndex;
}

void*
pl_ecs_get_component(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX)
        return NULL;

    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    plComponentManager* ptManager = &ptLibrary->_sbtManagers[tType];

    if(ptLibrary->_sbtEntityGenerations[tEntity.uIndex] != tEntity.uGeneration)
        return NULL;

    size_t szIndex = pl_ecs_get_index(ptLibrary, tType, tEntity);

    if(szIndex == UINT64_MAX)
        return NULL;

    unsigned char* pucData = ptManager->pComponents;
    return &pucData[szIndex * gptEcsCtx->sbtComponentDescriptions[tType].szSize];
}

void
pl_ecs_remove_entity(plComponentLibrary* ptLibrary, plEntity tEntity)
{

    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    pl_sb_push(ptLibrary->_sbtEntityFreeIndices, tEntity.uIndex);

    // remove from tag hashmap
    plTagComponent* ptTag = pl_ecs_get_component(ptLibrary, gptEcsCtx->tTagComponentType, tEntity);
    if(ptTag)
    {
        pl_hm_remove_str(&ptLibrary->_atHashmaps[uComponentTypeCount], ptTag->acName);
    }

    ptLibrary->_sbtEntityGenerations[tEntity.uIndex]++;

    // remove from individual managers
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(pl_hm_has_key(&ptLibrary->_atHashmaps[i], tEntity.uIndex))
        {
            pl_hm_remove(&ptLibrary->_atHashmaps[i], tEntity.uIndex);
            const uint64_t uEntityValue = pl_hm_get_free_index(&ptLibrary->_atHashmaps[i]);
            
            // must keep valid entities contiguous (move last entity into removed slot)
            if(pl_sb_size(ptLibrary->_sbtManagers[i].sbtEntities) > 1)
            {
                plEntity tLastEntity = pl_sb_back(ptLibrary->_sbtManagers[i].sbtEntities);
                pl_hm_remove(&ptLibrary->_atHashmaps[i], tLastEntity.uIndex);
                uint64_t _unUsed = pl_hm_get_free_index(&ptLibrary->_atHashmaps[i]); // burn slot
                pl_hm_insert(&ptLibrary->_atHashmaps[i], tLastEntity.uIndex, uEntityValue);
            }

            pl_sb_del_swap(ptLibrary->_sbtManagers[i].sbtEntities, uEntityValue);

            memmove(
                &((char*)ptLibrary->_sbtManagers[i].pComponents)[ptLibrary->_sbtManagers[i].szSize * uEntityValue],
                &((char*)ptLibrary->_sbtManagers[i].pComponents)[ptLibrary->_sbtManagers[i].szSize * ptLibrary->_sbtManagers[i].uCount],
                ptLibrary->_sbtManagers[i].szSize
            );
            ptLibrary->_sbtManagers[i].uCount--;
        }
    }
}

bool
pl_ecs_has_component(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    return pl_ecs_has_entity(ptLibrary, tType, tEntity);
}

uint32_t
pl_ecs_get_components(plComponentLibrary* ptLibrary, plEcsTypeKey tType, void** ppComponentsOut, const plEntity** pptEntitiesOut)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    plComponentManager* ptManager = &ptLibrary->_sbtManagers[tType];
    
    if(ppComponentsOut)
    {
        *ppComponentsOut = ptManager->pComponents;
    }

    if(pptEntitiesOut)
    {
        *pptEntitiesOut = ptManager->sbtEntities;
    }

    return ptManager->uCount;
}

void*
pl_ecs_add_component(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX)
        return NULL;

    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    plComponentManager* ptManager = &ptLibrary->_sbtManagers[tType];

    if(ptManager->ptParentLibrary->_sbtEntityGenerations[tEntity.uIndex] != tEntity.uGeneration)
        return NULL;

    uint64_t uComponentIndex = pl_hm_get_free_index(&ptLibrary->_atHashmaps[tType]);
    bool bAddSlot = false; // can't add component with SB without correct type
    if(uComponentIndex == UINT64_MAX)
    {
        uComponentIndex = pl_sb_size(ptManager->sbtEntities);
        pl_sb_add(ptManager->sbtEntities);
        bAddSlot = true;
    }
    pl_hm_insert(&ptLibrary->_atHashmaps[tType], (uint64_t)tEntity.uIndex, uComponentIndex);

    ptManager->sbtEntities[uComponentIndex] = tEntity;

    
    if(bAddSlot)
    {
        ptManager->uCount++;
        if(ptManager->uCapacity == 0) // first allocation
        {
            ptManager->uCapacity = 16;
            ptManager->pComponents = PL_ALLOC(ptManager->szSize * ptManager->uCapacity);
            memset(ptManager->pComponents, 0, ptManager->szSize * ptManager->uCapacity);
        }

        if(ptManager->uCount > ptManager->uCapacity) // need to grow
        {
            void* pOldComponents = ptManager->pComponents;
            ptManager->pComponents = PL_ALLOC(ptManager->szSize * ptManager->uCapacity * 2);
            memset(ptManager->pComponents, 0, ptManager->szSize * ptManager->uCapacity * 2);
            memcpy(ptManager->pComponents, pOldComponents, ptManager->szSize * ptManager->uCapacity);
            PL_FREE(pOldComponents);
            ptManager->uCapacity *= 2;
        }
    }
    char* pNewComponent = &((char*)ptManager->pComponents)[ptManager->szSize * uComponentIndex];
    if(gptEcsCtx->sbtComponentDescriptions[tType]._pTemplate)
        memcpy(pNewComponent, gptEcsCtx->sbtComponentDescriptions[tType]._pTemplate, ptManager->szSize);
    return pNewComponent;
}

plEntity
pl_ecs_create_entity(plComponentLibrary* ptLibrary, const char* pcName)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    plEntity tNewEntity = {0};
    if(pl_sb_size(ptLibrary->_sbtEntityFreeIndices) > 0) // free slot available
    {
        tNewEntity.uIndex = pl_sb_pop(ptLibrary->_sbtEntityFreeIndices);
        tNewEntity.uGeneration = ptLibrary->_sbtEntityGenerations[tNewEntity.uIndex];
    }
    else // create new slot
    {
        tNewEntity.uIndex = pl_sb_size(ptLibrary->_sbtEntityGenerations);
        pl_sb_push(ptLibrary->_sbtEntityGenerations, 0);
    }

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);

    plTagComponent* ptTag = pl_ecs_add_component(ptLibrary, gptEcsCtx->tTagComponentType, tNewEntity);
    if(pcName)
        strncpy(ptTag->acName, pcName, 128);
    else
        strncpy(ptTag->acName, "unnamed", 128);

    if(pcName)
        pl_hm_insert_str(&ptLibrary->_atHashmaps[uComponentTypeCount], pcName, tNewEntity.uIndex);

    return tNewEntity;
}

plEntity
pl_ecs_create_script(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plScriptComponent** pptCompOut)
{
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    pl_log_debug_f(gptLog, gptEcsCtx->uLogChannel, "created script: '%s'", pcFile);
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary, pcFile);
    plScriptComponent* ptScript =  pl_ecs_add_component(ptLibrary, gptEcsCtx->tScriptComponentType, tNewEntity);
    ptScript->tFlags = tFlags;
    strncpy(ptScript->acFile, pcFile, PL_MAX_PATH_LENGTH);

    gptExtensionRegistry->load(pcFile, "pl_load_script", "pl_unload_script", tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptI* ptScriptApi = gptApiRegistry->get_api(pcFile, (plVersion)plScriptI_version);
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
    if(ptLibrary == NULL)
        ptLibrary = gptEcsCtx->ptDefaultLibrary;

    pl_log_debug_f(gptLog, gptEcsCtx->uLogChannel, "attach script: '%s'", pcFile);
    plScriptComponent* ptScript =  pl_ecs_add_component(ptLibrary, gptEcsCtx->tScriptComponentType, tEntity);
    ptScript->tFlags = tFlags;
    strncpy(ptScript->acFile, pcFile, PL_MAX_NAME_LENGTH);

    gptExtensionRegistry->load(pcFile, "pl_load_script", "pl_unload_script", tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptI* ptScriptApi = gptApiRegistry->get_api(pcFile, (plVersion)plScriptI_version);
    ptScript->_ptApi = ptScriptApi;
    PL_ASSERT(ptScriptApi->run);

    if(ptScriptApi->setup)
        ptScriptApi->setup(ptLibrary, tEntity);

    if(pptCompOut)
        *pptCompOut = ptScript;
}

plMat4
pl_ecs_compute_parent_transform(plComponentLibrary* ptLibrary, plEntity tChildEntity)
{
    plMat4 tResult = pl_identity_mat4();

    plHierarchyComponent* ptHierarchyComponent = pl_ecs_get_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tChildEntity);
    if(ptHierarchyComponent)
    {
        plEntity tParentEntity = ptHierarchyComponent->tParent;
        while(tParentEntity.uIndex != 0)
        {
            plTransformComponent* ptParentTransform = pl_ecs_get_component(ptLibrary, gptEcsCtx->tTransformComponentType, tParentEntity);
            if(ptParentTransform)
            {
                plMat4 tParentTransform = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);
                tResult = pl_mul_mat4(&tParentTransform, &tResult);
            }

            ptHierarchyComponent = pl_ecs_get_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tParentEntity);
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
pl_ecs_create_transform(plComponentLibrary* ptLibrary, const char* pcName, plTransformComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed transform";
    pl_log_debug_f(gptLog, gptEcsCtx->uLogChannel, "created transform: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary, pcName);

    plTransformComponent* ptTransform = pl_ecs_add_component(ptLibrary, gptEcsCtx->tTransformComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptTransform;

    return tNewEntity;  
}

void
pl_ecs_attach_component(plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tEntity);
    }
    else
    {
        ptHierarchyComponent = pl_ecs_add_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tEntity);
    }
    ptHierarchyComponent->tParent = tParent;
}

void
pl_ecs_deattach_component(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tEntity);
    }
    else
    {
        ptHierarchyComponent = pl_ecs_add_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tEntity);
    }
    ptHierarchyComponent->tParent.uIndex = UINT32_MAX;
}

void
pl_run_transform_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plTransformComponent* ptComponents = NULL;
    const uint32_t uComponentCount = pl_ecs_get_components(ptLibrary, gptEcsCtx->tTransformComponentType, (void**)&ptComponents, NULL);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plTransformComponent* ptTransform = &ptComponents[i];
        if(ptTransform->tFlags & PL_TRANSFORM_FLAGS_DIRTY)
        {
            ptTransform->tWorld = pl_rotation_translation_scale(ptTransform->tRotation, ptTransform->tTranslation, ptTransform->tScale);
            ptTransform->tFlags &= ~PL_TRANSFORM_FLAGS_DIRTY;
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_run_hierarchy_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plHierarchyComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = pl_ecs_get_components(ptLibrary, gptEcsCtx->tHierarchyComponentType, (void**)&ptComponents, &ptEntities);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        const plEntity tChildEntity = ptEntities[i];
        plHierarchyComponent* ptHierarchyComponent = pl_ecs_get_component(ptLibrary, gptEcsCtx->tHierarchyComponentType, tChildEntity);
        plTransformComponent* ptParentTransform = pl_ecs_get_component(ptLibrary, gptEcsCtx->tTransformComponentType, ptHierarchyComponent->tParent);
        plTransformComponent* ptChildTransform = pl_ecs_get_component(ptLibrary, gptEcsCtx->tTransformComponentType, tChildEntity);
        if(ptParentTransform && ptChildTransform)
        {
            ptChildTransform->tWorld = pl_mul_mat4(&ptParentTransform->tWorld, &ptChildTransform->tWorld);
            ptChildTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_run_script_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plScriptComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = pl_ecs_get_components(ptLibrary, gptEcsCtx->tScriptComponentType, (void**)&ptComponents, &ptEntities);

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

uint64_t
pl_ecs_get_log_channel(void)
{
    return gptEcsCtx->uLogChannel;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plEcsI tApi = {
        .initialize                  = pl_ecs_initialize,
        .finalize                    = pl_ecs_finalize,
        .cleanup                     = pl_ecs_cleanup,
        .create_library              = pl_ecs_create_library,
        .get_default_library         = pl_ecs_get_default_library,
        .cleanup_library             = pl_ecs_cleanup_library,
        .reset_library               = pl_ecs_reset_library,
        .register_type               = pl_ecs_register_type,
        .remove_entity               = pl_ecs_remove_entity,
        .get_entity_by_name          = pl_ecs_get_entity,
        .get_current_entity          = pl_ecs_get_current_entity,
        .is_entity_valid             = pl_ecs_is_entity_valid,
        .has_component               = pl_ecs_has_component,
        .get_index                   = pl_ecs_get_index,
        .get_components              = pl_ecs_get_components,
        .get_log_channel             = pl_ecs_get_log_channel,
        .get_component               = pl_ecs_get_component,
        .add_component               = pl_ecs_add_component,
        .set_library_type_data       = pl_ecs_set_internal_data,
        .get_library_type_data       = pl_ecs_get_internal_data,
        .create_entity               = pl_ecs_create_entity,
        .get_ecs_type_key_tag        = pl_ecs_get_ecs_type_key_tag,
        .get_ecs_type_key_layer      = pl_ecs_get_ecs_type_key_layer,
        .create_transform            = pl_ecs_create_transform,
        .create_script               = pl_ecs_create_script,
        .attach_script               = pl_ecs_attach_script,
        .attach_component            = pl_ecs_attach_component,
        .deattach_component          = pl_ecs_deattach_component,
        .compute_parent_transform    = pl_ecs_compute_parent_transform,
        .run_transform_update_system = pl_run_transform_update_system,
        .run_hierarchy_update_system = pl_run_hierarchy_update_system,
        .run_script_update_system    = pl_run_script_update_system,
        .get_ecs_type_key_transform  = pl_ecs_get_ecs_type_key_transform,
        .get_ecs_type_key_hierarchy  = pl_ecs_get_ecs_type_key_hierarchy,
        .get_ecs_type_key_script     = pl_ecs_get_ecs_type_key_script,
    };
    pl_set_api(ptApiRegistry, plEcsI, &tApi);

    gptApiRegistry       = ptApiRegistry;
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptMemory            = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptJob               = pl_get_api_latest(ptApiRegistry, plJobI);
    gptProfile           = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog               = pl_get_api_latest(ptApiRegistry, plLogI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptEcsCtx->uLogChannel = gptLog->get_channel_id("ECS");
        gptEcsCtx = ptDataRegistry->get_data("plEcsContext");
    }
    else // first load
    {

        static plEcsContext tCtx = {0};
        gptEcsCtx = &tCtx;

        plLogExtChannelInit tLogInit = {
            .tType       = PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER,
            .uEntryCount = 256
        };
        gptEcsCtx->uLogChannel = gptLog->add_channel("ECS", tLogInit);
        ptDataRegistry->set_data("plEcsContext", gptEcsCtx);
    }
}

static void
pl_unload_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plEcsI* ptApi = pl_get_api_latest(ptApiRegistry, plEcsI);
    ptApiRegistry->remove_api(ptApi);
}