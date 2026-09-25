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
// [SECTION] unity build
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
#include "pl_script_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_platform_ext.h"
#include "pl_json_ext.h"
#include "pl_asset_ext.h"
#include "pl_vfs_ext.h"

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

    static const plProfileI*      gptProfile = NULL;
    static const plLogI*          gptLog     = NULL;
    static const plStringInternI* gptString  = NULL;
    static const plTimerI*        gptTimer   = NULL;
    static const plJsonI*         gptJson    = NULL;
    static const plIOI*           gptIOI     = NULL;
    static const plVfsI*          gptVfs     = NULL;
    static plIO*                  gptIO      = NULL;
    static const plAssetI*        gptAsset   = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plComponentManagerData
{
    plEcsLibraryDataCleanup tCleanup;
    void* pInternal;
} plComponentManagerData;

typedef struct _plComponentManager
{
    plHashMap              tHashmap; // map entity -> index in sbtEntities/pComponents
    plEntity*              sbtEntities; // aligned with pComponents
    uint32_t               uCapacity;
    void*                  pComponents; // aligned with sbtEntites
    plComponentManagerData tUserManagerData;
} plComponentManager;

typedef struct _plEntityData
{
    uint32_t   uGeneration;
    plEntityId tId;
    bool       bNotOwned;
} plEntityData;

typedef struct _plComponentLibrary
{
    plEcsChange*  sbtChanges;
    plEcsTypeKey* sbtChangeComponentTypes;

    // [INTERNAL]
    plHashMap           _tIdHashmap;
    plEntityData*       _sbtEntityData;
    uint32_t*           _sbtEntityFreeIndices;
    plComponentManager* _atManagers; // just for internal convenience
} plComponentLibrary;

typedef struct _plEcsContext
{
    bool                 bFinalized;
    uint64_t             uLogChannel;
    plComponentDesc*     sbtComponentDescriptions;
    plEcsTypeKey         tTagComponentType;
    plEcsTypeKey         tLibraryComponentType;
    uint64_t             uRandomSeed;
    plAssetTypeKey       tAssetTypeKey;
} plEcsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plEcsContext* gptEcsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// tag component stuff
static void pl__ecs_tag_destroy    (void*, const plComponentLibrary*);
static void pl__ecs_tag_serialize  (void*, const plComponentLibrary*, plEntityId, plJsonObject*);
static void pl__ecs_tag_deserialize(plJsonObject*, plComponentLibrary*, plEntityId, void*);
static void pl__ecs_tag_clone      (const void*, plComponentLibrary*, void*, plComponentLibrary*);

// library component stuff
static void pl__ecs_library_clone      (const void*, plComponentLibrary*, void*, plComponentLibrary*);
static void pl__ecs_library_destroy    (void*, const plComponentLibrary*);
static void pl__ecs_library_resolve    (plComponentLibrary*, plEntityId, plHashMap64*, void*);
static void pl__ecs_library_serialize  (void*, const plComponentLibrary*, plEntityId, plJsonObject*);
static void pl__ecs_library_deserialize(plJsonObject*, plComponentLibrary*, plEntityId, void*);

// library asset stuff
static bool pl__ecs_asset_serialize  (const char*, const void*, plAssetEncoding);
static bool pl__ecs_asset_deserialize(const char*, void*);
static void pl__ecs_destroy          (void* pLibrary);

// library helpers
static void pl__ecs_resolve_components  (plComponentLibrary*, plEntity, plHashMap64*);
static void pl__scene_ecs_add_components(plJsonObject*, plComponentLibrary*, plEntity);

// forward declarations
void       pl_ecs_get_entities(const plComponentLibrary*, plEntity*, uint32_t*);
uint32_t   pl_ecs_get_type_descriptions(const plComponentDesc**);
plEntityId pl_ecs_get_entity_id        (const plComponentLibrary*, plEntity);
plEntity   pl_ecs_get_entity_by_id     (const plComponentLibrary*, plEntityId);
void*      pl_ecs_add_component (plComponentLibrary*, plEcsTypeKey, plEntity);
plEntity   pl_ecs_create_entity_with_id(plComponentLibrary*, const char* name, plEntityId);
void*      pl_ecs_get_component (const plComponentLibrary*, plEcsTypeKey, plEntity);
plEntity   pl_ecs_create_entity        (plComponentLibrary*, const char* name);
void       pl_ecs_init_library   (plComponentLibrary*);
void       pl_ecs_cleanup_library(plComponentLibrary*);

bool
pl_ecs_is_entity_valid(const plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX || tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return false;
    const plEntityData* ptData = &ptLibrary->_sbtEntityData[tEntity.uIndex];
    return ptData->tId != 0 && ptData->uGeneration == tEntity.uGeneration;
}

static inline bool
pl_ecs_has_entity(const plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(!pl_ecs_is_entity_valid(ptLibrary, tEntity))
        return false;

    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    return pl_hm_has_key(&ptLibrary->_atManagers[tType].tHashmap, tEntity.uIndex);
}

