/*
   pl_shader_tools_ext.c
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
#include "pl_shader_tools_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_stats_ext.h"
#include "pl_shader_ext.h"
#include "pl_vfs_ext.h"

// libs
#include "pl_json.h"
#include "pl_string.h"

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

    static const plGraphicsI* gptGfx    = NULL;
    static const plStatsI*    gptStats  = NULL;
    static const plShaderI*   gptShader = NULL;
    static const plVfsI*      gptVfs    = NULL;

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

typedef struct _plMetaShaderInfo
{
    plBindGroupLayoutHandle atBindGroupLayouts[3];
} plMetaShaderInfo;

typedef struct _plShaderVariantContext
{
    plDevice* ptDevice;

    // graphics shaders
    plHashMap32          tParentHashmap;
    plShaderVariantData* sbtVariants;

    // compute shaders
    plHashMap32                 tComputeParentHashmap;
    plComputeShaderVariantData* sbtComputeVariants;

    // compute shaders meta
    // plHashMap32       tComputeMetaShaderHashmap;
    // plMetaShaderInfo* sbtComputeMetaVariants;

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

static plShaderVariantContext* gptShaderToolsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static size_t pl__shader_variant_get_data_type_size(plDataType);

//-----------------------------------------------------------------------------
// [SECTION] public implementation
//-----------------------------------------------------------------------------

void
pl_shader_variant_initialize(plShaderToolsInit tDesc)
{
    gptShaderToolsCtx->ptDevice = tDesc.ptDevice;

    // retrieve stats
    gptShaderToolsCtx->pdParentShaderCount        = gptStats->get_counter("parent shaders");
    gptShaderToolsCtx->pdParentComputeShaderCount = gptStats->get_counter("parent compute shaders");
    gptShaderToolsCtx->pdVariantsCount            = gptStats->get_counter("shader variants");
    gptShaderToolsCtx->pdComputeVariantsCount     = gptStats->get_counter("compute shader variants");
}

void
pl_shader_variant_update_stats(void)
{
    *gptShaderToolsCtx->pdParentShaderCount        = gptShaderToolsCtx->dParentShaderCount;
    *gptShaderToolsCtx->pdParentComputeShaderCount = gptShaderToolsCtx->dParentComputeShaderCount;
    *gptShaderToolsCtx->pdVariantsCount            = gptShaderToolsCtx->dVariantsCount;
    *gptShaderToolsCtx->pdComputeVariantsCount     = gptShaderToolsCtx->dComputeVariantsCount;
}

void
pl_shader_variant_cleanup(void)
{
    const uint32_t uVariantDataCount = pl_sb_size(gptShaderToolsCtx->sbtVariants);
    for(uint32_t i = 0; i < uVariantDataCount; i++)
    {
        plShaderVariantData* ptVariant = &gptShaderToolsCtx->sbtVariants[i];

        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            plShader* ptShader = gptGfx->get_shader(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[j]);
            gptGfx->queue_shader_for_deletion(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[j]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);
    }

    const uint32_t uComputeVariantDataCount = pl_sb_size(gptShaderToolsCtx->sbtComputeVariants);
    for(uint32_t i = 0; i < uComputeVariantDataCount; i++)
    {
        plComputeShaderVariantData* ptVariant = &gptShaderToolsCtx->sbtComputeVariants[i];

        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[j]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[j]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);
    }

    pl_sb_free(gptShaderToolsCtx->sbtVariants);
    pl_sb_free(gptShaderToolsCtx->sbtComputeVariants);
    pl_hm32_free(&gptShaderToolsCtx->tParentHashmap);
    pl_hm32_free(&gptShaderToolsCtx->tComputeParentHashmap);

    gptShaderToolsCtx->dParentShaderCount = 0.0;
    gptShaderToolsCtx->dParentComputeShaderCount = 0.0;
    gptShaderToolsCtx->dVariantsCount = 0.0;
    gptShaderToolsCtx->dComputeVariantsCount = 0.0;
}

void
pl_shader_variant_reset_variants(plShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderToolsCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plShaderVariantData* ptVariant = &gptShaderToolsCtx->sbtVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plShader* ptShader = gptGfx->get_shader(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_shader_for_deletion(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_reset(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        gptShaderToolsCtx->dVariantsCount -= uVariantCount;
    }
}

void
pl_shader_variant_reset_compute_variants(plComputeShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderToolsCtx->tComputeParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plComputeShaderVariantData* ptVariant = &gptShaderToolsCtx->sbtComputeVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_reset(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        gptShaderToolsCtx->dComputeVariantsCount -= uVariantCount;
    }
}

void
pl_shader_variant_clear_variants(plShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderToolsCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plShaderVariantData* ptVariant = &gptShaderToolsCtx->sbtVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plShader* ptShader = gptGfx->get_shader(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_shader_for_deletion(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        pl_hm32_remove(&gptShaderToolsCtx->tParentHashmap, tParentHandle.uData);

        gptShaderToolsCtx->dVariantsCount -= uVariantCount;
        gptShaderToolsCtx->dParentShaderCount--;
    }
}

void
pl_shader_variant_clear_compute_variants(plComputeShaderHandle tParentHandle)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_ex(&gptShaderToolsCtx->tComputeParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        plComputeShaderVariantData* ptVariant = &gptShaderToolsCtx->sbtComputeVariants[uVariantIndex];
        const uint32_t uVariantCount = pl_sb_size(ptVariant->sbtVariantHandles);
        for(uint32_t i = 0; i < uVariantCount; i++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderToolsCtx->ptDevice, ptVariant->sbtVariantHandles[i]); 
        }

        pl_sb_free(ptVariant->sbtVariantHandles);
        pl_hm64_free(&ptVariant->tVariantHashmap);

        pl_hm32_remove(&gptShaderToolsCtx->tParentHashmap, tParentHandle.uData);

        gptShaderToolsCtx->dComputeVariantsCount -= uVariantCount;
        gptShaderToolsCtx->dParentComputeShaderCount--;
    }
}

plShaderHandle
pl_shader_variant_get_variant(plShaderHandle tParentHandle, plGraphicsState tGraphicsState, const void* pTempConstantData)
{
    plDevice* ptDevice = gptShaderToolsCtx->ptDevice;

    plShader* ptShader = gptGfx->get_shader(ptDevice, tParentHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += pl__shader_variant_get_data_type_size(ptConstant->tType);
    }

    // retrieve shader variant data
    uint32_t uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_ex(&gptShaderToolsCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        uVariantIndex = pl_hm32_get_free_index(&gptShaderToolsCtx->tParentHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderToolsCtx->sbtVariants);
            pl_sb_push(gptShaderToolsCtx->sbtVariants, (plShaderVariantData){.tParentHandle = tParentHandle});
        }
        pl_hm32_insert(&gptShaderToolsCtx->tParentHashmap, tParentHandle.uData, uVariantIndex);
        gptShaderToolsCtx->dParentShaderCount++;
    }

    plShaderVariantData* ptVariantData = &gptShaderToolsCtx->sbtVariants[uVariantIndex];

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
    gptShaderToolsCtx->dVariantsCount++;
    return tShader;
}

plComputeShaderHandle
pl_shader_variant_get_compute_variant(plComputeShaderHandle tParentHandle, const void* pTempConstantData)
{
    plDevice* ptDevice = gptShaderToolsCtx->ptDevice;

    plComputeShader* ptShader = gptGfx->get_compute_shader(ptDevice, tParentHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += pl__shader_variant_get_data_type_size(ptConstant->tType);
    }

    // retrieve shader variant data
    uint32_t uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_ex(&gptShaderToolsCtx->tComputeParentHashmap, tParentHandle.uData, &uVariantIndex))
    {
        uVariantIndex = pl_hm32_get_free_index(&gptShaderToolsCtx->tComputeParentHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderToolsCtx->sbtComputeVariants);
            pl_sb_push(gptShaderToolsCtx->sbtComputeVariants, (plComputeShaderVariantData){.tParentHandle = tParentHandle});
        }
        pl_hm32_insert(&gptShaderToolsCtx->tComputeParentHashmap, tParentHandle.uData, uVariantIndex);
        gptShaderToolsCtx->dParentComputeShaderCount++;
    }

    plComputeShaderVariantData* ptVariantData = &gptShaderToolsCtx->sbtComputeVariants[uVariantIndex];

    const uint64_t ulVariantHash = pl_hm_hash(pTempConstantData, szSpecializationSize, (uint64_t)tParentHandle.uData);
    const uint64_t ulIndex = pl_hm_lookup(&ptVariantData->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptVariantData->sbtVariantHandles[ulIndex];

    plComputeShaderDesc tDesc = ptShader->tDesc;
    tDesc.pTempConstantData = pTempConstantData;

    plComputeShaderHandle tShader = gptGfx->create_compute_shader(ptDevice, &tDesc);

    pl_hm_insert(&ptVariantData->tVariantHashmap, ulVariantHash, pl_sb_size(ptVariantData->sbtVariantHandles));
    pl_sb_push(ptVariantData->sbtVariantHandles, tShader);
    gptShaderToolsCtx->dComputeVariantsCount++;
    return tShader;
}

// plBindGroupLayoutHandle
// pl_shader_tool_get_compute_shader_bind_group_layout(plComputeShaderHandle tHandle, uint32_t uIndex)
// {
//     const uint64_t ulIndex = pl_hm32_lookup(&gptShaderToolsCtx->tComputeMetaShaderHashmap, tHandle.uData);
//     return gptShaderToolsCtx->sbtComputeMetaVariants[ulIndex].atBindGroupLayouts[uIndex];
// }

// plComputeShaderHandle
// pl_shader_tool_load_compute_shader(const char* pcPath, const char* pcName, const void* pTempConstantData)
// {
//     size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
//     plVfsFileHandle tShaderJson = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
//     gptVfs->read_file(tShaderJson, NULL, &szImageFileSize);
//     char* pucBuffer = (char*)PL_ALLOC(szImageFileSize + 1);
//     memset(pucBuffer, 0, szImageFileSize + 1);
//     gptVfs->read_file(tShaderJson, pucBuffer, &szImageFileSize);
//     gptVfs->close_file(tShaderJson);

//     plJsonObject* ptRootJsonObject = NULL;
//     pl_load_json(pucBuffer, &ptRootJsonObject);

//     uint32_t uShaderCount = 0;
//     plJsonObject* ptComputeShaders = pl_json_array_member(ptRootJsonObject, "compute shaders", &uShaderCount);
//     plJsonObject* ptComputeShader = NULL;
//     for(uint32_t i = 0; i < uShaderCount; i++)
//     {
//         plJsonObject* ptCandidateComputeShader = pl_json_member_by_index(ptComputeShaders, i);
//         char acNameBuffer[256] = {0};
//         pl_json_string_member(ptCandidateComputeShader, "pcName", acNameBuffer, 256);
//         if(pl_str_equal(pcName, acNameBuffer))
//         {
//             ptComputeShader = ptCandidateComputeShader;
//             break;
//         }

//     }

//     plJsonObject* ptShaderMember = pl_json_member(ptComputeShader, "tShader");
//     char acFileBuffer[256] = {0};
//     char acEntryBuffer[64] = {0};
//     pl_json_string_member(ptShaderMember, "file", acFileBuffer, 256);
//     pl_json_string_member(ptShaderMember, "entry", acEntryBuffer, 256);

//     plComputeShaderDesc tComputeShaderDesc = {0};
//     tComputeShaderDesc.tShader = gptShader->load_glsl(acFileBuffer, acEntryBuffer, NULL, NULL);

//     uint32_t uConstantCount = 0;
//     plJsonObject* ptConstants = pl_json_array_member(ptComputeShader, "atConstants", &uConstantCount);
//     for(uint32_t i = 0; i < uConstantCount; i++)
//     {
//         plJsonObject* ptConstant = pl_json_member_by_index(ptConstants, i);
//         tComputeShaderDesc.atConstants[i].uID = pl_json_uint_member(ptConstant, "uID", 0);
//         tComputeShaderDesc.atConstants[i].uOffset = pl_json_uint_member(ptConstant, "uOffset", 0);

//         char acTypeBuffer[64] = {0};
//         pl_json_string_member(ptConstant, "tType", acTypeBuffer, 256);
//         if(acTypeBuffer[13] == 'I')
//             tComputeShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
//         else
//         {
//             PL_ASSERT(false);
//         }
//     }

//     uint32_t uBindGroupLayoutCount = 0;
//     plJsonObject* ptBindGroupLayouts = pl_json_array_member(ptComputeShader, "atBindGroupLayouts", &uBindGroupLayoutCount);
//     for(uint32_t i = 0; i < uBindGroupLayoutCount; i++)
//     {
//         plJsonObject* ptBindGroupLayout = pl_json_member_by_index(ptBindGroupLayouts, i);

//         uint32_t uBufferBindingCount = 0;
//         plJsonObject* ptBufferBindings = pl_json_array_member(ptBindGroupLayout, "atBufferBindings", &uBufferBindingCount);
//         for(uint32_t j = 0; j < uBufferBindingCount; j++)
//         {
//             plJsonObject* ptBinding = pl_json_member_by_index(ptBufferBindings, j);
//             tComputeShaderDesc.atBindGroupLayouts[i].atBufferBindings[j].uSlot = pl_json_uint_member(ptBinding, "uSlot", 0);

//             char acTypeBuffer[64] = {0};
//             pl_json_string_member(ptBinding, "tType", acTypeBuffer, 256);
//             if(acTypeBuffer[23] == 'S')
//                 tComputeShaderDesc.atBindGroupLayouts[i].atBufferBindings[j].tType = PL_BUFFER_BINDING_TYPE_STORAGE;
//             else
//             {
//                 PL_ASSERT(false);
//             }

//             char acStage0[64] = {0};
//             char acStage1[64] = {0};
//             char acStage2[64] = {0};
//             char* aacStages[3] = {acStage0, acStage1, acStage2};
//             uint32_t auLengths[3] = {64, 64, 64};
//             uint32_t uStageCount = 0;
//             pl_json_string_array_member(ptBinding, "tStages", aacStages, &uStageCount, auLengths);
//             for(uint32_t k = 0; k < uStageCount; k++)
//             {
//                 if((aacStages[k])[16] == 'C')
//                 {
//                     tComputeShaderDesc.atBindGroupLayouts[i].atBufferBindings[j].tStages |= PL_SHADER_STAGE_COMPUTE;
//                 }
//                 else
//                 {
//                     PL_ASSERT(false);
//                 }
//             }

//         }
//     }
//     tComputeShaderDesc.pTempConstantData = pTempConstantData;

//     plComputeShaderHandle tShader = gptGfx->create_compute_shader(gptShaderToolsCtx->ptDevice, &tComputeShaderDesc);

//     pl_unload_json(&ptRootJsonObject);

//     PL_FREE(pucBuffer);

//     plMetaShaderInfo tInfo = {0};
//     for(uint32_t i = 0; i < uBindGroupLayoutCount; i++)
//     {
//         tInfo.atBindGroupLayouts[i] = gptGfx->create_bind_group_layout(gptShaderToolsCtx->ptDevice, &tComputeShaderDesc.atBindGroupLayouts[i]);
//     }

//     uint32_t uVariantIndex = UINT32_MAX;
//     if(!pl_hm32_has_key_ex(&gptShaderToolsCtx->tComputeMetaShaderHashmap, tShader.uData, &uVariantIndex))
//     {
//         uVariantIndex = pl_hm32_get_free_index(&gptShaderToolsCtx->tComputeMetaShaderHashmap);
//         if(uVariantIndex == PL_DS_HASH32_INVALID)
//         {
//             uVariantIndex = pl_sb_size(gptShaderToolsCtx->sbtComputeMetaVariants);
//             pl_sb_push(gptShaderToolsCtx->sbtComputeMetaVariants, tInfo);
//         }
//         pl_hm32_insert(&gptShaderToolsCtx->tComputeMetaShaderHashmap, tShader.uData, uVariantIndex);
//     }

//     return tShader;
// }

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
    const plShaderToolsI tApi = {
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
    pl_set_api(ptApiRegistry, plShaderToolsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptGfx    = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptStats  = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptShaderToolsCtx = ptDataRegistry->get_data("plShaderVariantContext");
    }
    else
    {
        static plShaderVariantContext gtShaderVariantCtx = {0};
        gptShaderToolsCtx = &gtShaderVariantCtx;
        ptDataRegistry->set_data("plShaderVariantContext", gptShaderToolsCtx);
    }
}

static void
pl_unload_shader_variant_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plShaderToolsI* ptApi = pl_get_api_latest(ptApiRegistry, plShaderToolsI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_JSON_IMPLEMENTATION
    #include "pl_json.h"
    #undef PL_JSON_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif