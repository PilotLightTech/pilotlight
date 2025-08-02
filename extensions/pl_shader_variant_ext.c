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
#include "pl_shader_ext.h"
#include "pl_vfs_ext.h"

// libs
#include "pl_json.h"
#include "pl_string.h"
#include "pl_memory.h"

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

typedef struct _plShaderToolsContext
{
    plDevice* ptDevice;

    // graphics shaders
    plHashMap32          tParentHashmap;
    plShaderVariantData* sbtGraphicsVariants;

    // compute shaders
    plHashMap32                 tComputeParentHashmap;
    plComputeShaderVariantData* sbtComputeVariants;

    // shaders meta
    plHashMap32       tGraphicsHashmap; // string -> index
    plMetaShaderInfo* sbtMetaVariants;
    plShaderHandle*   sbtShaderMeta;
    plShaderDesc*     sbtShaderDesc;

    // compute shaders meta
    plHashMap32            tComputeHashmap; // string -> index
    plMetaShaderInfo*      sbtComputeMetaVariants;
    plComputeShaderHandle* sbtComputeMeta;

    // bind group layouts
    plHashMap32              tBindGroupLayoutsHashmap;
    plBindGroupLayoutHandle* sbtBindGroupLayouts;

    // stats
    double* pdParentShaderCount;
    double* pdParentComputeShaderCount;
    double* pdVariantsCount;
    double* pdComputeVariantsCount;
    double  dParentShaderCount;
    double  dParentComputeShaderCount;
    double  dVariantsCount;
    double  dComputeVariantsCount;
} plShaderToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plShaderToolsContext* gptShaderVariantCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plCompareMode         pl__shader_tools_get_compare_mode      (const char*);
static plBlendFactor         pl__shader_tools_get_blend_factor      (const char*);
static plBlendOp             pl__shader_tools_get_blend_op          (const char*);
static plShaderStageFlags    pl__shader_tools_get_shader_stage      (const char*);
static plBufferBindingType   pl__shader_tools_buffer_binding_type   (const char*);
static plTextureBindingType  pl__shader_tools_texture_binding_type  (const char*);
static plBindGroupLayoutDesc pl__shader_tools_bind_group_layout_desc(plJsonObject*);
static plDataType            pl__shader_tools_get_data_type         (const char*);
static plStencilOp           pl__shader_tools_get_stencil_op        (const char*);
static plVertexFormat        pl__shader_tools_get_vertex_format     (const char*);

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
    const uint32_t uVariantDataCount = pl_sb_size(gptShaderVariantCtx->sbtGraphicsVariants);
    for(uint32_t i = 0; i < uVariantDataCount; i++)
    {
        plShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtGraphicsVariants[i];

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

    pl_sb_free(gptShaderVariantCtx->sbtGraphicsVariants);
    pl_sb_free(gptShaderVariantCtx->sbtComputeVariants);
    pl_hm32_free(&gptShaderVariantCtx->tParentHashmap);
    pl_hm32_free(&gptShaderVariantCtx->tComputeParentHashmap);

    gptShaderVariantCtx->dParentShaderCount = 0.0;
    gptShaderVariantCtx->dParentComputeShaderCount = 0.0;
    gptShaderVariantCtx->dVariantsCount = 0.0;
    gptShaderVariantCtx->dComputeVariantsCount = 0.0;
}

plShaderHandle
pl_shader_tool_get_shader(const char* pcName, const plGraphicsState* ptGraphicsState, const void* pTempConstantData, const plRenderPassLayoutHandle* ptRenderPassLayout)
{

    uint32_t uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_str_ex(&gptShaderVariantCtx->tGraphicsHashmap, pcName, &uVariantIndex))
    {
        return (plShaderHandle){.uData = UINT32_MAX};
    }

    if(ptGraphicsState)
        gptShaderVariantCtx->sbtShaderDesc[uVariantIndex].tGraphicsState = *ptGraphicsState;
    else
        ptGraphicsState = &gptShaderVariantCtx->sbtShaderDesc[uVariantIndex].tGraphicsState;

    plShaderHandle tBaseHandle = gptShaderVariantCtx->sbtShaderMeta[uVariantIndex];
    if(tBaseHandle.uData == 0) // first run
    {
        PL_ASSERT(ptRenderPassLayout != NULL);
        gptShaderVariantCtx->sbtShaderDesc[uVariantIndex].pTempConstantData = pTempConstantData;
        gptShaderVariantCtx->sbtShaderDesc[uVariantIndex].tRenderPassLayout = *ptRenderPassLayout;

        gptShaderVariantCtx->sbtShaderDesc[uVariantIndex].pcDebugName = pcName;
        tBaseHandle = gptGfx->create_shader(gptShaderVariantCtx->ptDevice, &gptShaderVariantCtx->sbtShaderDesc[uVariantIndex]);
        gptShaderVariantCtx->sbtShaderMeta[uVariantIndex] = tBaseHandle;
    }

    plDevice* ptDevice = gptShaderVariantCtx->ptDevice;

    plShader* ptShader = gptGfx->get_shader(ptDevice, tBaseHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += gptGfx->get_data_type_size(ptConstant->tType);
    }

    if(szSpecializationSize == 0 && ptShader->tDesc.tGraphicsState.ulValue == ptGraphicsState->ulValue)
        return tBaseHandle;

    // retrieve shader variant data
    uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_ex(&gptShaderVariantCtx->tParentHashmap, tBaseHandle.uData, &uVariantIndex))
    {
        uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tParentHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtGraphicsVariants);
            pl_sb_push(gptShaderVariantCtx->sbtGraphicsVariants, (plShaderVariantData){.tParentHandle = tBaseHandle});
        }
        pl_hm32_insert(&gptShaderVariantCtx->tParentHashmap, tBaseHandle.uData, uVariantIndex);
        gptShaderVariantCtx->dParentShaderCount++;
    }

    plShaderVariantData* ptVariantData = &gptShaderVariantCtx->sbtGraphicsVariants[uVariantIndex];

    const uint64_t ulVariantHash = pl_hm_hash(pTempConstantData, szSpecializationSize, ptGraphicsState->ulValue);
    const uint64_t ulIndex = pl_hm_lookup(&ptVariantData->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptVariantData->sbtVariantHandles[ulIndex];

    plShaderDesc tDesc = ptShader->tDesc;
    tDesc.tGraphicsState = *ptGraphicsState;
    tDesc.pTempConstantData = pTempConstantData;
    tDesc.pcDebugName = pcName;

    plShaderHandle tShader = gptGfx->create_shader(ptDevice, &tDesc);

    pl_hm_insert(&ptVariantData->tVariantHashmap, ulVariantHash, pl_sb_size(ptVariantData->sbtVariantHandles));
    pl_sb_push(ptVariantData->sbtVariantHandles, tShader);
    gptShaderVariantCtx->dVariantsCount++;
    return tShader;
}

plBindGroupLayoutHandle
pl_shader_tool_get_shader_bind_group_layout(const char* pcName, uint32_t uIndex)
{
    const uint64_t ulIndex = pl_hm32_lookup_str(&gptShaderVariantCtx->tGraphicsHashmap, pcName);
    return gptShaderVariantCtx->sbtMetaVariants[ulIndex].atBindGroupLayouts[uIndex];
}

