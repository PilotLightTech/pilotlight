/*
   pl_renderer_internal.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] job system tasks
// [SECTION] resource creation helpers
// [SECTION] culling
// [SECTION] scene render helpers
// [SECTION] shader variant system
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] job system tasks
//-----------------------------------------------------------------------------

static void
pl__refr_job(plInvocationData tInvoData, void* pData)
{
    plMaterialComponent* sbtMaterials = pData;

    plMaterialComponent* ptMaterial = &sbtMaterials[tInvoData.uGlobalIndex];
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;

    for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
    {

        if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[i].tResource))
        {
            if(i == PL_TEXTURE_SLOT_BASE_COLOR_MAP || i == PL_TEXTURE_SLOT_EMISSIVE_MAP)
            {
                size_t szResourceSize = 0;
                const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[i].tResource, &szResourceSize);
                float* rawBytes = gptImage->load_hdr((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
                gptResource->set_buffer_data(ptMaterial->atTextureMaps[i].tResource, sizeof(float) * texWidth * texHeight * 4, rawBytes);
                ptMaterial->atTextureMaps[i].uWidth = texWidth;
                ptMaterial->atTextureMaps[i].uHeight = texHeight;
            }
            else
            {
                size_t szResourceSize = 0;
                const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[i].tResource, &szResourceSize);
                unsigned char* rawBytes = gptImage->load((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
                PL_ASSERT(rawBytes);
                ptMaterial->atTextureMaps[i].uWidth = texWidth;
                ptMaterial->atTextureMaps[i].uHeight = texHeight;
                gptResource->set_buffer_data(ptMaterial->atTextureMaps[i].tResource, texWidth * texHeight * 4, rawBytes);
            }
        }
    }
}

static void
pl__refr_cull_job(plInvocationData tInvoData, void* pData)
{
    plCullData* ptCullData = pData;
    plRefScene* ptScene = ptCullData->ptScene;
    plDrawable tDrawable = ptCullData->atDrawables[tInvoData.uGlobalIndex];
    plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tDrawable.tEntity);
    ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = true;
    if(pl__sat_visibility_test(ptCullData->ptCullCamera, &ptMesh->tAABBFinal))
    {
        ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = false;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] resource creation helpers
//-----------------------------------------------------------------------------

static plTextureHandle
pl__create_texture_helper(plMaterialComponent* ptMaterial, plTextureSlot tSlot, bool bHdr, int iMips)
{
    plDevice* ptDevice = gptData->ptDevice;

    if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[tSlot].tResource) == false)
        return gptData->tDummyTexture;
    
    // check if it exists already

    if(pl_hm_has_key(gptData->ptTextureHashmap, ptMaterial->atTextureMaps[tSlot].tResource.ulData))
    {
        uint64_t ulValue = pl_hm_lookup(gptData->ptTextureHashmap, ptMaterial->atTextureMaps[tSlot].tResource.ulData);
        return gptData->sbtTextureHandles[ulValue];
    }

    size_t szResourceSize = 0;

    plTextureHandle tTexture = {0};

    if(bHdr)
    {

        const float* rawBytes = gptResource->get_buffer_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)ptMaterial->atTextureMaps[tSlot].uWidth, (float)ptMaterial->atTextureMaps[tSlot].uHeight, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 1,
            .uMips       = iMips,
            .tType       = PL_TEXTURE_TYPE_2D,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = pl__refr_create_texture_with_data(&tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName, 0, rawBytes, szResourceSize);
    }
    else
    {
        const unsigned char* rawBytes = gptResource->get_buffer_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)ptMaterial->atTextureMaps[tSlot].uWidth, (float)ptMaterial->atTextureMaps[tSlot].uHeight, 1},
            .tFormat = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = pl__refr_create_texture_with_data(&tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName, 0, rawBytes, szResourceSize);
    }

    uint64_t ulValue = pl_hm_get_free_index(gptData->ptTextureHashmap);
    if(ulValue == UINT64_MAX)
    {
        ulValue = pl_sb_size(gptData->sbtTextureHandles);
        pl_sb_add(gptData->sbtTextureHandles);
    }
    gptData->sbtTextureHandles[ulValue] = tTexture;
    pl_hm_insert(gptData->ptTextureHashmap, ptMaterial->atTextureMaps[tSlot].tResource.ulData, ulValue);

    return tTexture;
}

static plTextureHandle
pl__refr_create_local_texture(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "texture alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
    return tHandle;
}

static plTextureHandle
pl__refr_create_texture(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "texture alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
    return tHandle;
}

static plTextureHandle
pl__refr_create_texture_with_data(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "texture alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, PL_TEXTURE_USAGE_SAMPLED, 0);


    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, szSize);


        const plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptDesc->tDimensions.x,
            .uImageHeight = (uint32_t)ptDesc->tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptData->tStagingBufferHandle[0], tHandle, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(ptBlitEncoder, tHandle);
    }

    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    return tHandle;
}

static plBufferHandle
pl__refr_create_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "sbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static plBufferHandle 
pl__refr_create_cached_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);
    pl_temp_allocator_reset(&tTempAllocator);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptData->ptStagingCachedAllocator->allocate(gptData->ptStagingCachedAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "scbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static plBufferHandle
pl__refr_create_local_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    
    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "lbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    // if data is presented, upload using staging buffer
    if(pData)
    {
        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, ptDesc->szByteSize);

        // begin recording
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        
        // begin blit pass, copy buffer, end pass
        plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        gptGfx->copy_buffer(ptEncoder, gptData->tStagingBufferHandle[0], tHandle, 0, 0, ptDesc->szByteSize);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptEncoder);

        // finish recording
        gptGfx->end_command_recording(ptCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }
    return tHandle;
}

//-----------------------------------------------------------------------------
// [SECTION] culling
//-----------------------------------------------------------------------------

static bool
pl__sat_visibility_test(plCameraComponent* ptCamera, const plAABB* ptAABB)
{
    const float fTanFov = tanf(0.5f * ptCamera->fFieldOfView);

    const float fZNear = ptCamera->fNearZ;
    const float fZFar = ptCamera->fFarZ;

    // half width, half height
    const float fXNear = ptCamera->fAspectRatio * ptCamera->fNearZ * fTanFov;
    const float fYNear = ptCamera->fNearZ * fTanFov;

    // consider four adjacent corners of the AABB
    plVec3 atCorners[] = {
        {ptAABB->tMin.x, ptAABB->tMin.y, ptAABB->tMin.z},
        {ptAABB->tMax.x, ptAABB->tMin.y, ptAABB->tMin.z},
        {ptAABB->tMin.x, ptAABB->tMax.y, ptAABB->tMin.z},
        {ptAABB->tMin.x, ptAABB->tMin.y, ptAABB->tMax.z},
    };

    // transform corners
    for (size_t i = 0; i < 4; i++)
        atCorners[i] = pl_mul_mat4_vec3(&ptCamera->tViewMat, atCorners[i]);

    // Use transformed atCorners to calculate center, axes and extents

    typedef struct _plOBB
    {
        plVec3 tCenter;
        plVec3 tExtents;
        plVec3 atAxes[3]; // Orthonormal basis
    } plOBB;

    plOBB tObb = {
        .atAxes = {
            pl_sub_vec3(atCorners[1], atCorners[0]),
            pl_sub_vec3(atCorners[2], atCorners[0]),
            pl_sub_vec3(atCorners[3], atCorners[0])
        },
    };

    tObb.tCenter = pl_add_vec3(atCorners[0], pl_mul_vec3_scalarf((pl_add_vec3(tObb.atAxes[0], pl_add_vec3(tObb.atAxes[1], tObb.atAxes[2]))), 0.5f));
    tObb.tExtents = (plVec3){ pl_length_vec3(tObb.atAxes[0]), pl_length_vec3(tObb.atAxes[1]), pl_length_vec3(tObb.atAxes[2]) };

    // normalize
    tObb.atAxes[0] = pl_div_vec3_scalarf(tObb.atAxes[0], tObb.tExtents.x);
    tObb.atAxes[1] = pl_div_vec3_scalarf(tObb.atAxes[1], tObb.tExtents.y);
    tObb.atAxes[2] = pl_div_vec3_scalarf(tObb.atAxes[2], tObb.tExtents.z);
    tObb.tExtents = pl_mul_vec3_scalarf(tObb.tExtents, 0.5f);

    // axis along frustum
    {
        // Projected center of our OBB
        const float fMoC = tObb.tCenter.z;

        // Projected size of OBB
        float fRadius = 0.0f;
        for (size_t i = 0; i < 3; i++)
            fRadius += fabsf(tObb.atAxes[i].z) * tObb.tExtents.d[i];

        const float fObbMin = fMoC - fRadius;
        const float fObbMax = fMoC + fRadius;

        if (fObbMin > fZFar || fObbMax < fZNear)
            return false;
    }


    // other normals of frustum
    {
        const plVec3 atM[] = {
            { fZNear, 0.0f, fXNear }, // Left Plane
            { -fZNear, 0.0f, fXNear }, // Right plane
            { 0.0, -fZNear, fYNear }, // Top plane
            { 0.0, fZNear, fYNear }, // Bottom plane
        };
        for (size_t m = 0; m < 4; m++)
        {
            const float fMoX = fabsf(atM[m].x);
            const float fMoY = fabsf(atM[m].y);
            const float fMoZ = atM[m].z;
            const float fMoC = pl_dot_vec3(atM[m], tObb.tCenter);

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(atM[m], tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            const float fP = fXNear * fMoX + fYNear * fMoY;

            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // OBB axes
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3* ptM = &tObb.atAxes[m];
            const float fMoX = fabsf(ptM->x);
            const float fMoY = fabsf(ptM->y);
            const float fMoZ = ptM->z;
            const float fMoC = pl_dot_vec3(*ptM, tObb.tCenter);

            const float fObbRadius = tObb.tExtents.d[m];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // cross products between the edges
    // first R x A_i
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3 tM = { 0.0f, -tObb.atAxes[m].z, tObb.atAxes[m].y };
            const float fMoX = 0.0f;
            const float fMoY = fabsf(tM.y);
            const float fMoZ = tM.z;
            const float fMoC = tM.y * tObb.tCenter.y + tM.z * tObb.tCenter.z;

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(tM, tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // U x A_i
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3 tM = { tObb.atAxes[m].z, 0.0f, -tObb.atAxes[m].x };
            const float fMoX = fabsf(tM.x);
            const float fMoY = 0.0f;
            const float fMoZ = tM.z;
            const float fMoC = tM.x * tObb.tCenter.x + tM.z * tObb.tCenter.z;

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(tM, tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // frustum Edges X Ai
    {
        for (size_t obb_edge_idx = 0; obb_edge_idx < 3; obb_edge_idx++)
        {
            const plVec3 atM[] = {
                pl_cross_vec3((plVec3){-fXNear, 0.0f, fZNear}, tObb.atAxes[obb_edge_idx]), // Left Plane
                pl_cross_vec3((plVec3){ fXNear, 0.0f, fZNear }, tObb.atAxes[obb_edge_idx]), // Right plane
                pl_cross_vec3((plVec3){ 0.0f, fYNear, fZNear }, tObb.atAxes[obb_edge_idx]), // Top plane
                pl_cross_vec3((plVec3){ 0.0, -fYNear, fZNear }, tObb.atAxes[obb_edge_idx]) // Bottom plane
            };

            for (size_t m = 0; m < 4; m++)
            {
                const float fMoX = fabsf(atM[m].x);
                const float fMoY = fabsf(atM[m].y);
                const float fMoZ = atM[m].z;

                const float fEpsilon = 1e-4f;
                if (fMoX < fEpsilon && fMoY < fEpsilon && fabsf(fMoZ) < fEpsilon) continue;

                const float fMoC = pl_dot_vec3(atM[m], tObb.tCenter);

                float fObbRadius = 0.0f;
                for (size_t i = 0; i < 3; i++)
                    fObbRadius += fabsf(pl_dot_vec3(atM[m], tObb.atAxes[i])) * tObb.tExtents.d[i];

                const float fObbMin = fMoC - fObbRadius;
                const float fObbMax = fMoC + fObbRadius;

                // frustum projection
                const float fP = fXNear * fMoX + fYNear * fMoY;
                float fTau0 = fZNear * fMoZ - fP;
                float fTau1 = fZNear * fMoZ + fP;

                if (fTau0 < 0.0f)
                    fTau0 *= fZFar / fZNear;

                if (fTau1 > 0.0f)
                    fTau1 *= fZFar / fZNear;

                if (fObbMin > fTau1 || fObbMax < fTau0)
                    return false;
            }
        }
    }

    // no intersections detected
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] scene render helpers
//-----------------------------------------------------------------------------

static void
pl_refr_update_skin_textures(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    // update skin textures
    if(gptData->uCurrentStagingFrameIndex != uFrameIdx)
    {
        gptData->uStagingOffset = 0;
        gptData->uCurrentStagingFrameIndex = uFrameIdx;
    }
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBindGroupLayout tBindGroupLayout1 = {
            .atTextureBindings = {
                {.uSlot =  0, .tStages = PL_STAGE_ALL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
            }
        };
        const plBindGroupDesc tBindGroup1Desc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tBindGroupLayout1,
            .pcDebugName = "skin temporary bind group"
        };
        ptScene->sbtSkinData[i].tTempBindGroup = gptGfx->create_bind_group(ptDevice, &tBindGroup1Desc);
        const plBindGroupUpdateTextureData tTextureData = {.tTexture = ptScene->sbtSkinData[i].atDynamicTexture[uFrameIdx], .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
        plBindGroupUpdateData tBGData0 = {
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->sbtSkinData[i].tTempBindGroup, &tBGData0);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptScene->sbtSkinData[i].tTempBindGroup);

        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[uFrameIdx]);

        plTexture* ptSkinTexture = gptGfx->get_texture(ptDevice, ptScene->sbtSkinData[i].atDynamicTexture[uFrameIdx]);
        plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptSkinTexture->tDesc.tDimensions.x,
            .uImageHeight = (uint32_t)ptSkinTexture->tDesc.tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1,
            .szBufferOffset = gptData->uStagingOffset
        };
        gptData->uStagingOffset += sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y;
        
        plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptScene->sbtSkinData[i].tEntity);
        memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tBufferImageCopy.szBufferOffset], ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        // memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptData->tStagingBufferHandle[uFrameIdx], ptScene->sbtSkinData[i].atDynamicTexture[uFrameIdx], 1, &tBufferImageCopy);
    }

    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_perform_skinning(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // update skin textures
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);

    typedef struct _SkinDynamicData
    {
        int iSourceDataOffset;
        int iDestDataOffset;
        int iDestVertexOffset;
        int iUnused;
    } SkinDynamicData;

    if(uSkinCount)
    {

        const plPassBufferResource atPassBuffers[] = {
            { .tHandle = ptScene->tSkinStorageBuffer, .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_READ },
            { .tHandle = ptScene->tVertexBuffer,      .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->tStorageBuffer,     .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 3,
            .atBuffers = atPassBuffers
        };

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
        for(uint32_t i = 0; i < uSkinCount; i++)
        {
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            SkinDynamicData* ptDynamicData = (SkinDynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iSourceDataOffset = ptScene->sbtSkinData[i].iSourceDataOffset;
            ptDynamicData->iDestDataOffset = ptScene->sbtSkinData[i].iDestDataOffset;
            ptDynamicData->iDestVertexOffset = ptScene->sbtSkinData[i].iDestVertexOffset;

            const plDispatch tDispach = {
                .uGroupCountX     = ptScene->sbtSkinData[i].uVertexCount,
                .uGroupCountY     = 1,
                .uGroupCountZ     = 1,
                .uThreadPerGroupX = 1,
                .uThreadPerGroupY = 1,
                .uThreadPerGroupZ = 1
            };
            const plBindGroupHandle atBindGroups[] = {
                ptScene->tSkinBindGroup0,
                ptScene->sbtSkinData[i].tTempBindGroup
            };
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, ptScene->sbtSkinData[i].tShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptComputeEncoder, ptScene->sbtSkinData[i].tShader);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        }
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
    }
    pl_end_cpu_sample(gptProfile, 0);
}

static bool
pl_refr_pack_shadow_atlas(uint32_t uSceneHandle, const uint32_t* auViewHandles, uint32_t uViewCount)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    pl_sb_reset(ptScene->sbtShadowRects);

    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    const uint32_t uLightCount = pl_sb_size(sbtLights);

    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = &sbtLights[uLightIndex];

        // skip light if it doesn't cast shadows
        if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
        {
            continue;
        }

        if(ptLight->tType == PL_LIGHT_TYPE_DIRECTIONAL)
        {
            for(uint32_t i = 0; i < uViewCount; i++)
            {
                plPackRect tPackRect = {
                    .iWidth  = (int)(ptLight->uShadowResolution * ptLight->uCascadeCount),
                    .iHeight = (int)ptLight->uShadowResolution,
                    .iId     = (int)uLightIndex
                };
                pl_sb_push(ptScene->sbtShadowRects, tPackRect);
            }
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {
            plPackRect tPackRect = {
                .iWidth  = (int)(ptLight->uShadowResolution * 2),
                .iHeight = (int)(ptLight->uShadowResolution * 3),
                .iId     = (int)uLightIndex
            };
            pl_sb_push(ptScene->sbtShadowRects, tPackRect);
        }
    }

    uint32_t uDShadowCastingLightIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = &sbtLights[uLightIndex];

        // skip light if it doesn't cast shadows
        if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
        {
            continue;
        }
    }

    const uint32_t uRectCount = pl_sb_size(ptScene->sbtShadowRects);
    gptRect->pack_rects(ptScene->uShadowAtlasResolution, ptScene->uShadowAtlasResolution,
        ptScene->sbtShadowRects, uRectCount);

    bool bPacked = true;
    for(uint32_t i = 0; i < uRectCount; i++)
    {
        if(!ptScene->sbtShadowRects[i].iWasPacked)
        {
            bPacked = false;
            PL_ASSERT(false && "Shadow atlas too small");
            break;
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
    return bPacked;
}

static void
pl_refr_generate_shadow_maps(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plRefScene*   ptScene    = &gptData->sbtScenes[uSceneHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    const uint32_t uLightCount = pl_sb_size(ptScene->sbtShadowRects);

    uint32_t uPCameraBufferOffset = 0;
    uint32_t uCastingLightIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];
        const plLightComponent* ptLight = &sbtLights[ptRect->iId];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {
            pl_sb_add(ptScene->sbtPLightShadowData);

            plGPUPLightShadowData* ptShadowData = &ptScene->sbtPLightShadowData[pl_sb_size(ptScene->sbtPLightShadowData) - 1];
            ptShadowData->iShadowMapTexIdx = ptScene->atShadowTextureBindlessIndices[uFrameIdx];
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 atCamViewProjs[6] = {0};

            plCameraComponent tShadowCamera = {
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPos         = ptLight->tPosition,
                .fNearZ       = ptLight->fRadius,
                .fFarZ        = ptLight->fRange,
                .fFieldOfView = PL_PI_2,
                .fAspectRatio = 1.0f
            };
            gptCamera->update(&tShadowCamera);

            const plVec2 atPitchYaw[6] = {
                {0.0, 0.0},
                {0.0, PL_PI},
                {0.0, PL_PI_2},
                {0.0, -PL_PI_2},
                {PL_PI_2, 0.0},
                {-PL_PI_2, 0.0}
            };

            for(uint32_t i = 0; i < 6; i++)
            {

                gptCamera->set_pitch_yaw(&tShadowCamera, atPitchYaw[i].x, atPitchYaw[i].y);
                gptCamera->update(&tShadowCamera);
                atCamViewProjs[i] = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
                ptShadowData->viewProjMat[i] = atCamViewProjs[i];
            }

            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atPShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[uPCameraBufferOffset], atCamViewProjs, sizeof(plMat4) * 6);
            uPCameraBufferOffset += sizeof(plMat4) * 6;
        }   
    }

    plBindGroupLayout tBindGroupLayout0 = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            }
        }
    };
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tBindGroupLayout0,
        .pcDebugName = "temporary global bind group 0"
    };
    plBindGroupHandle tGlobalBG = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);

    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptScene->atPShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = uPCameraBufferOffset
        }
    };

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 1,
        .atBufferBindings = atBufferData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG, &tBGData0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG);

    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtDeferredDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtForwardDrawables);

    // const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tShadowData.tOpaqueRenderPass)->tDesc.tDimensions;

    typedef struct _plShadowDynamicData
    {
        int    iIndex;
        int    iDataOffset;
        int    iVertexOffset;
        int    iMaterialIndex;
        plMat4 tModel;
    } plShadowDynamicData;

    uint32_t uCameraBufferIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];
        const plLightComponent* ptLight = &sbtLights[ptRect->iId];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {

            const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtDeferredDrawables);
            const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtForwardDrawables);

            if(gptData->bMultiViewportShadows)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);

                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDeferredDrawables[i];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = gptData->tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                        .atBindGroups = {
                            ptScene->tGlobalBindGroup,
                            tGlobalBG
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = 6
                    });
                }

                for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtForwardDrawables[i];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = tDrawable.tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                        .atBindGroups = {
                            ptScene->tGlobalBindGroup,
                            tGlobalBG
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = 6
                    });

                };

                plDrawArea tArea = 
                {
                    .ptDrawStream = ptStream,
                    .atScissors = {
                        {
                            .iOffsetX = (int)(ptRect->iX),
                            .iOffsetY = ptRect->iY,
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        },
                        {
                            .iOffsetX = (int)(ptRect->iX + ptLight->uShadowResolution),
                            .iOffsetY = ptRect->iY,
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        },
                        {
                            .iOffsetX = ptRect->iX,
                            .iOffsetY = (int)(ptRect->iY + ptLight->uShadowResolution),
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        },
                        {
                            .iOffsetX = (int)(ptRect->iX + ptLight->uShadowResolution),
                            .iOffsetY = (int)(ptRect->iY + ptLight->uShadowResolution),
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        },
                        {
                            .iOffsetX = ptRect->iX,
                            .iOffsetY = (int)(ptRect->iY + 2 * ptLight->uShadowResolution),
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        },
                        {
                            .iOffsetX = (int)(ptRect->iX + ptLight->uShadowResolution),
                            .iOffsetY = (int)(ptRect->iY + 2 * ptLight->uShadowResolution),
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        },
                    },
                    .atViewports = {
                        {
                            .fX = (float)(ptRect->iX),
                            .fY = (float)ptRect->iY,
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        },
                        {
                            .fX = (float)(ptRect->iX + ptLight->uShadowResolution),
                            .fY = (float)ptRect->iY,
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        },
                        {
                            .fX = (float)ptRect->iX,
                            .fY = (float)(ptRect->iY + ptLight->uShadowResolution),
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        },
                        {
                            .fX = (float)(ptRect->iX + ptLight->uShadowResolution),
                            .fY = (float)(ptRect->iY + ptLight->uShadowResolution),
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        },
                        {
                            .fX = (float)ptRect->iX,
                            .fY = (float)(ptRect->iY + 2 * ptLight->uShadowResolution),
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        },
                        {
                            .fX = (float)(ptRect->iX + ptLight->uShadowResolution),
                            .fY = (float)(ptRect->iY + 2 * ptLight->uShadowResolution),
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        }
                    }
                };

                gptGfx->draw_stream(ptEncoder, 1, &tArea);
            }
            else
            {
                for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
                {

                    gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                    gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);
                    for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtDeferredDrawables[i];
                        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                        ptDynamicData->tModel = ptTransform->tWorld;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCameraBufferIndex + uFaceIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader         = gptData->tShadowShader,
                            .auDynamicBuffers = {
                                tDynamicBinding.uBufferHandle
                            },
                            .atVertexBuffers = {
                                ptScene->tVertexBuffer,
                            },
                            .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                            .uIndexOffset         = tDrawable.uIndexOffset,
                            .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                            .atBindGroups = {
                                ptScene->tGlobalBindGroup,
                                tGlobalBG
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = 0,
                            .uInstanceCount = 1
                        });
                    }

                    for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtForwardDrawables[i];
                        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                        ptDynamicData->tModel = ptTransform->tWorld;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCameraBufferIndex + uFaceIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader        = tDrawable.tShadowShader,
                            .auDynamicBuffers = {
                                tDynamicBinding.uBufferHandle
                            },
                            .atVertexBuffers = {
                                ptScene->tVertexBuffer,
                            },
                            .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                            .uIndexOffset         = tDrawable.uIndexOffset,
                            .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                            .atBindGroups = {
                                ptScene->tGlobalBindGroup,
                                tGlobalBG
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = 0,
                            .uInstanceCount = 1
                        });

                    };

                    uint32_t uXIndex = uFaceIndex % 2;

                    const uint32_t auYIndices[] = {
                        0, 0,
                        1, 1,
                        2, 2,
                    };

                    plDrawArea tArea = 
                    {
                        .ptDrawStream = ptStream,
                        .atScissors = {
                            {
                                .iOffsetX = (int)(ptRect->iX + uXIndex * ptLight->uShadowResolution),
                                .iOffsetY = (int)(ptRect->iY + auYIndices[uFaceIndex] * ptLight->uShadowResolution),
                                .uWidth  = ptLight->uShadowResolution,
                                .uHeight = ptLight->uShadowResolution,
                            },
                        },
                        .atViewports = {
                            {
                                .fX = (float)(ptRect->iX + uXIndex * ptLight->uShadowResolution),
                                .fY = (float)(ptRect->iY + auYIndices[uFaceIndex] * ptLight->uShadowResolution),
                                .fWidth  = (float)ptLight->uShadowResolution,
                                .fHeight = (float)ptLight->uShadowResolution,
                                .fMaxDepth = 1.0f
                            }
                        }
                    };

                    gptGfx->draw_stream(ptEncoder, 1, &tArea);
                }
            }
            uCameraBufferIndex+=6;
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_generate_cascaded_shadow_map(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, uint32_t uViewCount, plEntity tCamera)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plRefScene*   ptScene    = &gptData->sbtScenes[uSceneHandle];
    plRefView*    ptView     = &ptScene->atViews[uViewHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    plCameraComponent* ptSceneCamera = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, tCamera);

    // common calculations
    const float fFarClip = ptSceneCamera->fFarZ;
    const float fNearClip = ptSceneCamera->fNearZ;
    const float fClipRange = fFarClip - fNearClip;

    const float fMinZ = fNearClip;
    const float fMaxZ = fNearClip + fClipRange;

    const float fRange = fMaxZ - fMinZ;
    const float fRatio = fMaxZ / fMinZ;

    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    const uint32_t uLightCount = pl_sb_size(ptScene->sbtShadowRects);

    // count rects
    // uint32_t uShadowCastingLightIndex = 0;

    int iLastIndex = -1;
    int iCurrentView = -1;
    float fCascadeSplitLambda = gptData->fLambdaSplit;
    uint32_t uDCameraBufferOffset = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];
        const plLightComponent* ptLight = &sbtLights[ptRect->iId];

        if(ptRect->iId == iLastIndex)
        {
            iCurrentView++;
        }
        else
        {
            iLastIndex = ptRect->iId;
            iCurrentView = 0;
        }

        if(ptLight->tType != PL_LIGHT_TYPE_DIRECTIONAL || iCurrentView != (int)uViewHandle)
        {
            continue;
        }

        const uint32_t uDataOffset = pl_sb_size(ptView->sbtDLightShadowData);
        pl_sb_add(ptView->sbtDLightShadowData);

        plGPUDLightShadowData* ptShadowData = &ptView->sbtDLightShadowData[uDataOffset];
        ptShadowData->iShadowMapTexIdx = ptScene->atShadowTextureBindlessIndices[uFrameIdx];
        ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

        plMat4 atCamViewProjs[PL_MAX_SHADOW_CASCADES] = {0};
        float fLastSplitDist = 0.0;
        float afLambdaCascadeSplits[PL_MAX_SHADOW_CASCADES] = {0};
        for(uint32_t uCascade = 0; uCascade < ptLight->uCascadeCount; uCascade++)
        {
            float fSplitDist = 0.0f;
            if(fCascadeSplitLambda > 0.0f)
            {
                const float p = (uCascade + 1) / (float)ptLight->uCascadeCount;
                const float fLog = fMinZ * powf(fRatio, p);
                const float fUniform = fMinZ + fRange * p;
                const float fD = fCascadeSplitLambda * (fLog - fUniform) + fUniform;
                afLambdaCascadeSplits[uCascade] = (fD - fNearClip) / fClipRange;
                fSplitDist = afLambdaCascadeSplits[uCascade];
                ptShadowData->cascadeSplits.d[uCascade] = (fNearClip + fSplitDist * fClipRange);
            }
            else
            {
                fSplitDist = ptLight->afCascadeSplits[uCascade] / fClipRange;
                ptShadowData->cascadeSplits.d[uCascade] = ptLight->afCascadeSplits[uCascade];
            }

            plVec3 atCameraCorners[] = {
                { -1.0f,  1.0f, 1.0f },
                { -1.0f, -1.0f, 1.0f },
                {  1.0f, -1.0f, 1.0f },
                {  1.0f,  1.0f, 1.0f },
                { -1.0f,  1.0f, 0.0f },
                { -1.0f, -1.0f, 0.0f },
                {  1.0f, -1.0f, 0.0f },
                {  1.0f,  1.0f, 0.0f },
            };
            // plMat4 tCameraInversion = pl_mul_mat4(&tCameraProjMat, &ptSceneCamera->tViewMat);
            plMat4 tCameraInversion = pl_mul_mat4(&ptSceneCamera->tProjMat, &ptSceneCamera->tViewMat);
            tCameraInversion = pl_mat4_invert(&tCameraInversion);

            // convert to world space
            for(uint32_t i = 0; i < 8; i++)
            {
                plVec4 tInvCorner = pl_mul_mat4_vec4(&tCameraInversion, (plVec4){.xyz = atCameraCorners[i], .w = 1.0f});
                atCameraCorners[i] = pl_div_vec3_scalarf(tInvCorner.xyz, tInvCorner.w);
            }

            for(uint32_t i = 0; i < 4; i++)
            {
                const plVec3 tDist = pl_sub_vec3(atCameraCorners[i + 4], atCameraCorners[i]);
                atCameraCorners[i + 4] = pl_add_vec3(atCameraCorners[i], pl_mul_vec3_scalarf(tDist, fSplitDist));
                atCameraCorners[i] = pl_add_vec3(atCameraCorners[i], pl_mul_vec3_scalarf(tDist, fLastSplitDist));
            }

            // get frustum center
            plVec3 tFrustumCenter = {0};
            for(uint32_t i = 0; i < 8; i++)
                tFrustumCenter = pl_add_vec3(tFrustumCenter, atCameraCorners[i]);
            tFrustumCenter = pl_div_vec3_scalarf(tFrustumCenter, 8.0f);

            float fRadius = 0.0f;
            for (uint32_t i = 0; i < 8; i++)
            {
                float fDistance = pl_length_vec3(pl_sub_vec3(atCameraCorners[i], tFrustumCenter));
                fRadius = pl_max(fRadius, fDistance);
            }
            fRadius = ceilf(fRadius * 16.0f) / 16.0f;

            plVec3 tDirection = ptLight->tDirection;

            tDirection = pl_norm_vec3(tDirection);
            plVec3 tEye = pl_sub_vec3(tFrustumCenter, pl_mul_vec3_scalarf(tDirection, fRadius + 50.0f));

            plCameraComponent tShadowCamera = {
                .tType = PL_CAMERA_TYPE_ORTHOGRAPHIC
            };
            gptCamera->look_at(&tShadowCamera, tEye, tFrustumCenter);
            tShadowCamera.fWidth = fRadius * 2.0f;
            tShadowCamera.fHeight = fRadius * 2.0f;
            tShadowCamera.fNearZ = 0.0f;
            tShadowCamera.fFarZ = fRadius * 2.0f + 50.0f;
            gptCamera->update(&tShadowCamera);
            tShadowCamera.fAspectRatio = 1.0f;
            tShadowCamera.fFieldOfView = atan2f(fRadius, (fRadius + 50.0f));
            tShadowCamera.fNearZ = 0.01f;
            fLastSplitDist = fSplitDist;

            // tShadowCamera.tProjMat.col[2].z *= -1.0f;
            atCamViewProjs[uCascade] = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            
            ptShadowData->cascadeViewProjMat[uCascade] = atCamViewProjs[uCascade];
        }

        char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptView->atDShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
        memcpy(&pcBufferStart[uDCameraBufferOffset], atCamViewProjs, sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);
        uDCameraBufferOffset += sizeof(plMat4) * PL_MAX_SHADOW_CASCADES;
    }

    plBindGroupLayout tBindGroupLayout0 = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            }
        }
    };
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tBindGroupLayout0,
        .pcDebugName = "temporary global bind group 0"
    };
    plBindGroupHandle tGlobalBG = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);

    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptView->atDShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = uDCameraBufferOffset
        }
    };

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 1,
        .atBufferBindings = atBufferData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG, &tBGData0);
    
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG);

    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtDeferredDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtForwardDrawables);

    // const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tShadowData.tOpaqueRenderPass)->tDesc.tDimensions;

    typedef struct _plShadowDynamicData
    {
        int    iIndex;
        int    iDataOffset;
        int    iVertexOffset;
        int    iMaterialIndex;
        plMat4 tModel;
    } plShadowDynamicData;

    iLastIndex = -1;
    iCurrentView = -1;
    uint32_t uDOffset = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];
        const plLightComponent* ptLight = &sbtLights[ptRect->iId];

        if(ptRect->iId == iLastIndex)
        {
            iCurrentView++;
        }
        else
        {
            iLastIndex = ptRect->iId;
            iCurrentView = 0;
        }

        if(ptLight->tType != PL_LIGHT_TYPE_DIRECTIONAL || iCurrentView != (int)uViewHandle)
        {
            continue;
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtDeferredDrawables);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtForwardDrawables);



        if(gptData->bMultiViewportShadows)
        {


            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);

            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDeferredDrawables[i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)uDOffset;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader         = gptData->tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                    .atBindGroups = {
                        ptScene->tGlobalBindGroup,
                        tGlobalBG
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = ptLight->uCascadeCount
                });
            }

            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtForwardDrawables[i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)uDOffset;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = tDrawable.tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                    .atBindGroups = {
                        ptScene->tGlobalBindGroup,
                        tGlobalBG
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = ptLight->uCascadeCount
                });

            };
  
            plDrawArea tArea = 
            {
                .ptDrawStream = ptStream,
                .atScissors = {
                    {
                        .iOffsetX = (int)(ptRect->iX + 0 * ptLight->uShadowResolution),
                        .iOffsetY = ptRect->iY,
                        .uWidth  = ptLight->uShadowResolution,
                        .uHeight = ptLight->uShadowResolution,
                    },
                    {
                        .iOffsetX = (int)(ptRect->iX + 1 * ptLight->uShadowResolution),
                        .iOffsetY = ptRect->iY,
                        .uWidth  = ptLight->uShadowResolution,
                        .uHeight = ptLight->uShadowResolution,
                    },
                    {
                        .iOffsetX = (int)(ptRect->iX + 2 * ptLight->uShadowResolution),
                        .iOffsetY = ptRect->iY,
                        .uWidth  = ptLight->uShadowResolution,
                        .uHeight = ptLight->uShadowResolution,
                    },
                    {
                        .iOffsetX = (int)(ptRect->iX + 3 * ptLight->uShadowResolution),
                        .iOffsetY = ptRect->iY,
                        .uWidth  = ptLight->uShadowResolution,
                        .uHeight = ptLight->uShadowResolution,
                    }
                },
                .atViewports = {
                    {
                        .fX = (float)(ptRect->iX + 0 * ptLight->uShadowResolution),
                        .fY = (float)ptRect->iY,
                        .fWidth  = (float)ptLight->uShadowResolution,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(ptRect->iX + 1 * ptLight->uShadowResolution),
                        .fY = (float)ptRect->iY,
                        .fWidth  = (float)ptLight->uShadowResolution,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(ptRect->iX + 2 * ptLight->uShadowResolution),
                        .fY = (float)ptRect->iY,
                        .fWidth  = (float)ptLight->uShadowResolution,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(ptRect->iX + 3 * ptLight->uShadowResolution),
                        .fY = (float)ptRect->iY,
                        .fWidth  = (float)ptLight->uShadowResolution,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                }
            };

            gptGfx->draw_stream(ptEncoder, 1, &tArea);
        }
        else
        {
            for(uint32_t uCascade = 0; uCascade < ptLight->uCascadeCount; uCascade++)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDeferredDrawables[i];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCascade + (int)uDOffset;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = gptData->tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                        .atBindGroups = {
                            ptScene->tGlobalBindGroup,
                            tGlobalBG
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = ptLight->uCascadeCount
                    });
                }

                for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtForwardDrawables[i];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCascade + (int)uDOffset;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = tDrawable.tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                        .atBindGroups = {
                            ptScene->tGlobalBindGroup,
                            tGlobalBG
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = ptLight->uCascadeCount
                    });

                };

                plDrawArea tArea = 
                {
                    .ptDrawStream = ptStream,
                    .atScissors = {
                        {
                            .iOffsetX = (int)(ptRect->iX + uCascade * ptLight->uShadowResolution),
                            .iOffsetY = ptRect->iY,
                            .uWidth  = ptLight->uShadowResolution,
                            .uHeight = ptLight->uShadowResolution,
                        }
                    },
                    .atViewports = {
                        {
                            .fX = (float)(ptRect->iX + uCascade * ptLight->uShadowResolution),
                            .fY = (float)ptRect->iY,
                            .fWidth  = (float)ptLight->uShadowResolution,
                            .fHeight = (float)ptLight->uShadowResolution,
                            .fMaxDepth = 1.0f
                        }
                    }
                };

                gptGfx->draw_stream(ptEncoder, 1, &tArea);   
            } 
        }
        uDOffset+=4;
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_post_process_scene(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, const plMat4* ptMVP)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plRefScene*   ptScene    = &gptData->sbtScenes[uSceneHandle];
    plRefView*    ptView     = &ptScene->atViews[uViewHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tPostProcessRenderPass)->tDesc.tDimensions;

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tPostProcessRenderPass, NULL);

    plDrawIndex tDraw = {
        .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
        .uIndexCount    = 6,
        .uInstanceCount = 1,
    };

    const plBindGroupLayout tOutlineBindGroupLayout = {
        .atSamplerBindings = {
            { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };

    // create bind group
    const plBindGroupDesc tOutlineBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tOutlineBindGroupLayout,
        .pcDebugName = "temp bind group 0"
    };
    plBindGroupHandle tJFABindGroup0 = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);

    const plBindGroupUpdateSamplerData tOutlineSamplerData = {
        .tSampler = gptData->tDefaultSampler,
        .uSlot = 0
    };

    // update bind group (actually point descriptors to GPU resources)
    const plBindGroupUpdateTextureData tOutlineTextureData[] = 
    {
        {
            .tTexture = ptView->tRawOutputTexture[uFrameIdx],
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptView->tLastUVMask,
            .uSlot    = 2,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED,
            .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        },
    };

    const plBindGroupUpdateData tJFABGData = {
        .uSamplerCount = 1,
        .atSamplerBindings = &tOutlineSamplerData,
        .uTextureCount = 2,
        .atTextureBindings = tOutlineTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tJFABindGroup0, &tJFABGData);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tJFABindGroup0);

    typedef struct _plPostProcessOptions
    {
        float fTargetWidth;
        int   _padding[3];
        plVec4 tOutlineColor;
    } plPostProcessOptions;


    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

    plPostProcessOptions* ptDynamicData = (plPostProcessOptions*)tDynamicBinding.pcData;
    const plVec4 tOutlineColor = (plVec4){(float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 0.0f, 1.0f};
    ptDynamicData->fTargetWidth = (float)gptData->uOutlineWidth * tOutlineColor.r + 1.0f;
    ptDynamicData->tOutlineColor = tOutlineColor;

    gptGfx->bind_shader(ptEncoder, ptScene->tTonemapShader);
    gptGfx->bind_vertex_buffer(ptEncoder, gptData->tFullQuadVertexBuffer);
    gptGfx->bind_graphics_bind_groups(ptEncoder, ptScene->tTonemapShader, 0, 1, &tJFABindGroup0, 1, &tDynamicBinding);
    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    gptDrawBackend->submit_3d_drawlist(ptView->pt3DGizmoDrawList, ptEncoder, tDimensions.x, tDimensions.y, ptMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);

    gptGfx->end_render_pass(ptEncoder);
    pl_end_cpu_sample(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] shader variant system
//-----------------------------------------------------------------------------

static plShaderHandle
pl__get_shader_variant(uint32_t uSceneHandle, plShaderHandle tHandle, const plShaderVariant* ptVariant)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice* ptDevice = gptData->ptDevice;

    plShader* ptShader = gptGfx->get_shader(ptDevice, tHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += pl__get_data_type_size2(ptConstant->tType);
    }

    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, szSpecializationSize, ptVariant->tGraphicsState.ulValue);
    const uint64_t ulIndex = pl_hm_lookup(gptData->ptVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return gptData->_sbtVariantHandles[ulIndex];

    plShaderDesc tDesc = ptShader->tDesc;
    tDesc.tGraphicsState = ptVariant->tGraphicsState;
    tDesc.pTempConstantData = ptVariant->pTempConstantData;

    plShaderHandle tShader = gptGfx->create_shader(ptDevice, &tDesc);

    pl_hm_insert(gptData->ptVariantHashmap, ulVariantHash, pl_sb_size(gptData->_sbtVariantHandles));
    pl_sb_push(gptData->_sbtVariantHandles, tShader);
    return tShader;
}

//-----------------------------------------------------------------------------
// [SECTION] misc.
//-----------------------------------------------------------------------------

static void
pl_refr_create_global_shaders(void)
{
    int aiConstantData[7] = {0, 0, 0, 0, 0, 1, 1};

    plShaderDesc tDeferredShaderDescription = {
        .tPixelShader  = gptShader->load_glsl("../shaders/primitive.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/primitive.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride  = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT3}}
            }
        },
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 0,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
                .atSamplerBindings = {
                    {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                    {.uSlot = 4100, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                }
            },
            {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
            },
        }
    };
    for(uint32_t i = 0; i < 5; i++)
    {
        tDeferredShaderDescription.atConstants[i].uID = i;
        tDeferredShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tDeferredShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tDeferredShader = gptGfx->create_shader(gptData->ptDevice, &tDeferredShaderDescription);

    plShaderDesc tForwardShaderDescription = {
        .tPixelShader = gptShader->load_glsl("../shaders/transparent.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/transparent.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT3}}
            }
        },
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 2,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
                .atSamplerBindings = {
                    {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                    {.uSlot = 4100, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                }
            },
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 3, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 4, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                },
                .atSamplerBindings = {
                    {.uSlot = 5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            }
        }
    };
    for(uint32_t i = 0; i < 7; i++)
    {
        tForwardShaderDescription.atConstants[i].uID = i;
        tForwardShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tForwardShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tForwardShader = gptGfx->create_shader(gptData->ptDevice, &tForwardShaderDescription);


    static const plShaderMacroDefinition tDefinition = {
        .pcName = "PL_MULTIPLE_VIEWPORTS",
        .szNameLength = 21
    };
    static plShaderOptions tShadowShaderOptions = {
        .uMacroDefinitionCount = 1,
        .ptMacroDefinitions = &tDefinition,
        .apcIncludeDirectories = {
            "../shaders/"
        },
        #ifndef PL_OFFLINE_SHADERS_ONLY
        .tFlags = PL_SHADER_FLAGS_ALWAYS_COMPILE | PL_SHADER_FLAGS_INCLUDE_DEBUG
        #endif
    };

    plShaderModule tShadowPixelShader = gptShader->load_glsl("../shaders/shadow.frag", "main", NULL, NULL);
    plShaderModule tVertexShader = gptShader->load_glsl("../shaders/shadow.vert", "main", NULL, gptData->bMultiViewportShadows ? &tShadowShaderOptions : NULL);

    plShaderDesc tShadowShaderDescription = {
        .tPixelShader = tShadowPixelShader,
        .tVertexShader = tVertexShader,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT3}}
            }
        },
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .tRenderPassLayout = gptData->tDepthRenderPassLayout,
        .uSubpassIndex = 0,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
                .atSamplerBindings = {
                    {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                    {.uSlot = 4100, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                }
            },
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            }
        }
    };
    for(uint32_t i = 0; i < 4; i++)
    {
        tShadowShaderDescription.atConstants[i].uID = i;
        tShadowShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tShadowShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }

    gptData->tAlphaShadowShader = gptGfx->create_shader(gptData->ptDevice, &tShadowShaderDescription);
    tShadowShaderDescription.tPixelShader.puCode = NULL;
    tShadowShaderDescription.tPixelShader.szCodeSize = 0;
    gptData->tShadowShader = gptGfx->create_shader(gptData->ptDevice, &tShadowShaderDescription);
        
    const plShaderDesc tPickShaderDescription = {
        .tPixelShader = gptShader->load_glsl("../shaders/picking.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/picking.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT3}}
            }
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tPickRenderPassLayout,
        .uSubpassIndex = 0,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            }
        }
    };
    gptData->tPickShader = gptGfx->create_shader(gptData->ptDevice, &tPickShaderDescription);

    const plShaderDesc tUVShaderDesc = {
        .tPixelShader = gptShader->load_glsl("../shaders/uvmap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/uvmap.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilTestEnabled = 1,
            .ulStencilMode        = PL_COMPARE_MODE_LESS,
            .ulStencilRef         = 128,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 4,
                .atAttributes = {
                    {.uByteOffset = 0,                 .tFormat = PL_VERTEX_FORMAT_FLOAT2},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_VERTEX_FORMAT_FLOAT2},
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled = false
            }
        },
        .tRenderPassLayout = gptData->tUVRenderPassLayout
    };
    gptData->tUVShader = gptGfx->create_shader(gptData->ptDevice, &tUVShaderDesc);

    // create skybox shader
    plShaderDesc tSkyboxShaderDesc = {
        .tPixelShader = gptShader->load_glsl("../shaders/skybox.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/skybox.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT3}}
            }
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 2,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atSamplerBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                }
            },
            {
                .atTextureBindings = {
                    { .uSlot = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            }
        }
    };
    gptData->tSkyboxShader = gptGfx->create_shader(gptData->ptDevice, &tSkyboxShaderDesc);
}

static void
pl__add_drawable_skin_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex, plDrawable* atDrawables)
{
    plEntity tEntity = atDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
    plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);

    if(ptMesh->tSkinComponent.uIndex == UINT32_MAX)
        return;

    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtSkinVertexDataBuffer);
    const uint32_t uVertexCount          = pl_sb_size(ptMesh->sbtVertexPositions);

    // stride within storage buffer
    uint32_t uStride = 0;

    uint64_t ulVertexStreamMask = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexPositions) > 0)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_POSITION; }
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)    { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)   { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(pl_sb_size(ptMesh->sbtVertexWeights[0]) > 0) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexWeights[1]) > 0) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(pl_sb_size(ptMesh->sbtVertexJoints[0]) > 0)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexJoints[1]) > 0)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

    pl_sb_add_n(ptScene->sbtSkinVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // positions
    const uint32_t uVertexPositionCount = pl_sb_size(ptMesh->sbtVertexPositions);
    for(uint32_t i = 0; i < uVertexPositionCount; i++)
    {
        const plVec3* ptPosition = &ptMesh->sbtVertexPositions[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptPosition->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptPosition->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptPosition->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 1.0f;
    }

    if(uVertexPositionCount > 0)
        uOffset += 1;

    // normals
    const uint32_t uVertexNormalCount = pl_sb_size(ptMesh->sbtVertexNormals);
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptNormal->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptNormal->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptNormal->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = pl_sb_size(ptMesh->sbtVertexTangents);
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // joints 0
    const uint32_t uVertexJoint0Count = pl_sb_size(ptMesh->sbtVertexJoints[0]);
    for(uint32_t i = 0; i < uVertexJoint0Count; i++)
    {
        const plVec4* ptJoint = &ptMesh->sbtVertexJoints[0][i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptJoint->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptJoint->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptJoint->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptJoint->w;
    }

    if(uVertexJoint0Count > 0)
        uOffset += 1;

    // weights 0
    const uint32_t uVertexWeights0Count = pl_sb_size(ptMesh->sbtVertexWeights[0]);
    for(uint32_t i = 0; i < uVertexWeights0Count; i++)
    {
        const plVec4* ptWeight = &ptMesh->sbtVertexWeights[0][i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptWeight->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptWeight->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptWeight->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptWeight->w;
    }

    if(uVertexWeights0Count > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    // stride within storage buffer
    uint32_t uDestStride = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)               { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)              { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexColors[0]) > 0)             { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexColors[1]) > 0)             { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[2]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[4]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[6]) > 0) { uDestStride += 1; }

    plSkinData tSkinData = {
        .tEntity = ptMesh->tSkinComponent,
        .uVertexCount = uVertexCount,
        .iSourceDataOffset = uVertexDataStartIndex,
        .iDestDataOffset = atDrawables[uDrawableIndex].uDataOffset,
        .iDestVertexOffset = atDrawables[uDrawableIndex].uVertexOffset,
    };

    plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptMesh->tSkinComponent);
    unsigned int textureWidth = (unsigned int)ceilf(sqrtf((float)(pl_sb_size(ptSkinComponent->sbtJoints) * 8)));
    pl_sb_resize(ptSkinComponent->sbtTextureData, textureWidth * textureWidth);
    const plTextureDesc tSkinTextureDesc = {
        .tDimensions = {(float)textureWidth, (float)textureWidth, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };

    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGfx->get_frames_in_flight(); uFrameIndex++)
        tSkinData.atDynamicTexture[uFrameIndex] = pl__refr_create_texture_with_data(&tSkinTextureDesc, "joint texture", uFrameIndex, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * textureWidth * textureWidth);

    int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
    const plComputeShaderDesc tComputeShaderDesc = {
        .tShader = gptShader->load_glsl("../shaders/skinning.comp", "main", NULL, NULL),
        .pTempConstantData = aiSpecializationData,
        .atConstants = {
            { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
            { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT},
            { .uID = 3, .uOffset = 3 * sizeof(int), .tType = PL_DATA_TYPE_INT}
        },
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                },
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_COMPUTE}
                }
            },
            {
                .atTextureBindings = {
                    {.uSlot =  0, .tStages = PL_STAGE_ALL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    tSkinData.tShader = gptGfx->create_compute_shader(gptData->ptDevice, &tComputeShaderDesc);
    pl_temp_allocator_reset(&gptData->tTempAllocator);
    atDrawables[uDrawableIndex].uSkinIndex = pl_sb_size(ptScene->sbtSkinData);
    pl_sb_push(ptScene->sbtSkinData, tSkinData);
}

static void
pl__add_drawable_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex, plDrawable* atDrawables)
{

    plEntity tEntity = atDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
    plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);

    const uint32_t uVertexPosStartIndex  = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexBufferStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);
    const uint32_t uIndexCount           = pl_sb_size(ptMesh->sbuIndices);
    const uint32_t uVertexCount          = pl_sb_size(ptMesh->sbtVertexPositions);

    // add index buffer data
    pl_sb_add_n(ptScene->sbuIndexBuffer, uIndexCount);
    for(uint32_t j = 0; j < uIndexCount; j++)
        ptScene->sbuIndexBuffer[uIndexBufferStart + j] = uVertexPosStartIndex + ptMesh->sbuIndices[j];

    // add vertex position data
    pl_sb_add_n(ptScene->sbtVertexPosBuffer, uVertexCount);
    memcpy(&ptScene->sbtVertexPosBuffer[uVertexPosStartIndex], ptMesh->sbtVertexPositions, sizeof(plVec3) * uVertexCount);

    // stride within storage buffer
    uint32_t uStride = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)               { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)              { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(pl_sb_size(ptMesh->sbtVertexColors[0]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }
    if(pl_sb_size(ptMesh->sbtVertexColors[1]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[2]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[4]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[6]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3; }

    pl_sb_add_n(ptScene->sbtVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // normals
    const uint32_t uVertexNormalCount = pl_sb_size(ptMesh->sbtVertexNormals);
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptNormal->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptNormal->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptNormal->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = pl_sb_size(ptMesh->sbtVertexTangents);
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // texture coordinates 0
    for(uint32_t i = 0; i < 8; i+=2)
    {
        const uint32_t uVertexTexCount0 = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[i]);
        const uint32_t uVertexTexCount1 = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[i + 1]);

        if(uVertexTexCount1 > 0)
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates0 = &(ptMesh->sbtVertexTextureCoordinates[i])[j];
                const plVec2* ptTextureCoordinates1 = &(ptMesh->sbtVertexTextureCoordinates[i + 1])[j];
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].x = ptTextureCoordinates0->u;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].y = ptTextureCoordinates0->v;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].z = ptTextureCoordinates1->u;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].w = ptTextureCoordinates1->v;
            }
        }
        else
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates = &(ptMesh->sbtVertexTextureCoordinates[i])[j];
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].x = ptTextureCoordinates->u;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].y = ptTextureCoordinates->v;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].z = 0.0f;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].w = 0.0f;
            } 
        }

        if(uVertexTexCount0 > 0)
            uOffset += 1;
    }

    // color 0
    const uint32_t uVertexColorCount = pl_sb_size(ptMesh->sbtVertexColors[0]);
    for(uint32_t i = 0; i < uVertexColorCount; i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    atDrawables[uDrawableIndex].uIndexCount      = uIndexCount;
    atDrawables[uDrawableIndex].uVertexCount     = uVertexCount;
    atDrawables[uDrawableIndex].uIndexOffset     = uIndexBufferStart;
    atDrawables[uDrawableIndex].uVertexOffset    = uVertexPosStartIndex;
    atDrawables[uDrawableIndex].uDataOffset      = uVertexDataStartIndex;
}

static plBlendState
pl__get_blend_state(plBlendMode tBlendMode)
{

    static const plBlendState atStateMap[PL_BLEND_MODE_COUNT] =
    {
        // PL_BLEND_MODE_OPAQUE
        { 
            .bBlendEnabled = false,
        },

        // PL_BLEND_MODE_ALPHA
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_PREMULTIPLY
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_ONE,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_ADDITIVE
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_MULTIPLY
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_DST_COLOR,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_DST_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_CLIP_MASK
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_ZERO,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_DST_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ZERO,
            .tAlphaOp        = PL_BLEND_OP_ADD
        }
    };

    PL_ASSERT(tBlendMode < PL_BLEND_MODE_COUNT && "blend mode out of range");
    return atStateMap[tBlendMode];
}

static size_t
pl__get_data_type_size2(plDataType tType)
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

static uint32_t
pl__get_bindless_texture_index(uint32_t uSceneHandle, plTextureHandle tTexture)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    if(pl_hm_has_key(ptScene->ptTextureIndexHashmap, tTexture.uData))
    {
        return (uint32_t)pl_hm_lookup(ptScene->ptTextureIndexHashmap, tTexture.uData);
    }

    uint64_t ulValue = pl_hm_get_free_index(ptScene->ptTextureIndexHashmap);
    if(ulValue == UINT64_MAX)
    {
        ulValue = ptScene->uTextureIndexCount++;

        // TODO: handle when greater than 4096
    }
    
    const plBindGroupUpdateTextureData tGlobalTextureData[] = {
        {
            .tTexture = tTexture,
            .uSlot    = 4,
            .uIndex   = (uint32_t)ulValue,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uTextureCount = 1,
        .atTextureBindings = tGlobalTextureData
    };

    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tGlobalBindGroup, &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}

static uint32_t
pl__get_bindless_cube_texture_index(uint32_t uSceneHandle, plTextureHandle tTexture)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    if(pl_hm_has_key(ptScene->ptCubeTextureIndexHashmap, tTexture.uData))
    {
        return (uint32_t)pl_hm_lookup(ptScene->ptCubeTextureIndexHashmap, tTexture.uData);
    }

    uint64_t ulValue = pl_hm_get_free_index(ptScene->ptCubeTextureIndexHashmap);
    if(ulValue == UINT64_MAX)
    {
        ulValue = ptScene->uCubeTextureIndexCount++;

        // TODO: handle when greater than 4096
    }
    
    const plBindGroupUpdateTextureData tGlobalTextureData[] = {
        {
            .tTexture = tTexture,
            .uSlot    = 4100,
            .uIndex   = (uint32_t)ulValue,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uTextureCount = 1,
        .atTextureBindings = tGlobalTextureData
    };

    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tGlobalBindGroup, &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}
