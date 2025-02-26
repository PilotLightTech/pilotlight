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
pl__refr_cull_job(plInvocationData tInvoData, void* pData)
{
    plCullData* ptCullData = pData;
    plRefScene* ptScene = ptCullData->ptScene;
    plDrawable tDrawable = ptCullData->atDrawables[tInvoData.uGlobalIndex];
    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
    ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = true;
    if(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE)
    {
        if(pl__sat_visibility_test(ptCullData->ptCullCamera, &ptObject->tAABB))
        {
            ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = false;
        }
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
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created local texture %s %u", pcName, uIdentifier);
    pl_log_info_f(gptLog, gptData->uLogChannel, "created local texture %s %u", pcName, uIdentifier);
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
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created texture %s %u", pcName, uIdentifier);
    pl_log_info_f(gptLog, gptData->uLogChannel, "created texture %s %u", pcName, uIdentifier);
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
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, PL_TEXTURE_USAGE_SAMPLED, 0);


    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[gptGfx->get_current_frame_index()]);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, szSize);


        const plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptDesc->tDimensions.x,
            .uImageHeight = (uint32_t)ptDesc->tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptData->tStagingBufferHandle[gptGfx->get_current_frame_index()], tHandle, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(ptBlitEncoder, tHandle);
    }

    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created texture %s %u", pcName, uIdentifier);
    pl_log_info_f(gptLog, gptData->uLogChannel, "created texture %s %u", pcName, uIdentifier);
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

    // PL_DEVICE_BUDDY_BLOCK_SIZE

    plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptStagingUnCachedAllocator;
    else
        ptAllocator = gptData->ptStagingUnCachedBuddyAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "sbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created staging buffer %s %u", pcName, uIdentifier);
    pl_log_info_f(gptLog, gptData->uLogChannel, "created staging buffer %s %u", pcName, uIdentifier);
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

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created cached staging buffer %s %u", pcName, uIdentifier);
    pl_log_info_f(gptLog, gptData->uLogChannel, "created cached staging buffer %s %u", pcName, uIdentifier);
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
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[gptGfx->get_current_frame_index()]);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, ptDesc->szByteSize);

        // begin recording
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        
        // begin blit pass, copy buffer, end pass
        plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        gptGfx->copy_buffer(ptEncoder, gptData->tStagingBufferHandle[gptGfx->get_current_frame_index()], tHandle, 0, 0, ptDesc->szByteSize);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptEncoder);

        // finish recording
        gptGfx->end_command_recording(ptCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created local buffer %s %u", pcName, uIdentifier);
    pl_log_info_f(gptLog, gptData->uLogChannel, "created local buffer %s %u", pcName, uIdentifier);
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
        uint32_t uMaxSize;
    } SkinDynamicData;

    if(uSkinCount)
    {

        const plPassBufferResource atPassBuffers[] = {
            { .tHandle = ptScene->tSkinStorageBuffer, .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_READ },
            { .tHandle = ptScene->tVertexBuffer,      .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->tStorageBuffer,     .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 3,
            .atBuffers = atPassBuffers
        };

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
        for(uint32_t i = 0; i < uSkinCount; i++)
        {
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            SkinDynamicData* ptDynamicData = (SkinDynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iSourceDataOffset = ptScene->sbtSkinData[i].iSourceDataOffset;
            ptDynamicData->iDestDataOffset = ptScene->sbtSkinData[i].iDestDataOffset;
            ptDynamicData->iDestVertexOffset = ptScene->sbtSkinData[i].iDestVertexOffset;
            ptDynamicData->uMaxSize = ptScene->sbtSkinData[i].uVertexCount;

            const plDispatch tDispach = {
                .uGroupCountX     = (uint32_t)ceilf((float)ptScene->sbtSkinData[i].uVertexCount / 64.0f),
                .uGroupCountY     = 1,
                .uGroupCountZ     = 1,
                .uThreadPerGroupX = 64,
                .uThreadPerGroupY = 1,
                .uThreadPerGroupZ = 1
            };
            const plBindGroupHandle atBindGroups[] = {
                ptScene->tSkinBindGroup0,
                ptScene->sbtSkinData[i].atBindGroup[gptGfx->get_current_frame_index()]
            };
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, ptScene->sbtSkinData[i].tShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptComputeEncoder, ptScene->sbtSkinData[i].tShader);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        }
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
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

    const plEnvironmentProbeComponent* sbtProbes = ptScene->tComponentLibrary.tEnvironmentProbeCompManager.pComponents;
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    const uint32_t uProbeCount = pl_sb_size(sbtProbes);
    const uint32_t uLightCount = pl_sb_size(sbtLights);

    // for packing, using rect id as (16 bits) + (3 bits) + (11 bits) + (2bit) for light, view, probe, mode

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
            for(uint32_t uView = 0; uView < uViewCount; uView++)
            {
                const plPackRect tPackRect = {
                    .iWidth  = (int)(ptLight->uShadowResolution * ptLight->uCascadeCount),
                    .iHeight = (int)ptLight->uShadowResolution,
                    .iId     = (int)uLightIndex + ((int)(uView) << 16)
                };
                pl_sb_push(ptScene->sbtShadowRects, tPackRect);
            }

            for(uint32_t uProbe = 0; uProbe < uProbeCount; uProbe++)
            {
                for(uint32_t uView = 0; uView < 6; uView++)
                {
                    const plPackRect tPackRect = {
                        .iWidth  = (int)(ptLight->uShadowResolution),
                        .iHeight = (int)ptLight->uShadowResolution,
                        .iId     = (int)uLightIndex + ((int)uView << 16) + ((int)(uProbe) << 18) + (1 << 30)
                    };
                    pl_sb_push(ptScene->sbtShadowRects, tPackRect);
                }
            }
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {
            const plPackRect tPackRect = {
                .iWidth  = (int)(ptLight->uShadowResolution * 2),
                .iHeight = (int)(ptLight->uShadowResolution * 3),
                .iId     = (int)uLightIndex
            };
            pl_sb_push(ptScene->sbtShadowRects, tPackRect);
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {
            const plPackRect tPackRect = {
                .iWidth  = (int)ptLight->uShadowResolution,
                .iHeight = (int)ptLight->uShadowResolution,
                .iId     = (int)uLightIndex
            };
            pl_sb_push(ptScene->sbtShadowRects, tPackRect);
        }
    }

    // pack rects
    const uint32_t uRectCount = pl_sb_size(ptScene->sbtShadowRects);
    gptRect->pack_rects(ptScene->uShadowAtlasResolution, ptScene->uShadowAtlasResolution, ptScene->sbtShadowRects, uRectCount);

    // ensure rects are packed
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
    uint32_t uSCameraBufferOffset = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 19) & ~(~0u << 12);
        int iMode = ptRect->iId >> 31;

        const plLightComponent* ptLight = &sbtLights[iLight];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {
            pl_sb_add(ptScene->sbtPLightShadowData);

            plGPULightShadowData* ptShadowData = &ptScene->sbtPLightShadowData[pl_sb_size(ptScene->sbtPLightShadowData) - 1];
            ptShadowData->iShadowMapTexIdx = ptScene->atShadowTextureBindlessIndices;
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

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {
            pl_sb_add(ptScene->sbtSLightShadowData);

            plGPULightShadowData* ptShadowData = &ptScene->sbtSLightShadowData[pl_sb_size(ptScene->sbtSLightShadowData) - 1];
            ptShadowData->iShadowMapTexIdx = ptScene->atShadowTextureBindlessIndices;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 tCamViewProjs = {0};

            plCameraComponent tShadowCamera = {
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPos         = ptLight->tPosition,
                .fNearZ       = ptLight->fRadius,
                .fFarZ        = ptLight->fRange,
                .fFieldOfView = ptLight->fOuterConeAngle * 2.0f,
                .fAspectRatio = 1.0f
            };

            plVec3 tDirection = pl_norm_vec3(ptLight->tDirection);
            gptCamera->look_at(&tShadowCamera, ptLight->tPosition, pl_add_vec3(ptLight->tPosition, tDirection));
            gptCamera->update(&tShadowCamera);
            tCamViewProjs = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            ptShadowData->viewProjMat[0] = tCamViewProjs;
            
            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atSShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[uSCameraBufferOffset], &tCamViewProjs, sizeof(plMat4));
            uSCameraBufferOffset += sizeof(plMat4);
        }    
    }

    plBindGroupLayout tBindGroupLayout0 = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        }
    };
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tBindGroupLayout0,
        .pcDebugName = "temporary global bind group 0"
    };
    plBindGroupHandle tGlobalBG0 = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);
    plBindGroupHandle tGlobalBG1 = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);

    const plBindGroupUpdateBufferData atBufferData0[] = 
    {
        {
            .tBuffer       = ptScene->atPShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = uPCameraBufferOffset
        }
    };

    const plBindGroupUpdateBufferData atBufferData1[] = 
    {
        {
            .tBuffer       = ptScene->atSShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = uSCameraBufferOffset
        }
    };

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 1,
        .atBufferBindings = atBufferData0
    };
    plBindGroupUpdateData tBGData1 = {
        .uBufferCount = 1,
        .atBufferBindings = atBufferData1
    };
    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG0, &tBGData0);
    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG1, &tBGData1);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG1);

    const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbuShadowDeferredDrawables);
    const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbuShadowForwardDrawables);

    // const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tShadowData.tOpaqueRenderPass)->tDesc.tDimensions;

    typedef struct _plShadowDynamicData
    {
        int    iIndex;
        int    iDataOffset;
        int    iVertexOffset;
        int    iMaterialIndex;
        plMat4 tModel;
    } plShadowDynamicData;

    uint32_t uPCameraBufferIndex = 0;
    uint32_t uSCameraBufferIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 19) & ~(~0u << 12);
        int iMode = ptRect->iId >> 31;

        const plLightComponent* ptLight = &sbtLights[iLight];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {

            if(gptData->bMultiViewportShadows)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uPCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = gptData->tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uTriangleCount,
                        .uVertexOffset        = tDrawable.uStaticVertexOffset,
                        .atBindGroups = {
                            ptScene->atGlobalBindGroup[uFrameIdx],
                            tGlobalBG0
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = 6
                    });
                }

                *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uPCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = tDrawable.tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uTriangleCount,
                        .uVertexOffset        = tDrawable.uStaticVertexOffset,
                        .atBindGroups = {
                            ptScene->atGlobalBindGroup[uFrameIdx],
                            tGlobalBG0
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
                    *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                    for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];
                        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->tModel = ptTransform->tWorld;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uPCameraBufferIndex + uFaceIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader         = gptData->tShadowShader,
                            .auDynamicBuffers = {
                                tDynamicBinding.uBufferHandle
                            },
                            .atVertexBuffers = {
                                ptScene->tVertexBuffer,
                            },
                            .tIndexBuffer         = tDrawable.tIndexBuffer,
                            .uIndexOffset         = tDrawable.uIndexOffset,
                            .uTriangleCount       = tDrawable.uTriangleCount,
                            .uVertexOffset        = tDrawable.uStaticVertexOffset,
                            .atBindGroups = {
                                ptScene->atGlobalBindGroup[uFrameIdx],
                                tGlobalBG0
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = 0,
                            .uInstanceCount = 1
                        });
                    }

                    *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                    for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->tModel = ptTransform->tWorld;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uPCameraBufferIndex + uFaceIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader        = tDrawable.tShadowShader,
                            .auDynamicBuffers = {
                                tDynamicBinding.uBufferHandle
                            },
                            .atVertexBuffers = {
                                ptScene->tVertexBuffer,
                            },
                            .tIndexBuffer         = tDrawable.tIndexBuffer,
                            .uIndexOffset         = tDrawable.uIndexOffset,
                            .uTriangleCount       = tDrawable.uTriangleCount,
                            .uVertexOffset        = tDrawable.uStaticVertexOffset,
                            .atBindGroups = {
                                ptScene->atGlobalBindGroup[uFrameIdx],
                                tGlobalBG0
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
            uPCameraBufferIndex+=6;
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {

            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)uSCameraBufferIndex;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader         = gptData->tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uTriangleCount,
                    .uVertexOffset        = tDrawable.uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atGlobalBindGroup[uFrameIdx],
                        tGlobalBG1
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = 1
                });
            }

            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)uSCameraBufferIndex;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = tDrawable.tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uTriangleCount,
                    .uVertexOffset        = tDrawable.uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atGlobalBindGroup[uFrameIdx],
                        tGlobalBG1
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = 1
                });

            };

            plDrawArea tArea = 
            {
                .ptDrawStream = ptStream,
                .atScissors = {
                    {
                        .iOffsetX = ptRect->iX,
                        .iOffsetY = ptRect->iY,
                        .uWidth  = ptLight->uShadowResolution,
                        .uHeight = ptLight->uShadowResolution,
                    },
                },
                .atViewports = {
                    {
                        .fX = (float)ptRect->iX,
                        .fY = (float)ptRect->iY,
                        .fWidth  = (float)ptLight->uShadowResolution,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    }
                }
            };

            gptGfx->draw_stream(ptEncoder, 1, &tArea);
            uSCameraBufferIndex++;
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_generate_cascaded_shadow_map(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, uint32_t uProbeIndex, int iRequestMode, plDirectionLightShadowData* ptDShadowData, plCameraComponent* ptSceneCamera)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plRefScene*    ptScene   = &gptData->sbtScenes[uSceneHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    const uint32_t uInitialOffset = ptDShadowData->uOffset;

    const float g = 1.0f / tanf(ptSceneCamera->fFieldOfView / 2.0f);
    const float s = ptSceneCamera->fAspectRatio;

    // common calculations
    const float fFarClip = ptSceneCamera->fFarZ;
    const float fNearClip = ptSceneCamera->fNearZ;
    const float fClipRange = fFarClip - fNearClip;

    const float fMinZ = fNearClip;
    const float fMaxZ = fNearClip + fClipRange;

    const float fRange = fMaxZ - fMinZ;
    const float fRatio = fMaxZ / fMinZ;

    // TODO: we shouldn't have to check all rects, optimize this

    const uint32_t uAtlasRectCount = pl_sb_size(ptScene->sbtShadowRects);
    for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uRectIndex];

        // decode rect info
        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 18) & ~(~0u << 11);
        int iMode = ptRect->iId >> 30;

        const plLightComponent* ptLight = &sbtLights[iLight];

        // check if light applies
        if(ptLight->tType != PL_LIGHT_TYPE_DIRECTIONAL || iView != (int)uViewHandle || iMode != iRequestMode || iProbe != (int)uProbeIndex)
        {
            continue;
        }

        const uint32_t uDataOffset = pl_sb_size(ptDShadowData->sbtDLightShadowData);
        pl_sb_add(ptDShadowData->sbtDLightShadowData);

        // copy GPU light shadow data
        plGPULightShadowData* ptShadowData = &ptDShadowData->sbtDLightShadowData[uDataOffset];
        ptShadowData->iShadowMapTexIdx    = ptScene->atShadowTextureBindlessIndices;
        ptShadowData->fFactor             = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fXOffset            = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fYOffset            = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->tCascadeSplits.d[0] = 10000.0f;
        ptShadowData->tCascadeSplits.d[1] = 10000.0f;
        ptShadowData->tCascadeSplits.d[2] = 10000.0f;
        ptShadowData->tCascadeSplits.d[3] = 10000.0f;

        plMat4 atCamViewProjs[PL_MAX_SHADOW_CASCADES] = {0};
        float fLastSplitDist = 0.0f;
        const plVec3 tDirection = pl_norm_vec3(ptLight->tDirection);
        const uint32_t uCascadeCount = iMode == 0 ? ptLight->uCascadeCount : 1; // probe only needs single cascade

        const float afCascadeSplits[4] = {
            iMode == 0 ? ptLight->afCascadeSplits[0] : 1.0f, // use whole frustum for environment probes
            ptLight->afCascadeSplits[1],
            ptLight->afCascadeSplits[2],
            ptLight->afCascadeSplits[3]
        };

        for(uint32_t uCascade = 0; uCascade < uCascadeCount; uCascade++)
        {
            float fSplitDist = afCascadeSplits[uCascade] * ptSceneCamera->fFarZ;
            ptShadowData->tCascadeSplits.d[uCascade] = fSplitDist;

            // camera space
            plVec3 atCameraCorners2[] = {
                { -fLastSplitDist * s / g,  fLastSplitDist / g, fLastSplitDist },
                { -fLastSplitDist * s / g, -fLastSplitDist / g, fLastSplitDist },
                {  fLastSplitDist * s / g, -fLastSplitDist / g, fLastSplitDist },
                {  fLastSplitDist * s / g,  fLastSplitDist / g, fLastSplitDist },
                { -fSplitDist * s / g,  fSplitDist / g, fSplitDist },
                { -fSplitDist * s / g, -fSplitDist / g, fSplitDist },
                {  fSplitDist * s / g, -fSplitDist / g, fSplitDist },
                {  fSplitDist * s / g,  fSplitDist / g, fSplitDist },
            };

            // find max diagonal in camera space
            float fD0 = ceilf(pl_length_vec3(pl_sub_vec3(atCameraCorners2[0], atCameraCorners2[6])));
            float fD1 = ceilf(pl_length_vec3(pl_sub_vec3(atCameraCorners2[6], atCameraCorners2[4])));
            float fD = pl_max(fD0, fD1) * 1.0f;
            const float fUnitPerTexel = fD / (float)ptLight->uShadowResolution;

            // convert to world space
            const plMat4 tCameraInversion = pl_mat4_invert(&ptSceneCamera->tViewMat);
            for(uint32_t i = 0; i < 8; i++)
            {
                plVec4 tInvCorner = pl_mul_mat4_vec4(&tCameraInversion, (plVec4){.xyz = atCameraCorners2[i], .w = 1.0f});
                atCameraCorners2[i] = tInvCorner.xyz;
            }

            // convert to light space
            plCameraComponent tShadowCamera = {
                .tType = PL_CAMERA_TYPE_ORTHOGRAPHIC
            };
            gptCamera->look_at(&tShadowCamera, (plVec3){0}, tDirection);
            tShadowCamera.fWidth  = fD;
            tShadowCamera.fHeight = fD;
            tShadowCamera.fNearZ  = -fD;
            tShadowCamera.fFarZ   = fD;
            gptCamera->update(&tShadowCamera);

            for(uint32_t i = 0; i < 8; i++)
            {
                plVec4 tInvCorner = pl_mul_mat4_vec4(&tShadowCamera.tViewMat, (plVec4){.xyz = atCameraCorners2[i], .w = 1.0f});
                atCameraCorners2[i] = tInvCorner.xyz;
            }

            // find center
            float fXMin = FLT_MAX;
            float fXMax = -FLT_MAX;
            float fYMin = FLT_MAX;
            float fYMax = -FLT_MAX;
            float fZMin = FLT_MAX;
            float fZMax = -FLT_MAX;
            for(uint32_t i = 0; i < 8; i++)
            {
                fXMin = pl_min(atCameraCorners2[i].x, fXMin);
                fYMin = pl_min(atCameraCorners2[i].y, fYMin);
                fZMin = pl_min(atCameraCorners2[i].z, fZMin);
                fXMax = pl_max(atCameraCorners2[i].x, fXMax);
                fYMax = pl_max(atCameraCorners2[i].y, fYMax);
                fZMax = pl_max(atCameraCorners2[i].z, fZMax);
            }

            const plVec4 tLightPosLightSpace = { 
                fUnitPerTexel * floorf((fXMax + fXMin) / (fUnitPerTexel * 2.0f)), // texel snapping
                fUnitPerTexel * floorf((fYMax + fYMin) / (fUnitPerTexel * 2.0f)), // texel snapping
                -fD,
                1.0f
            };

            const plMat4 tLightInversion = pl_mat4_invert(&tShadowCamera.tViewMat);
            const plVec4 tLightPos = pl_mul_mat4_vec4(&tLightInversion, (plVec4){.xyz = tLightPosLightSpace.xyz, .w = 1.0f});

            gptCamera->look_at(&tShadowCamera, tLightPos.xyz, pl_add_vec3(tLightPos.xyz, tDirection));
            gptCamera->update(&tShadowCamera);

            atCamViewProjs[uCascade] = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            ptShadowData->viewProjMat[uCascade] = atCamViewProjs[uCascade];
            fLastSplitDist = fSplitDist;
        }

        // TODO: rework to not waste so much space (don't use max cascades as stride)

        // copy data to GPU buffer
        char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptDShadowData->atDShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
        memcpy(&pcBufferStart[ptDShadowData->uOffset], atCamViewProjs, sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);
        ptDShadowData->uOffset += sizeof(plMat4) * PL_MAX_SHADOW_CASCADES;
    }

    plBindGroupLayout tBindGroupLayout0 = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        }
    };
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tBindGroupLayout0,
        .pcDebugName = "temporary global bind group 0"
    };
    

    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptDShadowData->atDShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = ptDShadowData->uOffset
        }
    };

    const uint32_t uIndexingOffset = uInitialOffset / (sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 1,
        .atBufferBindings = atBufferData
    };
    plBindGroupHandle tShadowBG1 = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);
    gptGfx->update_bind_group(gptData->ptDevice, tShadowBG1, &tBGData0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tShadowBG1);

    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbuShadowDeferredDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbuShadowForwardDrawables);

    typedef struct _plShadowDynamicData
    {
        int    iIndex;
        int    iDataOffset;
        int    iVertexOffset;
        int    iMaterialIndex;
        plMat4 tModel;
    } plShadowDynamicData;

    for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uRectIndex];

        // decode rect info
        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 18) & ~(~0u << 11);
        int iMode = ptRect->iId >> 30;

        const plLightComponent* ptLight = &sbtLights[iLight];

        // check if light applies
        if(ptLight->tType != PL_LIGHT_TYPE_DIRECTIONAL || iView != (int)uViewHandle || iMode != iRequestMode || iProbe != (int)uProbeIndex)
        {
            continue;
        }

        const uint32_t uCascadeCount = iMode == 0 ? ptLight->uCascadeCount : 1; // probe only needs single cascade

        if(gptData->bMultiViewportShadows)
        {
            gptGfx->reset_draw_stream(ptStream, uOpaqueDrawableCount + uTransparentDrawableCount);
            gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uOpaqueDrawableCount;
            for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)ptDShadowData->uOffsetIndex + (int)uIndexingOffset;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader         = gptData->tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uTriangleCount,
                    .uVertexOffset        = tDrawable.uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atGlobalBindGroup[uFrameIdx],
                        tShadowBG1
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = uCascadeCount
                });
            }

            *gptData->pdDrawCalls += (double)uTransparentDrawableCount;
            for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)ptDShadowData->uOffsetIndex + (int)uIndexingOffset;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = tDrawable.tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uTriangleCount,
                    .uVertexOffset        = tDrawable.uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atGlobalBindGroup[uFrameIdx],
                        tShadowBG1
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = uCascadeCount
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
                        .fWidth  = uCascadeCount > 1 ? (float)ptLight->uShadowResolution : 0.0f,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(ptRect->iX + 2 * ptLight->uShadowResolution),
                        .fY = (float)ptRect->iY,
                        .fWidth  = uCascadeCount > 1 ? (float)ptLight->uShadowResolution : 0.0f,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(ptRect->iX + 3 * ptLight->uShadowResolution),
                        .fY = (float)ptRect->iY,
                        .fWidth  = uCascadeCount > 1 ? (float)ptLight->uShadowResolution : 0.0f,
                        .fHeight = (float)ptLight->uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                }
            };

            gptGfx->draw_stream(ptEncoder, 1, &tArea);
        }
        else
        {
            for(uint32_t uCascade = 0; uCascade < uCascadeCount; uCascade++)
            {

                gptGfx->reset_draw_stream(ptStream, uOpaqueDrawableCount + uTransparentDrawableCount);
                gptGfx->set_depth_bias(ptEncoder, gptData->fShadowConstantDepthBias, 0.0f, gptData->fShadowSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uOpaqueDrawableCount;
                for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCascade + (int)ptDShadowData->uOffsetIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = gptData->tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uTriangleCount,
                        .uVertexOffset        = tDrawable.uStaticVertexOffset,
                        .atBindGroups = {
                            ptScene->atGlobalBindGroup[uFrameIdx],
                            tShadowBG1
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = 1
                    });
                }

                *gptData->pdDrawCalls += (double)uTransparentDrawableCount;
                for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                    plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->tModel = ptTransform->tWorld;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCascade + (int)ptDShadowData->uOffsetIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = tDrawable.tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = tDrawable.tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uTriangleCount,
                        .uVertexOffset        = tDrawable.uStaticVertexOffset,
                        .atBindGroups = {
                            ptScene->atGlobalBindGroup[uFrameIdx],
                            tShadowBG1
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = 0,
                        .uInstanceCount = uCascadeCount
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
        ptDShadowData->uOffsetIndex+=4;
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
            { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
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
            .tTexture = ptView->tRawOutputTexture,
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
    *gptData->pdDrawCalls += 1.0;
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

    plComputeShaderDesc tFilterComputeShaderDesc = {
        .tShader = gptShader->load_glsl("filter_environment.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                },
                .atBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_SHADER_STAGE_COMPUTE},
                },
                .atSamplerBindings = { {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE}}
            }
        }
    };
    gptData->tEnvFilterShader = gptGfx->create_compute_shader(gptData->ptDevice, &tFilterComputeShaderDesc);

    int aiConstantData[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

    plShaderDesc tDeferredShaderDescription = {
        .tPixelShader  = gptShader->load_glsl("primitive.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("primitive.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulWireframe          = gptData->bWireframe,
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
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                    }
                },
                .atSamplerBindings = {
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                    {.uSlot = 4100, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                }
            },
            {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 0,
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
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
        .tPixelShader = gptShader->load_glsl("transparent.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("transparent.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = gptData->bWireframe,
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
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                    }
                },
                .atSamplerBindings = {
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                    {.uSlot = 4100, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                }
            },
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 3, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 4, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 5, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 6, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    { .uSlot = 7, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                },
                .atSamplerBindings = {
                    {.uSlot = 8, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                },
            }
        }
    };
    for(uint32_t i = 0; i < 9; i++)
    {
        tForwardShaderDescription.atConstants[i].uID = i;
        tForwardShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tForwardShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tForwardShader = gptGfx->create_shader(gptData->ptDevice, &tForwardShaderDescription);


    // static const plShaderMacroDefinition tDefinition = {
    //     .pcName = "PL_MULTIPLE_VIEWPORTS",
    //     .szNameLength = 21
    // };
    // static plShaderOptions tShadowShaderOptions = {
    //     .uMacroDefinitionCount = 1,
    //     .ptMacroDefinitions = &tDefinition,
    //     .apcIncludeDirectories = {
    //         "../shaders/"
    //     },
    //     .apcDirectories = {
    //         "./",
    //         "../shaders/"
    //     },
    //     .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT
    // };
    // #ifndef PL_OFFLINE_SHADERS_ONLY
    // tShadowShaderOptions.tFlags |= PL_SHADER_FLAGS_ALWAYS_COMPILE | PL_SHADER_FLAGS_INCLUDE_DEBUG;
    // #endif

    plShaderModule tShadowPixelShader = gptShader->load_glsl("shadow.frag", "main", NULL, NULL);
    plShaderModule tVertexShader = gptShader->load_glsl("shadow.vert", "main", NULL, NULL);

    plShaderDesc tShadowShaderDescription = {
        .tPixelShader = tShadowPixelShader,
        .tVertexShader = tVertexShader,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthClampEnabled  = 1,
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
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                    }
                },
                .atSamplerBindings = {
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                    {.uSlot = 4100, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                }
            },
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
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
    tShadowShaderDescription.tGraphicsState.ulCullMode = PL_CULL_MODE_CULL_BACK;
    gptData->tShadowShader = gptGfx->create_shader(gptData->ptDevice, &tShadowShaderDescription);
        
    const plShaderDesc tPickShaderDescription = {
        .tPixelShader = gptShader->load_glsl("picking.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("picking.vert", "main", NULL, NULL),
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
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                },
            }
        }
    };
    gptData->tPickShader = gptGfx->create_shader(gptData->ptDevice, &tPickShaderDescription);

    const plShaderDesc tUVShaderDesc = {
        .tPixelShader = gptShader->load_glsl("uvmap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("uvmap.vert", "main", NULL, NULL),
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
        .tPixelShader = gptShader->load_glsl("skybox.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("skybox.vert", "main", NULL, NULL),
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
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                },
                .atSamplerBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                }
            },
            {
                .atTextureBindings = {
                    { .uSlot = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
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
    pl_sb_resize(ptSkinComponent->sbtTextureData, pl_sb_size(ptSkinComponent->sbtJoints) * 8);

    const plBufferDesc tSkinBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = pl_sb_size(ptSkinComponent->sbtJoints) * 8 * sizeof(plMat4),
        .pcDebugName = "skin buffer"
    };

    plBindGroupLayout tSkinBindGroupLayout = {
        .atBufferBindings = {
            {.uSlot =  0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_BUFFER_BINDING_TYPE_STORAGE}
        }
    };

    const plBindGroupDesc tSkinBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .ptLayout    = &tSkinBindGroupLayout,
        .pcDebugName = "skin bind group"
    };
    
    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGfx->get_frames_in_flight(); uFrameIndex++)
    {
        tSkinData.atDynamicSkinBuffer[uFrameIndex] = pl__refr_create_staging_buffer(&tSkinBufferDesc, "joint buffer", uFrameIndex);

        const plBindGroupUpdateBufferData tSkinBGBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = tSkinData.atDynamicSkinBuffer[uFrameIndex], .szBufferRange = tSkinBufferDesc.szByteSize }
        };

        const plBindGroupUpdateData tSkinBGData = {
            .uBufferCount      = 1,
            .atBufferBindings  = tSkinBGBufferData
        };
        tSkinData.atBindGroup[uFrameIndex] = gptGfx->create_bind_group(gptData->ptDevice, &tSkinBindGroupDesc);
        gptGfx->update_bind_group(gptData->ptDevice, tSkinData.atBindGroup[uFrameIndex], &tSkinBGData);
    }

    int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
    const plComputeShaderDesc tComputeShaderDesc = {
        .tShader = gptShader->load_glsl("skinning.comp", "main", NULL, NULL),
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
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
                },
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE}
                }
            },
            {
                .atBufferBindings = {
                    {.uSlot =  0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_BUFFER_BINDING_TYPE_STORAGE}
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
    const uint32_t uVertexColorCount0 = pl_sb_size(ptMesh->sbtVertexColors[0]);
    for(uint32_t i = 0; i < uVertexColorCount0; i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount0 > 0)
        uOffset += 1;

    const uint32_t uVertexColorCount1 = pl_sb_size(ptMesh->sbtVertexColors[1]);
    for(uint32_t i = 0; i < uVertexColorCount1; i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors[1][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount1 > 0)
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

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atGlobalBindGroup[i], &tGlobalBindGroupData);

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

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atGlobalBindGroup[i], &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}

static void
pl__create_probe_data(uint32_t uSceneHandle, plEntity tProbeHandle)
{
    // for convience
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    plEnvironmentProbeComponent* ptProbe = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, tProbeHandle);

    plEnvironmentProbeData tProbeData = {
        .tEntity = tProbeHandle,
        .tTargetSize = {(float)ptProbe->uResolution, (float)ptProbe->uResolution}
    };

    // create offscreen per-frame resources
    const plTextureDesc tRawOutputTextureCubeDesc = {
        .tDimensions   = {tProbeData.tTargetSize.x, tProbeData.tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 6,
        .uMips         = 0,
        .tType         = PL_TEXTURE_TYPE_CUBE,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen final cube"
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = {tProbeData.tTargetSize.x, tProbeData.tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 6,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_CUBE,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {tProbeData.tTargetSize.x, tProbeData.tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 6,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_CUBE,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "albedo texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {tProbeData.tTargetSize.x, tProbeData.tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 6,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_CUBE,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = {tProbeData.tTargetSize.x, tProbeData.tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 6,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_CUBE,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "emissive texture"
    };

    const plBufferDesc atGlobalBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "global buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGPULightShadowData),
        .pcDebugName = "shadow data buffer"
    };

    const plBindGroupLayout tLightingBindGroupLayout = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
        }
    };

    // textures
    tProbeData.tRawOutputTexture        = pl__refr_create_texture(&tRawOutputTextureCubeDesc,  "offscreen raw cube", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tAlbedoTexture           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tNormalTexture           = pl__refr_create_texture(&tNormalTextureDesc, "normal original", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tAOMetalRoughnessTexture = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tDepthTexture            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth original", 0, PL_TEXTURE_USAGE_SAMPLED);

    plTextureViewDesc tAlbedoTextureViewDesc = {
        .tFormat     = tAlbedoTextureDesc.tFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tProbeData.tAlbedoTexture,
        .tType       = PL_TEXTURE_TYPE_2D
    };

    plTextureViewDesc tNormalTextureViewDesc = {
        .tFormat     = tNormalTextureDesc.tFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tProbeData.tNormalTexture,
        .tType       = PL_TEXTURE_TYPE_2D
    };

    plTextureViewDesc tEmmissiveTexureViewDesc = {
        .tFormat     = tEmmissiveTexDesc.tFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tProbeData.tAOMetalRoughnessTexture,
        .tType       = PL_TEXTURE_TYPE_2D
    };

    plTextureViewDesc tRawOutputTextureViewDesc = {
        .tFormat     = tRawOutputTextureCubeDesc.tFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tProbeData.tRawOutputTexture,
        .tType       = PL_TEXTURE_TYPE_2D
    };

    plTextureViewDesc tDepthTextureViewDesc = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tProbeData.tDepthTexture,
        .tType       = PL_TEXTURE_TYPE_2D
    };

    // create offscreen render pass
    plRenderPassAttachments atAttachmentSets[6][PL_MAX_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        // buffers
        tProbeData.atGlobalBuffers[i] = pl__refr_create_staging_buffer(&atGlobalBuffersDesc, "global", i);
        
        for(uint32_t uFace = 0; uFace < 6; uFace++)
        {

            tAlbedoTextureViewDesc.uBaseLayer = uFace;
            tNormalTextureViewDesc.uBaseLayer = uFace;
            tEmmissiveTexureViewDesc.uBaseLayer = uFace;
            tRawOutputTextureViewDesc.uBaseLayer = uFace;
            tDepthTextureViewDesc.uBaseLayer = uFace;

            tProbeData.atAlbedoTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tAlbedoTextureViewDesc);
            tProbeData.atNormalTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tNormalTextureViewDesc);
            tProbeData.atAOMetalRoughnessTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tEmmissiveTexureViewDesc);
            tProbeData.atRawOutputTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tRawOutputTextureViewDesc);
            tProbeData.atDepthTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tDepthTextureViewDesc);

            // attachment sets
            atAttachmentSets[uFace][i].atViewAttachments[0] = tProbeData.atDepthTextureViews[uFace];
            atAttachmentSets[uFace][i].atViewAttachments[1] = tProbeData.atRawOutputTextureViews[uFace];
            atAttachmentSets[uFace][i].atViewAttachments[2] = tProbeData.atAlbedoTextureViews[uFace];
            atAttachmentSets[uFace][i].atViewAttachments[3] = tProbeData.atNormalTextureViews[uFace];
            atAttachmentSets[uFace][i].atViewAttachments[4] = tProbeData.atAOMetalRoughnessTextureViews[uFace];
        }

        tProbeData.tDirectionLightShadowData.atDLightShadowDataBuffer[i] = pl__refr_create_staging_buffer(&atLightShadowDataBufferDesc, "d shadow", i);
        tProbeData.tDirectionLightShadowData.atDShadowCameraBuffers[i] = pl__refr_create_staging_buffer(&atCameraBuffersDesc, "d shadow buffer", i);

    }

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool = gptData->ptBindGroupPool,
        .ptLayout = &tLightingBindGroupLayout,
        .pcDebugName = "lighting bind group"
    };
    
    for(uint32_t uFace = 0; uFace < 6; uFace++)
    {
        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = tProbeData.atAlbedoTextureViews[uFace],
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = tProbeData.atNormalTextureViews[uFace],
                .uSlot    = 1,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = tProbeData.atAOMetalRoughnessTextureViews[uFace],
                .uSlot    = 2,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = tProbeData.atDepthTextureViews[uFace],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 4,
            .atTextureBindings = atBGTextureData
        };
        tProbeData.atLightingBindGroup[uFace] = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);
        gptGfx->update_bind_group(gptData->ptDevice, tProbeData.atLightingBindGroup[uFace], &tBGData);
    }

    const plRenderPassDesc tRenderPassDesc = {
        .tLayout = gptData->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = tProbeData.tTargetSize.x, .y = tProbeData.tTargetSize.y}
    };

    for(uint32_t uFace = 0; uFace < 6; uFace++)
    {
        tProbeData.atRenderPasses[uFace] = gptGfx->create_render_pass(gptData->ptDevice, &tRenderPassDesc, atAttachmentSets[uFace]);
    }

    const plTextureDesc tLutTextureDesc = {
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    tProbeData.tGGXLUTTexture = pl__refr_create_texture(&tLutTextureDesc, "lut texture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

    const plTextureDesc tSpecularTextureDesc = {
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_CUBE,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    tProbeData.tLambertianEnvTexture = pl__refr_create_texture(&tSpecularTextureDesc, "specular texture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = (uint32_t)floorf(log2f((float)ptProbe->uResolution)) - 3, // guarantee final dispatch during filtering is 16 threads
        .tType       = PL_TEXTURE_TYPE_CUBE,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    tProbeData.tGGXEnvTexture = pl__refr_create_texture(&tTextureDesc, "tGGXEnvTexture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

    tProbeData.uGGXLUT = pl__get_bindless_texture_index(uSceneHandle, tProbeData.tGGXLUTTexture);
    tProbeData.uLambertianEnvSampler = pl__get_bindless_cube_texture_index(uSceneHandle, tProbeData.tLambertianEnvTexture);
    tProbeData.uGGXEnvSampler = pl__get_bindless_cube_texture_index(uSceneHandle, tProbeData.tGGXEnvTexture);

    pl_sb_push(ptScene->sbtProbeData, tProbeData);

    plObjectComponent* ptProbeObj = gptECS->add_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tProbeHandle);
    ptProbeObj->tMesh = ptScene->tProbeMesh;
    ptProbeObj->tTransform = tProbeHandle;

    plTransformComponent* ptProbeTransform = gptECS->add_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tProbeHandle);

    ptProbe = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, tProbeHandle);
    ptProbeTransform->tTranslation = ptProbe->tPosition;

    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtStagedDrawables);
    pl_sb_add(ptScene->sbtStagedDrawables);
    ptScene->sbtStagedDrawables[uTransparentStart].tEntity = tProbeHandle;
    ptScene->sbtStagedDrawables[uTransparentStart].tFlags = PL_DRAWABLE_FLAG_PROBE | PL_DRAWABLE_FLAG_FORWARD;
};