plBindGroupLayoutHandle
pl_shader_tool_get_compute_shader_bind_group_layout(const char* pcName, uint32_t uIndex)
{
    const uint64_t ulIndex = pl_hm32_lookup_str(&gptShaderVariantCtx->tComputeHashmap, pcName);
    return gptShaderVariantCtx->sbtComputeMetaVariants[ulIndex].atBindGroupLayouts[uIndex];
}

plComputeShaderHandle
pl_shader_tool_get_compute_shader(const char* pcName, const void* pTempConstantData)
{
    uint32_t uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_str_ex(&gptShaderVariantCtx->tComputeHashmap, pcName, &uVariantIndex))
    {
        return (plComputeShaderHandle){.uData = UINT32_MAX};
    }

    plComputeShaderHandle tBaseHandle = gptShaderVariantCtx->sbtComputeMeta[uVariantIndex];

    plDevice* ptDevice = gptShaderVariantCtx->ptDevice;

    plComputeShader* ptShader = gptGfx->get_compute_shader(ptDevice, tBaseHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += gptGfx->get_data_type_size(ptConstant->tType);
    }

    if(szSpecializationSize == 0)
        return tBaseHandle;

    // retrieve shader variant data
    uVariantIndex = UINT32_MAX;
    if(!pl_hm32_has_key_ex(&gptShaderVariantCtx->tComputeParentHashmap, tBaseHandle.uData, &uVariantIndex))
    {
        uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tComputeParentHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtComputeVariants);
            pl_sb_push(gptShaderVariantCtx->sbtComputeVariants, (plComputeShaderVariantData){.tParentHandle = tBaseHandle});
        }
        pl_hm32_insert(&gptShaderVariantCtx->tComputeParentHashmap, tBaseHandle.uData, uVariantIndex);
        gptShaderVariantCtx->dParentComputeShaderCount++;
    }

    plComputeShaderVariantData* ptVariantData = &gptShaderVariantCtx->sbtComputeVariants[uVariantIndex];

    const uint64_t ulVariantHash = pl_hm_hash(pTempConstantData, szSpecializationSize, (uint64_t)tBaseHandle.uData);
    const uint64_t ulIndex = pl_hm_lookup(&ptVariantData->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptVariantData->sbtVariantHandles[ulIndex];

    plComputeShaderDesc tDesc = ptShader->tDesc;
    tDesc.pTempConstantData = pTempConstantData;
    tDesc.pcDebugName = pcName;

    plComputeShaderHandle tShader = gptGfx->create_compute_shader(ptDevice, &tDesc);

    pl_hm_insert(&ptVariantData->tVariantHashmap, ulVariantHash, pl_sb_size(ptVariantData->sbtVariantHandles));
    pl_sb_push(ptVariantData->sbtVariantHandles, tShader);
    gptShaderVariantCtx->dComputeVariantsCount++;
    return tShader;
}