static inline uint64_t
pl__splitmix64(uint64_t* pulState)
{
    uint64_t z = (*pulState += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static inline uint64_t
pl__generate_entity_id(plComponentLibrary* ptLibrary)
{
    uint64_t ulId;
    do
    {
        ulId = pl__splitmix64(&gptEcsCtx->uRandomSeed);
    }
    while(ulId == 0 || pl_hm_has_key(&ptLibrary->_tIdHashmap, ulId));

    return ulId;
}
 
//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_ecs_merge_library(plComponentLibrary* ptLibrary, plComponentLibrary* ptPatch)
{
    uint32_t uPatchEntityCount = 0;
    plEntity* atPatchEntities = NULL;
    pl_ecs_get_entities(ptPatch, atPatchEntities, &uPatchEntityCount);
    atPatchEntities = PL_ALLOC(uPatchEntityCount * sizeof(plEntity));
    pl_ecs_get_entities(ptPatch, atPatchEntities, &uPatchEntityCount);

    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

    for(uint32_t uEntityIndex = 0; uEntityIndex < uPatchEntityCount; uEntityIndex++)
    {
        plEntity tPatchEntity = atPatchEntities[uEntityIndex];
        plEntityId tEntityId = pl_ecs_get_entity_id(ptPatch, tPatchEntity);
        plEntity tDestEntity = pl_ecs_get_entity_by_id(ptLibrary, tEntityId);

        // if(ptPatch->_sbtEntityData[tPatchEntity.uIndex].bNotOwned)
        //     continue;

        bool bEntityExists = pl_ecs_is_entity_valid(ptLibrary, tDestEntity);

        if(!bEntityExists)
        {
            plTagComponent* ptTag = pl_ecs_get_component(ptPatch, gptEcsCtx->tTagComponentType, tPatchEntity);
            tDestEntity = pl_ecs_create_entity_with_id(ptLibrary, ptTag ? ptTag->pcName : NULL, tEntityId);
        }
        ptLibrary->_sbtEntityData[tDestEntity.uIndex].bNotOwned = ptPatch->_sbtEntityData[tPatchEntity.uIndex].bNotOwned;

        for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
        {
            const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];

            void* pComponentECSData = pl_ecs_get_component(ptPatch, ptCompDesc->tTypeKey, tPatchEntity);
            if(pComponentECSData == NULL)
                continue;

            void* pDestComponentECSData = pl_ecs_get_component(ptLibrary, ptCompDesc->tTypeKey, tDestEntity);

            if(pDestComponentECSData == NULL) // adding
                pDestComponentECSData = pl_ecs_add_component(ptLibrary, ptCompDesc->tTypeKey, tDestEntity);
            else if(ptCompDesc->destroy)
            {
                ptCompDesc->destroy(pDestComponentECSData, ptLibrary);
            }

            if(ptCompDesc->clone)
            {
                ptCompDesc->clone(pComponentECSData, ptPatch, pDestComponentECSData, ptLibrary);
            }
            else
            {
                memcpy(pDestComponentECSData, pComponentECSData, ptCompDesc->szSize);
            }  
        }
    }

    PL_FREE(atPatchEntities);
    return true;
}

bool
pl_ecs_clone_library_into(plComponentLibrary* ptLibrary, plComponentLibrary* ptSource, plHashMap64* ptEntityLookup)
{
    uint32_t uSourceEntityCount = 0;
    plEntity* atSourceEntities = NULL;
    pl_ecs_get_entities(ptSource, atSourceEntities, &uSourceEntityCount);
    atSourceEntities = PL_ALLOC(uSourceEntityCount * sizeof(plEntity));
    pl_ecs_get_entities(ptSource, atSourceEntities, &uSourceEntityCount);

    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

    for(uint32_t uEntityIndex = 0; uEntityIndex < uSourceEntityCount; uEntityIndex++)
    {
        plEntity tSourceEntity = atSourceEntities[uEntityIndex];

        if(ptSource->_sbtEntityData[tSourceEntity.uIndex].bNotOwned)
            continue;

        plEntityId tSourceEntityId = pl_ecs_get_entity_id(ptSource, tSourceEntity);
        plTagComponent* ptTagComp = pl_ecs_get_component(ptSource, gptEcsCtx->tTagComponentType, tSourceEntity);

        plEntity tNewEntity = pl_ecs_create_entity(ptLibrary, ptTagComp ? ptTagComp->pcName : NULL);
        plEntityId tNewEntityId = pl_ecs_get_entity_id(ptLibrary, tNewEntity);

        if(ptEntityLookup)
        {
            pl_hm_insert(ptEntityLookup, tSourceEntityId, pl_ecs_get_entity_id(ptLibrary, tNewEntity));
        }

        for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
        {
            const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];

            // if(ptCompDesc->tTypeKey == gptEcsCtx->tTagComponentType)
            //     continue;

            void* pComponentECSData = pl_ecs_get_component(ptSource, ptCompDesc->tTypeKey, tSourceEntity);
            if(pComponentECSData == NULL)
                continue;

            void* pDestComponentECSData = pl_ecs_add_component(ptLibrary, ptCompDesc->tTypeKey, tNewEntity);
            if(ptCompDesc->clone)
            {
                ptCompDesc->clone(pComponentECSData, ptSource, pDestComponentECSData, ptLibrary);
            }
            else
            {
                memcpy(pDestComponentECSData, pComponentECSData, ptCompDesc->szSize);
            }  
        }
        ptLibrary->_sbtEntityData[tNewEntity.uIndex].bNotOwned = true;
    }

    PL_FREE(atSourceEntities);
    return true;
}

plEntityId
pl_ecs_generate_id(plComponentLibrary* ptLibrary, const char* pcName, uint64_t uSeed)
{
    if(pcName)
    {
        return pl_hm_hash_str(pcName, uSeed);
    }
    if(ptLibrary)
        return pl__generate_entity_id(ptLibrary);
    else
    {
        uint64_t ulId;
        do
        {
            ulId = pl__splitmix64(&gptEcsCtx->uRandomSeed);
        }
        while(ulId == 0);

        return ulId;
    }
}

plEcsTypeKey
pl_ecs_register_type(plComponentDesc tDesc, const void* pTemplate)
{
    PL_ASSERT(!gptEcsCtx->bFinalized && "ECS setup already finalized!");
    
    if(gptEcsCtx->bFinalized)
        return UINT32_MAX;
        
    if(pTemplate)
    {
        tDesc._pTemplate = PL_ALLOC(tDesc.szSize);
        memcpy(tDesc._pTemplate, pTemplate, tDesc.szSize);
    }
    tDesc.tTypeKey = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    pl_sb_push(gptEcsCtx->sbtComponentDescriptions, tDesc);
    return tDesc.tTypeKey;
}

const plComponentDesc*
pl_ecs_get_type_description(plEcsTypeKey tTypeKey)
{
    PL_ASSERT(tTypeKey < pl_sb_size(gptEcsCtx->sbtComponentDescriptions));
    return &gptEcsCtx->sbtComponentDescriptions[tTypeKey];
}

uint32_t
pl_ecs_get_type_descriptions(const plComponentDesc** pptComponentDescOut)
{
    *pptComponentDescOut = gptEcsCtx->sbtComponentDescriptions;
    return pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
}

