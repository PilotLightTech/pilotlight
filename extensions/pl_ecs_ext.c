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
    static plIO*                  gptIO      = NULL;
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

typedef struct _plEntityData
{
    uint32_t uGeneration;
    plEntityId tId;
} plEntityData;

typedef struct _plComponentLibrary
{
    // [INTERNAL]
    plHashMap           _tIdHashmap;
    plEntityData*       _sbtEntityData;
    uint32_t*           _sbtEntityFreeIndices;
    plHashMap*          _atHashmaps; // map entity -> index in sbtEntities/pComponents
    plComponentManager* _sbtManagers; // just for internal convenience
} plComponentLibrary;

typedef struct _plEcsContext
{
    bool             bFinalized;
    uint64_t         uLogChannel;
    plComponentDesc* sbtComponentDescriptions;
    plEcsTypeKey     tTagComponentType;
    uint64_t         uRandomSeed;
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
    if(!pl_ecs_is_entity_valid(ptLibrary, tEntity))
        return false;

    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    return pl_hm_has_key(&ptLibrary->_atHashmaps[tType], tEntity.uIndex);
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

static void
pl__ecs_tag_cleanup(plComponentLibrary* ptLibrary)
{
    plTagComponent* ptComponents = NULL;
    const uint32_t uComponentCount = pl_ecs_get_components(ptLibrary, gptEcsCtx->tTagComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        gptString->remove(ptComponents[i].pcName);
    }
}

static void
pl__ecs_tag_serialize(void* pComponent, plJsonObject* ptJson)
{
    plTagComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "name", ptComponent->pcName);
}

static void
pl__ecs_tag_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plTagComponent* ptComponent = pComponent;
    char acName[256] = {0};
    gptJson->string_member(ptJson, "name", acName, 256);
    ptComponent->pcName = gptString->intern(acName);
    
}
 
//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_ecs_set_entity_name(plComponentLibrary* ptLibrary, plEntity tEntity, const char* pcName)
{
    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    plTagComponent* ptTagComponent = pl_ecs_get_component(ptLibrary, gptEcsCtx->tTagComponentType, tEntity);
    pl_hm_remove_str(&ptLibrary->_atHashmaps[uComponentTypeCount], ptTagComponent->pcName);
    gptString->remove(ptTagComponent->pcName);
    pl_hm_insert_str(&ptLibrary->_atHashmaps[uComponentTypeCount], pcName, tEntity.uIndex);
    gptString->intern(pcName);
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
        .pcDisplayName = "Tag",
        .pcName        = "tag",
        .szSize        = sizeof(plTagComponent),
        .cleanup       = pl__ecs_tag_cleanup,
        .reset         = pl__ecs_tag_cleanup,
        .serialize     = pl__ecs_tag_serialize,
        .deserialize   = pl__ecs_tag_deserialize,
    }, NULL);

    int iProcessId = 67; // TODO: add helper for actual process id
    gptEcsCtx->uRandomSeed = (uint64_t)(gptTimer->get_raw_time() * 1000000000.0) ^ ((uint64_t)iProcessId << 32);
}

plEcsTypeKey
pl_ecs_get_ecs_type_key_tag(void)
{
    return gptEcsCtx->tTagComponentType;
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

    plEntityData tEntityData = {
        .tId = UINT64_MAX,
        .uGeneration = UINT32_MAX-1
    };
    pl_sb_push(ptLibrary->_sbtEntityData, tEntityData);

    PL_LOG_INFO_API(gptLog, gptEcsCtx->uLogChannel, "initialized component library");

    return true;
}

void
pl_ecs_finalize(void)
{
    gptEcsCtx->bFinalized = true;
}


void
pl_ecs_set_library_type_data(plComponentLibrary* ptLibrary, plEcsTypeKey tType, void* pData)
{
    ptLibrary->_sbtManagers[tType].pInternal = pData;
}

void*
pl_ecs_get_library_type_data(plComponentLibrary* ptLibrary, plEcsTypeKey tType)
{
    return ptLibrary->_sbtManagers[tType].pInternal;
}

void
pl_ecs_reset_library(plComponentLibrary* ptLibrary)
{

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(gptEcsCtx->sbtComponentDescriptions[i].reset)
            gptEcsCtx->sbtComponentDescriptions[i].reset(ptLibrary);

        ptLibrary->_sbtManagers[i].uCount = 0;
        pl_sb_reset(ptLibrary->_sbtManagers[i].sbtEntities);
        pl_hm_free(&ptLibrary->_atHashmaps[i]);
    }
    pl_hm_free(&ptLibrary->_tIdHashmap);
    pl_hm_free(&ptLibrary->_atHashmaps[uComponentTypeCount]);
    // general
    pl_sb_reset(ptLibrary->_sbtEntityFreeIndices);
    pl_sb_reset(ptLibrary->_sbtEntityData);
    plEntityData tEntityData = {
        .uGeneration = UINT32_MAX-1
    };
    pl_sb_push(ptLibrary->_sbtEntityData, tEntityData);
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
    pl_sb_free(ptLibrary->_sbtEntityData);
    pl_hm_free(&ptLibrary->_tIdHashmap);
    PL_FREE(ptLibrary->_atHashmaps);

    PL_FREE(ptLibrary);
    *pptLibrary = NULL;
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

bool
pl_ecs_is_entity_valid(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX || tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return false;
    const plEntityData* ptData = &ptLibrary->_sbtEntityData[tEntity.uIndex];
    return ptData->tId != 0 && ptData->uGeneration == tEntity.uGeneration;
}

