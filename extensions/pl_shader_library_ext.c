/*
   pl_shader_library_ext.c
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
#include "pl_shader_library_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_shader_ext.h"
#include "pl_vfs_ext.h"
#include "pl_profile_ext.h"

// libs
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

    static const plGraphicsI* gptGfx     = NULL;
    static const plShaderI*   gptShader  = NULL;
    static const plVfsI*      gptVfs     = NULL;
    static const plProfileI*  gptProfile = NULL;

#endif

// libs
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plShaderLibraryGraphicsEntry
{
    plShaderDesc     tDesc; // recipe/template
    plHashMap64      tVariantHashmap;
    plShaderHandle*  sbtVariantHandles;

    // uint32_t         uGeneration;
    bool             bActive;
} plShaderLibraryGraphicsEntry;

typedef struct _plShaderLibraryComputeEntry
{
    plComputeShaderDesc     tDesc; // recipe/template
    plHashMap64             tVariantHashmap;
    plComputeShaderHandle*  sbtVariantHandles;

    // uint32_t         uGeneration;
    bool             bActive;
} plShaderLibraryComputeEntry;

typedef struct _plShaderLibrary
{
    // shaders meta
    plHashMap32       tGraphicsHashmap; // string -> index
    plShaderLibraryGraphicsEntry* sbtShaderEntries;

    // compute shaders meta
    plHashMap32            tComputeHashmap; // string -> index
    plShaderLibraryComputeEntry* sbtComputeShaderEntries;

    // bind group layouts
    plHashMap32              tBindGroupLayoutsHashmap;
    plBindGroupLayoutHandle* sbtBindGroupLayouts;

} plShaderLibrary;

typedef struct _plShaderToolsContext
{
    plDevice* ptDevice;
} plShaderToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plShaderToolsContext* gptShaderLibraryCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public implementation
//-----------------------------------------------------------------------------

void
pl_shader_library_initialize(plShaderLibraryInit tDesc)
{
    gptShaderLibraryCtx->ptDevice = tDesc.ptDevice;
}

void
pl_shader_library_cleanup_library(plShaderLibrary* ptLibrary)
{
    const uint32_t uEntryCount = pl_sb_size(ptLibrary->sbtShaderEntries);
    for(uint32_t i = 0; i < uEntryCount; i++)
    {
        plShaderLibraryGraphicsEntry* ptEntry = &ptLibrary->sbtShaderEntries[i];

        if(!ptEntry->bActive)
            continue;

        const uint32_t uVariantCount = pl_sb_size(ptEntry->sbtVariantHandles);
        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            gptGfx->queue_shader_for_deletion(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[j]); 
        }

        pl_sb_free(ptEntry->sbtVariantHandles);
        pl_hm64_free(&ptEntry->tVariantHashmap);
    }

    const uint32_t uComputeVariantDataCount = pl_sb_size(ptLibrary->sbtComputeShaderEntries);
    for(uint32_t i = 0; i < uComputeVariantDataCount; i++)
    {
        plShaderLibraryComputeEntry* ptEntry = &ptLibrary->sbtComputeShaderEntries[i];

        const uint32_t uVariantCount = pl_sb_size(ptEntry->sbtVariantHandles);
        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            plComputeShader* ptShader = gptGfx->get_compute_shader(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[j]);
            gptGfx->queue_compute_shader_for_deletion(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[j]); 
        }

        pl_sb_free(ptEntry->sbtVariantHandles);
        pl_hm64_free(&ptEntry->tVariantHashmap);
    }

    pl_sb_free(ptLibrary->sbtShaderEntries);
    pl_sb_free(ptLibrary->sbtComputeShaderEntries);
    pl_sb_free(ptLibrary->sbtBindGroupLayouts);
    pl_hm32_free(&ptLibrary->tGraphicsHashmap);
    pl_hm32_free(&ptLibrary->tComputeHashmap);
    pl_hm32_free(&ptLibrary->tBindGroupLayoutsHashmap);
    PL_FREE(ptLibrary);
}

void
pl_shader_library_unregister_shader(plShaderLibrary* ptLibrary, const char* pcName)
{
    uint32_t uBaseIndex = UINT32_MAX;
    if(!pl_hm32_has_key_str_ex(&ptLibrary->tGraphicsHashmap, pcName, &uBaseIndex))
    {
        return;
    }

    plShaderLibraryGraphicsEntry* ptEntry = &ptLibrary->sbtShaderEntries[uBaseIndex];
    const uint32_t uVariantCount = pl_sb_size(ptEntry->sbtVariantHandles);
    for(uint32_t i = 0; i < uVariantCount; i++)
    {
        gptGfx->queue_shader_for_deletion(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[i]); 
    }
    pl_sb_free(ptEntry->sbtVariantHandles);
    pl_hm64_free(&ptEntry->tVariantHashmap);
    ptEntry->bActive = false;
    pl_hm32_remove_str(&ptLibrary->tGraphicsHashmap, pcName);
}

void
pl_shader_library_unregister_shaders(plShaderLibrary* ptLibrary)
{
    uint32_t uShaderCount = pl_sb_size(ptLibrary->sbtShaderEntries);
    for(uint32_t uBaseIndex = 0; uBaseIndex < uShaderCount; uBaseIndex++)
    {
        plShaderLibraryGraphicsEntry* ptEntry = &ptLibrary->sbtShaderEntries[uBaseIndex];
        if(ptEntry->bActive)
        {
            const uint32_t uVariantCount = pl_sb_size(ptEntry->sbtVariantHandles);
            for(uint32_t i = 0; i < uVariantCount; i++)
            {
                gptGfx->queue_shader_for_deletion(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[i]); 
            }
            pl_sb_free(ptEntry->sbtVariantHandles);
            pl_hm64_free(&ptEntry->tVariantHashmap);
            ptEntry->bActive = false;
        }
    }
    pl_hm32_free(&ptLibrary->tGraphicsHashmap);
    pl_sb_reset(ptLibrary->sbtShaderEntries);
}

void
pl_shader_library_unregister_compute_shader(plShaderLibrary* ptLibrary, const char* pcName)
{
    uint32_t uBaseIndex = UINT32_MAX;
    if(!pl_hm32_has_key_str_ex(&ptLibrary->tComputeHashmap, pcName, &uBaseIndex))
    {
        return;
    }

    plShaderLibraryComputeEntry* ptEntry = &ptLibrary->sbtComputeShaderEntries[uBaseIndex];
    const uint32_t uVariantCount = pl_sb_size(ptEntry->sbtVariantHandles);
    for(uint32_t i = 0; i < uVariantCount; i++)
    {
        gptGfx->queue_compute_shader_for_deletion(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[i]); 
    }
    pl_sb_free(ptEntry->sbtVariantHandles);
    pl_hm64_free(&ptEntry->tVariantHashmap);
    ptEntry->bActive = false;
    pl_hm32_remove_str(&ptLibrary->tComputeHashmap, pcName);
}

void
pl_shader_library_unregister_compute_shaders(plShaderLibrary* ptLibrary)
{
    uint32_t uShaderCount = pl_sb_size(ptLibrary->sbtComputeShaderEntries);
    for(uint32_t uBaseIndex = 0; uBaseIndex < uShaderCount; uBaseIndex++)
    {
        plShaderLibraryComputeEntry* ptEntry = &ptLibrary->sbtComputeShaderEntries[uBaseIndex];
        if(ptEntry->bActive)
        {
            const uint32_t uVariantCount = pl_sb_size(ptEntry->sbtVariantHandles);
            for(uint32_t i = 0; i < uVariantCount; i++)
            {
                gptGfx->queue_compute_shader_for_deletion(gptShaderLibraryCtx->ptDevice, ptEntry->sbtVariantHandles[i]); 
            }
            pl_sb_free(ptEntry->sbtVariantHandles);
            pl_hm64_free(&ptEntry->tVariantHashmap);
            ptEntry->bActive = false;
        }
    }
    pl_hm32_free(&ptLibrary->tComputeHashmap);
    pl_sb_reset(ptLibrary->sbtComputeShaderEntries);
}

plShaderHandle
pl_shader_library_get_shader(plShaderLibrary* ptLibrary, const char* pcName, const plShaderVariantDesc* ptDesc)
{

    uint32_t uEntryIndex = UINT32_MAX;
    if(!pl_hm32_has_key_str_ex(&ptLibrary->tGraphicsHashmap, pcName, &uEntryIndex))
    {
        return (plShaderHandle){.uData = UINT32_MAX};
    }

    plShaderLibraryGraphicsEntry* ptEntry = &ptLibrary->sbtShaderEntries[uEntryIndex];

    const plGraphicsState* ptGraphicsState = ptDesc->ptGraphicsState ? ptDesc->ptGraphicsState : &ptEntry->tDesc.tGraphicsState;

    plDevice* ptDevice = gptShaderLibraryCtx->ptDevice;

    size_t szVertexSpecializationSize = 0;
    for(uint32_t i = 0; i < ptEntry->tDesc._uVertexConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptEntry->tDesc.atVertexConstants[i];
        szVertexSpecializationSize += gptGfx->get_data_type_size(ptConstant->eType);
    }

    size_t szFragmentSpecializationSize = 0;
    for(uint32_t i = 0; i < ptEntry->tDesc._uFragmentConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptEntry->tDesc.atFragmentConstants[i];
        szFragmentSpecializationSize += gptGfx->get_data_type_size(ptConstant->eType);
    }

    uint64_t ulVariantHash = pl_hm_hash(ptDesc->pVertexConstants, szVertexSpecializationSize, ptGraphicsState->ulValue);
    ulVariantHash = pl_hm_hash(ptDesc->pFragmentConstants, szFragmentSpecializationSize, ulVariantHash);
    const uint64_t ulIndex = pl_hm_lookup(&ptEntry->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptEntry->sbtVariantHandles[ulIndex];

    pl_hm_insert(&ptEntry->tVariantHashmap, ulVariantHash, pl_sb_size(ptEntry->sbtVariantHandles));

    plShaderDesc tDesc = ptEntry->tDesc;
    tDesc.tGraphicsState = *ptGraphicsState;
    tDesc.pVertexTempConstantData = ptDesc->pVertexConstants;
    tDesc.pFragmentTempConstantData = ptDesc->pFragmentConstants;
    tDesc.pcDebugName = pcName;

    plShaderHandle tShader = gptGfx->create_shader(ptDevice, &tDesc);
    pl_sb_push(ptEntry->sbtVariantHandles, tShader);
    return tShader;
}

void
pl_shader_library_register_shader(plShaderLibrary* ptLibrary, const char* pcName, const plShaderDesc* ptDesc)
{
    if(pl_hm32_has_key_str(&ptLibrary->tGraphicsHashmap, pcName))
    {
        return;
    }

    uint32_t uVariantIndex = pl_hm32_get_free_index(&ptLibrary->tGraphicsHashmap);
    if(uVariantIndex == PL_DS_HASH32_INVALID)
    {
        uVariantIndex = pl_sb_size(ptLibrary->sbtShaderEntries);
        pl_sb_add(ptLibrary->sbtShaderEntries);
    }
    pl_hm32_insert_str(&ptLibrary->tGraphicsHashmap, pcName, uVariantIndex);

    plShaderLibraryGraphicsEntry* ptEntry = &ptLibrary->sbtShaderEntries[uVariantIndex];
    ptEntry->tDesc = *ptDesc;
    ptEntry->bActive = true;

    plShaderVariantDesc tVariantInfo = {
        .ptGraphicsState = &ptDesc->tGraphicsState,
        .ptAttachmentInfo = &ptDesc->tRenderAttachmentInfo,
        .pFragmentConstants = ptDesc->pFragmentTempConstantData,
        .pVertexConstants = ptDesc->pVertexTempConstantData
    };

    plShaderHandle tHandle = pl_shader_library_get_shader(ptLibrary, pcName, &tVariantInfo);
    (void)tHandle;
}

plComputeShaderHandle
pl_shader_library_get_compute_shader(plShaderLibrary* ptLibrary, const char* pcName, const plComputeShaderVariantDesc* ptDesc)
{
    uint32_t uEntryIndex = UINT32_MAX;
    if(!pl_hm32_has_key_str_ex(&ptLibrary->tComputeHashmap, pcName, &uEntryIndex))
    {
        return (plComputeShaderHandle){.uData = UINT32_MAX};
    }

    plDevice* ptDevice = gptShaderLibraryCtx->ptDevice;

    plShaderLibraryComputeEntry* ptEntry = &ptLibrary->sbtComputeShaderEntries[uEntryIndex];

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptEntry->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptEntry->tDesc.atConstants[i];
        szSpecializationSize += gptGfx->get_data_type_size(ptConstant->eType);
    }

    const void* pConstants = NULL;
    if(ptDesc && ptDesc->pConstants)
        pConstants = ptDesc->pConstants;

    const uint64_t ulVariantHash = pl_hm_hash(pConstants, szSpecializationSize, (uint64_t)uEntryIndex);
    const uint64_t ulIndex = pl_hm_lookup(&ptEntry->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptEntry->sbtVariantHandles[ulIndex];

    pl_hm_insert(&ptEntry->tVariantHashmap, ulVariantHash, pl_sb_size(ptEntry->sbtVariantHandles));

    plComputeShaderDesc tDesc = ptEntry->tDesc;
    tDesc.pTempConstantData = pConstants;
    tDesc.pcDebugName = pcName;

    plComputeShaderHandle tShader = gptGfx->create_compute_shader(ptDevice, &tDesc);

    pl_sb_push(ptEntry->sbtVariantHandles, tShader);
    return tShader;
}

void
pl_shader_library_register_compute_shader(plShaderLibrary* ptLibrary, const char* pcName, const plComputeShaderDesc* ptDesc)
{
    if(pl_hm32_has_key_str(&ptLibrary->tComputeHashmap, pcName))
    {
        return;
    }

    uint32_t uVariantIndex = pl_hm32_get_free_index(&ptLibrary->tComputeHashmap);
    if(uVariantIndex == PL_DS_HASH32_INVALID)
    {
        uVariantIndex = pl_sb_size(ptLibrary->sbtComputeShaderEntries);
        pl_sb_add(ptLibrary->sbtComputeShaderEntries);
    }
    pl_hm32_insert_str(&ptLibrary->tComputeHashmap, pcName, uVariantIndex);

    plShaderLibraryComputeEntry* ptEntry = &ptLibrary->sbtComputeShaderEntries[uVariantIndex];
    ptEntry->tDesc = *ptDesc;
    ptEntry->bActive = true;

    for (uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptEntry->tDesc.atConstants[i];
        if(ptConstant->eType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        ptEntry->tDesc._uConstantCount++;
    }

    plComputeShaderVariantDesc tVariantDesc = {
        .pConstants = ptDesc->pTempConstantData
    };

    plComputeShaderHandle tHandle = pl_shader_library_get_compute_shader(ptLibrary, pcName, &tVariantDesc);
    (void)tHandle;
}

void
pl_shader_library_register_bind_group_layout(plShaderLibrary* ptLibrary, const char* pcName, plBindGroupLayoutHandle tHandle)
{
    if(pl_hm32_has_key_str(&ptLibrary->tBindGroupLayoutsHashmap, pcName))
    {
        return;
    }

    uint32_t uVariantIndex = pl_hm32_get_free_index(&ptLibrary->tBindGroupLayoutsHashmap);
    if(uVariantIndex == PL_DS_HASH32_INVALID)
    {
        uVariantIndex = pl_sb_size(ptLibrary->sbtBindGroupLayouts);
        pl_sb_add(ptLibrary->sbtBindGroupLayouts);
    }
    pl_hm32_insert_str(&ptLibrary->tBindGroupLayoutsHashmap, pcName, uVariantIndex);
    ptLibrary->sbtBindGroupLayouts[uVariantIndex] = tHandle;
}

plBindGroupLayoutHandle
pl_shader_library_get_bind_group_layout(plShaderLibrary* ptLibrary, const char* pcName)
{

    uint32_t uVariantIndex = UINT32_MAX;
    if(pl_hm32_has_key_str_ex(&ptLibrary->tBindGroupLayoutsHashmap, pcName, &uVariantIndex))
    {
        return ptLibrary->sbtBindGroupLayouts[uVariantIndex];
    }
    return (plBindGroupLayoutHandle){.uData = UINT32_MAX};
}

plShaderLibrary*
pl_shader_library_create_library(void)
{
    plShaderLibrary* ptLibrary = PL_ALLOC(sizeof(plShaderLibrary));
    memset(ptLibrary, 0, sizeof(plShaderLibrary));

    return ptLibrary;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_shader_library_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plShaderLibraryI tApi = {
        .initialize                     = pl_shader_library_initialize,
        .cleanup_library                = pl_shader_library_cleanup_library,
        .get_compute_shader             = pl_shader_library_get_compute_shader,
        .get_shader                     = pl_shader_library_get_shader,
        .get_bind_group_layout          = pl_shader_library_get_bind_group_layout,
        .register_bind_group_layout     = pl_shader_library_register_bind_group_layout,
        .register_shader                = pl_shader_library_register_shader,
        .register_compute_shader        = pl_shader_library_register_compute_shader,
        .unregister_shader              = pl_shader_library_unregister_shader,
        .unregister_shaders             = pl_shader_library_unregister_shaders,
        .unregister_compute_shader      = pl_shader_library_unregister_compute_shader,
        .unregister_compute_shaders     = pl_shader_library_unregister_compute_shaders,
        .create_library                 = pl_shader_library_create_library,
    };
    pl_set_api(ptApiRegistry, plShaderLibraryI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptShader  = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptVfs     = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptShaderLibraryCtx = ptDataRegistry->get_data("plShaderToolsContext");
    }
    else
    {
        static plShaderToolsContext gtShaderVariantCtx = {0};
        gptShaderLibraryCtx = &gtShaderVariantCtx;
        ptDataRegistry->set_data("plShaderToolsContext", gptShaderLibraryCtx);
    }
}

void
pl_unload_shader_library_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plShaderLibraryI* ptApi = pl_get_api_latest(ptApiRegistry, plShaderLibraryI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

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