static uint64_t
pl__update_environment_probes(uint32_t uSceneHandle, uint64_t ulValue)
{

    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plDrawStream* ptStream = &gptData->tDrawStream;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptData->atCmdPools[uFrameIdx];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[uFrameIdx];

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~common data~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plVec3 atPitchYawRoll[6] = {
        { 0.0f,    PL_PI_2 },
        { 0.0f,    -PL_PI_2 },
        { PL_PI_2,    PL_PI },
        { -PL_PI_2,    PL_PI },
        { PL_PI,    0.0f, PL_PI },
        { 0.0f,    0.0f },
    };

    const plBindGroupLayout tSceneBGLayout = {
        .atBufferBindings = {
            { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 3, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 4, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 5, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 6, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 7, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
        },
        .atSamplerBindings = {
            {.uSlot = 8, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
        },
    };
    const plBindGroupDesc tSceneBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .ptLayout    = &tSceneBGLayout,
        .pcDebugName = "probe scene bg"
    };

    plBindGroupLayout tSkyboxBG1Layout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        },
        .atSamplerBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
        }
    };

    const plBindGroupDesc tSkyboxBG1Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .ptLayout    = &tSkyboxBG1Layout,
        .pcDebugName = "skybox bg 1"
    };

    const plBindGroupUpdateSamplerData tSkyboxBG1SamplerData = {
        .tSampler = gptData->tSkyboxSampler,
        .uSlot    = 1
    };

    const plBindGroupUpdateSamplerData tShadowSamplerData = {
        .tSampler = gptData->tShadowSampler,
        .uSlot    = 8
    };

    const plBindGroupLayout tGBufferFillBG1Layout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT,
            }
        }
    };

    const plBindGroupDesc tGBufferFillBG1Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .ptLayout    = &tGBufferFillBG1Layout,
        .pcDebugName = "gbuffer fille bg1"
    };

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, ptProbe->tEntity);

        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY)))
        {
            continue;
        }

        // reset probe data
        pl_sb_reset(ptProbe->tDirectionLightShadowData.sbtDLightShadowData);
        ptProbe->tDirectionLightShadowData.uOffset = 0;
        ptProbe->tDirectionLightShadowData.uOffsetIndex = 0;

        const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~probe face pre-calc~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // temporary data for probes

        // create scene bind group (camera, lights, shadows)
        const plBindGroupUpdateBufferData tSceneBGBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptProbe->atGlobalBuffers[uFrameIdx],                                    .szBufferRange = sizeof(BindGroup_0) * 6 },
            { .uSlot = 1, .tBuffer = ptScene->atDLightBuffer[uFrameIdx],                                     .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtDLightData)},
            { .uSlot = 2, .tBuffer = ptScene->atPLightBuffer[uFrameIdx],                                     .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtPLightData)},
            { .uSlot = 3, .tBuffer = ptScene->atSLightBuffer[uFrameIdx],                                     .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtSLightData)},
            { .uSlot = 4, .tBuffer = ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptProbe->tDirectionLightShadowData.sbtDLightShadowData)},
            { .uSlot = 5, .tBuffer = ptScene->atPLightShadowDataBuffer[uFrameIdx],                           .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptScene->sbtPLightShadowData)},
            { .uSlot = 6, .tBuffer = ptScene->atSLightShadowDataBuffer[uFrameIdx],                           .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptScene->sbtSLightShadowData)},
            { .uSlot = 7, .tBuffer = ptScene->atGPUProbeDataBuffers[uFrameIdx],                              .szBufferRange = sizeof(plGPUProbeData) * pl_sb_size(ptScene->sbtGPUProbeData)},
        };

        const plBindGroupUpdateData tSceneBGData = {
            .uBufferCount      = 8,
            .atBufferBindings  = tSceneBGBufferData,
            .uSamplerCount     = 1,
            .atSamplerBindings = &tShadowSamplerData
        };
        plBindGroupHandle tSceneBG = gptGfx->create_bind_group(ptDevice, &tSceneBGDesc);
        gptGfx->update_bind_group(gptData->ptDevice, tSceneBG, &tSceneBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tSceneBG);

        // create skybox bind group 1
        const plBindGroupUpdateBufferData tSkyboxBG1BufferData = {
            .tBuffer       = ptProbe->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0) * 6
        };

        const plBindGroupUpdateData tSkyboxBG1Data = {
            .uBufferCount      = 1,
            .atBufferBindings  = &tSkyboxBG1BufferData,
            .uSamplerCount     = 1,
            .atSamplerBindings = &tSkyboxBG1SamplerData,
        };

        plBindGroupHandle tSkyboxBG1 = gptGfx->create_bind_group(ptDevice, &tSkyboxBG1Desc);
        gptGfx->update_bind_group(gptData->ptDevice, tSkyboxBG1, &tSkyboxBG1Data);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tSkyboxBG1);

        plDrawArea tArea = {
            .ptDrawStream = ptStream,
            .atScissors = 
            {
                    {
                        .uWidth  = (uint32_t)ptProbeComp->uResolution,
                        .uHeight = (uint32_t)ptProbeComp->uResolution,
                    }
            },
            .atViewports =
            {
                    {
                        .fWidth  = (float)ptProbeComp->uResolution,
                        .fHeight = (float)ptProbeComp->uResolution,
                        .fMaxDepth = 1.0f
                    }
            }
        };
        

        plCameraComponent atEnvironmentCamera[6] = {0};

        for(uint32_t uFace = 0; uFace < 6; uFace++)
        {

            atEnvironmentCamera[uFace] = (plCameraComponent){
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPos         = ptProbeComp->tPosition,
                .fNearZ       = 0.26f,
                .fFarZ        = ptProbeComp->fRange,
                .fFieldOfView = PL_PI_2,
                .fAspectRatio = 1.0f,
                .fRoll        = atPitchYawRoll[uFace].z
            };
            gptCamera->set_pitch_yaw(&atEnvironmentCamera[uFace], atPitchYawRoll[uFace].x, atPitchYawRoll[uFace].y);
            gptCamera->update(&atEnvironmentCamera[uFace]);

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~cascaded shadow maps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            // TODO: these can be parallel

            const plBeginCommandInfo tBeginCSMInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {ulValue},
            };

            plCommandBuffer* ptCSMCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCSMCommandBuffer, &tBeginCSMInfo);

            plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCommandBuffer, ptScene->tShadowRenderPass, NULL);

            pl_refr_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCommandBuffer, uSceneHandle, uFace, uProbeIndex, 1, &ptProbe->tDirectionLightShadowData,  &atEnvironmentCamera[uFace]);

            gptGfx->end_render_pass(ptCSMEncoder);
            gptGfx->end_command_recording(ptCSMCommandBuffer);

            const plSubmitInfo tSubmitCSMInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {++ulValue}
            };
            gptGfx->submit_command_buffer(ptCSMCommandBuffer, &tSubmitCSMInfo);
            gptGfx->return_command_buffer(ptCSMCommandBuffer);

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~probe render prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            const BindGroup_0 tProbeBindGroupBuffer = {
                .tViewportSize         = {.x = ptProbe->tTargetSize.x, .y = ptProbe->tTargetSize.y, .z = 1.0f, .w = 1.0f},
                .tViewportInfo         = {0},
                .tCameraPos            = atEnvironmentCamera[uFace].tPos,
                .tCameraProjection     = atEnvironmentCamera[uFace].tProjMat,
                .tCameraView           = atEnvironmentCamera[uFace].tViewMat,
                .tCameraViewProjection = pl_mul_mat4(&atEnvironmentCamera[uFace].tProjMat, &atEnvironmentCamera[uFace].tViewMat)
            };

            // copy global buffer data for probe rendering
            const uint32_t uProbeGlobalBufferOffset = sizeof(BindGroup_0) * uFace;
            plBuffer* ptProbeGlobalBuffer = gptGfx->get_buffer(ptDevice, ptProbe->atGlobalBuffers[uFrameIdx]);
            memcpy(&ptProbeGlobalBuffer->tMemoryAllocation.pHostMapped[uProbeGlobalBufferOffset], &tProbeBindGroupBuffer, sizeof(BindGroup_0));
        }

        // copy probe shadow data to GPU buffer
        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx]);
        memcpy(ptDShadowDataBuffer->tMemoryAllocation.pHostMapped, ptProbe->tDirectionLightShadowData.sbtDLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptProbe->tDirectionLightShadowData.sbtDLightShadowData));

        // create g-buffer fill bind group 1
        const plBindGroupUpdateBufferData tGBufferFillBG1BufferData = {
            .tBuffer       = ptProbe->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0) * 6
        };

        const plBindGroupUpdateData tGBufferFillBG1Data = {
            .uBufferCount     = 1,
            .atBufferBindings = &tGBufferFillBG1BufferData
        };

        plBindGroupHandle tGBufferFillBG1 = gptGfx->create_bind_group(ptDevice, &tGBufferFillBG1Desc);
        gptGfx->update_bind_group(gptData->ptDevice, tGBufferFillBG1, &tGBufferFillBG1Data);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tGBufferFillBG1);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~probe rendering~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        const plBeginCommandInfo tProbeBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCmdBuffer, &tProbeBeginInfo);

        for(uint32_t uFaceIndex = 0; uFaceIndex < ptProbeComp->uInterval; uFaceIndex++)
        {

            uint32_t uFace = ptProbe->uCurrentFace + uFaceIndex;
            uFace = uFace % 6;

            //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

            plRenderEncoder* ptProbeEncoder = gptGfx->begin_render_pass(ptCmdBuffer, ptProbe->atRenderPasses[uFace], NULL);
            gptGfx->set_depth_bias(ptProbeEncoder, 0.0f, 0.0f, 0.0f);

            plCullData tCullData = {
                .ptScene      = ptScene,
                .ptCullCamera = &atEnvironmentCamera[uFace],
                .atDrawables  = ptScene->sbtDrawables
            };
            
            plJobDesc tJobDesc = {
                .task  = pl__refr_cull_job,
                .pData = &tCullData
            };

            plAtomicCounter* ptCullCounter = NULL;
            gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
            gptJob->wait_for_counter(ptCullCounter);
            pl_sb_reset(ptProbe->sbtVisibleOpaqueDrawables[uFace]);
            pl_sb_reset(ptProbe->sbtVisibleTransparentDrawables[uFace]);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                {
                    if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                        pl_sb_push(ptProbe->sbtVisibleOpaqueDrawables[uFace], tDrawable);
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                        pl_sb_push(ptProbe->sbtVisibleTransparentDrawables[uFace], tDrawable);
                }
            }

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            
            const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptProbe->sbtVisibleOpaqueDrawables[uFace]);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount);
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptProbe->sbtVisibleOpaqueDrawables[uFace][i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;
                ptDynamicData->uGlobalIndex = uFace;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = tDrawable.tEnvShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uTriangleCount,
                    .uVertexOffset        = tDrawable.uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atGlobalBindGroup[uFrameIdx],
                        tGBufferFillBG1
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount  = 1
                });
            }

            gptGfx->draw_stream(ptProbeEncoder, 1, &tArea); 
            
            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            gptGfx->next_subpass(ptProbeEncoder, NULL);

            // create lighting dynamic bind group

            typedef struct _plLightingDynamicData
            {
                uint32_t uGlobalIndex;
                uint32_t _auUnused[3];
            } plLightingDynamicData;

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
            plLightingDynamicData* ptLightingDynamicData = (plLightingDynamicData*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex          = uFace;

            
            gptGfx->reset_draw_stream(ptStream, 1);
            *gptData->pdDrawCalls += 1.0;
            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = ptScene->tEnvLightingShader,
                .auDynamicBuffers = {
                    tLightingDynamicData.uBufferHandle
                },
                .atVertexBuffers = {
                    gptData->tFullQuadVertexBuffer
                },
                .tIndexBuffer         = gptData->tFullQuadIndexBuffer,
                .uIndexOffset         = 0,
                .uTriangleCount       = 2,
                .atBindGroups = {
                    ptScene->atGlobalBindGroup[uFrameIdx],
                    ptProbe->atLightingBindGroup[uFace],
                    tSceneBG
                },
                .auDynamicBufferOffsets = {
                    tLightingDynamicData.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
            gptGfx->draw_stream(ptProbeEncoder, 1, &tArea);
            
            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 2 - forward~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            gptGfx->next_subpass(ptProbeEncoder, NULL);

            if(ptScene->tSkyboxTexture.uIndex != 0 && ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY)
            {

                plDynamicBinding tSkyboxDynamicData = pl__allocate_dynamic_data(ptDevice);
                plSkyboxDynamicData* ptSkyboxDynamicData = (plSkyboxDynamicData*)tSkyboxDynamicData.pcData;
                ptSkyboxDynamicData->tModel = pl_mat4_translate_vec3(ptProbeComp->tPosition);
                ptSkyboxDynamicData->uGlobalIndex = uFace;

                gptGfx->reset_draw_stream(ptStream, 1);
                *gptData->pdDrawCalls += 1.0;
                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = gptData->tSkyboxShader,
                    .auDynamicBuffers = {
                        tSkyboxDynamicData.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = ptScene->tIndexBuffer,
                    .uIndexOffset         = ptScene->tSkyboxDrawable.uIndexOffset,
                    .uTriangleCount       = ptScene->tSkyboxDrawable.uIndexCount / 3,
                    .atBindGroups = {
                        tSkyboxBG1,
                        ptScene->tSkyboxBindGroup
                    },
                    .auDynamicBufferOffsets = {
                        tSkyboxDynamicData.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = 1
                });
                gptGfx->draw_stream(ptProbeEncoder, 1, &tArea);
            }
            
            // transparent & complex material objects

            const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptProbe->sbtVisibleTransparentDrawables[uFace]);
            gptGfx->reset_draw_stream(ptStream, uVisibleTransparentDrawCount);
            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptProbe->sbtVisibleTransparentDrawables[uFace][i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;
                ptDynamicData->uGlobalIndex = uFace;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = tDrawable.tEnvShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = tDrawable.tIndexBuffer,
                    .uIndexOffset         = tDrawable.uIndexOffset,
                    .uTriangleCount       = tDrawable.uTriangleCount,
                    .uVertexOffset        = tDrawable.uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atGlobalBindGroup[uFrameIdx],
                        tSceneBG
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = 0,
                    .uInstanceCount = 1
                });
            }
            gptGfx->draw_stream(ptProbeEncoder, 1, &tArea);

            gptGfx->end_render_pass(ptProbeEncoder);
        }
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submission~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        
        gptGfx->end_command_recording(ptCmdBuffer);

        const plSubmitInfo tProbeSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCmdBuffer, &tProbeSubmitInfo);
        gptGfx->return_command_buffer(ptCmdBuffer);
        ulValue = pl_create_environment_map_from_texture(uSceneHandle, ptProbe, ulValue);

        ptProbe->uCurrentFace = (ptProbe->uCurrentFace + ptProbeComp->uInterval) % 6;

        if(ptProbe->uCurrentFace == 0)
            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
    }

    return ulValue;
}

