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
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] job system tasks
//-----------------------------------------------------------------------------

static void
pl__renderer_cull_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plCullData* ptCullData = pData;
    plScene* ptScene = ptCullData->ptScene;
    plDrawable tDrawable = ptCullData->atDrawables[tInvoData.uGlobalIndex];
    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
    ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = true;

    if(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE)
    {
        if(ptCullData->atDrawables[tInvoData.uGlobalIndex].uInstanceCount == 1) // ignore instanced
        {
            if(pl__renderer_sat_visibility_test(ptCullData->ptCullCamera, &ptObject->tAABB))
            {
                ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = false;
            }
        }
        else
        {
            ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = false;
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] resource creation helpers
//---------------------------7--------------------------------------------------

static plTextureHandle
pl__renderer_create_local_texture(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
 
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

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "load texture");
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
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
pl__renderer_create_texture(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
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

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "create texture");
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
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
pl__renderer_create_texture_with_data(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
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

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "create texture 2");
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, PL_TEXTURE_USAGE_SAMPLED, 0);


    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        // create staging buffer
        const plBufferDesc tStagingBufferDesc = {
            .tUsage      = PL_BUFFER_USAGE_STAGING,
            .szByteSize  = szSize,
            .pcDebugName = "temp staging buffer"
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory for the vertex buffer
        const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
            ptBuffer->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT,
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            "temp staging memory");

        gptGfx->bind_buffer_to_memory(ptDevice, tStagingBuffer, &tStagingBufferAllocation);
        memcpy(ptBuffer->tMemoryAllocation.pHostMapped, pData, szSize);
        
        const plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptDesc->tDimensions.x,
            .uImageHeight = (uint32_t)ptDesc->tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(ptBlitEncoder, tStagingBuffer, tHandle, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(ptBlitEncoder, tHandle);
        gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);
    }

    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
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
pl__renderer_create_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);

    plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptBuffer->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
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
pl__renderer_create_cached_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
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
pl__renderer_create_local_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    
    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptBuffer->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
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

        plBufferHandle tStagingBuffer = gptData->atStagingBufferHandle[gptGfx->get_current_frame_index()].tStagingBufferHandle;

        if(!gptGfx->is_buffer_valid(ptDevice, tStagingBuffer))
        {
            const plBufferDesc tStagingBufferDesc = {
                .tUsage      = PL_BUFFER_USAGE_STAGING,
                .szByteSize  = 268435456,
                .pcDebugName = "Renderer Staging Buffer"
            };

            gptData->atStagingBufferHandle[gptGfx->get_current_frame_index()].tStagingBufferHandle = pl__renderer_create_staging_buffer(&tStagingBufferDesc, "staging", gptGfx->get_current_frame_index());
            tStagingBuffer = gptData->atStagingBufferHandle[gptGfx->get_current_frame_index()].tStagingBufferHandle;
            gptData->atStagingBufferHandle[gptGfx->get_current_frame_index()].szOffset = 0;
            gptData->atStagingBufferHandle[gptGfx->get_current_frame_index()].szSize = tStagingBufferDesc.szByteSize;
        }
        gptData->atStagingBufferHandle[gptGfx->get_current_frame_index()].dLastTimeActive = gptIO->dTime;

        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, tStagingBuffer);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, szSize);

        // begin recording
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "create buffer");
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        
        // begin blit pass, copy buffer, end pass
        plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        gptGfx->copy_buffer(ptEncoder, tStagingBuffer, tHandle, 0, 0, szSize);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
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
pl__renderer_sat_visibility_test(plCamera* ptCamera, const plAABB* ptAABB)
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
pl__renderer_perform_skinning(plCommandBuffer* ptCommandBuffer, plScene* ptScene)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;

    // update skin textures
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);

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
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);
        const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
        for(uint32_t i = 0; i < uSkinCount; i++)
        {
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plGpuDynSkinData* ptDynamicData = (plGpuDynSkinData*)tDynamicBinding.pcData;
            ptDynamicData->iSourceDataOffset = ptScene->sbtSkinData[i].iSourceDataOffset;
            ptDynamicData->iDestDataOffset = ptScene->sbtSkinData[i].iDestDataOffset;
            ptDynamicData->iDestVertexOffset = ptScene->sbtSkinData[i].iDestVertexOffset;
            ptDynamicData->uMaxSize = ptScene->sbtSkinData[i].uVertexCount;

            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtSkinData[i].tObjectEntity);
            plTransformComponent* ptObjectTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            ptDynamicData->tInverseWorld = pl_mat4_invert(&ptObjectTransform->tWorld);

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
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
    }
    pl_end_cpu_sample(gptProfile, 0);
}