void
pl_ecs_initialize(plEcsInit tInit)
{
    gptEcsCtx->tTagComponentType = pl_ecs_register_type((plComponentDesc){
        .pcName        = "tag",
        .szSize        = sizeof(plTagComponent),
        .destroy       = pl__ecs_tag_destroy,
        .clone         = pl__ecs_tag_clone,
        .serialize     = pl__ecs_tag_serialize,
        .deserialize   = pl__ecs_tag_deserialize,
    }, NULL);

    gptEcsCtx->tLibraryComponentType = pl_ecs_register_type((plComponentDesc){
        .pcName        = "library",
        .szSize        = sizeof(plLibraryComponent),
        .destroy       = pl__ecs_library_destroy,
        .serialize     = pl__ecs_library_serialize,
        .deserialize   = pl__ecs_library_deserialize,
        .resolve       = pl__ecs_library_resolve,
        .clone         = pl__ecs_library_clone,
    }, NULL);

    int iProcessId = 67; // TODO: add helper for actual process id
    gptEcsCtx->uRandomSeed = (uint64_t)(gptTimer->get_raw_time() * 1000000000.0) ^ ((uint64_t)iProcessId << 32);
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_tag(void)
{
    return gptEcsCtx->tTagComponentType;
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_library(void)
{
    return gptEcsCtx->tLibraryComponentType;
}

plComponentLibrary*
pl_ecs_create_library(void)
{

    PL_ASSERT(gptEcsCtx->bFinalized && "ECS not finalized");
    if(!gptEcsCtx->bFinalized)
        return NULL;

    plComponentLibrary* ptLibrary = PL_ALLOC(sizeof(plComponentLibrary));
    memset(ptLibrary, 0, sizeof(plComponentLibrary));
    pl_ecs_init_library(ptLibrary);
    return ptLibrary;
}

void
pl_ecs_destroy_library(plComponentLibrary* ptLibrary)
{
    pl_ecs_cleanup_library(ptLibrary);
    PL_FREE(ptLibrary);
}

void
pl_ecs_init_library(plComponentLibrary* ptLibrary)
{
    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);

    ptLibrary->_atManagers = PL_ALLOC(pl_sb_size(gptEcsCtx->sbtComponentDescriptions) * sizeof(plComponentManager));
    memset(ptLibrary->_atManagers, 0, pl_sb_size(gptEcsCtx->sbtComponentDescriptions) * sizeof(plComponentManager));

    plEntityData tEntityData = {
        .tId         = UINT64_MAX,
        .uGeneration = UINT32_MAX-1
    };
    pl_sb_push(ptLibrary->_sbtEntityData, tEntityData);

    PL_LOG_INFO_API(gptLog, gptEcsCtx->uLogChannel, "initialized component library");
}

void
pl_ecs_finalize(void)
{
    gptEcsCtx->bFinalized = true;
}


void
pl_ecs_set_library_type_data(plComponentLibrary* ptLibrary, plEcsTypeKey tType, void* pData, plEcsLibraryDataCleanup tCleanup)
{
    ptLibrary->_atManagers[tType].tUserManagerData.pInternal = pData;
    ptLibrary->_atManagers[tType].tUserManagerData.tCleanup = tCleanup;
}

void*
pl_ecs_get_library_type_data(plComponentLibrary* ptLibrary, plEcsTypeKey tType)
{
    return ptLibrary->_atManagers[tType].tUserManagerData.pInternal;
}

void
pl_ecs_cleanup_library(plComponentLibrary* ptLibrary)
{
    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(ptLibrary->_atManagers)
        {
            if(gptEcsCtx->sbtComponentDescriptions[i].destroy)
            {
                for(uint32_t j = 0; j < pl_sb_size(ptLibrary->_atManagers[i].sbtEntities); j++)
                {
                    gptEcsCtx->sbtComponentDescriptions[i].destroy(
                        &((uint8_t*)ptLibrary->_atManagers[i].pComponents)[j * gptEcsCtx->sbtComponentDescriptions[i].szSize],
                        ptLibrary);
                    memset(
                        &((uint8_t*)ptLibrary->_atManagers[i].pComponents)[j * gptEcsCtx->sbtComponentDescriptions[i].szSize],
                    0, gptEcsCtx->sbtComponentDescriptions[i].szSize);
                }
            }

            if(ptLibrary->_atManagers[i].tUserManagerData.pInternal && ptLibrary->_atManagers[i].tUserManagerData.tCleanup)
            {
                ptLibrary->_atManagers[i].tUserManagerData.tCleanup(ptLibrary->_atManagers[i].tUserManagerData.pInternal);
                ptLibrary->_atManagers[i].tUserManagerData.pInternal = NULL;
                ptLibrary->_atManagers[i].tUserManagerData.tCleanup = NULL;
            }

            ptLibrary->_atManagers[i].uCapacity = 0;
            if(ptLibrary->_atManagers[i].pComponents)
            {
                PL_FREE(ptLibrary->_atManagers[i].pComponents);
                ptLibrary->_atManagers[i].pComponents = NULL;
            }
            pl_sb_free(ptLibrary->_atManagers[i].sbtEntities);
            pl_hm_free(&ptLibrary->_atManagers[i].tHashmap);
        }
    }

    // general
    PL_FREE(ptLibrary->_atManagers);
    ptLibrary->_atManagers = NULL;
    pl_sb_free(ptLibrary->_sbtEntityFreeIndices);
    pl_sb_free(ptLibrary->_sbtEntityData);
    pl_hm_free(&ptLibrary->_tIdHashmap);
    pl_sb_free(ptLibrary->sbtChanges);
    pl_sb_free(ptLibrary->sbtChangeComponentTypes);
}

void
pl_ecs_cleanup(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptEcsCtx->sbtComponentDescriptions); i++)
    {
        if(gptEcsCtx->sbtComponentDescriptions[i]._pTemplate)
        {
            PL_FREE(gptEcsCtx->sbtComponentDescriptions[i]._pTemplate);
            gptEcsCtx->sbtComponentDescriptions[i]._pTemplate = NULL;
        }
    }
    pl_sb_free(gptEcsCtx->sbtComponentDescriptions);
}

void
pl_ecs_get_changes(const plComponentLibrary* ptLibrary, plEcsChange** pptChanges, uint32_t* puCountOut, plEcsTypeKey** pptTypes)
{
    if(pptChanges)
    {
        *pptChanges = ptLibrary->sbtChanges;
    }

    if(puCountOut)
    {
        *puCountOut = pl_sb_size(ptLibrary->sbtChanges);
    }

    if(pptTypes)
    {
        *pptTypes = ptLibrary->sbtChangeComponentTypes;
    }
}

void
pl_ecs_clear_changes(const plComponentLibrary* ptLibrary)
{
    pl_sb_reset(ptLibrary->sbtChanges);
    pl_sb_reset(ptLibrary->sbtChangeComponentTypes);
}

void
pl_ecs_mark_component_changed(const plComponentLibrary* ptLibrary, plEntity tEntity, plEcsTypeKey tType)
{
    plEcsChange tChange = {
        .eType = PL_ECS_CHANGE_COMPONENT_CHANGED,
        .tEntity = tEntity,
        .tComponentType = tType
    };
    pl_sb_push(ptLibrary->sbtChanges, tChange);
}

plEntity
pl_ecs_get_current_entity(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return (plEntity){UINT32_MAX, UINT32_MAX};
    tEntity.uGeneration = ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration;
    return tEntity;
}

