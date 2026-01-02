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
#include "pl_image_ext.h"
#include "pl_vfs_ext.h"
#include "pl_platform_ext.h"

// unstable extensions
#include "pl_dxt_ext.h"
#include "pl_dds_ext.h"

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
    static const plImageI*         gptImage         = NULL;
    static const plVfsI*           gptVfs           = NULL;
    static const plDdsI*           gptDds           = NULL;
    static const plDxtI*           gptDxt           = NULL;
    static const plFileI*          gptFile          = NULL;
#endif

// libs
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plResource        plResource;
typedef struct _plResourceManager plResourceManager;

// new
typedef struct _plResourceStagingBuffer plResourceStagingBuffer;
typedef struct _plTextureUploadJob plTextureUploadJob;

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

typedef struct _plResourceStagingBuffer
{
    plBufferHandle tStagingBufferHandle;
    size_t         szSize;
    size_t         szOffset;
} plResourceStagingBuffer;

typedef struct _plTextureUploadJob
{
    plTextureHandle   tTexture;
    plBufferImageCopy tBufferImageCopy;
} plTextureUploadJob;

typedef struct _plResourceManager
{
    plResourceManagerInit tDesc;

    plResource*     sbtResources;
    uint32_t*       sbtResourceGenerations;
    plHashMap64     tNameHashmap;
    plTempAllocator tTempAllocator;

    // command pools
    plCommandPool* atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];

    // staging (more robust system should replace this)
    plResourceStagingBuffer tStagingBuffer;
    plTextureUploadJob*     sbtTextureUploadJobs;

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

    if(tDesc.pcCacheDirectory == NULL)
        tDesc.pcCacheDirectory = "../cache";

    gptResourceManager->tDesc = tDesc;
    gptFile->create_directory(tDesc.pcCacheDirectory);
    gptVfs->mount_directory("/cache", tDesc.pcCacheDirectory, PL_VFS_MOUNT_FLAGS_NONE);

    plDevice* ptDevice = tDesc.ptDevice;

    // load gpu allocators
    gptResourceManager->ptLocalBuddyAllocator           = gptGpuAllocators->get_local_buddy_allocator(ptDevice);
    gptResourceManager->ptLocalDedicatedAllocator       = gptGpuAllocators->get_local_dedicated_allocator(ptDevice);
    gptResourceManager->ptStagingUnCachedAllocator      = gptGpuAllocators->get_staging_uncached_allocator(ptDevice);
    gptResourceManager->ptStagingUnCachedBuddyAllocator = gptGpuAllocators->get_staging_uncached_buddy_allocator(ptDevice);
    gptResourceManager->ptStagingCachedAllocator        = gptGpuAllocators->get_staging_cached_allocator(ptDevice);

    // create per frame resources
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptResourceManager->atCmdPools[i] = gptGfx->create_command_pool(ptDevice, NULL);

    // pushing invalid data so "default-to-zero" should work
    // for resource handles
    pl_sb_push(gptResourceManager->sbtResourceGenerations, 4194303);
    pl_sb_add(gptResourceManager->sbtResources);
}

void
pl_resource_cleanup(void)
{
    pl_sb_free(gptResourceManager->sbtResourceGenerations);
    pl_sb_free(gptResourceManager->sbtResources);
    pl_sb_free(gptResourceManager->sbtTextureUploadJobs);
    pl_hm_free(&gptResourceManager->tNameHashmap);
    pl_temp_allocator_free(&gptResourceManager->tTempAllocator);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->cleanup_command_pool(gptResourceManager->atCmdPools[i]);
}

void
pl__resource_create_staging_buffer(size_t szSize)
{
    plDevice* ptDevice = gptResourceManager->tDesc.ptDevice;

    plDeviceMemoryAllocatorI* ptAllocator = gptResourceManager->ptStagingUnCachedAllocator;

    // create staging buffers
    const plBufferDesc tStagingBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize = pl_max(268435456, szSize),
        .pcDebugName = "Resource Staging Buffer"
    };

    plBuffer* ptBuffer = NULL;
    gptResourceManager->tStagingBuffer.tStagingBufferHandle = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptBuffer);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        "resource manager staging buffer");

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle, &tAllocation);
    gptResourceManager->tStagingBuffer.szOffset = 0;
    gptResourceManager->tStagingBuffer.szSize = tStagingBufferDesc.szByteSize;
}