static uint64_t
pl_create_environment_map_from_texture(uint32_t uSceneHandle, plEnvironmentProbeData* ptProbe, uint64_t ulValue)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[gptGfx->get_current_frame_index()];

    plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptProbe->tRawOutputTexture);
    const int iResolution = (int)(ptTexture->tDesc.tDimensions.x);
    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, ptProbe->tEntity);

    // copy to cube
    {

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        const plBeginCommandInfo tBeginInfo1 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo1);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
        gptGfx->generate_mipmaps(ptBlitEncoder, ptProbe->tRawOutputTexture);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    plBindGroupLayout tBindgroupLayout = {
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        },
        .atBufferBindings = {
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_SHADER_STAGE_COMPUTE},
        },
        .atSamplerBindings = { {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE}}
    };

    typedef struct _FilterShaderSpecData{
        int   iResolution;
        float fRoughness;
        int   iSampleCount;
        int   iWidth;
        float fLodBias;
        int   iDistribution;
        int   iIsGeneratingLut;
        int   iCurrentMipLevel;
    } FilterShaderSpecData;

    // create lut
    
    {

        const plBindGroupDesc tFilterBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tBindgroupLayout,
            .pcDebugName = "lut bind group"
        };
        plBindGroupHandle tLutBindGroup = gptGfx->create_bind_group(ptDevice, &tFilterBindGroupDesc);
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 2, .tBuffer = ptScene->atFilterWorkingBuffers[0], .szBufferRange = uFaceSize},
            { .uSlot = 3, .tBuffer = ptScene->atFilterWorkingBuffers[1], .szBufferRange = uFaceSize},
            { .uSlot = 4, .tBuffer = ptScene->atFilterWorkingBuffers[2], .szBufferRange = uFaceSize},
            { .uSlot = 5, .tBuffer = ptScene->atFilterWorkingBuffers[3], .szBufferRange = uFaceSize},
            { .uSlot = 6, .tBuffer = ptScene->atFilterWorkingBuffers[4], .szBufferRange = uFaceSize},
            { .uSlot = 7, .tBuffer = ptScene->atFilterWorkingBuffers[5], .szBufferRange = uFaceSize},
            { .uSlot = 8, .tBuffer = ptScene->atFilterWorkingBuffers[6], .szBufferRange = uFaceSize},
        };

        const plBindGroupUpdateSamplerData tSamplerData = {
            .tSampler = gptData->tSkyboxSampler,
            .uSlot = 0
        };
        const plBindGroupUpdateTextureData tTextureData = {
            .tTexture = ptProbe->tRawOutputTexture,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        };
        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBufferBindings = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(ptDevice, tLutBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLutBindGroup);

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 2,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        const plBeginCommandInfo tBeginInfo0 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo0);

        const plPassBufferResource atPassBuffers[] = {
            { .tHandle = ptScene->atFilterWorkingBuffers[0], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[1], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[2], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[3], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[4], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[5], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[6], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 7,
            .atBuffers = atPassBuffers
        };

        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        FilterShaderSpecData* ptDynamicData = (FilterShaderSpecData*)tDynamicBinding.pcData;
        ptDynamicData->iResolution = iResolution;
        ptDynamicData->fRoughness = 0.0f;
        ptDynamicData->iSampleCount = (int)ptProbeComp->uSamples;
        ptDynamicData->iWidth = iResolution;
        ptDynamicData->fLodBias = 0.0f;
        ptDynamicData->iDistribution = 1;
        ptDynamicData->iIsGeneratingLut = 1;
        ptDynamicData->iCurrentMipLevel = 0;

        plDynamicBinding tIrradianceDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        FilterShaderSpecData* ptIrrDynamicData = (FilterShaderSpecData*)tIrradianceDynamicBinding.pcData;
        ptIrrDynamicData->iResolution = iResolution;
        ptIrrDynamicData->fRoughness = 0.0f;
        ptIrrDynamicData->iSampleCount = (int)ptProbeComp->uSamples;
        ptIrrDynamicData->iWidth = iResolution;
        ptIrrDynamicData->fLodBias = 0.0f;
        ptIrrDynamicData->iDistribution = 0;
        ptIrrDynamicData->iIsGeneratingLut = 0;
        ptIrrDynamicData->iCurrentMipLevel = 0;

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptData->tEnvFilterShader, 0, 1, &tLutBindGroup, 1, &tDynamicBinding);
        gptGfx->bind_compute_shader(ptComputeEncoder, gptData->tEnvFilterShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);

        gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptData->tEnvFilterShader, 0, 1, &tLutBindGroup, 1, &tIrradianceDynamicBinding);
        gptGfx->bind_compute_shader(ptComputeEncoder, gptData->tEnvFilterShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo0 = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
        gptGfx->return_command_buffer(ptCommandBuffer);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        const plBeginCommandInfo tBeginInfo1 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo1);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        const plBufferImageCopy tBufferImageCopy0 = {
            .uImageWidth = iResolution,
            .uImageHeight = iResolution,
            .uImageDepth = 1,
            .uLayerCount = 1,
            .szBufferOffset = 0,
            .uBaseArrayLayer = 0,
        };
        gptGfx->copy_buffer_to_texture(ptBlitEncoder, ptScene->atFilterWorkingBuffers[6], ptProbe->tGGXLUTTexture, 1, &tBufferImageCopy0);

        for(uint32_t i = 0; i < 6; i++)
        {
            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth = iResolution,
                .uImageHeight = iResolution,
                .uImageDepth = 1,
                .uLayerCount    = 1,
                .szBufferOffset = 0,
                .uBaseArrayLayer = i,
            };
            gptGfx->copy_buffer_to_texture(ptBlitEncoder, ptScene->atFilterWorkingBuffers[i], ptProbe->tLambertianEnvTexture, 1, &tBufferImageCopy);
        }
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }
    
    {

        const plBindGroupUpdateSamplerData tSamplerData = {
            .tSampler = gptData->tSkyboxSampler,
            .uSlot = 0
        };
        const plBindGroupUpdateTextureData tTextureData = {
            .tTexture = ptProbe->tRawOutputTexture,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        };

        const size_t uMaxFaceSize = (size_t)iResolution * (size_t)iResolution * 4 * sizeof(float);


        const plBindGroupDesc tFilterComputeBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tBindgroupLayout,
            .pcDebugName = "lut bindgroup"
        };
        plBindGroupHandle tLutBindGroup = gptGfx->create_bind_group(ptDevice, &tFilterComputeBindGroupDesc);
        
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 2, .tBuffer = ptScene->atFilterWorkingBuffers[0], .szBufferRange = uMaxFaceSize},
            { .uSlot = 3, .tBuffer = ptScene->atFilterWorkingBuffers[1], .szBufferRange = uMaxFaceSize},
            { .uSlot = 4, .tBuffer = ptScene->atFilterWorkingBuffers[2], .szBufferRange = uMaxFaceSize},
            { .uSlot = 5, .tBuffer = ptScene->atFilterWorkingBuffers[3], .szBufferRange = uMaxFaceSize},
            { .uSlot = 6, .tBuffer = ptScene->atFilterWorkingBuffers[4], .szBufferRange = uMaxFaceSize},
            { .uSlot = 7, .tBuffer = ptScene->atFilterWorkingBuffers[5], .szBufferRange = uMaxFaceSize},
            { .uSlot = 8, .tBuffer = ptScene->atFilterWorkingBuffers[6], .szBufferRange = uMaxFaceSize}
        };

        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBufferBindings = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(ptDevice, tLutBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLutBindGroup);

        const plPassBufferResource atInnerPassBuffers[] = {
            { .tHandle = ptScene->atFilterWorkingBuffers[0], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[1], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[2], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[3], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[4], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[5], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->atFilterWorkingBuffers[6], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tInnerPassResources = {
            .uBufferCount = 7,
            .atBuffers = atInnerPassBuffers
        };

        plTexture* ptEnvTexture = gptGfx->get_texture(ptDevice, ptProbe->tGGXEnvTexture);
        for(int i = ptEnvTexture->tDesc.uMips - 1; i != -1; i--)
        {
            int currentWidth = iResolution >> i;

            if(currentWidth < 16)
                continue;

            // const size_t uCurrentFaceSize = (size_t)currentWidth * (size_t)currentWidth * 4 * sizeof(float);

            const plDispatch tDispach = {
                .uGroupCountX     = (uint32_t)currentWidth / 16,
                .uGroupCountY     = (uint32_t)currentWidth / 16,
                .uGroupCountZ     = 2,
                .uThreadPerGroupX = 16,
                .uThreadPerGroupY = 16,
                .uThreadPerGroupZ = 3
            };

            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            const plBeginCommandInfo tBeginInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {ulValue},
            };
            gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            FilterShaderSpecData* ptDynamicData = (FilterShaderSpecData*)tDynamicBinding.pcData;
            ptDynamicData->iResolution = iResolution;
            ptDynamicData->fRoughness = (float)i / (float)(ptEnvTexture->tDesc.uMips - 1);
            ptDynamicData->iSampleCount = i == 0 ? 1 : (int)ptProbeComp->uSamples;
            ptDynamicData->iWidth = currentWidth;
            ptDynamicData->fLodBias = 0.0f;
            ptDynamicData->iDistribution = 1;
            ptDynamicData->iIsGeneratingLut = 0;
            ptDynamicData->iCurrentMipLevel = i;

            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tInnerPassResources);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptData->tEnvFilterShader, 0, 1, &tLutBindGroup, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptComputeEncoder, gptData->tEnvFilterShader);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
            gptGfx->end_compute_pass(ptComputeEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {++ulValue}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
            gptGfx->return_command_buffer(ptCommandBuffer);

            ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            const plBeginCommandInfo tBeginInfo2 = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {ulValue},
            };
            gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo2);
            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

            for(uint32_t j = 0; j < 6; j++)
            {
                const plBufferImageCopy tBufferImageCopy = {
                    .uImageWidth = currentWidth,
                    .uImageHeight = currentWidth,
                    .uImageDepth = 1,
                    .uLayerCount     = 1,
                    .szBufferOffset  = 0,
                    .uBaseArrayLayer = j,
                    .uMipLevel       = i
                };
                gptGfx->copy_buffer_to_texture(ptBlitEncoder, ptScene->atFilterWorkingBuffers[j], ptProbe->tGGXEnvTexture, 1, &tBufferImageCopy);
            }
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptBlitEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo0 = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {++ulValue}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
            // gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);

        }
    }

    pl_end_cpu_sample(gptProfile, 0);
    return ulValue;
}