size_t
pl_ecs_get_index(const plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{ 
    if(!pl_ecs_is_entity_valid(ptLibrary, tEntity))
        return SIZE_MAX;

    size_t szIndex = pl_hm_lookup(&ptLibrary->_atManagers[tType].tHashmap, (uint64_t)tEntity.uIndex);
    return szIndex;
}

void*
pl_ecs_get_component(const plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return NULL;

    plComponentManager* ptManager = &ptLibrary->_atManagers[tType];

    if(ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration != tEntity.uGeneration)
        return NULL;

    size_t szIndex = pl_ecs_get_index(ptLibrary, tType, tEntity);

    if(szIndex == UINT64_MAX)
        return NULL;

    unsigned char* pucData = ptManager->pComponents;
    return &pucData[szIndex * gptEcsCtx->sbtComponentDescriptions[tType].szSize];
}

void
pl_ecs_get_entities(const plComponentLibrary* ptLibrary, plEntity* atEntitesOut, uint32_t* puEntityCount)
{
    if(puEntityCount)
    {
        *puEntityCount = pl_sb_size(ptLibrary->_sbtEntityData) - pl_sb_size(ptLibrary->_sbtEntityFreeIndices) - 1;
    }

    if(atEntitesOut)
    {
        const uint32_t uEntityCount = pl_sb_size(ptLibrary->_sbtEntityData);
        uint32_t uCurrentIndex = 0;
        for(uint32_t i = 1; i < uEntityCount; i++)
        {
            if(ptLibrary->_sbtEntityData[i].tId != 0)
            {
                atEntitesOut[uCurrentIndex].uIndex = i;
                atEntitesOut[uCurrentIndex].uGeneration = ptLibrary->_sbtEntityData[i].uGeneration;
                uCurrentIndex++;
            }
        }

        PL_ASSERT(uCurrentIndex == pl_sb_size(ptLibrary->_sbtEntityData) - pl_sb_size(ptLibrary->_sbtEntityFreeIndices) - 1);
    }
}

void
pl_ecs_remove_entity(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(!pl_ecs_is_entity_valid(ptLibrary, tEntity))
        return;

    plEcsChange tChange = {
        .eType = PL_ECS_CHANGE_ENTITY_REMOVED,
        .tEntity = tEntity,
        .tEntityRemoved.uComponentOffset = pl_sb_size(ptLibrary->sbtChangeComponentTypes)
    };

    pl_hm_remove(&ptLibrary->_tIdHashmap, ptLibrary->_sbtEntityData[tEntity.uIndex].tId);

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    pl_sb_push(ptLibrary->_sbtEntityFreeIndices, tEntity.uIndex);

    ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration++;
    ptLibrary->_sbtEntityData[tEntity.uIndex].tId = 0;

    // remove from individual managers
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(pl_hm_has_key(&ptLibrary->_atManagers[i].tHashmap, tEntity.uIndex))
        {
            
            plComponentManager* ptManager = &ptLibrary->_atManagers[i];

            pl_sb_push(ptLibrary->sbtChangeComponentTypes, i);
            tChange.tEntityRemoved.uComponentCount++;

            size_t szCompSize = gptEcsCtx->sbtComponentDescriptions[i].szSize;

            // retrieve/consume the component slot that was just freed
            const uint64_t uRemovedIndex = pl_hm_lookup(&ptManager->tHashmap, tEntity.uIndex);
            const uint64_t uLastIndex = pl_sb_size(ptManager->sbtEntities) - 1;

            if(gptEcsCtx->sbtComponentDescriptions[i].destroy)
            {
                void* pComponent = &((char*)ptManager->pComponents)[szCompSize * uRemovedIndex];
                gptEcsCtx->sbtComponentDescriptions[i].destroy(pComponent, ptLibrary);
            }

            // remove entity -> component mapping
            pl_hm_remove(&ptManager->tHashmap, tEntity.uIndex);

            // consume the freed component index
            pl_hm_get_free_index(&ptManager->tHashmap);

            // move last component/entity into the hole
            if(uRemovedIndex != uLastIndex)
            {
                const plEntity tLastEntity = ptManager->sbtEntities[uLastIndex];

                // removing this adds uLastIndex to the free list
                pl_hm_remove(&ptManager->tHashmap, tLastEntity.uIndex);

                // consume that free index too, because dense storage is shrinking
                pl_hm_get_free_index(&ptManager->tHashmap);

                // last component now lives in removed slot
                pl_hm_insert(&ptManager->tHashmap, tLastEntity.uIndex, uRemovedIndex);

                memmove(&((char*)ptManager->pComponents)[szCompSize * uRemovedIndex], &((char*)ptManager->pComponents)[szCompSize * uLastIndex], szCompSize);
            }

            pl_sb_del_swap(ptManager->sbtEntities, uRemovedIndex);
        }
    }
    pl_sb_push(ptLibrary->sbtChanges, tChange);
}

bool
pl_ecs_has_component(const plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    return pl_ecs_has_entity(ptLibrary, tType, tEntity);
}

uint32_t
pl_ecs_get_components(const plComponentLibrary* ptLibrary, plEcsTypeKey tType, void** ppComponentsOut, const plEntity** pptEntitiesOut)
{
    plComponentManager* ptManager = &ptLibrary->_atManagers[tType];
    
    if(ppComponentsOut)
    {
        *ppComponentsOut = ptManager->pComponents;
    }

    if(pptEntitiesOut)
    {
        *pptEntitiesOut = ptManager->sbtEntities;
    }

    return pl_sb_size(ptManager->sbtEntities);
}

void*
pl_ecs_add_component(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return NULL;

    plComponentManager* ptManager = &ptLibrary->_atManagers[tType];

    if(ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration != tEntity.uGeneration)
        return NULL;

    void* pExistingComponent = pl_ecs_get_component(ptLibrary, tType, tEntity);
    if(pExistingComponent)
        return pExistingComponent;

    uint64_t uComponentIndex = pl_hm_get_free_index(&ptManager->tHashmap);
    bool bAddSlot = false; // can't add component with SB without correct type
    if(uComponentIndex == UINT64_MAX)
    {
        uComponentIndex = pl_sb_size(ptManager->sbtEntities);
        pl_sb_add(ptManager->sbtEntities);
        bAddSlot = true;
    }
    pl_hm_insert(&ptManager->tHashmap, (uint64_t)tEntity.uIndex, uComponentIndex);

    ptManager->sbtEntities[uComponentIndex] = tEntity;

    size_t szCompSize = gptEcsCtx->sbtComponentDescriptions[tType].szSize;
    
    if(bAddSlot)
    {
        if(ptManager->uCapacity == 0) // first allocation
        {
            ptManager->uCapacity = 16;
            ptManager->pComponents = PL_ALLOC(szCompSize * ptManager->uCapacity);
            memset(ptManager->pComponents, 0, szCompSize * ptManager->uCapacity);
        }

        if(pl_sb_size(ptManager->sbtEntities) > ptManager->uCapacity) // need to grow
        {
            void* pOldComponents = ptManager->pComponents;
            ptManager->pComponents = PL_ALLOC(szCompSize * ptManager->uCapacity * 2);
            memset(ptManager->pComponents, 0, szCompSize * ptManager->uCapacity * 2);
            memcpy(ptManager->pComponents, pOldComponents, szCompSize * ptManager->uCapacity);
            PL_FREE(pOldComponents);
            ptManager->uCapacity *= 2;
        }
    }
    char* pNewComponent = &((char*)ptManager->pComponents)[szCompSize * uComponentIndex];
    if(gptEcsCtx->sbtComponentDescriptions[tType]._pTemplate)
        memcpy(pNewComponent, gptEcsCtx->sbtComponentDescriptions[tType]._pTemplate, szCompSize);
    else
        memset(pNewComponent, 0, szCompSize);

    plEcsChange tChange = {
        .eType = PL_ECS_CHANGE_COMPONENT_ADDED,
        .tEntity = tEntity,
        .tComponentType = tType
    };
    pl_sb_push(ptLibrary->sbtChanges, tChange);

    return pNewComponent;
}

plEntityId
pl_ecs_get_entity_id(const plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return UINT64_MAX;

    if(ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration != tEntity.uGeneration)
        return UINT64_MAX;

    return ptLibrary->_sbtEntityData[tEntity.uIndex].tId;
}

plEntity
pl_ecs_get_entity_by_id(const plComponentLibrary* ptLibrary, plEntityId tId)
{
    if(pl_hm_has_key(&ptLibrary->_tIdHashmap, tId))
    {
        return (plEntity){
            .uData = pl_hm_lookup(&ptLibrary->_tIdHashmap, tId)
        };
    }
    return (plEntity){UINT32_MAX, UINT32_MAX};
}

plEntity
pl_ecs_create_entity_with_id(plComponentLibrary* ptLibrary, const char* pcName, plEntityId tId)
{
    PL_ASSERT(tId != 0);

    if(tId == UINT64_MAX)
        tId = pl__generate_entity_id(ptLibrary);

    if(pl_hm_has_key(&ptLibrary->_tIdHashmap, tId))
    {
        PL_ASSERT(false && "entity id already in use");
        return (plEntity){UINT32_MAX, UINT32_MAX};
    }

    plEntity tNewEntity = {0};
    if(pl_sb_size(ptLibrary->_sbtEntityFreeIndices) > 0) // free slot available
    {
        tNewEntity.uIndex = pl_sb_pop(ptLibrary->_sbtEntityFreeIndices);
        tNewEntity.uGeneration = ptLibrary->_sbtEntityData[tNewEntity.uIndex].uGeneration;
    }
    else // create new slot
    {
        tNewEntity.uIndex = pl_sb_size(ptLibrary->_sbtEntityData);
        plEntityData tEntityData = {
            .uGeneration = 0
        };
        pl_sb_push(ptLibrary->_sbtEntityData, tEntityData);
    }
    ptLibrary->_sbtEntityData[tNewEntity.uIndex].tId = tId;

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);

    // char acBuffer[128] = {0};
    // if(pcName == NULL)
    // {
    //     pl_sprintf(acBuffer, "No Name: %llu", tId);
    // }

    if(pcName)
    {
        plTagComponent* ptTag = pl_ecs_add_component(ptLibrary, gptEcsCtx->tTagComponentType, tNewEntity);
        if(pcName)
            ptTag->pcName = gptString->intern(pcName);
    }
    // else
    //     ptTag->pcName = gptString->intern(acBuffer);

    // PL_LOG_DEBUG_API_F(gptLog, gptEcsCtx->uLogChannel, "created entity: %s, %llu", ptTag->pcName, tId);

    pl_hm_insert(&ptLibrary->_tIdHashmap, tId, tNewEntity.uData);

    plEcsChange tChange = {
        .eType = PL_ECS_CHANGE_ENTITY_ADDED,
        .tEntity = tNewEntity
    };
    pl_sb_push(ptLibrary->sbtChanges, tChange);

    return tNewEntity;
}

plEntity
pl_ecs_create_entity(plComponentLibrary* ptLibrary, const char* pcName)
{
    return pl_ecs_create_entity_with_id(ptLibrary, pcName, pl__generate_entity_id(ptLibrary));
}

uint64_t
pl_ecs_get_log_channel(void)
{
    return gptEcsCtx->uLogChannel;
}

void
pl_ecs_register_asset_types(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName          = "Component Library",
        .pcFileExtension = "plscene",
        .szSize          = sizeof(plComponentLibrary),
        .serialize       = pl__ecs_asset_serialize,
        .deserialize     = pl__ecs_asset_deserialize,
        .cleanup         = pl__ecs_destroy
    };
    gptEcsCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_ecs_get_asset_type_key(void)
{
    return gptEcsCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementations
//-----------------------------------------------------------------------------

static bool
pl__ecs_asset_serialize(const char* pcName, const void* pLibrary, plAssetEncoding eEncoding)
{
    if(eEncoding == PL_ASSET_ENCODING_BINARY)
        return false;

    const plComponentLibrary* ptLibrary = pLibrary;

    plJsonObject* ptRoot = gptJson->new_root_object("root");
    gptJson->add_string_member(ptRoot, "format", "plscene");
    gptJson->add_uint32_member(ptRoot, "version", 1);

    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

    uint32_t uEntityCount = 0;
    pl_ecs_get_entities(ptLibrary, NULL, &uEntityCount);
    plEntity* atEntities = PL_ALLOC(uEntityCount * sizeof(plEntity));
    pl_ecs_get_entities(ptLibrary, atEntities, &uEntityCount);

    uint32_t uOwnedEntityCount = 0;
    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plEntity tEntity = atEntities[uEntityIndex];

        if(ptLibrary->_sbtEntityData[tEntity.uIndex].bNotOwned)
            continue;
        uOwnedEntityCount++;
    }
    
    plJsonObject* ptJsonEntities = gptJson->add_member_array(ptRoot, "entities", uOwnedEntityCount);

    uint32_t uCurrentEntityIndex = 0;
    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        
        plEntity tEntity = atEntities[uEntityIndex];

        if(ptLibrary->_sbtEntityData[tEntity.uIndex].bNotOwned)
            continue;

        plJsonObject* ptJsonNode = gptJson->member_by_index(ptJsonEntities, uCurrentEntityIndex);
        uCurrentEntityIndex++;

        plEntityId tEntityId = pl_ecs_get_entity_id(ptLibrary, tEntity);

        gptJson->add_uint64_member(ptJsonNode, "id", tEntityId);

        for(uint32_t uComponentIndex = 0; uComponentIndex < uComponentDescCount; uComponentIndex++)
        {
            const plComponentDesc* ptDesc = &atCompDescs[uComponentIndex];

            if(pl_ecs_has_component(ptLibrary, ptDesc->tTypeKey, tEntity))
            {
                plJsonObject* ptJsonComponent = gptJson->add_member(ptJsonNode, ptDesc->pcName);
                void* pComponent = pl_ecs_get_component(ptLibrary, ptDesc->tTypeKey, tEntity);
                if(ptDesc->serialize)
                {
                    ptDesc->serialize(pComponent, ptLibrary, tEntityId, ptJsonComponent);
                }
                else
                {
                    PL_ASSERT(false && "Component needs serialization implemented");
                }
            }
        }
    }

    PL_FREE(atEntities);

    uint32_t uBufferSize = 0;
    gptJson->write(ptRoot, NULL, &uBufferSize);
    char* pcBuffer = PL_ALLOC(uBufferSize);
    memset(pcBuffer, 0, uBufferSize);
    gptJson->write(ptRoot, pcBuffer, &uBufferSize);
    
    gptVfs->register_file(pcName, false);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
    gptVfs->close_file(tFileHandle);

    PL_FREE(pcBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static void
pl__scene_ecs_add_components(plJsonObject* ptJsonNode, plComponentLibrary* ptLibrary, plEntity tEntity)
{
    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

    for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
    {
        const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];

        // if(ptCompDesc->tTypeKey == gptEcsCtx->tTagComponentType)
        //     continue;

        plJsonObject* ptJsonComponent = gptJson->member(ptJsonNode, ptCompDesc->pcName);
        if(ptJsonComponent)
        {
            void* pComponentECSData = pl_ecs_add_component(ptLibrary, ptCompDesc->tTypeKey, tEntity);
            if(ptCompDesc->deserialize)
            {
                ptCompDesc->deserialize(ptJsonComponent, ptLibrary, pl_ecs_get_entity_id(ptLibrary, tEntity), pComponentECSData);   
            }
            else
            {
                PL_ASSERT(false && "Component needs deserialization implemented");
            }
        }
    }
}

static void
pl__ecs_resolve_components(plComponentLibrary* ptLibrary, plEntity tEntity, plHashMap64* ptHash)
{
    plEntityId tEntityId = pl_ecs_get_entity_id(ptLibrary, tEntity);

    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

    for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
    {
        const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];
        void* pComponentECSData = pl_ecs_get_component(ptLibrary, ptCompDesc->tTypeKey, tEntity);

        if(pComponentECSData == NULL)
            continue;

        if(ptCompDesc->resolve)
        {
            ptCompDesc->resolve(ptLibrary, tEntityId, ptHash, pComponentECSData);
        }
    }
}

static bool
pl__ecs_asset_deserialize(const char* pcName, void* pLibrary)
{
    if(!gptVfs->does_file_exist(pcName))
        return false;

    plComponentLibrary* ptLibrary = pLibrary;
    pl_ecs_init_library(ptLibrary);

    char acTempBuffer0[1024] = {0};

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    plJsonObject* ptRoot = NULL;
    gptJson->load((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = gptJson->uint32_member(ptRoot, "version", 0);

    // strncpy(acTempBuffer0, "/assets/environments/realistic.plenvironment", 1024);
    // if(gptJson->member_exist(ptRoot, "environment"))
    // {
    //     gptJson->string_member(ptRoot, "environment", acTempBuffer0, 1024);
    //     ptScene->tEnvironment = gptAsset->load(acTempBuffer0);
    // }

    // strncpy(acTempBuffer0, "/assets/settings/basic.renderer", 1024);
    // if(gptJson->member_exist(ptRoot, "renderer"))
    // {
    //     gptJson->string_member(ptRoot, "renderer", acTempBuffer0, 1024);
    //     ptScene->tRendererSettings = gptAsset->load(acTempBuffer0);
    // }

    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

    uint32_t uEntityCount = 0;
    plJsonObject* atJsonNodes = gptJson->array_member(ptRoot, "entities", &uEntityCount);

    // create all entities
    plEntity* atEntities = PL_ALLOC(uEntityCount * sizeof(plEntity));
    for(uint32_t i = 0; i < uEntityCount; i++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, i);
        plJsonType tJsonType = gptJson->get_type(gptJson->member(ptJsonNode, "id"));
        uint64_t uId = gptJson->uint64_member(ptJsonNode, "id", 0);

        char acName[256] = {0};
        plJsonObject* ptJsonTag = gptJson->member(ptJsonNode, "tag");
        if(ptJsonTag)
        {
            gptJson->string_member(ptJsonTag, "name", acName, 256);
        }
        atEntities[i] = pl_ecs_create_entity_with_id(ptLibrary, acName, uId);
    }

    // add entity components
    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, uEntityIndex);

        plEntity tEntity = atEntities[uEntityIndex];
        pl__scene_ecs_add_components(ptJsonNode, ptLibrary, tEntity);
    }

    // resolve references & add all components
    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, uEntityIndex);
        plEntity tEntity = atEntities[uEntityIndex];
        pl__ecs_resolve_components(ptLibrary, tEntity, NULL);
    }

    PL_FREE(atEntities);
    PL_FREE(puFileBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static void
pl__ecs_destroy(void* pLibrary)
{
    plComponentLibrary* ptLibrary = pLibrary;
    pl_ecs_cleanup_library(ptLibrary);
}

static void
pl__ecs_tag_serialize(void* pComponent, const plComponentLibrary* ptLibrary, plEntityId tEntityId, plJsonObject* ptJson)
{
    plTagComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "name", ptComponent->pcName);
}

static void
pl__ecs_tag_deserialize(plJsonObject* ptJson, plComponentLibrary* ptLibrary, plEntityId tEntityId, void* pComponent)
{
    plTagComponent* ptComponent = pComponent;
    char acName[256] = {0};
    gptJson->string_member(ptJson, "name", acName, 256);
    ptComponent->pcName = gptString->intern(acName);
}