bool
pl_shader_tool_load_manifest(const char* pcPath)
{
    if(!gptVfs->does_file_exist(pcPath))
        return false;

    plTempAllocator tTempAllocator = {0};

    size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
    plVfsFileHandle tShaderJson = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tShaderJson, NULL, &szImageFileSize);
    char* pucBuffer = (char*)PL_ALLOC(szImageFileSize + 1);
    memset(pucBuffer, 0, szImageFileSize + 1);
    gptVfs->read_file(tShaderJson, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tShaderJson);

    plJsonObject* ptRootJsonObject = NULL;
    pl_load_json(pucBuffer, &ptRootJsonObject);

    // load bind group layouts
    uint32_t uHighBindGroupLayoutCount = 0;
    plJsonObject* ptHighBindGroupLayouts = pl_json_array_member(ptRootJsonObject, "bind group layouts", &uHighBindGroupLayoutCount);
    for(uint32_t i = 0; i < uHighBindGroupLayoutCount; i++)
    {
        plJsonObject* ptBindGroupLayout = pl_json_member_by_index(ptHighBindGroupLayouts, i);
        char acNameBuffer[256] = {0};
        pl_json_string_member(ptBindGroupLayout, "pcName", acNameBuffer, 256);

        if(pl_hm32_has_key_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acNameBuffer))
        {
            pl_temp_allocator_free(&tTempAllocator);
            return false;
        }

        uint32_t uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tBindGroupLayoutsHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtBindGroupLayouts);
            pl_sb_add(gptShaderVariantCtx->sbtBindGroupLayouts);
        }
        pl_hm32_insert_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acNameBuffer, uVariantIndex);

        plBindGroupLayoutDesc tLayout = pl__shader_tools_bind_group_layout_desc(ptBindGroupLayout);
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptShaderVariantCtx->ptDevice, &tLayout);
        gptShaderVariantCtx->sbtBindGroupLayouts[uVariantIndex] = tHandle;
    }

    // load compute shaders
    uint32_t uComputeShaderCount = 0;
    plJsonObject* ptComputeShaders = pl_json_array_member(ptRootJsonObject, "compute shaders", &uComputeShaderCount);

    uint32_t uShaderCount = 0;
    plJsonObject* ptGraphicsShaders = pl_json_array_member(ptRootJsonObject, "graphics shaders", &uShaderCount);

    // bind group layout prepass
    for(uint32_t uShaderIndex = 0; uShaderIndex < uComputeShaderCount; uShaderIndex++)
    {
        plJsonObject* ptComputeShader = pl_json_member_by_index(ptComputeShaders, uShaderIndex);
        uint32_t uBindGroupLayoutCount = 0;
        plJsonObject* ptBindGroupLayouts = pl_json_array_member(ptComputeShader, "atBindGroupLayouts", &uBindGroupLayoutCount);
        for(uint32_t uBindGroupIndex = 0; uBindGroupIndex < uBindGroupLayoutCount; uBindGroupIndex++)
        {
            plJsonObject* ptBindGroupLayout = pl_json_member_by_index(ptBindGroupLayouts, uBindGroupIndex);

            if(pl_json_member_exist(ptBindGroupLayout, "pcName"))
            {


                if(pl_json_member_exist(ptBindGroupLayout, "atBufferBindings") ||
                    pl_json_member_exist(ptBindGroupLayout, "atSamplerBindings") ||
                    pl_json_member_exist(ptBindGroupLayout, "atTextureBindings"))
                    {

                        char acNameBuffer[256] = {0};
                        pl_json_string_member(ptBindGroupLayout, "pcName", acNameBuffer, 256);

                        if(pl_hm32_has_key_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acNameBuffer))
                        {
                            pl_temp_allocator_free(&tTempAllocator);
                            return false;
                        }

                        uint32_t uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tBindGroupLayoutsHashmap);
                        if(uVariantIndex == PL_DS_HASH32_INVALID)
                        {
                            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtBindGroupLayouts);
                            pl_sb_add(gptShaderVariantCtx->sbtBindGroupLayouts);
                        }
                        pl_hm32_insert_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acNameBuffer, uVariantIndex);

                        plBindGroupLayoutDesc tLayout = pl__shader_tools_bind_group_layout_desc(ptBindGroupLayout);
                        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptShaderVariantCtx->ptDevice, &tLayout);
                        gptShaderVariantCtx->sbtBindGroupLayouts[uVariantIndex] = tHandle;
                    }
            }
        }
    }

    // bind group layout prepass
    for(uint32_t uShaderIndex = 0; uShaderIndex < uShaderCount; uShaderIndex++)
    {
        plJsonObject* ptShader = pl_json_member_by_index(ptGraphicsShaders, uShaderIndex);
        uint32_t uBindGroupLayoutCount = 0;
        plJsonObject* ptBindGroupLayouts = pl_json_array_member(ptShader, "atBindGroupLayouts", &uBindGroupLayoutCount);
        for(uint32_t uBindGroupIndex = 0; uBindGroupIndex < uBindGroupLayoutCount; uBindGroupIndex++)
        {
            plJsonObject* ptBindGroupLayout = pl_json_member_by_index(ptBindGroupLayouts, uBindGroupIndex);

            if(pl_json_member_exist(ptBindGroupLayout, "pcName"))
            {


                if(pl_json_member_exist(ptBindGroupLayout, "atBufferBindings") ||
                    pl_json_member_exist(ptBindGroupLayout, "atSamplerBindings") ||
                    pl_json_member_exist(ptBindGroupLayout, "atTextureBindings"))
                    {

                        char acNameBuffer[256] = {0};
                        pl_json_string_member(ptBindGroupLayout, "pcName", acNameBuffer, 256);

                        if(pl_hm32_has_key_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acNameBuffer))
                        {
                            pl_temp_allocator_free(&tTempAllocator);
                            return false;
                        }

                        uint32_t uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tBindGroupLayoutsHashmap);
                        if(uVariantIndex == PL_DS_HASH32_INVALID)
                        {
                            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtBindGroupLayouts);
                            pl_sb_add(gptShaderVariantCtx->sbtBindGroupLayouts);
                        }
                        pl_hm32_insert_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acNameBuffer, uVariantIndex);

                        plBindGroupLayoutDesc tLayout = pl__shader_tools_bind_group_layout_desc(ptBindGroupLayout);
                        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptShaderVariantCtx->ptDevice, &tLayout);
                        gptShaderVariantCtx->sbtBindGroupLayouts[uVariantIndex] = tHandle;
                    }
            }
        }
    }

    for(uint32_t uShaderIndex = 0; uShaderIndex < uComputeShaderCount; uShaderIndex++)
    {
        plJsonObject* ptComputeShader = pl_json_member_by_index(ptComputeShaders, uShaderIndex);
        char acNameBuffer[256] = {0};
        pl_json_string_member(ptComputeShader, "pcName", acNameBuffer, 64);

        if(pl_hm32_has_key_str(&gptShaderVariantCtx->tComputeHashmap, acNameBuffer))
        {
            pl_temp_allocator_free(&tTempAllocator);
            return false;
        }

        uint32_t uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tComputeHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtComputeMeta);
            pl_sb_add(gptShaderVariantCtx->sbtComputeMeta);
            pl_sb_add(gptShaderVariantCtx->sbtComputeMetaVariants);
        }
        pl_hm32_insert_str(&gptShaderVariantCtx->tComputeHashmap, acNameBuffer, uVariantIndex);

        plJsonObject* ptShaderMember = pl_json_member(ptComputeShader, "tShader");
        char acFileBuffer[256] = {0};
        char acEntryBuffer[64] = {0};
        strncpy(acEntryBuffer, "main", 64);
        pl_json_string_member(ptShaderMember, "file", acFileBuffer, 256);
        pl_json_string_member(ptShaderMember, "entry", acEntryBuffer, 64);

        plMetaShaderInfo tInfo = {0};
        plComputeShaderDesc tComputeShaderDesc = {0};
        tComputeShaderDesc.pcDebugName = acNameBuffer;
        tComputeShaderDesc.tShader = gptShader->load_glsl(acFileBuffer, acEntryBuffer, NULL, NULL);

        size_t szMaxContantExtent = 0;
        uint32_t uConstantCount = 0;
        plJsonObject* ptConstants = pl_json_array_member(ptComputeShader, "atConstants", &uConstantCount);
        for(uint32_t i = 0; i < uConstantCount; i++)
        {
            plJsonObject* ptConstant = pl_json_member_by_index(ptConstants, i);
            tComputeShaderDesc.atConstants[i].uID = pl_json_uint_member(ptConstant, "uID", 0);
            tComputeShaderDesc.atConstants[i].uOffset = pl_json_uint_member(ptConstant, "uOffset", 0);

            char acTypeBuffer[64] = {0};
            pl_json_string_member(ptConstant, "tType", acTypeBuffer, 64);
            tComputeShaderDesc.atConstants[i].tType = pl__shader_tools_get_data_type(acTypeBuffer);

            const size_t szConstantExtent = gptGfx->get_data_type_size(tComputeShaderDesc.atConstants[i].tType) + tComputeShaderDesc.atConstants[i].uOffset;

            if(szConstantExtent > szMaxContantExtent)
                szMaxContantExtent = szConstantExtent;
        }

        if(szMaxContantExtent > 0)
        {
            void* pTempConstantData = pl_temp_allocator_alloc(&tTempAllocator, szMaxContantExtent);
            memset(pTempConstantData, 0, szMaxContantExtent);
            tComputeShaderDesc.pTempConstantData = pTempConstantData;
        }

        uint32_t uBindGroupLayoutCount = 0;
        plJsonObject* ptBindGroupLayouts = pl_json_array_member(ptComputeShader, "atBindGroupLayouts", &uBindGroupLayoutCount);
        for(uint32_t uBindGroupIndex = 0; uBindGroupIndex < uBindGroupLayoutCount; uBindGroupIndex++)
        {
            plJsonObject* ptBindGroupLayout = pl_json_member_by_index(ptBindGroupLayouts, uBindGroupIndex);
            tComputeShaderDesc.atBindGroupLayouts[uBindGroupIndex] = pl__shader_tools_bind_group_layout_desc(ptBindGroupLayout);

            if(pl_json_member_exist(ptBindGroupLayout, "pcName"))
            {
                char acBGNameBuffer[256] = {0};
                pl_json_string_member(ptBindGroupLayout, "pcName", acBGNameBuffer, 256);

                if(pl_hm32_has_key_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acBGNameBuffer))
                {
                    uint32_t uBGIndex = pl_hm32_lookup_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acBGNameBuffer);
                    tInfo.atBindGroupLayouts[uBindGroupIndex] = gptShaderVariantCtx->sbtBindGroupLayouts[uBGIndex];
                    tComputeShaderDesc.atBindGroupLayouts[uBindGroupIndex] = gptGfx->get_bind_group_layout(gptShaderVariantCtx->ptDevice, tInfo.atBindGroupLayouts[uBindGroupIndex])->tDesc;
                }
                else
                {
                    PL_ASSERT(false);
                }
            }
            else
            {
                tComputeShaderDesc.atBindGroupLayouts[uBindGroupIndex] = pl__shader_tools_bind_group_layout_desc(ptBindGroupLayout);
                tInfo.atBindGroupLayouts[uBindGroupIndex] = gptGfx->create_bind_group_layout(gptShaderVariantCtx->ptDevice, &tComputeShaderDesc.atBindGroupLayouts[uBindGroupIndex]);
            }

        }

        plComputeShaderHandle tShader = gptGfx->create_compute_shader(gptShaderVariantCtx->ptDevice, &tComputeShaderDesc);
        pl_temp_allocator_reset(&tTempAllocator);

        for(uint32_t i = 0; i < uBindGroupLayoutCount; i++)
        {
            tInfo.atBindGroupLayouts[i] = gptGfx->create_bind_group_layout(gptShaderVariantCtx->ptDevice, &tComputeShaderDesc.atBindGroupLayouts[i]);
        }

        gptShaderVariantCtx->sbtComputeMetaVariants[uVariantIndex] = tInfo;
        gptShaderVariantCtx->sbtComputeMeta[uVariantIndex] = tShader;
    }

    // load graphics shaders
    for(uint32_t uShaderIndex = 0; uShaderIndex < uShaderCount; uShaderIndex++)
    {
        plJsonObject* ptGraphicsShader = pl_json_member_by_index(ptGraphicsShaders, uShaderIndex);
        char acNameBuffer[256] = {0};
        pl_json_string_member(ptGraphicsShader, "pcName", acNameBuffer, 256);

        if(pl_hm32_has_key_str(&gptShaderVariantCtx->tGraphicsHashmap, acNameBuffer))
        {
            pl_temp_allocator_free(&tTempAllocator);
            return false;
        }

        uint32_t uVariantIndex = pl_hm32_get_free_index(&gptShaderVariantCtx->tGraphicsHashmap);
        if(uVariantIndex == PL_DS_HASH32_INVALID)
        {
            uVariantIndex = pl_sb_size(gptShaderVariantCtx->sbtShaderMeta);
            pl_sb_add(gptShaderVariantCtx->sbtShaderMeta);
            pl_sb_add(gptShaderVariantCtx->sbtMetaVariants);
            pl_sb_add(gptShaderVariantCtx->sbtShaderDesc);
        }
        pl_hm32_insert_str(&gptShaderVariantCtx->tGraphicsHashmap, acNameBuffer, uVariantIndex);

        plJsonObject* ptVertexShaderMember = pl_json_member(ptGraphicsShader, "tVertexShader");
        char acFileBuffer[256] = {0};
        char acEntryBuffer[64] = {0};
        strncpy(acEntryBuffer, "main", 64);
        pl_json_string_member(ptVertexShaderMember, "file", acFileBuffer, 256);
        pl_json_string_member(ptVertexShaderMember, "entry", acEntryBuffer, 64);

        plJsonObject* ptPixelShaderMember = pl_json_member(ptGraphicsShader, "tFragmentShader");
        char acFileBuffer2[256] = {0};
        char acEntryBuffer2[64] = {0};
        strncpy(acEntryBuffer2, "main", 64);
        if(ptPixelShaderMember)
        {
            pl_json_string_member(ptPixelShaderMember, "file", acFileBuffer2, 256);
            pl_json_string_member(ptPixelShaderMember, "entry", acEntryBuffer2, 64);
        }

        plShaderDesc tShaderDesc = {0};
        plMetaShaderInfo tInfo = {0};
        tShaderDesc.tVertexShader = gptShader->load_glsl(acFileBuffer, acEntryBuffer, NULL, NULL);

        if(ptPixelShaderMember)
            tShaderDesc.tFragmentShader = gptShader->load_glsl(acFileBuffer2, acEntryBuffer2, NULL, NULL);

        tShaderDesc.uSubpassIndex = pl_json_uint_member(ptGraphicsShader, "uSubpassIndex", 0);

        plJsonObject* ptGraphicsMember = pl_json_member(ptGraphicsShader, "tGraphicsState");
        if(ptGraphicsMember)
        {
            tShaderDesc.tGraphicsState.ulWireframe = pl_json_bool_member(ptGraphicsMember, "ulWireframe", false);
            tShaderDesc.tGraphicsState.ulDepthWriteEnabled = pl_json_bool_member(ptGraphicsMember, "ulDepthWriteEnabled", false);
            tShaderDesc.tGraphicsState.ulStencilRef = pl_json_uint_member(ptGraphicsMember, "ulStencilRef", 255);
            tShaderDesc.tGraphicsState.ulStencilMask = pl_json_uint_member(ptGraphicsMember, "ulStencilMask", 255);
            tShaderDesc.tGraphicsState.ulDepthClampEnabled = pl_json_bool_member(ptGraphicsMember, "ulDepthClampEnabled", false);
            tShaderDesc.tGraphicsState.ulStencilTestEnabled = pl_json_bool_member(ptGraphicsMember, "ulStencilTestEnabled", false);

            char acEnumBuffer[64] = {0};
            char* pcEnumValue = pl_json_string_member(ptGraphicsMember, "ulStencilOpFail", acEntryBuffer, 64);
            if(pl_json_member_exist(ptGraphicsMember, "ulStencilOpFail"))
                tShaderDesc.tGraphicsState.ulStencilOpFail = pl__shader_tools_get_stencil_op(pcEnumValue);

            pl_json_string_member(ptGraphicsMember, "ulStencilOpDepthFail", acEntryBuffer, 64);
            if(pl_json_member_exist(ptGraphicsMember, "ulStencilOpDepthFail"))
                tShaderDesc.tGraphicsState.ulStencilOpDepthFail = pl__shader_tools_get_stencil_op(pcEnumValue);

            pl_json_string_member(ptGraphicsMember, "ulStencilOpPass", acEntryBuffer, 64);
            if(pl_json_member_exist(ptGraphicsMember, "ulStencilOpDepthFail"))
                tShaderDesc.tGraphicsState.ulStencilOpPass = pl__shader_tools_get_stencil_op(pcEnumValue);

            pcEnumValue = pl_json_string_member(ptGraphicsMember, "ulStencilMode", acEntryBuffer, 64);
            if(pcEnumValue)
                tShaderDesc.tGraphicsState.ulStencilMode = pl__shader_tools_get_compare_mode(pcEnumValue);

            pcEnumValue = pl_json_string_member(ptGraphicsMember, "ulDepthMode", acEntryBuffer, 64);
            if(pcEnumValue)
                tShaderDesc.tGraphicsState.ulDepthMode = pl__shader_tools_get_compare_mode(pcEnumValue);

            pcEnumValue = pl_json_string_member(ptGraphicsMember, "ulCullMode", acEntryBuffer, 64);
            if(pcEnumValue)
            {
                if     (pcEnumValue[13] == 'N')                           tShaderDesc.tGraphicsState.ulCullMode = PL_CULL_MODE_NONE;
                else if(pcEnumValue[13] == 'C' && pcEnumValue[18] == 'F') tShaderDesc.tGraphicsState.ulCullMode = PL_CULL_MODE_CULL_FRONT;
                else if(pcEnumValue[13] == 'C')                           tShaderDesc.tGraphicsState.ulCullMode = PL_CULL_MODE_CULL_BACK;
                else
                {
                    PL_ASSERT(false);
                }
            }

        }

        uint32_t uBlendCount = 0;
        plJsonObject* ptBlendStates = pl_json_array_member(ptGraphicsShader, "atBlendStates", &uBlendCount);
        for(uint32_t i = 0; i < uBlendCount; i++)
        {
            plJsonObject* ptBlendState = pl_json_member_by_index(ptBlendStates, i);
            tShaderDesc.atBlendStates[i].bBlendEnabled = pl_json_bool_member(ptBlendState, "bBlendEnabled", false);

            char acBlendOp[64] = {0};
            char* pcBlendEnum = NULL;
            
            pcBlendEnum = pl_json_string_member(ptBlendState, "tSrcColorFactor", acBlendOp, 64);
            tShaderDesc.atBlendStates[i].tSrcColorFactor = pl__shader_tools_get_blend_factor(pcBlendEnum);

            pcBlendEnum = pl_json_string_member(ptBlendState, "tDstColorFactor", acBlendOp, 64);
            tShaderDesc.atBlendStates[i].tDstColorFactor = pl__shader_tools_get_blend_factor(pcBlendEnum);

            pcBlendEnum = pl_json_string_member(ptBlendState, "tSrcAlphaFactor", acBlendOp, 64);
            tShaderDesc.atBlendStates[i].tSrcAlphaFactor = pl__shader_tools_get_blend_factor(pcBlendEnum);

            pcBlendEnum = pl_json_string_member(ptBlendState, "tDstAlphaFactor", acBlendOp, 64);
            tShaderDesc.atBlendStates[i].tDstAlphaFactor = pl__shader_tools_get_blend_factor(pcBlendEnum);

            pcBlendEnum = pl_json_string_member(ptBlendState, "tAlphaOp", acBlendOp, 64);
            tShaderDesc.atBlendStates[i].tAlphaOp = pl__shader_tools_get_blend_op(pcBlendEnum);

            pcBlendEnum = pl_json_string_member(ptBlendState, "tColorOp", acBlendOp, 64);
            tShaderDesc.atBlendStates[i].tColorOp = pl__shader_tools_get_blend_op(pcBlendEnum);
        }

        uint32_t uVertexBufferCount = 0;
        plJsonObject* ptVertexBufferLayouts = pl_json_array_member(ptGraphicsShader, "atVertexBufferLayouts", &uVertexBufferCount);
        for(uint32_t i = 0; i < uVertexBufferCount; i++)
        {
            plJsonObject* ptVertexBufferLayout = pl_json_member_by_index(ptVertexBufferLayouts, i);
            tShaderDesc.atVertexBufferLayouts[i].uByteStride = pl_json_uint_member(ptVertexBufferLayout, "uByteStride", 0);

            uint32_t uAttributeCount = 0;
            plJsonObject* ptAttributes = pl_json_array_member(ptVertexBufferLayout, "atAttributes", &uAttributeCount);
            for(uint32_t j = 0; j < uAttributeCount; j++)
            {
                plJsonObject* ptAttribute = pl_json_member_by_index(ptAttributes, j);
                tShaderDesc.atVertexBufferLayouts[i].atAttributes[j].uByteOffset = pl_json_uint_member(ptAttribute, "uByteOffset", 0);
                tShaderDesc.atVertexBufferLayouts[i].atAttributes[j].uLocation = pl_json_uint_member(ptAttribute, "uLocation", 0);

                char acVertexFormatBuffer[64] = {0};
                pl_json_string_member(ptAttribute, "tFormat", acVertexFormatBuffer, 64);
                if(pl_json_member_exist(ptAttribute, "tFormat"))
                    tShaderDesc.atVertexBufferLayouts[i].atAttributes[j].tFormat = pl__shader_tools_get_vertex_format(acVertexFormatBuffer);
            }
        }

        uint32_t uConstantCount = 0;
        plJsonObject* ptConstants = pl_json_array_member(ptGraphicsShader, "atConstants", &uConstantCount);
        for(uint32_t i = 0; i < uConstantCount; i++)
        {
            plJsonObject* ptConstant = pl_json_member_by_index(ptConstants, i);
            tShaderDesc.atConstants[i].uID = pl_json_uint_member(ptConstant, "uID", 0);
            tShaderDesc.atConstants[i].uOffset = pl_json_uint_member(ptConstant, "uOffset", 0);

            char acTypeBuffer[64] = {0};
            pl_json_string_member(ptConstant, "tType", acTypeBuffer, 64);
            tShaderDesc.atConstants[i].tType = pl__shader_tools_get_data_type(acTypeBuffer);
        }

        uint32_t uBindGroupLayoutCount = 0;
        plJsonObject* ptBindGroupLayouts = pl_json_array_member(ptGraphicsShader, "atBindGroupLayouts", &uBindGroupLayoutCount);
        for(uint32_t i = 0; i < uBindGroupLayoutCount; i++)
        {
            plJsonObject* ptBindGroupLayout = pl_json_member_by_index(ptBindGroupLayouts, i);

            if(pl_json_member_exist(ptBindGroupLayout, "pcName"))
            {
                char acBGNameBuffer[256] = {0};
                pl_json_string_member(ptBindGroupLayout, "pcName", acBGNameBuffer, 256);

                if(pl_hm32_has_key_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acBGNameBuffer))
                {
                    uint32_t uBGIndex = pl_hm32_lookup_str(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, acBGNameBuffer);
                    tInfo.atBindGroupLayouts[i] = gptShaderVariantCtx->sbtBindGroupLayouts[uBGIndex];
                    plBindGroupLayout* ptLayout = gptGfx->get_bind_group_layout(gptShaderVariantCtx->ptDevice, tInfo.atBindGroupLayouts[i]);
                    tShaderDesc.atBindGroupLayouts[i] = ptLayout->tDesc;
                }
                else
                {
                    PL_ASSERT(false);
                }
            }
            else
            {
                tShaderDesc.atBindGroupLayouts[i] = pl__shader_tools_bind_group_layout_desc(ptBindGroupLayout);
                tInfo.atBindGroupLayouts[i] = gptGfx->create_bind_group_layout(gptShaderVariantCtx->ptDevice, &tShaderDesc.atBindGroupLayouts[i]);
            }
        }
        gptShaderVariantCtx->sbtMetaVariants[uVariantIndex] = tInfo;
        gptShaderVariantCtx->sbtShaderDesc[uVariantIndex] = tShaderDesc;
    }   

    pl_unload_json(&ptRootJsonObject);
    PL_FREE(pucBuffer);
    pl_temp_allocator_free(&tTempAllocator);
    return true;
}