plEntity
pl_ecs_get_entity_by_name(plComponentLibrary* ptLibrary, const char* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName, 0);
    uint64_t uIndex = 0;
    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    if(pl_hm_has_key_ex(&ptLibrary->_atHashmaps[uComponentTypeCount], ulHash, &uIndex))
    {
        return (plEntity){.uIndex = (uint32_t)uIndex, .uGeneration = ptLibrary->_sbtEntityData[uIndex].uGeneration};
    }
    return (plEntity){UINT32_MAX, UINT32_MAX};
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
pl_ecs_get_index(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{ 
    if(!pl_ecs_is_entity_valid(ptLibrary, tEntity))
        return false;

    size_t szIndex = pl_hm_lookup(&ptLibrary->_atHashmaps[tType], (uint64_t)tEntity.uIndex);
    return szIndex;
}

void*
pl_ecs_get_component(plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntity tEntity)
{
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return NULL;

    plComponentManager* ptManager = &ptLibrary->_sbtManagers[tType];

    if(ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration != tEntity.uGeneration)
        return NULL;

    size_t szIndex = pl_ecs_get_index(ptLibrary, tType, tEntity);

    if(szIndex == UINT64_MAX)
        return NULL;

    unsigned char* pucData = ptManager->pComponents;
    return &pucData[szIndex * gptEcsCtx->sbtComponentDescriptions[tType].szSize];
}

void
pl_ecs_get_entities(plComponentLibrary* ptLibrary, plEntity* atEntitesOut, uint32_t* puEntityCount)
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

    pl_hm_remove(&ptLibrary->_tIdHashmap, ptLibrary->_sbtEntityData[tEntity.uIndex].tId);

    const uint32_t uComponentTypeCount = pl_sb_size(gptEcsCtx->sbtComponentDescriptions);
    pl_sb_push(ptLibrary->_sbtEntityFreeIndices, tEntity.uIndex);

    // remove from tag hashmap
    plTagComponent* ptTag = pl_ecs_get_component(ptLibrary, gptEcsCtx->tTagComponentType, tEntity);
    if(ptTag)
    {
        pl_hm_remove_str(&ptLibrary->_atHashmaps[uComponentTypeCount], ptTag->pcName);
        gptString->remove(ptTag->pcName);
    }

    ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration++;
    ptLibrary->_sbtEntityData[tEntity.uIndex].tId = 0;

    // remove from individual managers
    for(uint32_t i = 0; i < uComponentTypeCount; i++)
    {
        if(pl_hm_has_key(&ptLibrary->_atHashmaps[i], tEntity.uIndex))
        {
            plComponentManager* ptManager = &ptLibrary->_sbtManagers[i];

            // index of component being removed
            pl_hm_remove(&ptLibrary->_atHashmaps[i], tEntity.uIndex);
            const uint64_t uRemovedIndex = pl_hm_get_free_index(&ptLibrary->_atHashmaps[i]);

            const uint64_t uLastIndex = ptManager->uCount - 1;

            // move last component/entity into the hole
            if(uRemovedIndex != uLastIndex)
            {
                const plEntity tLastEntity = ptManager->sbtEntities[uLastIndex];

                // update last entity's hashmap entry to point at the hole
                pl_hm_remove(&ptLibrary->_atHashmaps[i], tLastEntity.uIndex);
                pl_hm_get_free_index(&ptLibrary->_atHashmaps[i]); // burn old slot
                pl_hm_insert(&ptLibrary->_atHashmaps[i], tLastEntity.uIndex, uRemovedIndex);

                // move component data
                memmove(&((char*)ptManager->pComponents)[ptManager->szSize * uRemovedIndex],
                    &((char*)ptManager->pComponents)[ptManager->szSize * uLastIndex],
                    ptManager->szSize);
            }

            pl_sb_del_swap(ptManager->sbtEntities, uRemovedIndex);
            ptManager->uCount--;
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
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return NULL;

    plComponentManager* ptManager = &ptLibrary->_sbtManagers[tType];

    if(ptManager->ptParentLibrary->_sbtEntityData[tEntity.uIndex].uGeneration != tEntity.uGeneration)
        return NULL;

    void* pExistingComponent = pl_ecs_get_component(ptLibrary, tType, tEntity);
    if(pExistingComponent)
        return pExistingComponent;

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
    else
        memset(pNewComponent, 0, ptManager->szSize);
    return pNewComponent;
}

plEntityId
pl_ecs_get_entity_id(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(tEntity.uIndex >= pl_sb_size(ptLibrary->_sbtEntityData))
        return UINT64_MAX;

    if(ptLibrary->_sbtEntityData[tEntity.uIndex].uGeneration != tEntity.uGeneration)
        return UINT64_MAX;

    return ptLibrary->_sbtEntityData[tEntity.uIndex].tId;
}

plEntity
pl_ecs_get_entity_by_id(plComponentLibrary* ptLibrary, plEntityId tId)
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

    char acBuffer[128] = {0};
    if(pcName == NULL)
    {
        pl_sprintf(acBuffer, "No Name: %llu", tId);
    }

    plTagComponent* ptTag = pl_ecs_add_component(ptLibrary, gptEcsCtx->tTagComponentType, tNewEntity);
    if(pcName)
        ptTag->pcName = gptString->intern(pcName);
    else
        ptTag->pcName = gptString->intern(acBuffer);

    if(pcName)
    {
        pl_hm_insert_str(&ptLibrary->_atHashmaps[uComponentTypeCount], ptTag->pcName, tNewEntity.uIndex);
    }

    PL_LOG_DEBUG_API_F(gptLog, gptEcsCtx->uLogChannel, "created entity: %s, %llu", ptTag->pcName, tId);

    pl_hm_insert(&ptLibrary->_tIdHashmap, tId, tNewEntity.uData);

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

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plEcsI tApi = {
        .initialize            = pl_ecs_initialize,
        .finalize              = pl_ecs_finalize,
        .cleanup               = pl_ecs_cleanup,
        .create_library        = pl_ecs_create_library,
        .cleanup_library       = pl_ecs_cleanup_library,
        .reset_library         = pl_ecs_reset_library,
        .register_type         = pl_ecs_register_type,
        .remove_entity         = pl_ecs_remove_entity,
        .get_entity_by_name    = pl_ecs_get_entity_by_name,
        .get_current_entity    = pl_ecs_get_current_entity,
        .is_entity_valid       = pl_ecs_is_entity_valid,
        .has_component         = pl_ecs_has_component,
        .get_index             = pl_ecs_get_index,
        .get_components        = pl_ecs_get_components,
        .get_log_channel       = pl_ecs_get_log_channel,
        .get_component         = pl_ecs_get_component,
        .add_component         = pl_ecs_add_component,
        .set_library_type_data = pl_ecs_set_library_type_data,
        .get_library_type_data = pl_ecs_get_library_type_data,
        .create_entity         = pl_ecs_create_entity,
        .create_entity_with_id = pl_ecs_create_entity_with_id,
        .get_ecs_type_key_tag  = pl_ecs_get_ecs_type_key_tag,
        .get_entity_id         = pl_ecs_get_entity_id,
        .get_entity_by_id      = pl_ecs_get_entity_by_id,
        .get_type_description  = pl_ecs_get_type_description,
        .get_entities          = pl_ecs_get_entities,
        .generate_id           = pl_ecs_generate_id,
        .set_entity_name       = pl_ecs_set_entity_name,
        .get_type_descriptions = pl_ecs_get_type_descriptions
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