void
pl_resource_new_frame(void)
{
    const uint32_t uJobCount = pl_sb_size(gptResourceManager->sbtTextureUploadJobs);
    plDevice* ptDevice = gptResourceManager->tDesc.ptDevice;
    
    // queue staging buffer for deletion if no longer needed
    if(uJobCount == 0)
    {
        if(gptGfx->is_buffer_valid(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle))
        {
            gptGfx->queue_buffer_for_deletion(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle);
            gptResourceManager->tStagingBuffer.szSize = 0;
            gptResourceManager->tStagingBuffer.szOffset = 0;
        }
        return;
    }

    plCommandPool* ptCmdPool = gptResourceManager->atCmdPools[gptGfx->get_current_frame_index()];

    // update texture data & calculate mips
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "resource update");
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder,
        PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ,
        PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_TRANSFER_WRITE);

    for(uint32_t i = 0; i < uJobCount; i++)
    {
        const plTextureUploadJob* ptJob = &gptResourceManager->sbtTextureUploadJobs[i];
        
        gptGfx->set_texture_usage(ptBlitEncoder, ptJob->tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);
        gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptResourceManager->tStagingBuffer.tStagingBufferHandle, ptJob->tTexture, 1, &ptJob->tBufferImageCopy);
        gptGfx->generate_mipmaps(ptBlitEncoder, ptJob->tTexture);
    }

    gptGfx->pipeline_barrier_blit(ptBlitEncoder,
        PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_TRANSFER_WRITE,
        PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    pl_sb_reset(gptResourceManager->sbtTextureUploadJobs);
    gptResourceManager->tStagingBuffer.szOffset = 0;
}

plResourceHandle
pl_resource_load_ex(const char* pcName, plResourceLoadFlags tFlags, uint8_t* puOriginalFileData, size_t szFileByteSize, const char* pcContainerFileName, size_t szFileBytesOffset)
{
    // check if resource already exists
    const uint64_t ulHash = pl_hm_hash_str(pcName, 0);
    uint64_t ulExistingSlot = 0;
    if(pl_hm_has_key_ex(&gptResourceManager->tNameHashmap, ulHash, &ulExistingSlot))
    {
        plResourceHandle tResource = {0};
        tResource.uIndex      = (uint32_t)ulExistingSlot;
        tResource.uGeneration = gptResourceManager->sbtResourceGenerations[ulExistingSlot];
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

    // load file data if not manually loaded
    if(puFileData == NULL)
    {
        szFileByteSize = gptVfs->get_file_size_str(pcContainerFileName); //-V763
        puFileData = PL_ALLOC(szFileByteSize);
        memset(puFileData, 0, szFileByteSize);
        plVfsFileHandle tHandle = gptVfs->open_file(pcContainerFileName, PL_VFS_FILE_MODE_READ);
        gptVfs->read_file(tHandle, puFileData, &szFileByteSize);
        gptVfs->close_file(tHandle);
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

    // find our resource a home slot
    uint64_t uIndex = pl_hm_get_free_index(&gptResourceManager->tNameHashmap);
    if(uIndex == PL_DS_HASH_INVALID)
    {
        uIndex = pl_sb_size(gptResourceManager->sbtResourceGenerations);
        pl_sb_push(gptResourceManager->sbtResourceGenerations, 0);
        pl_sb_add(gptResourceManager->sbtResources);
    }
    pl_hm_insert_str(&gptResourceManager->tNameHashmap, pcName, uIndex);

    // perform any post processing our resource may need
    switch(tDataType) //-V785
    {
        case PL_RESOURCE_DATA_TYPE_IMAGE:
        {

            char* sbtNameConcat = NULL;
            char acFileNameOnly[256] = {0};
            pl_str_get_file_name_only(pcName, acFileNameOnly, 256);
            pl_sb_sprintf(sbtNameConcat, "/cache/%s.dds", acFileNameOnly);

            plDevice* ptDevice = gptResourceManager->tDesc.ptDevice;
            
            bool bCalculateMipsOnGpu = false;
            size_t szStagingOffset = 0;
            plTexture* ptTexture = NULL;
            plImageInfo tImageInfo = {0};
            plFormat tTextureFormat = PL_FORMAT_UNKNOWN;
            int iTextureFormatStride = 0;
            uint32_t uMips = 0;
            uint8_t* puFinalBytes = NULL;

            // prep texture for GPU if not done already
            if(!gptVfs->does_file_exist(sbtNameConcat))
            {

                bool bResizeNeeded = false;
                

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

                    uMips = gptGfx->calculate_mip_count((uint32_t)tImageInfo.iWidth, (uint32_t)tImageInfo.iHeight);


                    int iTextureWidth = 0;
                    int iTextureHeight = 0;
                    int iTextureChannels = 0;

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

                        puFinalBytes = (uint8_t*)pfRawBytes;
                        bCalculateMipsOnGpu = true;
                        tTextureFormat = PL_FORMAT_R32G32B32A32_FLOAT;
                        iTextureFormatStride = sizeof(float);
                    }
                    else if(tImageInfo.b16Bit)
                    {
                        unsigned short* puRawBytes = gptImage->load_16bit((unsigned char*)puFileData, (int)szFileByteSize, &iTextureWidth, &iTextureHeight, &iTextureChannels, 4);
                        puFinalBytes = (uint8_t*)puRawBytes;
                        bCalculateMipsOnGpu = true;
                        tTextureFormat = PL_FORMAT_R16G16B16A16_UNORM;
                        iTextureFormatStride = sizeof(short);
                    }
                    else if(tFlags & PL_RESOURCE_LOAD_FLAG_BLOCK_COMPRESSED)
                    {
                        unsigned char* puRawBytes = gptImage->load((unsigned char*)puFileData, (int)szFileByteSize, &iTextureWidth, &iTextureHeight, &iTextureChannels, 4);

                        if(bResizeNeeded)
                        {
                            unsigned char* puOldRawBytes = puRawBytes;
                            puRawBytes = stbir_resize_uint8_linear(puRawBytes, iTextureWidth, iTextureHeight, 0, NULL, tImageInfo.iWidth, tImageInfo.iHeight, 0, STBIR_RGBA);
                            PL_ASSERT(puRawBytes);
                            gptImage->free(puOldRawBytes);
                        }

                        size_t szRequiredStagingSize = 0;

                        plDxtInfo tDxtInfoOriginal = {
                            .tFlags    = PL_DXT_FLAGS_HIGH_QUALITY,
                            .uWidth    = (uint32_t)tImageInfo.iWidth,
                            .uHeight   = (uint32_t)tImageInfo.iHeight,
                            .uChannels = 4,
                            .puData    = puRawBytes
                        };
                        
                        gptDxt->compress(&tDxtInfoOriginal, NULL, &szRequiredStagingSize);

                        for(uint32_t uMipLevel = 1; uMipLevel < uMips; uMipLevel++)
                        {

                            int iCurrentWidth = (int)tImageInfo.iWidth / ((1 << (int)uMipLevel));
                            int iCurrentHeight = (int)tImageInfo.iHeight / ((1 << (int)uMipLevel));

                            plDxtInfo tDxtInfo = {
                                .tFlags    = PL_DXT_FLAGS_HIGH_QUALITY,
                                .uWidth    = (uint32_t)iCurrentWidth,
                                .uHeight   = (uint32_t)iCurrentHeight,
                                .uChannels = 4
                            };

                            size_t szMipCompressedSize = 0;
                            gptDxt->compress(&tDxtInfo, NULL, &szMipCompressedSize);
                            szRequiredStagingSize += szMipCompressedSize;
                        }

                        uint8_t* puStagingBuffer = PL_ALLOC(szRequiredStagingSize);
                        memset(puStagingBuffer, 0, szRequiredStagingSize);

                        size_t szMipCompressedSize = 0;
                        size_t szOriginalStagingOffset = szStagingOffset;
                        gptDxt->compress(&tDxtInfoOriginal, puStagingBuffer, &szMipCompressedSize);
                        szStagingOffset += szMipCompressedSize;

                        size_t szCurrentSize = szRequiredStagingSize;
                        size_t szMaxBufferSize = tImageInfo.iWidth * tImageInfo.iHeight * 4;
                        uint8_t* auWorkingBuffer[2] = {0};
                        auWorkingBuffer[0] = PL_ALLOC(szMaxBufferSize);
                        auWorkingBuffer[1] = puRawBytes;
                        memset(auWorkingBuffer[0], 0, szMaxBufferSize);

                        for(uint32_t uMipLevel = 1; uMipLevel < uMips; uMipLevel++)
                        {

                            uint8_t* puSrcBuffer = auWorkingBuffer[uMipLevel % 2];
                            uint8_t* puDstBuffer = auWorkingBuffer[(uMipLevel + 1) % 2];

                            int iCurrentWidth = (int)tImageInfo.iWidth / ((1 << (int)uMipLevel));
                            int iCurrentHeight = (int)tImageInfo.iHeight / ((1 << (int)uMipLevel));

                            int iLastWidth = iCurrentWidth * 2;
                            int iLastHeight = iCurrentHeight * 2;

                            szCurrentSize = 0;

                            // manual mip mapping
                            for(uint32_t i = 0; i < (uint32_t)iCurrentWidth; i++)
                            {
                                for(uint32_t j = 0; j < (uint32_t)iCurrentHeight; j++)
                                {
                                    uint32_t uSrcOriginX = i * 2;
                                    uint32_t uSrcOriginY = j * 2;

                                    uint8_t* ptPixel0 = &puSrcBuffer[uSrcOriginY * iLastWidth * 4 + uSrcOriginX * 4];
                                    uint8_t* ptPixel1 = &puSrcBuffer[uSrcOriginY * iLastWidth * 4 + (uSrcOriginX + 1) * 4];
                                    uint8_t* ptPixel2 = &puSrcBuffer[(uSrcOriginY + 1) * iLastWidth * 4 + uSrcOriginX * 4];
                                    uint8_t* ptPixel3 = &puSrcBuffer[(uSrcOriginY + 1) * iLastWidth * 4 + (uSrcOriginX + 1) * 4];

                                    uint8_t uRed   = (ptPixel0[0] + ptPixel1[0] + ptPixel2[0] + ptPixel3[0]) / 4;
                                    uint8_t uGreen = (ptPixel0[1] + ptPixel1[1] + ptPixel2[1] + ptPixel3[1]) / 4;
                                    uint8_t uBlue  = (ptPixel0[2] + ptPixel1[2] + ptPixel2[2] + ptPixel3[2]) / 4;
                                    uint8_t uAlpha = (ptPixel0[3] + ptPixel1[3] + ptPixel2[3] + ptPixel3[3]) / 4;

                                    puDstBuffer[j * iCurrentWidth * 4 + i * 4 + 0] = uRed;
                                    puDstBuffer[j * iCurrentWidth * 4 + i * 4 + 1] = uGreen;
                                    puDstBuffer[j * iCurrentWidth * 4 + i * 4 + 2] = uBlue;
                                    puDstBuffer[j * iCurrentWidth * 4 + i * 4 + 3] = uAlpha;
                                }
                            }

                            // compression

                            plDxtInfo tDxtInfo = {
                                .tFlags    = PL_DXT_FLAGS_HIGH_QUALITY,
                                .uWidth    = (uint32_t)iCurrentWidth,
                                .uHeight   = (uint32_t)iCurrentHeight,
                                .uChannels = 4,
                                .puData    = puDstBuffer
                            };
                            szMipCompressedSize = 0;
                            gptDxt->compress(&tDxtInfo, NULL, &szMipCompressedSize);
                            gptDxt->compress(&tDxtInfo, &puStagingBuffer[szStagingOffset], &szMipCompressedSize);
                            szStagingOffset += szMipCompressedSize;
                        }

                        gptImage->free(puRawBytes);
                        PL_FREE(auWorkingBuffer[0]);

                        plDdsWriteInfo tDDSWriteInfo = {
                            .uWidth    = (uint32_t)tImageInfo.iWidth,
                            .uHeight   = (uint32_t)tImageInfo.iHeight,
                            .uDepth    = 0,
                            .uMips     = uMips,
                            .uLayers   = 1,
                            .tFormat   = PL_FORMAT_BC3_UNORM,
                            .tType     = PL_TEXTURE_TYPE_2D
                        };

                        uint8_t* pcDdsFileBuffer = PL_ALLOC(gptDds->get_header_size() + szRequiredStagingSize);
                        gptDds->write_info(pcDdsFileBuffer, &tDDSWriteInfo);

                        memcpy(&pcDdsFileBuffer[gptDds->get_header_size()], puStagingBuffer, szRequiredStagingSize);
                        PL_FREE(puStagingBuffer);
                        plVfsFileHandle tHandle = gptVfs->open_file(sbtNameConcat, PL_VFS_FILE_MODE_WRITE);
                        gptVfs->write_file(tHandle, pcDdsFileBuffer, gptDds->get_header_size() + szRequiredStagingSize);
                        gptVfs->close_file(tHandle);
                        PL_FREE(pcDdsFileBuffer);
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

                        puFinalBytes = (uint8_t*)puRawBytes;
                        bCalculateMipsOnGpu = true;
                        tTextureFormat = PL_FORMAT_R8G8B8A8_UNORM;
                        iTextureFormatStride = 1;

                    }
                }
            }

            if(bCalculateMipsOnGpu)
            {
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

                size_t szRequiredStagingSize = 0;
                for(uint32_t uMipLevel = 0; uMipLevel < ptTexture->tDesc.uMips; uMipLevel++)
                {
                    int iCurrentWidth = (int)tImageInfo.iWidth / ((1 << (int)uMipLevel));
                    int iCurrentHeight = (int)tImageInfo.iHeight / ((1 << (int)uMipLevel));
                    szRequiredStagingSize += iCurrentWidth * iCurrentHeight * 4 * iTextureFormatStride;
                }

                if(!gptGfx->is_buffer_valid(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle))
                    pl__resource_create_staging_buffer(szRequiredStagingSize);
                else if(gptResourceManager->tStagingBuffer.szOffset + szRequiredStagingSize >= gptResourceManager->tStagingBuffer.szSize)
                {
                    pl_resource_new_frame();
                    pl_resource_new_frame(); // this one destroys the staging buffer
                    pl__resource_create_staging_buffer(szRequiredStagingSize);
                }

                plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle);
                memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[gptResourceManager->tStagingBuffer.szOffset], puFinalBytes, tImageInfo.iWidth * tImageInfo.iHeight * 4 * iTextureFormatStride);
                szStagingOffset = gptResourceManager->tStagingBuffer.szOffset;
                gptResourceManager->tStagingBuffer.szOffset += szRequiredStagingSize;

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

                plCommandPool* ptCmdPool = gptResourceManager->atCmdPools[gptGfx->get_current_frame_index()];

                // update texture data & calculate mips
                plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "resource update");
                gptGfx->begin_command_recording(ptCommandBuffer, NULL);
                plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
                gptGfx->pipeline_barrier_blit(ptBlitEncoder,
                    PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ,
                    PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_TRANSFER_WRITE);

                gptGfx->set_texture_usage(ptBlitEncoder, tResource.tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);

                plBufferImageCopy tBufferImageCopy = {
                    .uImageWidth = (uint32_t)ptTexture->tDesc.tDimensions.x,
                    .uImageHeight = (uint32_t)ptTexture->tDesc.tDimensions.y,
                    .uImageDepth = 1,
                    .uLayerCount = 1,
                    .szBufferOffset = szStagingOffset
                };

                gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptResourceManager->tStagingBuffer.tStagingBufferHandle, tResource.tTexture, 1, &tBufferImageCopy);
                gptGfx->generate_mipmaps(ptBlitEncoder, tResource.tTexture);

                gptGfx->pipeline_barrier_blit(ptBlitEncoder,
                    PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_TRANSFER_WRITE,
                    PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
                gptGfx->end_blit_pass(ptBlitEncoder);
                gptGfx->end_command_recording(ptCommandBuffer);
                gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
                gptGfx->wait_on_command_buffer(ptCommandBuffer);

                // copy mips back
                gptGfx->reset_command_buffer(ptCommandBuffer);
                gptGfx->begin_command_recording(ptCommandBuffer, NULL);
                ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
                gptGfx->pipeline_barrier_blit(ptBlitEncoder,
                    PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ,
                    PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_TRANSFER_WRITE);

                // gptGfx->set_texture_usage(ptBlitEncoder, tResource.tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);

                size_t szMipStagingOffset = szStagingOffset + tImageInfo.iWidth * tImageInfo.iHeight * 4 * iTextureFormatStride;
                for(uint32_t uMipLevel = 1; uMipLevel < uMips; uMipLevel++)
                {

                    int iCurrentWidth = (int)tImageInfo.iWidth / ((1 << (int)uMipLevel));
                    int iCurrentHeight = (int)tImageInfo.iHeight / ((1 << (int)uMipLevel));

                    plBufferImageCopy tImageBufferCopy = {
                        .uMipLevel      = uMipLevel,
                        .szBufferOffset = szMipStagingOffset,
                        .uImageWidth    = iCurrentWidth,
                        .uImageHeight   = iCurrentHeight,
                        .uImageDepth    = 1,
                        .uLayerCount    = 1,
                    };
                    gptGfx->copy_texture_to_buffer(ptBlitEncoder, tResource.tTexture, gptResourceManager->tStagingBuffer.tStagingBufferHandle, 1, &tImageBufferCopy);
                    szMipStagingOffset += iCurrentWidth * iCurrentHeight * 4 * iTextureFormatStride;
                }

                gptGfx->pipeline_barrier_blit(ptBlitEncoder,
                    PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_TRANSFER_WRITE,
                    PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
                    PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
                gptGfx->end_blit_pass(ptBlitEncoder);
                gptGfx->end_command_recording(ptCommandBuffer);
                gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
                gptGfx->wait_on_command_buffer(ptCommandBuffer);
                gptGfx->return_command_buffer(ptCommandBuffer);

                plDdsWriteInfo tDDSWriteInfo = {
                    .uWidth    = (uint32_t)tImageInfo.iWidth,
                    .uHeight   = (uint32_t)tImageInfo.iHeight,
                    .uDepth    = 0,
                    .uMips     = uMips,
                    .uLayers   = 1,
                    .tFormat   = tTextureDesc.tFormat,
                    .tType     = PL_TEXTURE_TYPE_2D
                };

                uint8_t* pcDdsFileBuffer = PL_ALLOC(gptDds->get_header_size() + szRequiredStagingSize);
                gptDds->write_info(pcDdsFileBuffer, &tDDSWriteInfo);

                memcpy(&pcDdsFileBuffer[gptDds->get_header_size()], &ptStagingBuffer->tMemoryAllocation.pHostMapped[szStagingOffset], szRequiredStagingSize);
                plVfsFileHandle tHandle = gptVfs->open_file(sbtNameConcat, PL_VFS_FILE_MODE_WRITE);
                gptVfs->write_file(tHandle, pcDdsFileBuffer, gptDds->get_header_size() + szRequiredStagingSize);
                gptVfs->close_file(tHandle);
                PL_FREE(pcDdsFileBuffer);
            }

            if(puFinalBytes)
            {
                gptImage->free(puFinalBytes);
                puFinalBytes = NULL;
            }

            // check if texture data is ready for GPU
            szStagingOffset = 0;
            if(!gptGfx->is_texture_valid(ptDevice, tResource.tTexture))
            {
                PL_ASSERT(gptVfs->does_file_exist(sbtNameConcat));

                size_t szShaderSize = gptVfs->get_file_size_str(sbtNameConcat);
                uint8_t* puDdsFileData = PL_ALLOC(szShaderSize);
                uint8_t* puOriginalDdsFileData = puDdsFileData;
                plVfsFileHandle tHandle = gptVfs->open_file(sbtNameConcat, PL_VFS_FILE_MODE_READ);
                gptVfs->read_file(tHandle, puDdsFileData, &szShaderSize);
                gptVfs->close_file(tHandle);

                plDdsReadInfo tDDSReadInfo = {0};
                gptDds->read_info(puDdsFileData, &tDDSReadInfo);

                // create texture
                const plTextureDesc tTextureDesc = {
                    .tDimensions = {(float)tDDSReadInfo.uWidth, (float)tDDSReadInfo.uHeight, 1},
                    .tFormat     = tDDSReadInfo.tFormat,
                    .uLayers     = 1,
                    .uMips       = tDDSReadInfo.uMips,
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

                size_t szRequiredStagingSize = 0;
                for(uint32_t i = 0; i < tDDSReadInfo.uMips; i++)
                    szRequiredStagingSize += tDDSReadInfo.atMipInfo[i].uSize;

                if(!gptGfx->is_buffer_valid(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle))
                    pl__resource_create_staging_buffer(szRequiredStagingSize);
                else if(gptResourceManager->tStagingBuffer.szOffset + szRequiredStagingSize >= gptResourceManager->tStagingBuffer.szSize)
                {
                    pl_resource_new_frame();
                    pl_resource_new_frame(); // this one destroys the staging buffer
                    pl__resource_create_staging_buffer(szRequiredStagingSize);
                }

                plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptResourceManager->tStagingBuffer.tStagingBufferHandle);

                plCommandPool* ptCmdPool = gptResourceManager->atCmdPools[gptGfx->get_current_frame_index()];
                plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "resource update");
                gptGfx->begin_command_recording(ptCommandBuffer, NULL);
                plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
                gptGfx->set_texture_usage(ptBlitEncoder, tResource.tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);

                puDdsFileData += gptDds->get_header_size();
                for(uint32_t i = 0; i < tDDSReadInfo.uMips; i++)
                {
                    memcpy((uint8_t*)&ptStagingBuffer->tMemoryAllocation.pHostMapped[szStagingOffset], puDdsFileData, tDDSReadInfo.atMipInfo[i].uSize);

                    int iCurrentWidth = (int)tDDSReadInfo.atMipInfo[i].uWidth;
                    int iCurrentHeight = (int)tDDSReadInfo.atMipInfo[i].uHeight;

                    const plBufferImageCopy tBufferImageCopy0 = {
                        .uImageWidth    = (uint32_t)iCurrentWidth,
                        .uImageHeight   = (uint32_t)iCurrentHeight,
                        .uImageDepth    = 1,
                        .uLayerCount    = 1,
                        .szBufferOffset = szStagingOffset,
                        .uMipLevel      = i
                    };

                    gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptResourceManager->tStagingBuffer.tStagingBufferHandle, tResource.tTexture, 1, &tBufferImageCopy0);

                    szStagingOffset += tDDSReadInfo.atMipInfo[i].uSize;
                    puDdsFileData += tDDSReadInfo.atMipInfo[i].uSize;
                }

                gptGfx->end_blit_pass(ptBlitEncoder);
                gptGfx->end_command_recording(ptCommandBuffer);
                gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
                gptGfx->wait_on_command_buffer(ptCommandBuffer);
                gptGfx->return_command_buffer(ptCommandBuffer);

                PL_FREE(puOriginalDdsFileData);
            }
            else
            {

            }

            pl_sb_free(sbtNameConcat);
            break;
        }
    }
    
    plResourceHandle tNewResource = {0};
    tNewResource.uIndex      = (uint32_t)uIndex;
    tNewResource.uGeneration = gptResourceManager->sbtResourceGenerations[uIndex];

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
    return pl_hm_has_key_str(&gptResourceManager->tNameHashmap, pcName);
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

        pl_hm_remove_str(&gptResourceManager->tNameHashmap, ptResource->acName);
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
        .new_frame     = pl_resource_new_frame,
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
        gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
        gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptDds           = pl_get_api_latest(ptApiRegistry, plDdsI);
        gptDxt           = pl_get_api_latest(ptApiRegistry, plDxtI);
        gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
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