bool
pl_shader_tool_unload_manifest(const char* pcPath)
{
    if(!gptVfs->does_file_exist(pcPath))
        return false;

    const uint32_t uShaderCount = pl_sb_size(gptShaderVariantCtx->sbtShaderMeta);
    for(uint32_t uShaderIndex = 0; uShaderIndex < uShaderCount; uShaderIndex++)
    {
        plShaderHandle tParentHandle = gptShaderVariantCtx->sbtShaderMeta[uShaderIndex];
        uint32_t uVariantIndex = UINT32_MAX;
        if(pl_hm32_has_key_ex(&gptShaderVariantCtx->tParentHashmap, tParentHandle.uData, &uVariantIndex))
        {
            plShaderVariantData* ptVariant = &gptShaderVariantCtx->sbtGraphicsVariants[uVariantIndex];
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

    const uint32_t uComputeShaderCount = pl_sb_size(gptShaderVariantCtx->sbtComputeMeta);
    for(uint32_t uShaderIndex = 0; uShaderIndex < uComputeShaderCount; uShaderIndex++)
    {
        plComputeShaderHandle tParentHandle = gptShaderVariantCtx->sbtComputeMeta[uShaderIndex];
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

    pl_sb_free(gptShaderVariantCtx->sbtMetaVariants);
    pl_sb_free(gptShaderVariantCtx->sbtShaderMeta);
    pl_sb_free(gptShaderVariantCtx->sbtShaderDesc);
    pl_sb_free(gptShaderVariantCtx->sbtComputeMetaVariants);
    pl_sb_free(gptShaderVariantCtx->sbtComputeMeta);
    pl_sb_free(gptShaderVariantCtx->sbtBindGroupLayouts);
    pl_sb_free(gptShaderVariantCtx->sbtGraphicsVariants);
    pl_sb_free(gptShaderVariantCtx->sbtComputeVariants);
    pl_hm32_free(&gptShaderVariantCtx->tGraphicsHashmap);
    pl_hm32_free(&gptShaderVariantCtx->tComputeHashmap);
    pl_hm32_free(&gptShaderVariantCtx->tBindGroupLayoutsHashmap);
    pl_hm32_free(&gptShaderVariantCtx->tComputeParentHashmap);
    pl_hm32_free(&gptShaderVariantCtx->tParentHashmap);
    gptShaderVariantCtx->dParentShaderCount = 0;
    gptShaderVariantCtx->dParentComputeShaderCount = 0;
    gptShaderVariantCtx->dVariantsCount = 0;
    gptShaderVariantCtx->dComputeVariantsCount = 0;
    return true;
}

plBindGroupLayoutHandle
pl_shader_tool_get_bind_group_layout(const char* pcName)
{

    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_str_ex(&gptShaderVariantCtx->tBindGroupLayoutsHashmap, pcName, &uVariantIndex))
    {
        return gptShaderVariantCtx->sbtBindGroupLayouts[uVariantIndex];
    }
    return (plBindGroupLayoutHandle){.uData = UINT32_MAX};
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plCompareMode
pl__shader_tools_get_compare_mode(const char* pcText)
{
    plCompareMode tMode = PL_COMPARE_MODE_UNSPECIFIED;

    if(pcText[0] == 0)
        return tMode;

    if(pcText[16] == 'A')                           tMode = PL_COMPARE_MODE_ALWAYS;
    else if(pcText[16] == 'E')                      tMode = PL_COMPARE_MODE_EQUAL;
    else if(pcText[16] == 'N' && pcText[17] == 'E') tMode = PL_COMPARE_MODE_NEVER;
    else if(pcText[16] == 'N')                      tMode = PL_COMPARE_MODE_NOT_EQUAL;
    else if(pcText[16] == 'L' && pcText[21] == '_') tMode = PL_COMPARE_MODE_LESS_OR_EQUAL;
    else if(pcText[16] == 'L')                      tMode = PL_COMPARE_MODE_LESS;
    else if(pcText[16] == 'G' && pcText[24] == '_') tMode = PL_COMPARE_MODE_GREATER_OR_EQUAL;
    else if(pcText[16] == 'G')                      tMode = PL_COMPARE_MODE_GREATER;
    else
    {
        PL_ASSERT(false);
    }
    return tMode;
}

static plBlendFactor
pl__shader_tools_get_blend_factor(const char* pcText)
{
    plBlendFactor tFactor = PL_BLEND_FACTOR_ZERO;

    if(pcText[0] == 0)
        return tFactor;

    if(pcText[16] == 'Z')                                                tFactor = PL_BLEND_FACTOR_ZERO;
    else if(pcText[16] == 'C' && pcText[25] == 'C')                      tFactor = PL_BLEND_FACTOR_CONSTANT_COLOR;
    else if(pcText[16] == 'C')                                           tFactor = PL_BLEND_FACTOR_CONSTANT_ALPHA;
    else if(pcText[16] == 'S' && pcText[19] == '1' && pcText[21] == 'C') tFactor = PL_BLEND_FACTOR_SRC1_COLOR;
    else if(pcText[16] == 'S' && pcText[19] == '1' && pcText[21] == 'A') tFactor = PL_BLEND_FACTOR_SRC1_ALPHA;
    else if(pcText[16] == 'S' && pcText[20] == 'C')                      tFactor = PL_BLEND_FACTOR_SRC_COLOR;
    else if(pcText[16] == 'S' && pcText[20] == 'A' && pcText[26] == 'S') tFactor = PL_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    else if(pcText[16] == 'S' && pcText[20] == 'A')                      tFactor = PL_BLEND_FACTOR_SRC_ALPHA;
    else if(pcText[16] == 'D' && pcText[20] == 'C')                      tFactor = PL_BLEND_FACTOR_DST_COLOR;
    else if(pcText[16] == 'D' && pcText[20] == 'A')                      tFactor = PL_BLEND_FACTOR_DST_ALPHA;
    else if(pcText[26] == 'C' && pcText[35] == 'C')                      tFactor = PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    else if(pcText[26] == 'C')                                           tFactor = PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    else if(pcText[26] == 'S' && pcText[29] == '1' && pcText[31] == 'C') tFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    else if(pcText[26] == 'S' && pcText[29] == '1' && pcText[31] == 'A') tFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    else if(pcText[26] == 'S' && pcText[30] == 'C')                      tFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    else if(pcText[26] == 'S' && pcText[30] == 'A')                      tFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    else if(pcText[26] == 'D' && pcText[30] == 'C')                      tFactor = PL_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    else if(pcText[26] == 'D' && pcText[30] == 'A')                      tFactor = PL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    else if(pcText[16] == 'O' )                                          tFactor = PL_BLEND_FACTOR_ONE;
    else
    {
        PL_ASSERT(false);
    }
    return tFactor;
}

static plBlendOp
pl__shader_tools_get_blend_op(const char* pcText)
{
    plBlendOp tOp = PL_BLEND_OP_ADD;

    if(pcText[0] == 0)
        return tOp;

    if     (pcText[12] == 'A') tOp = PL_BLEND_OP_ADD;
    else if(pcText[12] == 'S') tOp = PL_BLEND_OP_SUBTRACT;
    else if(pcText[12] == 'R') tOp = PL_BLEND_OP_REVERSE_SUBTRACT;
    else if(pcText[13] == 'I') tOp = PL_BLEND_OP_MIN;
    else if(pcText[13] == 'A') tOp = PL_BLEND_OP_MAX;
    return tOp;
}

static plShaderStageFlags
pl__shader_tools_get_shader_stage(const char* pcText)
{
    plShaderStageFlags tStage = PL_SHADER_STAGE_NONE;

    if     (pcText[16] == 'C') tStage = PL_SHADER_STAGE_COMPUTE;
    else if(pcText[16] == 'F') tStage = PL_SHADER_STAGE_FRAGMENT;
    else if(pcText[16] == 'V') tStage = PL_SHADER_STAGE_VERTEX;
    else if(pcText[16] == 'A') tStage = PL_SHADER_STAGE_ALL;
    else
    {
        PL_ASSERT(false);
    }

    return tStage;
}

static plStencilOp
pl__shader_tools_get_stencil_op(const char* pcText)
{
    plStencilOp tOp = PL_STENCIL_OP_KEEP;

    if     (pcText[14] == 'K')                      tOp = PL_STENCIL_OP_KEEP;
    else if(pcText[14] == 'Z')                      tOp = PL_STENCIL_OP_ZERO;
    else if(pcText[14] == 'R')                      tOp = PL_STENCIL_OP_REPLACE;
    else if(pcText[14] == 'I' && pcText[16] == 'V') tOp = PL_STENCIL_OP_INVERT;
    else if(pcText[14] == 'I' && pcText[28] == 'C') tOp = PL_STENCIL_OP_INCREMENT_AND_CLAMP;
    else if(pcText[14] == 'I')                      tOp = PL_STENCIL_OP_INCREMENT_AND_WRAP;
    else if(pcText[14] == 'D' && pcText[28] == 'C') tOp = PL_STENCIL_OP_DECREMENT_AND_CLAMP;
    else if(pcText[14] == 'D')                      tOp = PL_STENCIL_OP_DECREMENT_AND_WRAP;
    else
    {
        PL_ASSERT(false);
    }
    return tOp;
}

static plVertexFormat
pl__shader_tools_get_vertex_format(const char* pcText)
{

    plVertexFormat tFormat = PL_VERTEX_FORMAT_UNKNOWN;
    if     (pcText[17] == 'F' && pcText[22] == '2') tFormat = PL_VERTEX_FORMAT_FLOAT2;
    else if(pcText[17] == 'F' && pcText[22] == '3') tFormat = PL_VERTEX_FORMAT_FLOAT3;
    else if(pcText[17] == 'F' && pcText[22] == '4') tFormat = PL_VERTEX_FORMAT_FLOAT4;
    else if(pcText[17] == 'F')                      tFormat = PL_VERTEX_FORMAT_FLOAT;
    else if(pcText[17] == 'D' && pcText[23] == '2') tFormat = PL_VERTEX_FORMAT_DOUBLE2;
    else if(pcText[17] == 'D' && pcText[23] == '3') tFormat = PL_VERTEX_FORMAT_DOUBLE3;
    else if(pcText[17] == 'D' && pcText[23] == '4') tFormat = PL_VERTEX_FORMAT_DOUBLE4;
    else if(pcText[17] == 'D')                      tFormat = PL_VERTEX_FORMAT_DOUBLE;
    else if(pcText[17] == 'I' && pcText[20] == '2') tFormat = PL_VERTEX_FORMAT_INT2;
    else if(pcText[17] == 'I' && pcText[20] == '3') tFormat = PL_VERTEX_FORMAT_INT3;
    else if(pcText[17] == 'I' && pcText[20] == '4') tFormat = PL_VERTEX_FORMAT_INT4;
    else if(pcText[17] == 'I')                      tFormat = PL_VERTEX_FORMAT_INT;
    else if(pcText[17] == 'C' && pcText[21] == '2') tFormat = PL_VERTEX_FORMAT_CHAR2;
    else if(pcText[17] == 'C' && pcText[21] == '3') tFormat = PL_VERTEX_FORMAT_CHAR3;
    else if(pcText[17] == 'C' && pcText[21] == '4') tFormat = PL_VERTEX_FORMAT_CHAR4;
    else if(pcText[17] == 'C')                      tFormat = PL_VERTEX_FORMAT_CHAR;
    else if(pcText[17] == 'H' && pcText[21] == '2') tFormat = PL_VERTEX_FORMAT_HALF2;
    else if(pcText[17] == 'H' && pcText[21] == '3') tFormat = PL_VERTEX_FORMAT_HALF3;
    else if(pcText[17] == 'H' && pcText[21] == '4') tFormat = PL_VERTEX_FORMAT_HALF4;
    else if(pcText[17] == 'H')                      tFormat = PL_VERTEX_FORMAT_HALF;
    else if(pcText[17] == 'S' && pcText[22] == '2') tFormat = PL_VERTEX_FORMAT_SHORT2;
    else if(pcText[17] == 'S' && pcText[22] == '3') tFormat = PL_VERTEX_FORMAT_SHORT3;
    else if(pcText[17] == 'S' && pcText[22] == '4') tFormat = PL_VERTEX_FORMAT_SHORT4;
    else if(pcText[17] == 'S')                      tFormat = PL_VERTEX_FORMAT_SHORT;
    else if(pcText[18] == 'I' && pcText[21] == '2') tFormat = PL_VERTEX_FORMAT_UINT2;
    else if(pcText[18] == 'I' && pcText[21] == '3') tFormat = PL_VERTEX_FORMAT_UINT3;
    else if(pcText[18] == 'I' && pcText[21] == '4') tFormat = PL_VERTEX_FORMAT_UINT4;
    else if(pcText[18] == 'I')                      tFormat = PL_VERTEX_FORMAT_UINT;
    else if(pcText[18] == 'C' && pcText[22] == '2') tFormat = PL_VERTEX_FORMAT_UCHAR2;
    else if(pcText[18] == 'C' && pcText[22] == '3') tFormat = PL_VERTEX_FORMAT_UCHAR3;
    else if(pcText[18] == 'C' && pcText[22] == '4') tFormat = PL_VERTEX_FORMAT_UCHAR4;
    else if(pcText[18] == 'C')                      tFormat = PL_VERTEX_FORMAT_UCHAR;
    else if(pcText[18] == 'S' && pcText[23] == '2') tFormat = PL_VERTEX_FORMAT_USHORT2;
    else if(pcText[18] == 'S' && pcText[23] == '3') tFormat = PL_VERTEX_FORMAT_USHORT3;
    else if(pcText[18] == 'S' && pcText[23] == '4') tFormat = PL_VERTEX_FORMAT_USHORT4;
    else if(pcText[18] == 'S')                      tFormat = PL_VERTEX_FORMAT_USHORT;
    else
    {
        PL_ASSERT(false);
    }
    return tFormat;
}

static plDataType
pl__shader_tools_get_data_type(const char* pcText)
{
    plDataType tType = PL_DATA_TYPE_UNSPECIFIED;

    if     (pcText[13] == 'I' && pcText[16] == '4') tType = PL_DATA_TYPE_INT4;
    else if(pcText[13] == 'I' && pcText[16] == '3') tType = PL_DATA_TYPE_INT3;
    else if(pcText[13] == 'I' && pcText[16] == '2') tType = PL_DATA_TYPE_INT2;
    else if(pcText[13] == 'I')                      tType = PL_DATA_TYPE_INT;
    else if(pcText[13] == 'F' && pcText[18] == '4') tType = PL_DATA_TYPE_FLOAT4;
    else if(pcText[13] == 'F' && pcText[18] == '3') tType = PL_DATA_TYPE_FLOAT3;
    else if(pcText[13] == 'F' && pcText[18] == '2') tType = PL_DATA_TYPE_FLOAT2;
    else if(pcText[13] == 'F')                      tType = PL_DATA_TYPE_FLOAT;
    else if(pcText[13] == 'B' && pcText[17] == '4') tType = PL_DATA_TYPE_BOOL4;
    else if(pcText[13] == 'B' && pcText[17] == '3') tType = PL_DATA_TYPE_BOOL3;
    else if(pcText[13] == 'B' && pcText[17] == '2') tType = PL_DATA_TYPE_BOOL2;
    else if(pcText[13] == 'B')                      tType = PL_DATA_TYPE_BOOL;
    else if(pcText[13] == 'C' && pcText[17] == '4') tType = PL_DATA_TYPE_CHAR4;
    else if(pcText[13] == 'C' && pcText[17] == '3') tType = PL_DATA_TYPE_CHAR3;
    else if(pcText[13] == 'C' && pcText[17] == '2') tType = PL_DATA_TYPE_CHAR2;
    else if(pcText[13] == 'C')                      tType = PL_DATA_TYPE_CHAR;
    else if(pcText[13] == 'S' && pcText[18] == '4') tType = PL_DATA_TYPE_SHORT4;
    else if(pcText[13] == 'S' && pcText[18] == '3') tType = PL_DATA_TYPE_SHORT3;
    else if(pcText[13] == 'S' && pcText[18] == '2') tType = PL_DATA_TYPE_SHORT2;
    else if(pcText[13] == 'S')                      tType = PL_DATA_TYPE_SHORT;
    else if(pcText[14] == 'C' && pcText[18] == '4') tType = PL_DATA_TYPE_UCHAR4;
    else if(pcText[14] == 'C' && pcText[18] == '3') tType = PL_DATA_TYPE_UCHAR3;
    else if(pcText[14] == 'C' && pcText[18] == '2') tType = PL_DATA_TYPE_UCHAR2;
    else if(pcText[14] == 'C')                      tType = PL_DATA_TYPE_UCHAR;
    else if(pcText[14] == 'S' && pcText[19] == '4') tType = PL_DATA_TYPE_USHORT4;
    else if(pcText[14] == 'S' && pcText[19] == '3') tType = PL_DATA_TYPE_USHORT3;
    else if(pcText[14] == 'S' && pcText[19] == '2') tType = PL_DATA_TYPE_USHORT2;
    else if(pcText[14] == 'S')                      tType = PL_DATA_TYPE_USHORT;
    else if(pcText[14] == 'I' && pcText[17] == '4') tType = PL_DATA_TYPE_UINT4;
    else if(pcText[14] == 'I' && pcText[17] == '3') tType = PL_DATA_TYPE_UINT3;
    else if(pcText[14] == 'I' && pcText[17] == '2') tType = PL_DATA_TYPE_UINT2;
    else if(pcText[14] == 'I')                      tType = PL_DATA_TYPE_UINT;
    else
    {
        PL_ASSERT(false);
    }

    return tType;
}

static plBufferBindingType
pl__shader_tools_buffer_binding_type(const char* pcText)
{
    plBufferBindingType tType = PL_BUFFER_BINDING_TYPE_UNSPECIFIED;

    if     (pcText[23] == 'S') tType = PL_BUFFER_BINDING_TYPE_STORAGE;
    else if(pcText[23] == 'U') tType = PL_BUFFER_BINDING_TYPE_UNIFORM;
    else
    {
        PL_ASSERT(false);
    }

    return tType;
}

static plTextureBindingType
pl__shader_tools_texture_binding_type(const char* pcText)
{
    plTextureBindingType tType = PL_TEXTURE_BINDING_TYPE_UNSPECIFIED;

    if     (pcText[24] == 'S' && pcText[25] == 'T') tType = PL_TEXTURE_BINDING_TYPE_STORAGE;
    else if(pcText[24] == 'S' && pcText[25] == 'A') tType = PL_TEXTURE_BINDING_TYPE_SAMPLED;
    else if(pcText[24] == 'I')                      tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT;
    else
    {
        PL_ASSERT(false);
    }

    return tType;
}

static plBindGroupLayoutDesc
pl__shader_tools_bind_group_layout_desc(plJsonObject* ptBindGroupLayout)
{
    plBindGroupLayoutDesc tDesc = {0};
    uint32_t uBufferBindingCount = 0;
    plJsonObject* ptBufferBindings = pl_json_array_member(ptBindGroupLayout, "atBufferBindings", &uBufferBindingCount);
    for(uint32_t j = 0; j < uBufferBindingCount; j++)
    {
        plJsonObject* ptBinding = pl_json_member_by_index(ptBufferBindings, j);
        tDesc.atBufferBindings[j].uSlot = pl_json_uint_member(ptBinding, "uSlot", 0);

        char acTypeBuffer[64] = {0};
        pl_json_string_member(ptBinding, "tType", acTypeBuffer, 64);
        tDesc.atBufferBindings[j].tType = pl__shader_tools_buffer_binding_type(acTypeBuffer);

        char acStage0[64] = {0};
        char acStage1[64] = {0};
        char acStage2[64] = {0};
        char* aacStages[3] = {acStage0, acStage1, acStage2};
        uint32_t auLengths[3] = {64, 64, 64};
        uint32_t uStageCount = 0;
        pl_json_string_array_member(ptBinding, "tStages", aacStages, &uStageCount, auLengths);
        for(uint32_t k = 0; k < uStageCount; k++)
            tDesc.atBufferBindings[j].tStages |= pl__shader_tools_get_shader_stage(aacStages[k]);
    }

    uint32_t uTextureBindingCount = 0;
    plJsonObject* ptTextureBindings = pl_json_array_member(ptBindGroupLayout, "atTextureBindings", &uTextureBindingCount);
    for(uint32_t j = 0; j < uTextureBindingCount; j++)
    {
        plJsonObject* ptBinding = pl_json_member_by_index(ptTextureBindings, j);
        tDesc.atTextureBindings[j].uSlot = pl_json_uint_member(ptBinding, "uSlot", 0);
        tDesc.atTextureBindings[j].bNonUniformIndexing = pl_json_bool_member(ptBinding, "bNonUniformIndexing", false);
        tDesc.atTextureBindings[j].uDescriptorCount = pl_json_uint_member(ptBinding, "uDescriptorCount", 0);

        char acTypeBuffer[64] = {0};
        pl_json_string_member(ptBinding, "tType", acTypeBuffer, 64);
        tDesc.atTextureBindings[j].tType = pl__shader_tools_texture_binding_type(acTypeBuffer);

        char acStage0[64] = {0};
        char acStage1[64] = {0};
        char acStage2[64] = {0};
        char* aacStages[3] = {acStage0, acStage1, acStage2};
        uint32_t auLengths[3] = {64, 64, 64};
        uint32_t uStageCount = 0;
        pl_json_string_array_member(ptBinding, "tStages", aacStages, &uStageCount, auLengths);
        for(uint32_t k = 0; k < uStageCount; k++)
            tDesc.atTextureBindings[j].tStages |= pl__shader_tools_get_shader_stage(aacStages[k]);
    }

    uint32_t uSamplerBindingCount = 0;
    plJsonObject* ptSamplerBindings = pl_json_array_member(ptBindGroupLayout, "atSamplerBindings", &uSamplerBindingCount);
    for(uint32_t j = 0; j < uSamplerBindingCount; j++)
    {
        plJsonObject* ptBinding = pl_json_member_by_index(ptSamplerBindings, j);
        tDesc.atSamplerBindings[j].uSlot = pl_json_uint_member(ptBinding, "uSlot", 0);

        char acStage0[64] = {0};
        char acStage1[64] = {0};
        char acStage2[64] = {0};
        char* aacStages[3] = {acStage0, acStage1, acStage2};
        uint32_t auLengths[3] = {64, 64, 64};
        uint32_t uStageCount = 0;
        pl_json_string_array_member(ptBinding, "tStages", aacStages, &uStageCount, auLengths);
        for(uint32_t k = 0; k < uStageCount; k++)
            tDesc.atSamplerBindings[j].tStages |= pl__shader_tools_get_shader_stage(aacStages[k]);
    }
    return tDesc;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_shader_variant_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plShaderVariantI tApi = {
        .initialize                     = pl_shader_variant_initialize,
        .cleanup                        = pl_shader_variant_cleanup,
        .update_stats                   = pl_shader_variant_update_stats,
        .get_compute_shader             = pl_shader_tool_get_compute_shader,
        .get_compute_bind_group_layout  = pl_shader_tool_get_compute_shader_bind_group_layout,
        .get_shader                     = pl_shader_tool_get_shader,
        .get_graphics_bind_group_layout = pl_shader_tool_get_shader_bind_group_layout,
        .get_bind_group_layout          = pl_shader_tool_get_bind_group_layout,
        .load_manifest                  = pl_shader_tool_load_manifest,
        .unload_manifest                = pl_shader_tool_unload_manifest,
    };
    pl_set_api(ptApiRegistry, plShaderVariantI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptGfx    = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptStats  = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptShaderVariantCtx = ptDataRegistry->get_data("plShaderToolsContext");
    }
    else
    {
        static plShaderToolsContext gtShaderVariantCtx = {0};
        gptShaderVariantCtx = &gtShaderVariantCtx;
        ptDataRegistry->set_data("plShaderToolsContext", gptShaderVariantCtx);
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

#ifndef PL_UNITY_BUILD

    #define PL_JSON_IMPLEMENTATION
    #include "pl_json.h"
    #undef PL_JSON_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif