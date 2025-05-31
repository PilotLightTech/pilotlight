/*
   pl_shader_variant_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stddef.h> // size_t
#include "pl.h"
#include "pl_shader_variant_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_stats_ext.h"

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

    static const plGraphicsI* gptGfx   = NULL;
    static const plStatsI*    gptStats = NULL;

#endif

// libs
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plShaderVariantData
{
    plShaderHandle  tParentHandle;
    plHashMap64     tVariantHashmap;
    plShaderHandle* sbtVariantHandles; // needed for cleanup
} plShaderVariantData;

typedef struct _plComputeShaderVariantData
{
    plComputeShaderHandle  tParentHandle;
    plHashMap64            tVariantHashmap;
    plComputeShaderHandle* sbtVariantHandles; // needed for cleanup
} plComputeShaderVariantData;

typedef struct _plShaderVariantContext
{
    plDevice* ptDevice;

    // graphics shaders
    plHashMap32          tParentHashmap;
    plShaderVariantData* sbtVariants;

    // compute shaders
    plHashMap32                 tComputeParentHashmap;
    plComputeShaderVariantData* sbtComputeVariants;

    // stats
    double* pdParentShaderCount;
    double* pdParentComputeShaderCount;
    double* pdVariantsCount;
    double* pdComputeVariantsCount;
    double  dParentShaderCount;
    double  dParentComputeShaderCount;
    double  dVariantsCount;
    double  dComputeVariantsCount;
} plShaderVariantContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plShaderVariantContext* gptShaderVariantCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static size_t pl__shader_variant_get_data_type_size(plDataType);

//-----------------------------------------------------------------------------
// [SECTION] public implementation
//-----------------------------------------------------------------------------

void
pl_shader_variant_initialize(plShaderVariantInit tDesc)
{
    gptShaderVariantCtx->ptDevice = tDesc.ptDevice;

    // retrieve stats
    gptShaderVariantCtx->pdParentShaderCount        = gptStats->get_counter("parent shaders");
    gptShaderVariantCtx->pdParentComputeShaderCount = gptStats->get_counter("parent compute shaders");
    gptShaderVariantCtx->pdVariantsCount            = gptStats->get_counter("shader variants");
    gptShaderVariantCtx->pdComputeVariantsCount     = gptStats->get_counter("compute shader variants");
}

void
pl_shader_variant_update_stats(void)
{
    *gptShaderVariantCtx->pdParentShaderCount        = gptShaderVariantCtx->dParentShaderCount;
    *gptShaderVariantCtx->pdParentComputeShaderCount = gptShaderVariantCtx->dParentComputeShaderCount;
    *gptShaderVariantCtx->pdVariantsCount            = gptShaderVariantCtx->dVariantsCount;
    *gptShaderVariantCtx->pdComputeVariantsCount     = gptShaderVariantCtx->dComputeVariantsCount;
}

void
pl_shader_variant_cleanup(void)
{
    const uint32_t uVariantDataCount = pl_sb_size(gptShaderVariantCtx->sbtVariants);
    for(uint32_t i = 0; i < uVariantDataCount; i++)
    {
        plShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtVariants[i];

        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            plShader* ptShader = gptGfx->get_shader(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[j]);
            gptGfx->queue_shader_for_deletion(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[j]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);
    }

    const uint32_t uComputeVariantDataCount = pl_sb_size(gptShaderVariantCtx->sbtComputeVariants);
    for(uint32_t i = 0; i < uComputeVariantDataCount; i++)
    {
        plComputeShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtComputeVariants[i];

        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[j]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[j]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);
    }

    pl_sb_free(gptShaderVariantCtx->sbtVariants);
    pl_sb_free(gptShaderVariantCtx->sbtComputeVariants);
    pl_hm32_free(&gptShaderVariantCtx->tParentHashmap);
    pl_hm32_free(&gptShaderVariantCtx->tComputeParentHashmap);

    gptShaderVariantCtx->dParentShaderCount = 0.0;
    gptShaderVariantCtx->dParentComputeShaderCount = 0.0;
    gptShaderVariantCtx->dVariantsCount = 0.0;
    gptShaderVariantCtx->dComputeVariantsCount = 0.0;
}

void
pl_shader_variant_reset_variants(plShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plShader* ptShader = gptGfx->get_shader(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_shader_for_deletion(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_reset(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        gptShaderVariantCtx->dVariantsCount -= uVariantCount;
    }
}

void
pl_shader_variant_reset_compute_variants(plComputeShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderVariantCtx->tComputeParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plComputeShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtComputeVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_reset(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        gptShaderVariantCtx->dComputeVariantsCount -= uVariantCount;
    }
}

void
pl_shader_variant_clear_variants(plShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plShader* ptShader = gptGfx->get_shader(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_shader_for_deletion(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        pl_hm32_remove(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData);

        gptShaderVariantCtx->dVariantsCount -= uVariantCount;
        gptShaderVariantCtx->dParentShaderCount--;
    }
}

void
pl_shader_variant_clear_compute_variants(plComputeShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderVariantCtx->tComputeParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plComputeShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtComputeVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderVariantCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        pl_hm32_remove(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData);

        gptShaderVariantCtx->dComputeVariantsCount -= uVariantCount;
        gptShaderVariantCtx->dParentComputeShaderCount--;
    }
}

plShaderHandle
pl_shader_variant_get_variant(plShaderHandle tParentHandle, plGraphicsState tGraphicsState, const void* pTempConstantData)
{
    plDevice* ptDevice = gptShaderVariantCtx->ptDevice;

    plShader* ptShader = gptGfx->get_shader(ptDevice, tParentHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += pl__shader_variant_get_data_type_size(ptConstant->tType);
    }

    // retrieve shader variant data
    uint32_t uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_ex(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tParentHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtVariants);
            pl_sb_push(gptShaderVariantCtx->sbtVariants, (plShaderVariantData){.tParentHandle = tParentHandle});
        }
        pl_hm32_insert(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData, uVariantIndex);
        gptShaderVariantCtx->dParentShaderCount++;
    }

    plShaderVariantData* ptVariantData = &gptShaderVariantCtx->sbtVariants[uVariantIndex];

    const uint64_t ulVariantHash = pl_hm_hash(pTempConstantData, szSpecializationSize, tGraphicsState.ulValue);
    const uint64_t ulIndex = pl_hm_lookup(&ptVariantData->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptVariantData->sbtVariantHandles[ulIndex];

    plShaderDesc tDesc = ptShader->tDesc;
    tDesc.tGraphicsState = tGraphicsState;
    tDesc.pTempConstantData = pTempConstantData;

    plShaderHandle tShader = gptGfx->create_shader(ptDevice, &tDesc);

    pl_hm_insert(&ptVariantData->tVariantHashmap, ulVariantHash, pl_sb_size(ptVariantData->sbtVariantHandles));
    pl_sb_push(ptVariantData->sbtVariantHandles, tShader);
    gptShaderVariantCtx->dVariantsCount++;
    return tShader;
}

plComputeShaderHandle
pl_shader_variant_get_compute_variant(plComputeShaderHandle tParentHandle, const void* pTempConstantData)
{
    plDevice* ptDevice = gptShaderVariantCtx->ptDevice;

    plComputeShader* ptShader = gptGfx->get_compute_shader(ptDevice, tParentHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += pl__shader_variant_get_data_type_size(ptConstant->tType);
    }

    // retrieve shader variant data
    uint32_t uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_ex(&gptShaderVariantCtx->tComputeParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tComputeParentHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtComputeVariants);
            pl_sb_push(gptShaderVariantCtx->sbtComputeVariants, (plComputeShaderVariantData){.tParentHandle = tParentHandle});
        }
        pl_hm32_insert(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData, uVariantIndex);
        gptShaderVariantCtx->dParentComputeShaderCount++;
    }

    plComputeShaderVariantData* ptVariantData = &gptShaderVariantCtx->sbtComputeVariants[uVariantIndex];

    const uint64_t ulVariantHash = pl_hm_hash(pTempConstantData, szSpecializationSize, (uint64_t)tParentHandle.uData);
    const uint64_t ulIndex = pl_hm_lookup(&ptVariantData->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptVariantData->sbtVariantHandles[ulIndex];

    plComputeShaderDesc tDesc = ptShader->tDesc;
    tDesc.pTempConstantData = pTempConstantData;

    plComputeShaderHandle tShader = gptGfx->create_compute_shader(ptDevice, &tDesc);

    pl_hm_insert(&ptVariantData->tVariantHashmap, ulVariantHash, pl_sb_size(ptVariantData->sbtVariantHandles));
    pl_sb_push(ptVariantData->sbtVariantHandles, tShader);
    gptShaderVariantCtx->dComputeVariantsCount++;
    return tShader;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static size_t
pl__shader_variant_get_data_type_size(plDataType tType)
{
    switch(tType)
    {
        case PL_DATA_TYPE_BOOL:   return sizeof(int);
        case PL_DATA_TYPE_BOOL2:  return 2 * sizeof(int);
        case PL_DATA_TYPE_BOOL3:  return 3 * sizeof(int);
        case PL_DATA_TYPE_BOOL4:  return 4 * sizeof(int);
        
        case PL_DATA_TYPE_FLOAT:  return sizeof(float);
        case PL_DATA_TYPE_FLOAT2: return 2 * sizeof(float);
        case PL_DATA_TYPE_FLOAT3: return 3 * sizeof(float);
        case PL_DATA_TYPE_FLOAT4: return 4 * sizeof(float);

        case PL_DATA_TYPE_UNSIGNED_BYTE:
        case PL_DATA_TYPE_BYTE:  return sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT:
        case PL_DATA_TYPE_SHORT: return sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT:
        case PL_DATA_TYPE_INT:   return sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG:
        case PL_DATA_TYPE_LONG:  return sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE2:
        case PL_DATA_TYPE_BYTE2:  return 2 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT2:
        case PL_DATA_TYPE_SHORT2: return 2 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT2:
        case PL_DATA_TYPE_INT2:   return 2 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG2:
        case PL_DATA_TYPE_LONG2:  return 2 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE3:
        case PL_DATA_TYPE_BYTE3:  return 3 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT3:
        case PL_DATA_TYPE_SHORT3: return 3 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT3:
        case PL_DATA_TYPE_INT3:   return 3 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG3:
        case PL_DATA_TYPE_LONG3:  return 3 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE4:
        case PL_DATA_TYPE_BYTE4:  return 4 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT4:
        case PL_DATA_TYPE_SHORT4: return 4 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT4:
        case PL_DATA_TYPE_INT4:   return 4 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG4:
        case PL_DATA_TYPE_LONG4:  return 4 * sizeof(uint64_t);
    }

    PL_ASSERT(false && "Unsupported data type");
    return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_shader_variant_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plShaderVariantI tApi = {
        .initialize             = pl_shader_variant_initialize,
        .cleanup                = pl_shader_variant_cleanup,
        .get_variant            = pl_shader_variant_get_variant,
        .reset_variants         = pl_shader_variant_reset_variants,
        .clear_variants         = pl_shader_variant_clear_variants,
        .get_compute_variant    = pl_shader_variant_get_compute_variant,
        .reset_compute_variants = pl_shader_variant_reset_compute_variants,
        .clear_compute_variants = pl_shader_variant_clear_compute_variants,
        .update_stats           = pl_shader_variant_update_stats,
    };
    pl_set_api(ptApiRegistry, plShaderVariantI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptGfx   = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptStats = pl_get_api_latest(ptApiRegistry, plStatsI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptShaderVariantCtx = ptDataRegistry->get_data("plShaderVariantContext");
    }
    else
    {
        static plShaderVariantContext gtShaderVariantCtx = {0};
        gptShaderVariantCtx = &gtShaderVariantCtx;
        ptDataRegistry->set_data("plShaderVariantContext", gptShaderVariantCtx);
    }
}

static void
pl_unload_shader_variant_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plShaderVariantI* ptApi = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    ptApiRegistry->remove_api(ptApi);
}