static void
pl__refr_set_drawable_shaders(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice = gptData->ptDevice;

    int iSceneWideRenderingFlags = 0;
    if(gptData->bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {

        plEntity tEntity = ptScene->sbtDrawables[i].tEntity;

        // get actual components
        plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        uint32_t uMaterialIndex = UINT32_MAX;

        if(pl_hm_has_key(ptScene->ptMaterialHashmap, ptMesh->tMaterial.ulData))
        {
            uMaterialIndex = (uint32_t)pl_hm_lookup(ptScene->ptMaterialHashmap, ptMesh->tMaterial.ulData);
        }
        else
        {
            PL_ASSERT(false && "material not added to scene");
        }

        ptScene->sbtDrawables[i].uMaterialIndex = uMaterialIndex;

        int iDataStride = 0;
        int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy0)
        {
            iDataStride += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        int iTextureMappingFlags = 0;
        for(uint32_t j = 0; j < PL_TEXTURE_SLOT_COUNT; j++)
        {
            if((ptMaterial->atTextureMaps[j].acName[0] != 0))
                iTextureMappingFlags |= 1 << j; 
        }

        int iObjectRenderingFlags = iSceneWideRenderingFlags;

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW)
        {
            iObjectRenderingFlags |= PL_RENDERING_FLAG_SHADOWS;
        }

        // choose shader variant
        int aiConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iObjectRenderingFlags,
            pl_sb_capacity(ptScene->sbtDLightData),
            pl_sb_capacity(ptScene->sbtPLightData),
            pl_sb_capacity(ptScene->sbtSLightData),
            pl_sb_size(ptScene->sbtProbeData),
        };

        if(ptScene->sbtDrawables[i].tFlags & PL_DRAWABLE_FLAG_DEFERRED)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER,
                .ulCullMode           = PL_CULL_MODE_CULL_BACK,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            ptScene->sbtDrawables[i].tShader = pl__get_shader_variant(uSceneHandle, gptData->tDeferredShader, &tVariant);
            aiConstantData0[4] = gptData->bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[i].tEnvShader = pl__get_shader_variant(uSceneHandle, gptData->tDeferredShader, &tVariant);
        }

        else if(ptScene->sbtDrawables[i].tFlags & PL_DRAWABLE_FLAG_FORWARD)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            ptScene->sbtDrawables[i].tShader = pl__get_shader_variant(uSceneHandle, gptData->tForwardShader, &tVariant);
            aiConstantData0[4] = gptData->bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[i].tEnvShader = pl__get_shader_variant(uSceneHandle, gptData->tForwardShader, &tVariant);

            const plShaderVariant tShadowVariant = {
                .pTempConstantData =  aiConstantData0,
                .tGraphicsState    = {
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
                }
            };
            ptScene->sbtDrawables[i].tShadowShader = pl__get_shader_variant(uSceneHandle, gptData->tAlphaShadowShader, &tShadowVariant);
        }
    }
}

