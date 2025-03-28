/*
   pl_resource_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] internal enums
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_resource_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_file_ext.h"
#include "pl_image_ext.h"

// libs
#include "pl_string.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// stb
#include "stb_image_resize2.h"

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

    static const plGraphicsI*      gptGfx           = NULL;
    static const plGPUAllocatorsI* gptGpuAllocators = NULL;
    static const plFileI*          gptFile          = NULL;
    static const plImageI*         gptImage         = NULL;
#endif

// libs
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plResource        plResource;
typedef struct _plResourceManager plResourceManager;

// enums/flags
typedef int plResourceDataType;

//-----------------------------------------------------------------------------
// [SECTION] internal enums
//-----------------------------------------------------------------------------

enum _plResourceDataType
{
    PL_RESOURCE_DATA_TYPE_NONE = 0,
    PL_RESOURCE_DATA_TYPE_IMAGE
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plResource
{
    char                acName[PL_MAX_NAME_LENGTH];
    char                acContainerFileName[PL_MAX_NAME_LENGTH];
    plResourceLoadFlags tFlags;
    plResourceDataType  tType;
    uint8_t*            puFileData;
    size_t              szFileDataSize;
    size_t              szContainerFileOffset;
    plTextureHandle     tTexture;
} plResource;

typedef struct _plResourceManager
{
    plResourceManagerInit tDesc;

    plResource*     sbtResources;
    uint32_t*       sbtResourceGenerations;
    plHashMap*      ptNameHashmap;
    plTempAllocator tTempAllocator;

    // command pools
    plCommandPool* atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];

    // staging (more robust system should replace this)
    plBufferHandle tStagingBufferHandle[PL_MAX_FRAMES_IN_FLIGHT];

    // GPU allocators
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingCachedAllocator;
} plResourceManager;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plResourceManager* gptResourceManager = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public implementation
//-----------------------------------------------------------------------------

void
pl_resource_initialize(plResourceManagerInit tDesc)
{
    if(tDesc.uMaxTextureResolution == 0)
        tDesc.uMaxTextureResolution = 1024;

    gptResourceManager->tDesc = tDesc;

    plDevice* ptDevice = tDesc.ptDevice;

    // load gpu allocators
    gptResourceManager->ptLocalBuddyAllocator           = gptGpuAllocators->get_local_buddy_allocator(ptDevice);
    gptResourceManager->ptLocalDedicatedAllocator       = gptGpuAllocators->get_local_dedicated_allocator(ptDevice);
    gptResourceManager->ptStagingUnCachedAllocator      = gptGpuAllocators->get_staging_uncached_allocator(ptDevice);
    gptResourceManager->ptStagingUnCachedBuddyAllocator = gptGpuAllocators->get_staging_uncached_buddy_allocator(ptDevice);
    gptResourceManager->ptStagingCachedAllocator        = gptGpuAllocators->get_staging_cached_allocator(ptDevice);

    // create staging buffers
    const plBufferDesc tStagingBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STAGING,
        .szByteSize = 268435456,
        .pcDebugName = "Resource Staging Buffer"
    };

    plDeviceMemoryAllocatorI* ptAllocator = gptResourceManager->ptStagingUnCachedAllocator;

    // create per frame resources
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // command pools
        gptResourceManager->atCmdPools[i] = gptGfx->create_command_pool(ptDevice, NULL);

        plBuffer* ptBuffer = NULL;
        gptResourceManager->tStagingBufferHandle[i] = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptBuffer->tMemoryRequirements.ulSize,
            ptBuffer->tMemoryRequirements.ulAlignment,
            "resource manager staging buffer");

        // bind memory
        gptGfx->bind_buffer_to_memory(ptDevice, gptResourceManager->tStagingBufferHandle[i], &tAllocation);
    }

    // pushing invalid data so "default-to-zero" should work
    // for resource handles
    pl_sb_push(gptResourceManager->sbtResourceGenerations, UINT32_MAX);
    pl_sb_add(gptResourceManager->sbtResources);
}

void
pl_resource_cleanup(void)
{
    pl_sb_free(gptResourceManager->sbtResourceGenerations);
    pl_sb_free(gptResourceManager->sbtResources);
    pl_hm_free(gptResourceManager->ptNameHashmap);
    pl_temp_allocator_free(&gptResourceManager->tTempAllocator);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->cleanup_command_pool(gptResourceManager->atCmdPools[i]);
}

plResourceHandle
pl_resource_load_ex(const char* pcName, plResourceLoadFlags tFlags, uint8_t* puOriginalFileData, size_t szFileByteSize, const char* pcContainerFileName, size_t szFileBytesOffset)
{
    // check if resource already exists
    const uint64_t ulHash = pl_hm_hash_str(pcName);
    if(pl_hm_has_key(gptResourceManager->ptNameHashmap, ulHash))
    {
        uint64_t ulExistingSlot = pl_hm_lookup(gptResourceManager->ptNameHashmap, ulHash);

        plResourceHandle tResource = {
            .uIndex      = (uint32_t)ulExistingSlot,
            .uGeneration = gptResourceManager->sbtResourceGenerations[ulExistingSlot]
        };
        return tResource;
    }

    // figure out type of resource
    plResourceDataType tDataType = PL_RESOURCE_DATA_TYPE_NONE;
    char acFileExtension[8] = {0};
    pl_str_get_file_extension(pcName, acFileExtension, 8);

    for(uint32_t i = 0; i < 8; i++)
        acFileExtension[i] = pl_str_to_upper(acFileExtension[i]);

    const char* apcImageExtensions[] = { "PNG", "JPEG", "JPG", "PNG", "BMP", "TGA","HDR" };

    for(uint32_t i = 0; i < PL_ARRAYSIZE(apcImageExtensions); i++)
    {
        if(pl_str_equal(acFileExtension, apcImageExtensions[i]))
        {
            tDataType = PL_RESOURCE_DATA_TYPE_IMAGE;
            break;
        }
    }

    // data type not supported or found
    if(tDataType == PL_RESOURCE_DATA_TYPE_NONE)
        return (plResourceHandle){0};

    // we can assume the container file is the same as
    // the resource file
    if(pcContainerFileName == NULL)
    {
        pcContainerFileName = pcName;
        szFileBytesOffset = 0;
    }

    uint8_t* puFileData = puOriginalFileData;

    // load file data
    if(puFileData == NULL)
    {
        gptFile->binary_read(pcContainerFileName, &szFileByteSize, NULL);
        puFileData = PL_ALLOC(szFileByteSize);
        memset(puFileData, 0, szFileByteSize);
        gptFile->binary_read(pcContainerFileName, &szFileByteSize, puFileData);
    }

    plResource tResource = {
        .tFlags                = tFlags,
        .szFileDataSize        = 0,
        .puFileData            = NULL,
        .szContainerFileOffset = szFileBytesOffset
    };

    // if we are retaining file data, either copy the manually loaded data
    // or use the memory we just allocated
    if((tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_FILE_DATA))
    {
        tResource.szFileDataSize = szFileByteSize;
        if(puOriginalFileData)
        {
            tResource.puFileData = PL_ALLOC(szFileByteSize);
            memcpy(tResource.puFileData, puOriginalFileData, szFileByteSize);
        }
        else
            tResource.puFileData = puFileData;
    }


    strncpy(tResource.acName, pcName, PL_MAX_NAME_LENGTH);
    strncpy(tResource.acContainerFileName, pcContainerFileName, PL_MAX_NAME_LENGTH);

    // find our resource a home
    uint64_t uIndex = pl_hm_get_free_index(gptResourceManager->ptNameHashmap);
    if(uIndex == UINT64_MAX)
    {
        uIndex = pl_sb_size(gptResourceManager->sbtResourceGenerations);
        pl_sb_push(gptResourceManager->sbtResourceGenerations, 0);
        pl_sb_add(gptResourceManager->sbtResources);
    }
    pl_hm_insert_str(gptResourceManager->ptNameHashmap, pcName, uIndex);

    // perform any post processing our resource may need
    switch(tDataType) //-V785
    {
        case PL_RESOURCE_DATA_TYPE_IMAGE:
        {
            plDevice* ptDevice = gptResourceManager->tDesc.ptDevice;
            plCommandPool* ptCmdPool = gptResourceManager->atCmdPools[gptGfx->get_current_frame_index()];
            plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptResourceManager->tStagingBufferHandle[gptGfx->get_current_frame_index()]);
            plTexture* ptTexture = NULL;

            bool bResizeNeeded = false;
            plImageInfo tImageInfo = {0};
            if(gptImage->get_info((unsigned char*)puFileData, (int)szFileByteSize, &tImageInfo))
            {

                // determine if image needs to be resized
                uint32_t uMaxDim = (uint32_t)pl_max(tImageInfo.iWidth, tImageInfo.iHeight);
                if(uMaxDim > gptResourceManager->tDesc.uMaxTextureResolution && !tImageInfo.b16Bit)
                {
                    bResizeNeeded = true;
                    int iNewWidth = 0;
                    int iNewHeight = 0;

                    if(tImageInfo.iWidth > tImageInfo.iHeight)
                    {
                        iNewWidth = (int)gptResourceManager->tDesc.uMaxTextureResolution;
                        iNewHeight = (int)(((float)gptResourceManager->tDesc.uMaxTextureResolution / (float)tImageInfo.iWidth) * (float)tImageInfo.iHeight);
                    }
                    else
                    {
                        iNewWidth = (int)(((float)gptResourceManager->tDesc.uMaxTextureResolution / (float)tImageInfo.iHeight) * (float)tImageInfo.iWidth);
                        iNewHeight = (int)gptResourceManager->tDesc.uMaxTextureResolution;
                    }

                    tImageInfo.iWidth = iNewWidth;
                    tImageInfo.iHeight = iNewHeight;
                }

                int iTextureWidth = 0;
                int iTextureHeight = 0;
                int iTextureChannels = 0;
                plFormat tTextureFormat = PL_FORMAT_UNKNOWN;

                if(tImageInfo.bHDR)
                {
                    float* pfRawBytes = gptImage->load_hdr((unsigned char*)puFileData, (int)szFileByteSize, &iTextureWidth, &iTextureHeight, &iTextureChannels, 4);

                    if(bResizeNeeded)
                    {
                        float* pfOldRawBytes = pfRawBytes;
                        pfRawBytes = stbir_resize_float_linear(pfRawBytes, iTextureWidth, iTextureHeight, 0, NULL, tImageInfo.iWidth, tImageInfo.iHeight, 0, STBIR_RGBA);
                        PL_ASSERT(pfRawBytes);
                        gptImage->free(pfOldRawBytes);
                    }

                    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pfRawBytes, tImageInfo.iWidth * tImageInfo.iHeight * 4 * sizeof(float));
                    gptImage->free(pfRawBytes);

                    tTextureFormat = PL_FORMAT_R32G32B32A32_FLOAT;
                }
                else if(tImageInfo.b16Bit)
                {
                    unsigned short* puRawBytes = gptImage->load_16bit((unsigned char*)puFileData, (int)szFileByteSize, &iTextureWidth, &iTextureHeight, &iTextureChannels, 4);
                    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, puRawBytes, iTextureWidth * iTextureHeight * 4 * sizeof(short));
                    gptImage->free(puRawBytes);

                    tTextureFormat = PL_FORMAT_R16G16B16A16_UNORM;
                }
                else
                {
                    unsigned char* puRawBytes = gptImage->load((unsigned char*)puFileData, (int)szFileByteSize, &iTextureWidth, &iTextureHeight, &iTextureChannels, 4);

                    if(bResizeNeeded)
                    {
                        unsigned char* puOldRawBytes = puRawBytes;
                        puRawBytes = stbir_resize_uint8_linear(puRawBytes, iTextureWidth, iTextureHeight, 0, NULL, tImageInfo.iWidth, tImageInfo.iHeight, 0, STBIR_RGBA);
                        PL_ASSERT(puRawBytes);
                        gptImage->free(puOldRawBytes);
                    }

                    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, puRawBytes, tImageInfo.iWidth * tImageInfo.iHeight * 4);
                    gptImage->free(puRawBytes);

                    tTextureFormat = PL_FORMAT_R8G8B8A8_UNORM;
                }

                // create texture
                const plTextureDesc tTextureDesc = {
                    .tDimensions = {(float)tImageInfo.iWidth, (float)tImageInfo.iHeight, 1},
                    .tFormat     = tTextureFormat,
                    .uLayers     = 1,
                    .uMips       = 0,
                    .tType       = PL_TEXTURE_TYPE_2D,
                    .tUsage      = PL_TEXTURE_USAGE_SAMPLED
                };
            
                tResource.tTexture = gptGfx->create_texture(ptDevice, &tTextureDesc, &ptTexture);
                        
                // choose allocator
                plDeviceMemoryAllocatorI* ptAllocator = gptResourceManager->ptLocalBuddyAllocator;
                if(ptTexture->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
                    ptAllocator = gptResourceManager->ptLocalDedicatedAllocator;
            
                // allocate memory
                const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
                    ptTexture->tMemoryRequirements.uMemoryTypeBits,
                    ptTexture->tMemoryRequirements.ulSize,
                    ptTexture->tMemoryRequirements.ulAlignment,
                    pl_temp_allocator_sprintf(&gptResourceManager->tTempAllocator, "texture alloc %s", pcName));
                pl_temp_allocator_reset(&gptResourceManager->tTempAllocator);
            
                // bind memory
                gptGfx->bind_texture_to_memory(ptDevice, tResource.tTexture, &tAllocation);
                
                // update texture data & calculate mips
                plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
                gptGfx->begin_command_recording(ptCommandBuffer, NULL);
                plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
                gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
                gptGfx->set_texture_usage(ptBlitEncoder, tResource.tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);
                    
                const plBufferImageCopy tBufferImageCopy = {
                    .uImageWidth = (uint32_t)tTextureDesc.tDimensions.x,
                    .uImageHeight = (uint32_t)tTextureDesc.tDimensions.y,
                    .uImageDepth = 1,
                    .uLayerCount = 1
                };
            
                gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptResourceManager->tStagingBufferHandle[gptGfx->get_current_frame_index()], tResource.tTexture, 1, &tBufferImageCopy);
                gptGfx->generate_mipmaps(ptBlitEncoder, tResource.tTexture);
            
                gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
                gptGfx->end_blit_pass(ptBlitEncoder);
                gptGfx->end_command_recording(ptCommandBuffer);
                gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
                gptGfx->wait_on_command_buffer(ptCommandBuffer);
                gptGfx->return_command_buffer(ptCommandBuffer);
            }
            break;
        }
    }
    
    plResourceHandle tNewResource = {
        .uIndex      = (uint32_t)uIndex,
        .uGeneration = gptResourceManager->sbtResourceGenerations[uIndex]
    };

    gptResourceManager->sbtResources[uIndex] = tResource;

    // free data if we own it & are not retaining it
    if(!(tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_FILE_DATA) && puOriginalFileData == NULL)
    {
        PL_FREE(puFileData);
    }

    return tNewResource;
}

plResourceHandle
pl_resource_load(const char* pcName, plResourceLoadFlags tFlags)
{
    return pl_resource_load_ex(pcName, tFlags, NULL, 0, pcName, 0);
}

plTextureHandle
pl_resource_get_texture_handle(plResourceHandle tHandle)
{
    if(tHandle.uGeneration != gptResourceManager->sbtResourceGenerations[tHandle.uIndex])
        return (plTextureHandle){0};

    return gptResourceManager->sbtResources[tHandle.uIndex].tTexture;
}

bool
pl_resource_is_valid(plResourceHandle tHandle)
{
    return (tHandle.uGeneration == gptResourceManager->sbtResourceGenerations[tHandle.uIndex]);
}

bool
pl_resource_is_loaded(const char* pcName)
{
    return pl_hm_has_key_str(gptResourceManager->ptNameHashmap, pcName);
}

void
pl_resource_clear_all(void)
{
    const uint32_t uResourceCount = pl_sb_size(gptResourceManager->sbtResources);
    for(uint32_t i = 1; i < uResourceCount; i++)
    {
        plResource* ptResource = &gptResourceManager->sbtResources[i];
        if(gptGfx->is_texture_valid(gptResourceManager->tDesc.ptDevice, ptResource->tTexture))
        {
            gptGfx->queue_texture_for_deletion(gptResourceManager->tDesc.ptDevice, ptResource->tTexture);
        }

        // free file data if we retained it
        if(ptResource->tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_FILE_DATA)
        {
            if(ptResource->puFileData)
            {
                PL_FREE(ptResource->puFileData);
            }
            ptResource->puFileData = NULL;
            ptResource->szFileDataSize = 0;
        }

        if(pl_hm_has_key_str(gptResourceManager->ptNameHashmap, ptResource->acName))
        {
            pl_hm_remove_str(gptResourceManager->ptNameHashmap, ptResource->acName);
        }

        
    }
    if(gptResourceManager->ptNameHashmap)
    {
       PL_ASSERT(gptResourceManager->ptNameHashmap->_uItemCount == 0);
    }
}

const uint8_t*
pl_resource_get_file_data(plResourceHandle tHandle, size_t* pSzFileByteSizeOut)
{
    if(pl_resource_is_valid(tHandle))
    {
        if(pSzFileByteSizeOut)
            *pSzFileByteSizeOut = gptResourceManager->sbtResources[tHandle.uIndex].szFileDataSize;
        return gptResourceManager->sbtResources[tHandle.uIndex].puFileData;
    }

    if(pSzFileByteSizeOut)
        *pSzFileByteSizeOut = 0;
    return NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_resource_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plResourceI tApi = {
        .initialize    = pl_resource_initialize,
        .cleanup       = pl_resource_cleanup,
        .load          = pl_resource_load,
        .load_ex       = pl_resource_load_ex,
        .get_texture   = pl_resource_get_texture_handle,
        .is_valid      = pl_resource_is_valid,
        .is_loaded     = pl_resource_is_loaded,
        .clear         = pl_resource_clear_all,
        .get_file_data = pl_resource_get_file_data
    };
    pl_set_api(ptApiRegistry, plResourceI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
        gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptResourceManager = ptDataRegistry->get_data("plResourceManager");
    }
    else
    {
        static plResourceManager gtResourceManager = {0};
        gptResourceManager = &gtResourceManager;
        ptDataRegistry->set_data("plResourceManager", gptResourceManager);
    }
}

static void
pl_unload_resource_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plResourceI* ptApi = pl_get_api_latest(ptApiRegistry, plResourceI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_MALLOC(x, user_data) PL_ALLOC(x)
#define STBIR_FREE(x, user_data) PL_FREE(x)
#include "stb_image_resize2.h"
#undef STB_IMAGE_RESIZE_IMPLEMENTATION