static bool
pl__renderer_pack_shadow_atlas(plScene* ptScene)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    
    uint32_t uViewCount = pl_sb_size(ptScene->sbptViews);
    pl_sb_reset(ptScene->sbtShadowRects);

    plEnvironmentProbeComponent* ptProbes = NULL;
    plLightComponent* ptLights = NULL;

    const uint32_t uProbeCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, (void**)&ptProbes, NULL);
    const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);

    // for packing, using rect id as (16 bits) + (3 bits) + (11 bits) + (2bit) for light, view, probe, mode

    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = &ptLights[uLightIndex];

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
pl__renderer_generate_shadow_maps(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, plScene* ptScene)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    int aiConstantData[7] = {0, 0, 0, 0, 0, 0, 0};
    plShaderHandle tShadowShader = gptShaderVariant->get_shader("shadow", NULL, aiConstantData, aiConstantData, &gptData->tDepthRenderPassLayout);

    plLightComponent* ptLights = NULL;
    gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);
    const uint32_t uLightCount = pl_sb_size(ptScene->sbtShadowRects);
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 19) & ~(~0u << 12);
        int iMode = ptRect->iId >> 31;

        const plLightComponent* ptLight = &ptLights[iLight];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {
            pl_sb_add(ptScene->sbtLightShadowData);

            plGpuLightShadow* ptShadowData = &ptScene->sbtLightShadowData[pl_sb_size(ptScene->sbtLightShadowData) - 1];
            ptShadowData->iShadowMapTexIdx = ptScene->atShadowTextureBindlessIndices;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 atCamViewProjs[6] = {0};

            plCamera tShadowCamera = {
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

            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[ptScene->uShadowOffset], atCamViewProjs, sizeof(plMat4) * 6);
            ptScene->uShadowOffset += sizeof(plMat4) * 6;
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {
            pl_sb_add(ptScene->sbtLightShadowData);

            plGpuLightShadow* ptShadowData = &ptScene->sbtLightShadowData[pl_sb_size(ptScene->sbtLightShadowData) - 1];
            ptShadowData->iShadowMapTexIdx = ptScene->atShadowTextureBindlessIndices;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 tCamViewProjs = {0};

            plCamera tShadowCamera = {
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
            
            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[ptScene->uShadowOffset], &tCamViewProjs, sizeof(plMat4));
            ptScene->uShadowOffset += sizeof(plMat4);
        }    
    }

    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptData->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };
    plBindGroupHandle tGlobalBG0 = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);

    const plBindGroupUpdateBufferData atBufferData0[] = 
    {
        {
            .tBuffer       = ptScene->atShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = ptScene->uShadowOffset
        },
        {
            .tBuffer       = ptScene->atInstanceBuffer[uFrameIdx],
            .uSlot         = 1,
            .szBufferRange = sizeof(uint32_t) * 2 * 10000
        }
    };

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 2,
        .atBufferBindings = atBufferData0
    };

    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG0, &tBGData0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG0);

    const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbuShadowDeferredDrawables);
    const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbuShadowForwardDrawables);
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    uint32_t uCameraBufferIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 19) & ~(~0u << 12);
        int iMode = ptRect->iId >> 31;

        const plLightComponent* ptLight = &ptLights[iLight];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {

            if(gptData->tRuntimeOptions.bMultiViewportShadows)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptEncoder, gptData->tRuntimeOptions.fShadowConstantDepthBias, 0.0f, gptData->tRuntimeOptions.fShadowSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];

                    if(tDrawable.uInstanceCount != 0)
                    {

                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCameraBufferIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader         = tShadowShader,
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
                                ptScene->atBindGroups[uFrameIdx],
                                tGlobalBG0
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = tDrawable.uInstanceIndex,
                            .uInstanceCount = 6 * tDrawable.uInstanceCount
                        });
                    }
                }

                *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];

                    if(tDrawable.uInstanceCount != 0)
                    {

                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
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
                            .tIndexBuffer         = tDrawable.tIndexBuffer,
                            .uIndexOffset         = tDrawable.uIndexOffset,
                            .uTriangleCount       = tDrawable.uTriangleCount,
                            .uVertexOffset        = tDrawable.uStaticVertexOffset,
                            .atBindGroups = {
                                ptScene->atBindGroups[uFrameIdx],
                                tGlobalBG0
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = tDrawable.uInstanceIndex,
                            .uInstanceCount = 6 * tDrawable.uInstanceCount
                        });
                    }
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
                    gptGfx->set_depth_bias(ptEncoder, gptData->tRuntimeOptions.fShadowConstantDepthBias, 0.0f, gptData->tRuntimeOptions.fShadowSlopeDepthBias);
                    *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                    for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];

                        if(tDrawable.uInstanceCount != 0)
                        {
                            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                            
                            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                            plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                            ptDynamicData->iIndex = (int)uCameraBufferIndex + uFaceIndex;

                            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                            {
                                .tShader         = tShadowShader,
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
                                    ptScene->atBindGroups[uFrameIdx],
                                    tGlobalBG0
                                },
                                .auDynamicBufferOffsets = {
                                    tDynamicBinding.uByteOffset
                                },
                                .uInstanceOffset = tDrawable.uInstanceIndex,
                                .uInstanceCount = tDrawable.uInstanceCount
                            });
                        }
                    }

                    *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                    for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                        if(tDrawable.uInstanceCount != 0)
                        {
                            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                            
                            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                            plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
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
                                .tIndexBuffer         = tDrawable.tIndexBuffer,
                                .uIndexOffset         = tDrawable.uIndexOffset,
                                .uTriangleCount       = tDrawable.uTriangleCount,
                                .uVertexOffset        = tDrawable.uStaticVertexOffset,
                                .atBindGroups = {
                                    ptScene->atBindGroups[uFrameIdx],
                                    tGlobalBG0
                                },
                                .auDynamicBufferOffsets = {
                                    tDynamicBinding.uByteOffset
                                },
                                .uInstanceOffset = tDrawable.uInstanceIndex,
                                .uInstanceCount = tDrawable.uInstanceCount
                            });
                        }

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

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {

            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptEncoder, gptData->tRuntimeOptions.fShadowConstantDepthBias, 0.0f, gptData->tRuntimeOptions.fShadowSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];
                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = tShadowShader,
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
                            ptScene->atBindGroups[uFrameIdx],
                            tGlobalBG0
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = tDrawable.uInstanceIndex,
                        .uInstanceCount = tDrawable.uInstanceCount
                    });
                }
            }

            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
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
                        .tIndexBuffer         = tDrawable.tIndexBuffer,
                        .uIndexOffset         = tDrawable.uIndexOffset,
                        .uTriangleCount       = tDrawable.uTriangleCount,
                        .uVertexOffset        = tDrawable.uStaticVertexOffset,
                        .atBindGroups = {
                            ptScene->atBindGroups[uFrameIdx],
                            tGlobalBG0
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = tDrawable.uInstanceIndex,
                        .uInstanceCount = tDrawable.uInstanceCount
                    });
                }
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
            uCameraBufferIndex++;
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl__renderer_generate_cascaded_shadow_map(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, plScene* ptScene, uint32_t uViewHandle, uint32_t uProbeIndex, int iRequestMode, plDirectionLightShadowData* ptDShadowData, plCamera* ptSceneCamera)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    int aiConstantData[7] = {0, 0, 0, 0, 0, 0, 0};
    plShaderHandle tShadowShader = gptShaderVariant->get_shader("shadow", NULL, aiConstantData, aiConstantData, &gptData->tDepthRenderPassLayout);

    uint32_t* puOffset = &ptScene->uDShadowOffset;
    uint32_t* puOffsetIndex = &ptScene->uDShadowIndex;

    if(iRequestMode == 1)
    {
        puOffset = &ptDShadowData->uOffset;
        puOffsetIndex = &ptDShadowData->uOffsetIndex;
    }

    const uint32_t uInitialOffset = *puOffset;

    plLightComponent* ptLights = NULL;
    const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);

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

        const plLightComponent* ptLight = &ptLights[iLight];

        // check if light applies
        if(ptLight->tType != PL_LIGHT_TYPE_DIRECTIONAL || iView != (int)uViewHandle || iMode != iRequestMode || iProbe != (int)uProbeIndex)
        {
            continue;
        }

        const uint32_t uDataOffset = pl_sb_size(ptDShadowData->sbtDLightShadowData);
        pl_sb_add(ptDShadowData->sbtDLightShadowData);

        // copy GPU light shadow data
        plGpuLightShadow* ptShadowData = &ptDShadowData->sbtDLightShadowData[uDataOffset];
        ptShadowData->iShadowMapTexIdx    = ptScene->atShadowTextureBindlessIndices;
        ptShadowData->fFactor             = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fXOffset            = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fYOffset            = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->cascadeSplits.d[0] = 10000.0f;
        ptShadowData->cascadeSplits.d[1] = 10000.0f;
        ptShadowData->cascadeSplits.d[2] = 10000.0f;
        ptShadowData->cascadeSplits.d[3] = 10000.0f;

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
            ptShadowData->cascadeSplits.d[uCascade] = fSplitDist;

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
            plCamera tShadowCamera = {
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
        memcpy(&pcBufferStart[*puOffset], atCamViewProjs, sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);
        *puOffset += sizeof(plMat4) * PL_MAX_SHADOW_CASCADES;
    }

    plBindGroupLayoutDesc tBindGroupLayout0 = {
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
        }
    };
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptData->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };
    
    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptDShadowData->atDShadowCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = *puOffset
        },
        {
            .tBuffer       = ptScene->atInstanceBuffer[uFrameIdx],
            .uSlot         = 1,
            .szBufferRange = sizeof(uint32_t) * 2 * 10000
        }
    };

    const uint32_t uIndexingOffset = uInitialOffset / (sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 2,
        .atBufferBindings = atBufferData
    };
    plBindGroupHandle tShadowBG1 = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);
    gptGfx->update_bind_group(gptData->ptDevice, tShadowBG1, &tBGData0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tShadowBG1);

    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbuShadowDeferredDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbuShadowForwardDrawables);

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uRectIndex];

        // decode rect info
        int iLight = ptRect->iId & ~(~0u << 16);
        int iView = (ptRect->iId >> 16) & ~(~0u << 3);
        int iProbe = (ptRect->iId >> 18) & ~(~0u << 11);
        int iMode = ptRect->iId >> 30;

        const plLightComponent* ptLight = &ptLights[iLight];

        // check if light applies
        if(ptLight->tType != PL_LIGHT_TYPE_DIRECTIONAL || iView != (int)uViewHandle || iMode != iRequestMode || iProbe != (int)uProbeIndex)
        {
            continue;
        }

        const uint32_t uCascadeCount = iMode == 0 ? ptLight->uCascadeCount : 1; // probe only needs single cascade

        if(gptData->tRuntimeOptions.bMultiViewportShadows)
        {
            gptGfx->reset_draw_stream(ptStream, uOpaqueDrawableCount + uTransparentDrawableCount);
            gptGfx->set_depth_bias(ptEncoder, gptData->tRuntimeOptions.fShadowConstantDepthBias, 0.0f, gptData->tRuntimeOptions.fShadowSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uOpaqueDrawableCount;
            for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];

                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)*puOffsetIndex + (int)uIndexingOffset;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = tShadowShader,
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
                            ptScene->atBindGroups[uFrameIdx],
                            tShadowBG1
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = tDrawable.uInstanceIndex,
                        .uInstanceCount = uCascadeCount * tDrawable.uInstanceCount
                    });
                }
            }

            *gptData->pdDrawCalls += (double)uTransparentDrawableCount;
            for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];
                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)*puOffsetIndex + (int)uIndexingOffset;

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
                            ptScene->atBindGroups[uFrameIdx],
                            tShadowBG1
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = tDrawable.uInstanceIndex,
                        .uInstanceCount = uCascadeCount * tDrawable.uInstanceCount
                    });
                }

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
                gptGfx->set_depth_bias(ptEncoder, gptData->tRuntimeOptions.fShadowConstantDepthBias, 0.0f, gptData->tRuntimeOptions.fShadowSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uOpaqueDrawableCount;
                for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowDeferredDrawables[i]];

                    if(tDrawable.uInstanceCount != 0)
                    {
                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCascade + (int)*puOffsetIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader         = tShadowShader,
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
                                ptScene->atBindGroups[uFrameIdx],
                                tShadowBG1
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = tDrawable.uInstanceIndex,
                            .uInstanceCount = tDrawable.uInstanceCount
                        });
                    }
                }

                *gptData->pdDrawCalls += (double)uTransparentDrawableCount;
                for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuShadowForwardDrawables[i]];

                    if(tDrawable.uInstanceCount != 0)
                    {
                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCascade + (int)*puOffsetIndex;

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
                                ptScene->atBindGroups[uFrameIdx],
                                tShadowBG1
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = tDrawable.uInstanceIndex,
                            .uInstanceCount = uCascadeCount * tDrawable.uInstanceCount
                        });
                    }

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
        *puOffsetIndex += 4;
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl__renderer_post_process_scene(plCommandBuffer* ptCommandBuffer, plView* ptView, const plMat4* ptMVP)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plScene*      ptScene    = ptView->ptParentScene;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tPostProcessRenderPass)->tDesc.tDimensions;

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tPostProcessRenderPass, NULL);

    plDrawIndex tDraw = {
        .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
        .uIndexCount    = 6,
        .uInstanceCount = 1,
    };

    // create bind groups
    const plBindGroupDesc tOutlineBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("jumpfloodalgo2", 1),
        .pcDebugName = "temp bind group 0"
    };
    plBindGroupHandle tJFABindGroup0 = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);

    const plBindGroupUpdateSamplerData tOutlineSamplerData = {
        .tSampler = gptData->tSamplerLinearRepeat,
        .uSlot = 0
    };

    // update bind group (actually point descriptors to GPU resources)
    const plBindGroupUpdateTextureData tOutlineTextureData[] = 
    {
        {
            .tTexture = ptView->tRawOutputTexture,
            .uSlot    = 0,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptView->tLastUVMask,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED,
            .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        },
    };

    const plBindGroupUpdateData tJFABGData = {
        .uTextureCount = 2,
        .atTextureBindings = tOutlineTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tJFABindGroup0, &tJFABGData);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tJFABindGroup0);

    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

    plGpuDynPost* ptDynamicData = (plGpuDynPost*)tDynamicBinding.pcData;
    const plVec4 tOutlineColor = (plVec4){(float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 0.0f, 1.0f};
    ptDynamicData->fTargetWidth = (float)gptData->tRuntimeOptions.uOutlineWidth * tOutlineColor.r + 1.0f;
    ptDynamicData->tOutlineColor = tOutlineColor;

    plShaderHandle tTonemapShader = gptShaderVariant->get_shader("jumpfloodalgo2", NULL, NULL, NULL, &gptData->tPostProcessRenderPassLayout);
    gptGfx->bind_shader(ptEncoder, tTonemapShader);
    gptGfx->bind_vertex_buffer(ptEncoder, gptData->tFullQuadVertexBuffer);
    plBindGroupHandle atBindGroups[] = {ptScene->atBindGroups[uFrameIdx], tJFABindGroup0};
    gptGfx->bind_graphics_bind_groups(ptEncoder, tTonemapShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    gptDrawBackend->submit_3d_drawlist(ptView->pt3DGizmoDrawList, ptEncoder, tDimensions.x, tDimensions.y, ptMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);

    gptGfx->end_render_pass(ptEncoder);
    pl_end_cpu_sample(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] misc.
//-----------------------------------------------------------------------------

static void
pl__renderer_add_drawable_skin_data_to_global_buffers(plScene* ptScene, uint32_t uDrawableIndex)
{
    plEntity tEntity = ptScene->sbtDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent* ptObject   = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
    plMeshComponent*   ptMesh     = gptECS->get_component(ptScene->ptComponentLibrary, gptMesh->get_ecs_type_key_mesh(), ptObject->tMesh);

    if(ptMesh->tSkinComponent.uIndex == UINT32_MAX)
        return;

    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtSkinVertexDataBuffer);
    const uint32_t uVertexCount          = (uint32_t)ptMesh->szVertexCount;

    // stride within storage buffer
    uint32_t uStride = 0;

    uint64_t ulVertexStreamMask = 0;

    // calculate vertex stream mask based on provided data
    if(ptMesh->ptVertexPositions)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_POSITION; }
    if(ptMesh->ptVertexNormals)    { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(ptMesh->ptVertexTangents)   { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(ptMesh->ptVertexWeights[0]) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(ptMesh->ptVertexWeights[1]) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(ptMesh->ptVertexJoints[0])  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(ptMesh->ptVertexJoints[1])  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

    pl_sb_add_n(ptScene->sbtSkinVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // positions
    const uint32_t uVertexPositionCount = uVertexCount;
    for(uint32_t i = 0; i < uVertexPositionCount; i++)
    {
        const plVec3* ptPosition = &ptMesh->ptVertexPositions[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptPosition->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptPosition->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptPosition->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 1.0f;
    }

    if(uVertexPositionCount > 0)
        uOffset += 1;

    // normals
    const uint32_t uVertexNormalCount = ptMesh->ptVertexNormals ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->ptVertexNormals[i] = pl_norm_vec3(ptMesh->ptVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->ptVertexNormals[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptNormal->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptNormal->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptNormal->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = ptMesh->ptVertexTangents ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->ptVertexTangents[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // joints 0
    const uint32_t uVertexJoint0Count = ptMesh->ptVertexJoints[0] ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexJoint0Count; i++)
    {
        const plVec4* ptJoint = &ptMesh->ptVertexJoints[0][i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptJoint->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptJoint->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptJoint->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptJoint->w;
    }

    if(uVertexJoint0Count > 0)
        uOffset += 1;

    // weights 0
    const uint32_t uVertexWeights0Count = ptMesh->ptVertexWeights[0] ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexWeights0Count; i++)
    {
        const plVec4* ptWeight = &ptMesh->ptVertexWeights[0][i];
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
    if(ptMesh->ptVertexNormals)               { uDestStride += 1; }
    if(ptMesh->ptVertexTangents)              { uDestStride += 1; }
    if(ptMesh->ptVertexColors[0])             { uDestStride += 1; }
    if(ptMesh->ptVertexColors[1])             { uDestStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[0]) { uDestStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[2]) { uDestStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[4]) { uDestStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[6]) { uDestStride += 1; }

    plSkinData tSkinData = {
        .tEntity = ptMesh->tSkinComponent,
        .uVertexCount = uVertexCount,
        .iSourceDataOffset = uVertexDataStartIndex,
        .iDestDataOffset = ptScene->sbtDrawables[uDrawableIndex].uDataOffset,
        .iDestVertexOffset = ptScene->sbtDrawables[uDrawableIndex].uVertexOffset,
    };

    plSkinComponent* ptSkinComponent = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tSkinComponentType, ptMesh->tSkinComponent);
    pl_sb_resize(ptSkinComponent->sbtTextureData, pl_sb_size(ptSkinComponent->sbtJoints) * 8);

    const plBufferDesc tSkinBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = pl_sb_size(ptSkinComponent->sbtJoints) * 8 * sizeof(plMat4),
        .pcDebugName = "skin buffer"
    };

    int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
    tSkinData.tShader = gptShaderVariant->get_compute_shader("skinning", aiSpecializationData);

    const plBindGroupDesc tSkinBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("skinning", 1),
        .pcDebugName = "skin bind group"
    };
    
    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGfx->get_frames_in_flight(); uFrameIndex++)
    {
        tSkinData.atDynamicSkinBuffer[uFrameIndex] = pl__renderer_create_staging_buffer(&tSkinBufferDesc, "joint buffer", uFrameIndex);

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


    tSkinData.tObjectEntity = tEntity;
    pl_temp_allocator_reset(&gptData->tTempAllocator);
    ptScene->sbtDrawables[uDrawableIndex].uSkinIndex = pl_sb_size(ptScene->sbtSkinData);
    pl_sb_push(ptScene->sbtSkinData, tSkinData);
}

static void
pl__renderer_add_drawable_data_to_global_buffer(plScene* ptScene, uint32_t uDrawableIndex)
{

    plEntity tEntity = ptScene->sbtDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent* ptObject   = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
    plMeshComponent*   ptMesh     = gptECS->get_component(ptScene->ptComponentLibrary, gptMesh->get_ecs_type_key_mesh(), ptObject->tMesh);

    const uint32_t uVertexPosStartIndex  = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexBufferStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);
    const uint32_t uIndexCount           = (uint32_t)ptMesh->szIndexCount;
    const uint32_t uVertexCount          = (uint32_t)ptMesh->szVertexCount;

    // add index buffer data
    pl_sb_add_n(ptScene->sbuIndexBuffer, uIndexCount);
    for(uint32_t j = 0; j < uIndexCount; j++)
        ptScene->sbuIndexBuffer[uIndexBufferStart + j] = uVertexPosStartIndex + ptMesh->puIndices[j];

    // add vertex position data
    pl_sb_add_n(ptScene->sbtVertexPosBuffer, uVertexCount);
    memcpy(&ptScene->sbtVertexPosBuffer[uVertexPosStartIndex], ptMesh->ptVertexPositions, sizeof(plVec3) * uVertexCount);

    // stride within storage buffer
    uint32_t uStride = 0;

    // calculate vertex stream mask based on provided data
    if(ptMesh->ptVertexNormals)               { uStride += 1; }
    if(ptMesh->ptVertexTangents)              { uStride += 1; }
    if(ptMesh->ptVertexColors[0])             { uStride += 1; }
    if(ptMesh->ptVertexColors[1])             { uStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[0]) { uStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[2]) { uStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[4]) { uStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[6]) { uStride += 1; }

    ptMesh->ulVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_JOINTS_0;
    ptMesh->ulVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_JOINTS_1;
    ptMesh->ulVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0;
    ptMesh->ulVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1;

    pl_sb_add_n(ptScene->sbtVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // normals
    const uint32_t uVertexNormalCount = ptMesh->ptVertexNormals ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->ptVertexNormals[i] = pl_norm_vec3(ptMesh->ptVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->ptVertexNormals[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptNormal->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptNormal->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptNormal->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = ptMesh->ptVertexTangents ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->ptVertexTangents[i];
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
        const uint32_t uVertexTexCount0 = ptMesh->ptVertexTextureCoordinates[i] ? uVertexCount : 0;
        const uint32_t uVertexTexCount1 = ptMesh->ptVertexTextureCoordinates[i + 1] ? uVertexCount : 0;

        if(uVertexTexCount1 > 0)
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates0 = &(ptMesh->ptVertexTextureCoordinates[i])[j];
                const plVec2* ptTextureCoordinates1 = &(ptMesh->ptVertexTextureCoordinates[i + 1])[j];
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
                const plVec2* ptTextureCoordinates = &(ptMesh->ptVertexTextureCoordinates[i])[j];
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
    const uint32_t uVertexColorCount0 = ptMesh->ptVertexColors[0] ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexColorCount0; i++)
    {
        const plVec4* ptColor = &ptMesh->ptVertexColors[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount0 > 0)
        uOffset += 1;

    const uint32_t uVertexColorCount1 = ptMesh->ptVertexColors[1] ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexColorCount1; i++)
    {
        const plVec4* ptColor = &ptMesh->ptVertexColors[1][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount1 > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    ptScene->sbtDrawables[uDrawableIndex].uIndexCount      = uIndexCount;
    ptScene->sbtDrawables[uDrawableIndex].uVertexCount     = uVertexCount;
    ptScene->sbtDrawables[uDrawableIndex].uIndexOffset     = uIndexBufferStart;
    ptScene->sbtDrawables[uDrawableIndex].uVertexOffset    = uVertexPosStartIndex;
    ptScene->sbtDrawables[uDrawableIndex].uDataOffset      = uVertexDataStartIndex;
}

static plBlendState
pl__renderer_get_blend_state(plBlendMode tBlendMode)
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

static uint32_t
pl__renderer_get_bindless_texture_index(plScene* ptScene, plTextureHandle tTexture)
{

    uint64_t uIndex = 0;
    if(pl_hm_has_key_ex(&ptScene->tTextureIndexHashmap, tTexture.uData, &uIndex))
        return (uint32_t)uIndex;

    uint64_t ulValue = pl_hm_get_free_index(&ptScene->tTextureIndexHashmap);
    if(ulValue == PL_DS_HASH_INVALID)
    {
        ulValue = ptScene->uTextureIndexCount++;

        // TODO: handle when greater than 4096
    }
    
    const plBindGroupUpdateTextureData tGlobalTextureData[] = {
        {
            .tTexture = tTexture,
            .uSlot    = PL_MAX_BINDLESS_TEXTURE_SLOT,
            .uIndex   = (uint32_t)ulValue,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uTextureCount = 1,
        .atTextureBindings = tGlobalTextureData
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atBindGroups[i], &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}

static uint32_t
pl__renderer_get_bindless_cube_texture_index(plScene* ptScene, plTextureHandle tTexture)
{
    uint64_t uIndex = 0;
    if(pl_hm_has_key_ex(&ptScene->tCubeTextureIndexHashmap, tTexture.uData, &uIndex))
        return (uint32_t)uIndex;

    uint64_t ulValue = pl_hm_get_free_index(&ptScene->tCubeTextureIndexHashmap);
    if(ulValue == PL_DS_HASH_INVALID)
    {
        ulValue = ptScene->uCubeTextureIndexCount++;

        // TODO: handle when greater than 4096
    }
    
    const plBindGroupUpdateTextureData tGlobalTextureData[] = {
        {
            .tTexture = tTexture,
            .uSlot    = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT,
            .uIndex   = (uint32_t)ulValue,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uTextureCount = 1,
        .atTextureBindings = tGlobalTextureData
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atBindGroups[i], &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}

static void
pl__renderer_create_probe_data(plScene* ptScene, plEntity tProbeHandle)
{
    plEnvironmentProbeComponent* ptProbe = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, tProbeHandle);

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
        .pcDebugName   = "offscreen depth texture probe"
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

    const plBufferDesc atView2BuffersDesc = {
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
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGpuLightShadow),
        .pcDebugName = "shadow data buffer"
    };

    const plBufferDesc atViewBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
        .szByteSize = sizeof(plGpuViewData),
        .pcDebugName = "probe view buffer"
    };

    // textures
    tProbeData.tRawOutputTexture        = pl__renderer_create_texture(&tRawOutputTextureCubeDesc,  "offscreen raw cube", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tAlbedoTexture           = pl__renderer_create_texture(&tAlbedoTextureDesc, "albedo original", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tNormalTexture           = pl__renderer_create_texture(&tNormalTextureDesc, "normal original", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tAOMetalRoughnessTexture = pl__renderer_create_texture(&tEmmissiveTexDesc, "metalroughness original", 0, PL_TEXTURE_USAGE_SAMPLED);
    tProbeData.tDepthTexture            = pl__renderer_create_texture(&tDepthTextureDesc,      "offscreen depth original", 0, PL_TEXTURE_USAGE_SAMPLED);

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
        tProbeData.atView2Buffers[i] = pl__renderer_create_staging_buffer(&atView2BuffersDesc, "scene", i);
        tProbeData.atViewBuffers[i] = pl__renderer_create_staging_buffer(&atViewBuffersDesc, "view buffer", i);
        
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

        tProbeData.tDirectionLightShadowData.atDLightShadowDataBuffer[i] = pl__renderer_create_staging_buffer(&atLightShadowDataBufferDesc, "d shadow", i);
        tProbeData.tDirectionLightShadowData.atDShadowCameraBuffers[i]   = pl__renderer_create_staging_buffer(&atCameraBuffersDesc, "d shadow buffer", i);

    }

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_bind_group_layout("deferred lighting 1"),
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
    tProbeData.tBrdfLutTexture = pl__renderer_create_texture(&tLutTextureDesc, "lut texture", 0, PL_TEXTURE_USAGE_SAMPLED);

    const plTextureDesc tSpecularTextureDesc = {
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_CUBE,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    tProbeData.tLambertianEnvTexture = pl__renderer_create_texture(&tSpecularTextureDesc, "specular texture", 0, PL_TEXTURE_USAGE_SAMPLED);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = (uint32_t)floorf(log2f((float)ptProbe->uResolution)) - 3, // guarantee final dispatch during filtering is 16 threads
        .tType       = PL_TEXTURE_TYPE_CUBE,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    tProbeData.tGGXEnvTexture = pl__renderer_create_texture(&tTextureDesc, "tGGXEnvTexture", 0, PL_TEXTURE_USAGE_SAMPLED);

    tProbeData.tBrdfLutIndex = pl__renderer_get_bindless_texture_index(ptScene, tProbeData.tBrdfLutTexture);
    tProbeData.uLambertianEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, tProbeData.tLambertianEnvTexture);
    tProbeData.uGGXEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, tProbeData.tGGXEnvTexture);

    pl_sb_push(ptScene->sbtProbeData, tProbeData);

    plObjectComponent* ptProbeObj = gptECS->add_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tProbeHandle);
    ptProbeObj->tMesh = ptScene->tProbeMesh;
    ptProbeObj->tTransform = tProbeHandle;

    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtStagedEntities);
    pl_sb_add(ptScene->sbtStagedEntities);
    ptScene->sbtStagedEntities[uTransparentStart] = tProbeHandle;
};

static void
pl__renderer_update_probes(plScene* ptScene)
{

    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plDrawStream* ptStream = &gptData->tDrawStream;

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~common data~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plVec3 atPitchYawRoll[6] = {
        { 0.0f,    PL_PI_2 },
        { 0.0f,    -PL_PI_2 },
        { PL_PI_2,    PL_PI },
        { -PL_PI_2,    PL_PI },
        { PL_PI,    0.0f, PL_PI },
        { 0.0f,    0.0f },
    };

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptData->tViewBGLayout,
        .pcDebugName = "probe scene bg"
    };

    const plBindGroupUpdateSamplerData tSkyboxBG1SamplerData = {
        .tSampler = gptData->tSamplerLinearRepeat,
        .uSlot    = 1
    };

    const plBindGroupDesc tGBufferFillBG1Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("gbuffer_fill", 1),
        .pcDebugName = "gbuffer fill bg1"
    };

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);

        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY)))
        {
            continue;
        }

        plTransformComponent* ptProbeTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptProbe->tEntity);

        const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~probe face pre-calc~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // temporary data for probes

        // create scene bind group (camera, lights, shadows)
        const plBindGroupUpdateBufferData tViewBGBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptProbe->atViewBuffers[uFrameIdx],                                   .szBufferRange = sizeof(plGpuViewData) },
            { .uSlot = 1, .tBuffer = ptProbe->atView2Buffers[uFrameIdx],                                   .szBufferRange = sizeof(plGpuViewData) * 6 },
            { .uSlot = 2, .tBuffer = ptScene->atLightBuffer[uFrameIdx],                                     .szBufferRange = sizeof(plGpuLight) * pl_sb_size(ptScene->sbtLightData)},
            { .uSlot = 3, .tBuffer = ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGpuLightShadow) * pl_sb_size(ptProbe->tDirectionLightShadowData.sbtDLightShadowData)},
            { .uSlot = 4, .tBuffer = ptScene->atLightShadowDataBuffer[uFrameIdx],                           .szBufferRange = sizeof(plGpuLightShadow) * pl_sb_size(ptScene->sbtLightShadowData)},
            { .uSlot = 5, .tBuffer = ptScene->atGPUProbeDataBuffers[uFrameIdx],                              .szBufferRange = sizeof(plGpuProbe) * pl_sb_size(ptScene->sbtGPUProbeData)},
        };

        const plBindGroupUpdateData tViewBGData = {
            .uBufferCount      = 6,
            .atBufferBindings  = tViewBGBufferData
        };
        plBindGroupHandle tViewBG = gptGfx->create_bind_group(ptDevice, &tViewBGDesc);
        gptGfx->update_bind_group(gptData->ptDevice, tViewBG, &tViewBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tViewBG);


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
        

        plCamera atEnvironmentCamera[6] = {0};

        for(uint32_t uFace = 0; uFace < 6; uFace++)
        {

            atEnvironmentCamera[uFace] = (plCamera){
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPos         = ptProbeTransform->tTranslation,
                .fNearZ       = 0.26f,
                .fFarZ        = ptProbeComp->fRange,
                .fFieldOfView = PL_PI_2,
                .fAspectRatio = 1.0f,
                .fRoll        = atPitchYawRoll[uFace].z
            };
            gptCamera->set_pitch_yaw(&atEnvironmentCamera[uFace], atPitchYawRoll[uFace].x, atPitchYawRoll[uFace].y);
            gptCamera->update(&atEnvironmentCamera[uFace]);
        }


        // create g-buffer fill bind group 1
        const plBindGroupUpdateBufferData tGBufferFillBG1BufferData = {
            .tBuffer       = ptProbe->atView2Buffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(plGpuViewData) * 6
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
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };

        plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "probe");
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
                .task  = pl__renderer_cull_job,
                .pData = &tCullData
            };

            plAtomicCounter* ptCullCounter = NULL;
            gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
            gptJob->wait_for_counter(ptCullCounter);
            pl_sb_reset(ptProbe->sbuVisibleDeferredEntities[uFace]);
            pl_sb_reset(ptProbe->sbuVisibleForwardEntities[uFace]);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                {
                    if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                        pl_sb_push(ptProbe->sbuVisibleDeferredEntities[uFace], uDrawableIndex);
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                        pl_sb_push(ptProbe->sbuVisibleForwardEntities[uFace], uDrawableIndex);
                }
            }

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            
            const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptProbe->sbuVisibleDeferredEntities[uFace]);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount);
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable* ptDrawable = &ptScene->sbtDrawables[ptProbe->sbuVisibleDeferredEntities[uFace][i]];
                plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawable->tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plGpuDynData* ptDynamicData = (plGpuDynData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                ptDynamicData->iVertexOffset = ptDrawable->uDynamicVertexOffset;
                ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                ptDynamicData->uGlobalIndex = uFace;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = ptDrawable->tEnvShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer   = ptDrawable->tIndexBuffer,
                    .uIndexOffset   = ptDrawable->uIndexOffset,
                    .uTriangleCount = ptDrawable->uTriangleCount,
                    .uVertexOffset  = ptDrawable->uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atBindGroups[uFrameIdx],
                        tGBufferFillBG1
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = ptDrawable->uTransformIndex,
                    .uInstanceCount  = 1
                });
            }

            gptGfx->draw_stream(ptProbeEncoder, 1, &tArea); 
            
            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            gptGfx->next_subpass(ptProbeEncoder, NULL);

            // create lighting dynamic bind group

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = uFace;

            
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
                    ptScene->atBindGroups[uFrameIdx],
                    tViewBG,
                    ptProbe->atLightingBindGroup[uFace]
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
                plGpuDynSkybox* ptSkyboxDynamicData = (plGpuDynSkybox*)tSkyboxDynamicData.pcData;
                ptSkyboxDynamicData->tModel = pl_mat4_translate_vec3(ptProbeTransform->tTranslation);
                ptSkyboxDynamicData->uGlobalIndex = uFace;

                gptGfx->reset_draw_stream(ptStream, 1);
                *gptData->pdDrawCalls += 1.0;
                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = gptShaderVariant->get_shader("skybox", NULL, NULL, NULL, &gptData->tRenderPassLayout),
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
                        ptScene->atBindGroups[uFrameIdx],
                        tViewBG,
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

            const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptProbe->sbuVisibleForwardEntities[uFace]);
            gptGfx->reset_draw_stream(ptStream, uVisibleTransparentDrawCount);
            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable* ptDrawable = &ptScene->sbtDrawables[ptProbe->sbuVisibleForwardEntities[uFace][i]];
                plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawable->tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plGpuDynData* ptDynamicData = (plGpuDynData*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                ptDynamicData->iVertexOffset = ptDrawable->uDynamicVertexOffset;
                ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                ptDynamicData->uGlobalIndex = uFace;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = ptDrawable->tEnvShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer         = ptDrawable->tIndexBuffer,
                    .uIndexOffset         = ptDrawable->uIndexOffset,
                    .uTriangleCount       = ptDrawable->uTriangleCount,
                    .uVertexOffset        = ptDrawable->uStaticVertexOffset,
                    .atBindGroups = {
                        ptScene->atBindGroups[uFrameIdx],
                        tViewBG
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = ptDrawable->uTransformIndex,
                    .uInstanceCount = ptDrawable->uInstanceCount
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
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCmdBuffer, &tProbeSubmitInfo);
        gptGfx->return_command_buffer(ptCmdBuffer);
        pl__renderer_create_environment_map_from_texture(ptScene, ptProbe);

        ptProbe->uCurrentFace = (ptProbe->uCurrentFace + ptProbeComp->uInterval) % 6;

        if(ptProbe->uCurrentFace == 0)
            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
    }
}

static void
pl__renderer_create_environment_map_from_texture(plScene* ptScene, plEnvironmentProbeData* ptProbe)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    plComputeShaderHandle tBrdfLutShader = gptShaderVariant->get_compute_shader("brdf_lut", NULL);
    plComputeShaderHandle tCubeFilterSpecularShader = gptShaderVariant->get_compute_shader("cube_filter_specular", NULL);
    plComputeShaderHandle tCubeFilterDiffuseShader = gptShaderVariant->get_compute_shader("cube_filter_diffuse", NULL);

    plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptProbe->tRawOutputTexture);
    const int iResolution = (int)(ptTexture->tDesc.tDimensions.x);
    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);

    // copy to cube
    {

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "map to cube env");
        const plBeginCommandInfo tBeginInfo1 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo1);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
        gptGfx->generate_mipmaps(ptBlitEncoder, ptProbe->tRawOutputTexture);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~common bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // source sampler & cube map
    const plBindGroupDesc tCubeFilterBGSet0Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderVariant->get_bind_group_layout("cube_filter_set_0"),
        .pcDebugName = "cube_filter_set_0"
    };
    plBindGroupHandle tCubeFilterBGSet0 = gptGfx->create_bind_group(ptDevice, &tCubeFilterBGSet0Desc);
    
    const plBindGroupUpdateSamplerData tCubeFilterBGSet0SamplerData = {
        .tSampler = gptData->tSamplerLinearRepeat,
        .uSlot    = 0
    };
    const plBindGroupUpdateTextureData tCubeFilterBGSet0TextureData = {
        .tTexture = ptProbe->tRawOutputTexture,
        .uSlot    = 1,
        .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
    };
    const plBindGroupUpdateData tCubeFilterBGSet0Data = {
        .uSamplerCount = 1,
        .atSamplerBindings = &tCubeFilterBGSet0SamplerData,
        .uTextureCount = 1,
        .atTextureBindings = &tCubeFilterBGSet0TextureData
    };
    gptGfx->update_bind_group(ptDevice, tCubeFilterBGSet0, &tCubeFilterBGSet0Data);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tCubeFilterBGSet0);

    // specular & diffuse output buffers

    const plBindGroupDesc tCubeFilterBGSet1Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderVariant->get_bind_group_layout("cube_filter_set_1"),
        .pcDebugName = "cube_filter_set_1"
    };
    plBindGroupHandle tCubeFilterBGSet1 = gptGfx->create_bind_group(ptDevice, &tCubeFilterBGSet1Desc);

    const plBindGroupUpdateBufferData atCubeFilterBGSet1BufferData[] = {
        { .uSlot = 0, .tBuffer = ptScene->atFilterWorkingBuffers[0], .szBufferRange = uFaceSize},
        { .uSlot = 1, .tBuffer = ptScene->atFilterWorkingBuffers[1], .szBufferRange = uFaceSize},
        { .uSlot = 2, .tBuffer = ptScene->atFilterWorkingBuffers[2], .szBufferRange = uFaceSize},
        { .uSlot = 3, .tBuffer = ptScene->atFilterWorkingBuffers[3], .szBufferRange = uFaceSize},
        { .uSlot = 4, .tBuffer = ptScene->atFilterWorkingBuffers[4], .szBufferRange = uFaceSize},
        { .uSlot = 5, .tBuffer = ptScene->atFilterWorkingBuffers[5], .szBufferRange = uFaceSize}
    };

    const plBindGroupUpdateData tCubeFilterBGSet1Data = {
        .uBufferCount = 6,
        .atBufferBindings = atCubeFilterBGSet1BufferData,
    };
    gptGfx->update_bind_group(ptDevice, tCubeFilterBGSet1, &tCubeFilterBGSet1Data);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tCubeFilterBGSet1);

    // brdf lut

    const plBindGroupDesc tBrdfBGSet1Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("brdf_lut", 1),
        .pcDebugName = "brdf_lut_set_1"
    };
    plBindGroupHandle tBrdfBGSet1 = gptGfx->create_bind_group(ptDevice, &tBrdfBGSet1Desc);

    const plBindGroupUpdateBufferData tBrdfBGSet1BufferData[] = {
        { .uSlot = 0, .tBuffer = ptScene->atFilterWorkingBuffers[6], .szBufferRange = uFaceSize}
    };

    const plBindGroupUpdateData tBrdfBGSet1Data = {
        .uBufferCount = 1,
        .atBufferBindings = tBrdfBGSet1BufferData,
    };
    gptGfx->update_bind_group(ptDevice, tBrdfBGSet1, &tBrdfBGSet1Data);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tBrdfBGSet1);

    plBindGroupHandle atPartialBindGroupHandles[2] = {tCubeFilterBGSet0, tBrdfBGSet1};
    plBindGroupHandle atFullBindGroupHandles[2] = {tCubeFilterBGSet0, tCubeFilterBGSet1};

    {

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 2,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 2");
        const plBeginCommandInfo tBeginInfo0 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
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
        plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
        ptDynamicData->iResolution = iResolution;
        ptDynamicData->fRoughness = 0.0f;
        ptDynamicData->iSampleCount = (int)ptProbeComp->uSamples;
        ptDynamicData->iWidth = iResolution;
        ptDynamicData->fLodBias = 0.0f;
        ptDynamicData->iCurrentMipLevel = 0;

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tBrdfLutShader, 0, 2, atPartialBindGroupHandles, 1, &tDynamicBinding);
        gptGfx->bind_compute_shader(ptComputeEncoder, tBrdfLutShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);

        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tCubeFilterDiffuseShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
        gptGfx->bind_compute_shader(ptComputeEncoder, tCubeFilterDiffuseShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo0 = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
        gptGfx->return_command_buffer(ptCommandBuffer);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 3");
        const plBeginCommandInfo tBeginInfo1 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo1);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        const plBufferImageCopy tBufferImageCopy0 = {
            .uImageWidth = iResolution,
            .uImageHeight = iResolution,
            .uImageDepth = 1,
            .uLayerCount = 1,
            .szBufferOffset = 0,
            .uBaseArrayLayer = 0,
        };
        gptGfx->copy_buffer_to_texture(ptBlitEncoder, ptScene->atFilterWorkingBuffers[6], ptProbe->tBrdfLutTexture, 1, &tBufferImageCopy0);

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
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }
    
    {

        const size_t uMaxFaceSize = (size_t)iResolution * (size_t)iResolution * 4 * sizeof(float);

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

            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 4");
            const plBeginCommandInfo tBeginInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
            };
            gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
            ptDynamicData->iResolution = iResolution;
            ptDynamicData->fRoughness = (float)i / (float)(ptEnvTexture->tDesc.uMips - 1);
            ptDynamicData->iSampleCount = i == 0 ? 1 : (int)ptProbeComp->uSamples;
            ptDynamicData->iWidth = currentWidth;
            ptDynamicData->fLodBias = 0.0f;
            ptDynamicData->iCurrentMipLevel = i;

            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tInnerPassResources);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, tCubeFilterSpecularShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptComputeEncoder, tCubeFilterSpecularShader);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
            gptGfx->end_compute_pass(ptComputeEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
            gptGfx->return_command_buffer(ptCommandBuffer);

            ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 5");
            const plBeginCommandInfo tBeginInfo2 = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
            };
            gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo2);
            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

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
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptBlitEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo0 = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
            // gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);

        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl__renderer_set_drawable_shaders(plScene* ptScene)
{
    plDevice* ptDevice = gptData->ptDevice;

    int iSceneWideRenderingFlags = 0;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(gptData->tRuntimeOptions.bNormalMapping)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {

        plEntity tEntity = ptScene->sbtDrawables[i].tEntity;

        // get actual components
        plObjectComponent*   ptObject   = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, ptMesh->tMaterial);

        uint64_t uMaterialIndex = UINT64_MAX;

        if(!pl_hm_has_key_ex(&ptScene->tMaterialHashmap, ptMesh->tMaterial.uData, &uMaterialIndex))
        {
            PL_ASSERT(false && "material not added to scene");
        }

        ptScene->sbtDrawables[i].uMaterialIndex = (uint32_t)uMaterialIndex;

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
        int aiForwardFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iObjectRenderingFlags,
            pl_sb_capacity(ptScene->sbtLightData),
            pl_sb_size(ptScene->sbtProbeData),
            gptData->tRuntimeOptions.tShaderDebugMode
        };

        int aiGBufferFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            gptData->tRuntimeOptions.tShaderDebugMode,
            iObjectRenderingFlags
        };

        int aiVertexConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride
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
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[i].tShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tRenderPassLayout);
            ptScene->sbtDrawables[i].tEnvShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
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
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[i].tShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
            aiForwardFragmentConstantData0[3] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[i].tEnvShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, NULL);
  
        }
        if(ptMaterial->tBlendMode != PL_BLEND_MODE_OPAQUE)
        {
            plGraphicsState tShadowVariant = {
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
            };
            ptScene->sbtDrawables[i].tShadowShader = gptShaderVariant->get_shader("alphashadow", &tShadowVariant, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDepthRenderPassLayout);
        }
    }
}

static void
pl__renderer_sort_drawables(plScene* ptScene)
{
    plDevice* ptDevice = gptData->ptDevice;

    pl_sb_reset(ptScene->sbuShadowDeferredDrawables);
    pl_sb_reset(ptScene->sbuShadowForwardDrawables);

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {

        plEntity tEntity = ptScene->sbtDrawables[i].tEntity;

        // get actual components
        plObjectComponent*   ptObject   = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, ptMesh->tMaterial);

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_CAST_SHADOW && ptObject->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW)
        {
            if(ptMaterial->tBlendMode != PL_BLEND_MODE_OPAQUE)
            {
                pl_sb_push(ptScene->sbuShadowForwardDrawables, i);
            }
            else
            {
                pl_sb_push(ptScene->sbuShadowDeferredDrawables, i);
            }
        }

        ptScene->sbtDrawables[i].tIndexBuffer = ptScene->sbtDrawables[i].uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer;
    }
}

static void
pl__renderer_add_skybox_drawable(plScene* ptScene)
{
    const uint32_t uStartIndex     = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);

    const plDrawable tDrawable = {
        .uIndexCount     = 36,
        .uVertexCount    = 8,
        .uIndexOffset    = uIndexStart,
        .uVertexOffset   = uStartIndex,
        .uDataOffset     = uDataStartIndex,
        .uTransformIndex = ptScene->uNextTransformIndex++
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
pl__renderer_unstage_drawables(plScene* ptScene)
{
    plDevice* ptDevice = gptData->ptDevice;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // fill CPU buffers & drawable list

    pl_sb_reset(ptScene->sbtDrawables);
    pl_sb_reset(ptScene->sbtSkinData);

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtStagedEntities);
    pl_sb_resize(ptScene->sbtDrawables, uDrawableCount);

    // reserve sizes
    const uint32_t uInitialIndexCount  = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uInitialVertexCount = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uInitialVertexDataCount = pl_sb_size(ptScene->sbtVertexDataBuffer);

    uint32_t uAdditonalIndexCount = 0;
    uint32_t uAdditonalVertexCount = 0;
    uint32_t uAdditonalVertexDataCount = 0;
    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {
        plEntity tEntity = ptScene->sbtStagedEntities[i];

        // get actual components
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
        plMeshComponent*   ptMesh   = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
    
        uAdditonalIndexCount += (uint32_t)ptMesh->szIndexCount;
        uAdditonalVertexCount += (uint32_t)ptMesh->szVertexCount;

        uint32_t uStride = 0;

        // calculate vertex stream mask based on provided data
        if(ptMesh->ptVertexNormals)               { uStride += 1; }
        if(ptMesh->ptVertexTangents)              { uStride += 1; }
        if(ptMesh->ptVertexColors[0])             { uStride += 1; }
        if(ptMesh->ptVertexColors[1])             { uStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[0]) { uStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[2]) { uStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[4]) { uStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[6]) { uStride += 1; }
    
        uAdditonalVertexDataCount += uStride * (uint32_t)ptMesh->szVertexCount;
    }
    pl_sb_reserve(ptScene->sbuIndexBuffer, uInitialIndexCount + uAdditonalIndexCount);
    pl_sb_reserve(ptScene->sbtVertexPosBuffer, uInitialVertexCount + uAdditonalVertexCount);
    pl_sb_reserve(ptScene->sbtVertexDataBuffer, uInitialVertexDataCount + uAdditonalVertexDataCount);

    for(uint32_t i = 0; i < uDrawableCount; i++)
    {

        ptScene->sbtDrawables[i].tEntity = ptScene->sbtStagedEntities[i];
        ptScene->sbtDrawables[i].uSkinIndex = UINT32_MAX;
        pl_hm_insert(&ptScene->tDrawableHashmap, ptScene->sbtStagedEntities[i].uData, i);
        
        pl__renderer_add_drawable_data_to_global_buffer(ptScene, i);
        pl__renderer_add_drawable_skin_data_to_global_buffers(ptScene, i);
        ptScene->sbtDrawables[i].uTriangleCount = ptScene->sbtDrawables[i].uIndexCount == 0 ? ptScene->sbtDrawables[i].uVertexCount / 3 : ptScene->sbtDrawables[i].uIndexCount / 3;
        ptScene->sbtDrawables[i].uStaticVertexOffset = ptScene->sbtDrawables[i].uIndexCount == 0 ? ptScene->sbtDrawables[i].uVertexOffset : 0;
        ptScene->sbtDrawables[i].uDynamicVertexOffset = ptScene->sbtDrawables[i].uIndexCount == 0 ? 0 : ptScene->sbtDrawables[i].uVertexOffset;

        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);
        plMeshComponent* ptMesh = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, ptMesh->tMaterial);
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtDrawables[i].tEntity);
        
        if(ptProbeComp)
            ptScene->sbtDrawables[i].tFlags = PL_DRAWABLE_FLAG_PROBE | PL_DRAWABLE_FLAG_FORWARD;
        else
        {
            bool bForward = false;

            if(ptMaterial->tBaseColor.a != 1.0f || ptMaterial->tEmissiveColor.a > 0.0f)
                bForward = true;

            if(ptMaterial->tBlendMode == PL_BLEND_MODE_ALPHA)
                bForward = true;

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].tResource))
                bForward = true;

            if(bForward)
                ptScene->sbtDrawables[i].tFlags = PL_DRAWABLE_FLAG_FORWARD;
            else
                ptScene->sbtDrawables[i].tFlags = PL_DRAWABLE_FLAG_DEFERRED;
        }
    }

    for (uint32_t i = 0; i < uDrawableCount; i++)
    {
        ptScene->sbtDrawables[i].uInstanceCount = 1;
        ptScene->sbtDrawables[i].uTransformIndex = ptScene->uNextTransformIndex++;
        plObjectComponent* ptObjectA = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtStagedEntities[i]);
        for (uint32_t j = i; j < uDrawableCount - 1; j++)
        {
            plObjectComponent* ptObjectB = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtStagedEntities[j + 1]);
            if(ptObjectA->tMesh.uIndex == ptObjectB->tMesh.uIndex)
            {
                ptScene->sbtDrawables[i].uInstanceCount++;

                ptScene->sbtDrawables[j + 1].uInstanceCount = 0;
                ptScene->sbtDrawables[j + 1].uTransformIndex = ptScene->uNextTransformIndex++;
            }
            else
            {
                break;
            }
            
        }
        i += ptScene->sbtDrawables[i].uInstanceCount;
        i--;

    }

    if(ptScene->tSkyboxTexture.uIndex != 0)
    {
        pl__renderer_add_skybox_drawable(ptScene);
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

    ptScene->tIndexBuffer   = pl__renderer_create_local_buffer(&tIndexBufferDesc,   "index", 0, ptScene->sbuIndexBuffer, pl_sb_size(ptScene->sbuIndexBuffer) * sizeof(uint32_t));
    ptScene->tVertexBuffer  = pl__renderer_create_local_buffer(&tVertexBufferDesc,  "vertex", 0, ptScene->sbtVertexPosBuffer, pl_sb_size(ptScene->sbtVertexPosBuffer) * sizeof(plVec3));
    ptScene->tStorageBuffer = pl__renderer_create_local_buffer(&tStorageBufferDesc, "storage", 0, ptScene->sbtVertexDataBuffer, pl_sb_size(ptScene->sbtVertexDataBuffer) * sizeof(plVec4));

    if(tSkinStorageBufferDesc.szByteSize > 0)
    {
        ptScene->tSkinStorageBuffer  = pl__renderer_create_local_buffer(&tSkinStorageBufferDesc, "skin storage", 0, ptScene->sbtSkinVertexDataBuffer, pl_sb_size(ptScene->sbtSkinVertexDataBuffer) * sizeof(plVec4));
        
        const plBindGroupDesc tSkinBindGroupDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("skinning", 0),
            .pcDebugName = "skin bind group"
        };
        ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(ptDevice, &tSkinBindGroupDesc);

        const plBindGroupUpdateSamplerData atSamplerData[] = {
            { .uSlot = 3, .tSampler = gptData->tSamplerLinearRepeat}
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
                .uSlot         = 1,
                .szBufferRange = ptStorageBuffer->tDesc.szByteSize
            }
        };

        plBindGroupUpdateData tGlobalBindGroupData = {
            .uBufferCount = 1,
            .atBufferBindings = atGlobalBufferData,
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atBindGroups[i], &tGlobalBindGroupData);
    }

    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);
    pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
}