static void
pl__refr_sort_drawables(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice = gptData->ptDevice;

    pl_sb_reset(ptScene->sbuShadowDeferredDrawables);
    pl_sb_reset(ptScene->sbuShadowForwardDrawables);

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {

        plEntity tEntity = ptScene->sbtDrawables[i].tEntity;

        // get actual components
        plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_CAST_SHADOW && ptObject->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW)
        {
            if(ptScene->sbtDrawables[i].tFlags & PL_DRAWABLE_FLAG_FORWARD)
                pl_sb_push(ptScene->sbuShadowForwardDrawables, i);
            else if(ptScene->sbtDrawables[i].tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                pl_sb_push(ptScene->sbuShadowDeferredDrawables, i);
        }

        ptScene->sbtDrawables[i].tIndexBuffer = ptScene->sbtDrawables[i].uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer;
    }
}

static void
pl__refr_add_skybox_drawable(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    const uint32_t uStartIndex     = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);

    const plDrawable tDrawable = {
        .uIndexCount   = 36,
        .uVertexCount  = 8,
        .uIndexOffset  = uIndexStart,
        .uVertexOffset = uStartIndex,
        .uDataOffset   = uDataStartIndex
    };
    ptScene->tSkyboxDrawable = tDrawable;

    // indices
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);

    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);

    // vertices (position)
    const float fCubeSide = 1.0f;
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide})); 
}