static void
pl__ecs_tag_destroy(void* pComponent, const plComponentLibrary* ptLibrary)
{
    plTagComponent* ptComponent = pComponent;
    gptString->remove(ptComponent->pcName);
}

static void
pl__ecs_tag_clone(const void* pSrc, plComponentLibrary* ptSrcLib, void* Dest, plComponentLibrary* ptDestLib)
{
    const plTagComponent* ptSrcComponent = pSrc;
    plTagComponent* ptDestComponent = Dest;
    ptDestComponent->pcName = gptString->intern(ptSrcComponent->pcName);
}

static void
pl__ecs_library_clone(const void* pSource, plComponentLibrary* ptSourceLibrary, void* pDestination, plComponentLibrary* ptDestLibrary)
{
    const plLibraryComponent* ptSrc = pSource;
    plLibraryComponent* ptDst = pDestination;

    ptDst->tSourceLibrary = ptSrc->tSourceLibrary;

    ptDst->_ptLibrary = pl_ecs_create_library();
    pl_ecs_merge_library(ptDst->_ptLibrary, gptAsset->get_data(ptDst->tSourceLibrary));

    // do NOT copy owned runtime pointers directly
    // ptDst->ptLibrary = NULL;
    ptDst->_ptPatchLibrary = NULL;
    ptDst->_ptPatchHash = NULL;
}

static void
pl__ecs_library_destroy(void* pComponent, const plComponentLibrary* ptLibrary)
{
    plLibraryComponent* ptComponent = pComponent;

    if(ptComponent->_ptPatchHash)
    {
        pl_hm_free(ptComponent->_ptPatchHash);
        PL_FREE(ptComponent->_ptPatchHash);
        ptComponent->_ptPatchHash = NULL;
    }
    if(ptComponent->_ptPatchLibrary)
    {
        pl_ecs_destroy_library(ptComponent->_ptPatchLibrary);
        ptComponent->_ptPatchLibrary = NULL;  
    }
    if(ptComponent->_ptLibrary)
    {
        pl_ecs_destroy_library(ptComponent->_ptLibrary);
        ptComponent->_ptLibrary = NULL;  
    }
}

static void
pl__ecs_library_resolve(plComponentLibrary* ptLibrary, plEntityId tEntityId, plHashMap64* ptHashmap, void* pComponent)
{
    plLibraryComponent* ptComponent = pComponent;
    plComponentLibrary* ptSrcLibrary = ptComponent->_ptLibrary;

    plHashMap64 tHashmap = {0};
    pl_ecs_clone_library_into(ptLibrary, ptSrcLibrary, &tHashmap);

    uint32_t uSourceEntityCount = 0;
    plEntity* atSourceEntities = NULL;
    pl_ecs_get_entities(ptSrcLibrary, atSourceEntities, &uSourceEntityCount);
    atSourceEntities = PL_ALLOC(uSourceEntityCount * sizeof(plEntity));
    pl_ecs_get_entities(ptSrcLibrary, atSourceEntities, &uSourceEntityCount);

    if(uSourceEntityCount > 0)
    {
        ptComponent->_ptPatchHash = PL_ALLOC(sizeof(plHashMap64));
        memset(ptComponent->_ptPatchHash, 0, sizeof(plHashMap64));
    }

    for(uint32_t uEntityIndex = 0; uEntityIndex < uSourceEntityCount; uEntityIndex++)
    {
        plEntity tEntity = atSourceEntities[uEntityIndex];
        plEntityId tOldEntityId = pl_ecs_get_entity_id(ptSrcLibrary, tEntity);
        plEntityId tNewEntityId = pl_hm_lookup(&tHashmap, tOldEntityId);
        plEntity tNewEntity = pl_ecs_get_entity_by_id(ptLibrary, tNewEntityId);
        pl__ecs_resolve_components(ptLibrary, tNewEntity, &tHashmap);
        pl_hm_insert(ptComponent->_ptPatchHash, tOldEntityId, tNewEntityId);
    }

    pl_hm_free(&tHashmap);
    PL_FREE(atSourceEntities);

    pl_ecs_destroy_library(ptComponent->_ptLibrary);
    ptComponent->_ptLibrary = NULL;
}

static void
pl__ecs_library_serialize(void* pComponent, const plComponentLibrary* ptLibrary, plEntityId tEntityId, plJsonObject* ptJson)
{
    plLibraryComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "library", gptAsset->get_path(ptComponent->tSourceLibrary));

    if(ptComponent->_ptPatchLibrary)
    {
        // get patch entities
        uint32_t uSourceEntityCount = 0;
        plEntity* atSourceEntities = NULL;
        pl_ecs_get_entities(ptComponent->_ptPatchLibrary, atSourceEntities, &uSourceEntityCount);
        atSourceEntities = PL_ALLOC(uSourceEntityCount * sizeof(plEntity));
        pl_ecs_get_entities(ptComponent->_ptPatchLibrary, atSourceEntities, &uSourceEntityCount);

        const plComponentDesc* atCompDescs = NULL;
        uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

        plJsonObject* ptPatches = gptJson->add_member_array(ptJson, "patches", uSourceEntityCount);

        for(uint32_t uEntityIndex = 0; uEntityIndex < uSourceEntityCount; uEntityIndex++)
        {
            plJsonObject* ptJsonNode = gptJson->member_by_index(ptPatches, uEntityIndex);

            plEntity tSourceEntity = atSourceEntities[uEntityIndex];
            plEntityId tPatchEntityId = pl_ecs_get_entity_id(ptComponent->_ptPatchLibrary, tSourceEntity);

            plEntityId tActiveId = pl_hm_lookup(ptComponent->_ptPatchHash, tPatchEntityId);
            plEntity tActiveEntity = pl_ecs_get_entity_by_id(ptLibrary, tActiveId);

            gptJson->add_uint64_member(ptJsonNode, "id", tPatchEntityId);

            for(uint32_t uComponentIndex = 0; uComponentIndex < uComponentDescCount; uComponentIndex++)
            {
                const plComponentDesc* ptDesc = &atCompDescs[uComponentIndex];

                if(pl_ecs_has_component(ptComponent->_ptPatchLibrary, ptDesc->tTypeKey, tSourceEntity))
                {
                    plJsonObject* ptJsonComponent = gptJson->add_member(ptJsonNode, ptDesc->pcName);
                    void* pPatchComponent = pl_ecs_get_component(ptLibrary, ptDesc->tTypeKey, tActiveEntity);
                    if(ptDesc->serialize)
                    {
                        ptDesc->serialize(pPatchComponent, ptLibrary, tActiveId, ptJsonComponent);
                    }
                    else
                    {
                        PL_ASSERT(false && "Component needs serialization implemented");
                    }
                }
            }
        }

        PL_FREE(atSourceEntities);
    }
}

static void
pl__ecs_library_deserialize(plJsonObject* ptJson, plComponentLibrary* ptLibrary, plEntityId tEntityId, void* pComponent)
{
    plLibraryComponent* ptComponent = pComponent;

    char acTempBuffer0[1024] = {0};
    gptJson->string_member(ptJson, "library", acTempBuffer0, 1024);

    ptComponent->tSourceLibrary = gptAsset->load(acTempBuffer0); // let it map the data
    plComponentLibrary* ptSourceLibrary = gptAsset->get_data(ptComponent->tSourceLibrary);

    ptComponent->_ptLibrary = pl_ecs_create_library();
    pl_ecs_merge_library(ptComponent->_ptLibrary, ptSourceLibrary);

    uint32_t uPatchCount = 0;
    plJsonObject* ptPatches = gptJson->array_member(ptJson, "patches", &uPatchCount);
    if(ptPatches)
    {
        ptComponent->_ptPatchLibrary = pl_ecs_create_library();

        const plComponentDesc* atCompDescs = NULL;
        uint32_t uComponentDescCount = pl_ecs_get_type_descriptions(&atCompDescs);

        // create all entities
        plEntity* atEntities = PL_ALLOC(uPatchCount * sizeof(plEntity));
        for(uint32_t i = 0; i < uPatchCount; i++)
        {
            plJsonObject* ptJsonNode = gptJson->member_by_index(ptPatches, i);
            plJsonType tJsonType = gptJson->get_type(gptJson->member(ptJsonNode, "id"));
            uint64_t uId = gptJson->uint64_member(ptJsonNode, "id", 0);

            char acName[256] = {0};
            plJsonObject* ptJsonTag = gptJson->member(ptJsonNode, "tag");
            if(ptJsonTag)
            {
                gptJson->string_member(ptJsonTag, "name", acName, 256);
            }
            else
            {
                plEntity tEntity = pl_ecs_get_entity_by_id(ptComponent->_ptLibrary, uId);
                plTagComponent* ptTagComponent = pl_ecs_get_component(ptComponent->_ptLibrary, gptEcsCtx->tTagComponentType, tEntity);
                if(ptTagComponent)
                {
                    strncpy(acName, ptTagComponent->pcName, 256);
                }
            }
            atEntities[i] = pl_ecs_create_entity_with_id(ptComponent->_ptPatchLibrary, acName, uId);
        }

        // add entity components
        for(uint32_t uEntityIndex = 0; uEntityIndex < uPatchCount; uEntityIndex++)
        {
            plJsonObject* ptJsonNode = gptJson->member_by_index(ptPatches, uEntityIndex);

            plEntity tEntity = atEntities[uEntityIndex];
            pl__scene_ecs_add_components(ptJsonNode, ptComponent->_ptPatchLibrary, tEntity);
        }

        // resolve references & add all components
        for(uint32_t uEntityIndex = 0; uEntityIndex < uPatchCount; uEntityIndex++)
        {
            plJsonObject* ptJsonNode = gptJson->member_by_index(ptPatches, uEntityIndex);
            plEntity tEntity = atEntities[uEntityIndex];
            pl__ecs_resolve_components(ptComponent->_ptPatchLibrary, tEntity, NULL);
        }

        PL_FREE(atEntities);

        pl_ecs_merge_library(ptComponent->_ptLibrary, ptComponent->_ptPatchLibrary);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plEcsI tApi = {
        .initialize                = pl_ecs_initialize,
        .finalize                  = pl_ecs_finalize,
        .cleanup                   = pl_ecs_cleanup,
        .destroy_library           = pl_ecs_destroy_library,
        .create_library            = pl_ecs_create_library,
        .init_library              = pl_ecs_init_library,
        .cleanup_library           = pl_ecs_cleanup_library,
        .register_type             = pl_ecs_register_type,
        .remove_entity             = pl_ecs_remove_entity,
        .get_current_entity        = pl_ecs_get_current_entity,
        .is_entity_valid           = pl_ecs_is_entity_valid,
        .has_component             = pl_ecs_has_component,
        .get_index                 = pl_ecs_get_index,
        .get_components            = pl_ecs_get_components,
        .get_log_channel           = pl_ecs_get_log_channel,
        .get_component             = pl_ecs_get_component,
        .add_component             = pl_ecs_add_component,
        .set_library_type_data     = pl_ecs_set_library_type_data,
        .get_library_type_data     = pl_ecs_get_library_type_data,
        .create_entity             = pl_ecs_create_entity,
        .create_entity_with_id     = pl_ecs_create_entity_with_id,
        .get_ecs_type_key_tag      = pl_ecs_get_ecs_type_key_tag,
        .get_ecs_type_key_library  = pl_ecs_get_ecs_type_key_library,
        .get_entity_id             = pl_ecs_get_entity_id,
        .get_entity_by_id          = pl_ecs_get_entity_by_id,
        .get_type_description      = pl_ecs_get_type_description,
        .get_entities              = pl_ecs_get_entities,
        .generate_id               = pl_ecs_generate_id,
        .clone_library_into        = pl_ecs_clone_library_into,
        .merge_library             = pl_ecs_merge_library,
        .register_asset_types      = pl_ecs_register_asset_types,
        .get_asset_type_key        = pl_ecs_get_asset_type_key,
        .get_type_descriptions     = pl_ecs_get_type_descriptions,
        .get_changes               = pl_ecs_get_changes,
        .clear_changes             = pl_ecs_clear_changes,
        .mark_component_changed    = pl_ecs_mark_component_changed,
    };
    pl_set_api(ptApiRegistry, plEcsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptLog     = pl_get_api_latest(ptApiRegistry, plLogI);
        gptString  = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptIOI     = pl_get_api_latest(ptApiRegistry, plIOI);
        gptTimer   = pl_get_api_latest(ptApiRegistry, plTimerI);
        gptJson    = pl_get_api_latest(ptApiRegistry, plJsonI);
        gptAsset   = pl_get_api_latest(ptApiRegistry, plAssetI);
        gptVfs     = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptIO = gptIOI->get_io();
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptEcsCtx = ptDataRegistry->get_data("plEcsContext");
        gptEcsCtx->uLogChannel = gptLog->get_channel_id("ECS");
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

void
pl_unload_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plEcsI* ptApi = pl_get_api_latest(ptApiRegistry, plEcsI);
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

#endif // PL_UNITY_BUILD