static void
pl__refr_unstage_drawables(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice = gptData->ptDevice;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // fill CPU buffers & drawable list

    pl_sb_reset(ptScene->sbtDrawables);
    pl_sb_reset(ptScene->sbtSkinData);

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtStagedDrawables);
    pl_sb_reserve(ptScene->sbtDrawables, uDrawableCount);
    pl_hm_resize(ptScene->ptDrawableHashmap, uDrawableCount);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {

        ptScene->sbtStagedDrawables[i].uSkinIndex = UINT32_MAX;
        plEntity tEntity = ptScene->sbtStagedDrawables[i].tEntity;

        // if(pl_hm_has_key(ptScene->ptDrawableHashmap, tEntity.ulData))
        // {
        //     pl_hm_remove(ptScene->ptDrawableHashmap, tEntity.ulData);
        // }
        pl_hm_insert(ptScene->ptDrawableHashmap, tEntity.ulData, i);
        
        // add data to global buffers
        // if(ptScene->sbtStagedDrawables[i].uVertexCount == 0) // first time
        {
            pl__add_drawable_data_to_global_buffer(ptScene, i, ptScene->sbtStagedDrawables);
            pl__add_drawable_skin_data_to_global_buffer(ptScene, i, ptScene->sbtStagedDrawables);
        }

        ptScene->sbtStagedDrawables[i].uTriangleCount = ptScene->sbtStagedDrawables[i].uIndexCount == 0 ? ptScene->sbtStagedDrawables[i].uVertexCount / 3 : ptScene->sbtStagedDrawables[i].uIndexCount / 3;
        ptScene->sbtStagedDrawables[i].uStaticVertexOffset = ptScene->sbtStagedDrawables[i].uIndexCount == 0 ? ptScene->sbtStagedDrawables[i].uVertexOffset : 0;
        ptScene->sbtStagedDrawables[i].uDynamicVertexOffset = ptScene->sbtStagedDrawables[i].uIndexCount == 0 ? 0 : ptScene->sbtStagedDrawables[i].uVertexOffset;
        pl_sb_push(ptScene->sbtDrawables, ptScene->sbtStagedDrawables[i]);
    }

    if(ptScene->tSkyboxTexture.uIndex != 0)
    {
        pl__refr_add_skybox_drawable(uSceneHandle);
    }

    const plBufferDesc tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX,
        .szByteSize = pl_max(sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer), 1024),
        .pcDebugName = "index buffer"
    };
    
    const plBufferDesc tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer),
        .pcDebugName = "vertex buffer"
    };
     
    const plBufferDesc tStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer),
        .pcDebugName = "storage buffer"
    };

    const plBufferDesc tSkinStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtSkinVertexDataBuffer),
        .pcDebugName = "skin buffer"
    };

    if(ptScene->tIndexBuffer.uData != UINT32_MAX) gptGfx->queue_buffer_for_deletion(ptDevice, ptScene->tIndexBuffer);
    if(ptScene->tVertexBuffer.uData != UINT32_MAX) gptGfx->queue_buffer_for_deletion(ptDevice, ptScene->tVertexBuffer);
    if(ptScene->tStorageBuffer.uData != UINT32_MAX)
    {
        gptGfx->queue_buffer_for_deletion(ptDevice, ptScene->tStorageBuffer);
    }
    if(ptScene->tSkinStorageBuffer.uData != UINT32_MAX)
    {
        gptGfx->queue_buffer_for_deletion(ptDevice, ptScene->tSkinStorageBuffer);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptScene->tSkinBindGroup0);
        ptScene->tSkinStorageBuffer.uData = UINT32_MAX;
    }

    ptScene->tIndexBuffer   = pl__refr_create_local_buffer(&tIndexBufferDesc,   "index", uSceneHandle, ptScene->sbuIndexBuffer);
    ptScene->tVertexBuffer  = pl__refr_create_local_buffer(&tVertexBufferDesc,  "vertex", uSceneHandle, ptScene->sbtVertexPosBuffer);
    ptScene->tStorageBuffer = pl__refr_create_local_buffer(&tStorageBufferDesc, "storage", uSceneHandle, ptScene->sbtVertexDataBuffer);

    if(tSkinStorageBufferDesc.szByteSize > 0)
    {
        ptScene->tSkinStorageBuffer  = pl__refr_create_local_buffer(&tSkinStorageBufferDesc, "skin storage", uSceneHandle, ptScene->sbtSkinVertexDataBuffer);

        const plBindGroupLayout tSkinBindGroupLayout0 = {
            .atSamplerBindings = {
                {.uSlot =  3, .tStages = PL_SHADER_STAGE_COMPUTE}
            },
            .atBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_COMPUTE},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_COMPUTE},
                { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_COMPUTE},
            }
        };
        const plBindGroupDesc tSkinBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tSkinBindGroupLayout0,
            .pcDebugName = "skin bind group"
        };
        ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(ptDevice, &tSkinBindGroupDesc);

        const plBindGroupUpdateSamplerData atSamplerData[] = {
            { .uSlot = 3, .tSampler = gptData->tDefaultSampler}
        };
        const plBindGroupUpdateBufferData atBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptScene->tSkinStorageBuffer, .szBufferRange = tSkinStorageBufferDesc.szByteSize},
            { .uSlot = 1, .tBuffer = ptScene->tVertexBuffer,      .szBufferRange = tVertexBufferDesc.szByteSize},
            { .uSlot = 2, .tBuffer = ptScene->tStorageBuffer,     .szBufferRange = tStorageBufferDesc.szByteSize}

        };
        plBindGroupUpdateData tBGData0 = {
            .uBufferCount = 3,
            .atBufferBindings = atBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = atSamplerData,
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkinBindGroup0, &tBGData0);
    }

    plBuffer* ptStorageBuffer = gptGfx->get_buffer(ptDevice, ptScene->tStorageBuffer);


    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateBufferData atGlobalBufferData[] = 
        {
            {
                .tBuffer       = ptScene->tStorageBuffer,
                .uSlot         = 0,
                .szBufferRange = ptStorageBuffer->tDesc.szByteSize
            }
        };

        plBindGroupUpdateData tGlobalBindGroupData = {
            .uBufferCount = 1,
            .atBufferBindings = atGlobalBufferData,
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atGlobalBindGroup[i], &tGlobalBindGroupData);
    }

    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);
    pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
}