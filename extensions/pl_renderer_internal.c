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

static inline plVec3d
pl__to_double_vec(plVec3 tVec)
{
    return (plVec3d){(double)tVec.x, (double)tVec.y, (double)tVec.z};
}

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
//-----------------------------------------------------------------------------

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
    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "created local texture %s %u", pcName, uIdentifier);
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
    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "created texture %s %u", pcName, uIdentifier);
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

    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        const plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptDesc->tDimensions.x,
            .uImageHeight = (uint32_t)ptDesc->tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1
        };

        gptStage->stage_texture_upload(tHandle, &tBufferImageCopy, pData, (uint64_t)szSize, true);
        gptStage->flush();
    }

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created texture %s %u", pcName, uIdentifier);
    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "created texture %s %u", pcName, uIdentifier);
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
    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "created staging buffer %s %u", pcName, uIdentifier);
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
    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "created cached staging buffer %s %u", pcName, uIdentifier);
    return tHandle;
}

static plBufferHandle
pl__renderer_create_local_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
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

    gptScreenLog->add_message_ex(0, 0.0, PL_COLOR_32_WHITE, 1.0f, "created local buffer %s %u", pcName, uIdentifier);
    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "created local buffer %s %u", pcName, uIdentifier);
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
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;

    // update skin textures
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);

    if(uSkinCount)
    {

        const plPassBufferResource atPassBuffers[] = {
            { .tHandle = ptScene->tStorageBuffer, .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_READ | PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = ptScene->tVertexBuffer,  .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 2,
            .atBuffers = atPassBuffers
        };

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->push_compute_debug_group(ptComputeEncoder, "Skinning Compute", (plVec4){1.0f, 0.0f, 1.0f, 1.0f});
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
            ptDynamicData->uMatrixOffset = (uint32_t)ptScene->sbtSkinData[i].ptFreeListNode->uOffset / sizeof(plMat4);

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
                ptScene->atSkinBindGroup1[gptGfx->get_current_frame_index()]
            };
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, ptScene->sbtSkinData[i].tShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptComputeEncoder, ptScene->sbtSkinData[i].tShader);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        }
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->pop_compute_debug_group(ptComputeEncoder);
        gptGfx->end_compute_pass(ptComputeEncoder);
    }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static bool
pl__renderer_pack_shadow_atlas(plScene* ptScene)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    
    uint32_t uViewCount = pl_sb_size(ptScene->sbptViews);
    pl_sb_reset(ptScene->sbtShadowRects);
    pl_sb_reset(ptScene->sbtShadowRectData);

    plEnvironmentProbeComponent* ptProbes = NULL;

    const uint32_t uProbeCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, (void**)&ptProbes, NULL);
    uint32_t uLightCount = pl_sb_size(ptScene->sbtPointLights);

    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[uLightIndex].tEntity);

        // skip light if it doesn't cast shadows
        if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
        {
            continue;
        }

        const plPackRect tPackRect = {
            .iWidth  = (int)(ptLight->uShadowResolution * 2),
            .iHeight = (int)(ptLight->uShadowResolution * 3),
            .iId     = (int)pl_sb_size(ptScene->sbtShadowRectData)
        };
        pl_sb_push(ptScene->sbtShadowRects, tPackRect);

        plShadowPackData tPackData = {
            .uLightIndex = uLightIndex,
            .uViewIndex  = 0,
            .uProbeIndex = 0,
            .bAltMode    = false,
            .tType       = ptLight->tType
        };
        pl_sb_push(ptScene->sbtShadowRectData, tPackData);

    }

    uLightCount = pl_sb_size(ptScene->sbtSpotLights);
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[uLightIndex].tEntity);

        // skip light if it doesn't cast shadows
        if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
        {
            continue;
        }

        const plPackRect tPackRect = {
            .iWidth  = (int)ptLight->uShadowResolution,
            .iHeight = (int)ptLight->uShadowResolution,
            .iId     = (int)pl_sb_size(ptScene->sbtShadowRectData)
        };
        pl_sb_push(ptScene->sbtShadowRects, tPackRect);

        plShadowPackData tPackData = {
            .uLightIndex = uLightIndex,
            .uViewIndex  = 0,
            .uProbeIndex = 0,
            .bAltMode    = false,
            .tType       = ptLight->tType
        };
        pl_sb_push(ptScene->sbtShadowRectData, tPackData);
    }

    uLightCount = pl_sb_size(ptScene->sbtDirectionLights);
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[uLightIndex].tEntity);

        // skip light if it doesn't cast shadows
        if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
        {
            continue;
        }

        for(uint32_t uView = 0; uView < uViewCount; uView++)
        {
            const plPackRect tPackRect = {
                .iWidth  = (int)(ptLight->uShadowResolution * ptLight->uCascadeCount),
                .iHeight = (int)ptLight->uShadowResolution,
                .iId     = (int)pl_sb_size(ptScene->sbtShadowRectData)
            };
            pl_sb_push(ptScene->sbtShadowRects, tPackRect);

            plShadowPackData tPackData = {
                .uLightIndex = uLightIndex,
                .uViewIndex  = uView,
                .uProbeIndex = 0,
                .bAltMode    = false,
                .tType       = ptLight->tType
            };
            pl_sb_push(ptScene->sbtShadowRectData, tPackData);
        }

        for(uint32_t uProbe = 0; uProbe < uProbeCount; uProbe++)
        {
            for(uint32_t uView = 0; uView < 6; uView++)
            {
                const plPackRect tPackRect = {
                    .iWidth  = (int)(ptLight->uShadowResolution),
                    .iHeight = (int)ptLight->uShadowResolution,
                    .iId     = (int)pl_sb_size(ptScene->sbtShadowRectData)
                };
                pl_sb_push(ptScene->sbtShadowRects, tPackRect);

                plShadowPackData tPackData = {
                    .uLightIndex = uLightIndex,
                    .uViewIndex  = uView,
                    .uProbeIndex = uProbe,
                    .bAltMode    = true,
                    .tType       = ptLight->tType
                };
                pl_sb_push(ptScene->sbtShadowRectData, tPackData);
            }
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

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return bPacked;
}

static void
pl__renderer_generate_shadow_maps(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, plScene* ptScene)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    int aiConstantData[] = {0, 0};
    plShaderHandle tShadowShader = gptShaderVariant->get_shader("shadow", NULL, aiConstantData, aiConstantData, &gptData->tDepthRenderPassLayout);

    const uint32_t uLightCount = pl_sb_size(ptScene->sbtShadowRects);
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];

        plLightComponent* ptLight = NULL;
        if(ptData->tType == PL_LIGHT_TYPE_POINT)            ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_SPOT)        ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_DIRECTIONAL) ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptData->uLightIndex].tEntity);

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {
            plGpuPointLightShadow* ptShadowData = &ptScene->sbtPointLightShadowData[ptScene->sbtPointLightData[ptData->uLightIndex].iShadowIndex];
            ptShadowData->iShadowMapTexIdx = ptScene->uShadowAtlasIndex;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 atCamViewProjs[6] = {0};

            plCamera tShadowCamera = {
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPosDouble   = {(double)ptLight->tPosition.x, (double)ptLight->tPosition.y, (double)ptLight->tPosition.z},
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
            memcpy(&pcBufferStart[ptScene->sbtPointLights[ptData->uLightIndex].uShadowBufferOffset], atCamViewProjs, sizeof(plMat4) * 6);
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {
            plGpuSpotLightShadow* ptShadowData = &ptScene->sbtSpotLightShadowData[ptScene->sbtSpotLightData[ptData->uLightIndex].iShadowIndex];
            ptShadowData->iShadowMapTexIdx = ptScene->uShadowAtlasIndex;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 tCamViewProjs = {0};

            plCamera tShadowCamera = {
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPosDouble   = {(double)ptLight->tPosition.x, (double)ptLight->tPosition.y, (double)ptLight->tPosition.z},
                .fNearZ       = ptLight->fRadius,
                .fFarZ        = ptLight->fRange,
                .fFieldOfView = ptLight->fOuterConeAngle * 2.0f,
                .fAspectRatio = 1.0f
            };

            plVec3 tDirection = pl_norm_vec3(ptLight->tDirection);
            gptCamera->look_at(&tShadowCamera, pl__to_double_vec(ptLight->tPosition), pl__to_double_vec(pl_add_vec3(ptLight->tPosition, tDirection)));
            gptCamera->update(&tShadowCamera);
            tCamViewProjs = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            ptShadowData->viewProjMat = tCamViewProjs;
            
            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[ptScene->sbtSpotLights[ptData->uLightIndex].uShadowBufferOffset], &tCamViewProjs, sizeof(plMat4));
        }    
    }

    const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbuShadowDeferredDrawables);
    const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbuShadowForwardDrawables);
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    uint32_t uCameraBufferIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];


        plLightComponent* ptLight = NULL;
        if(ptData->tType == PL_LIGHT_TYPE_POINT)            ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_SPOT)        ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_DIRECTIONAL) ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptData->uLightIndex].tEntity);


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
                                ptScene->atSceneBindGroups[uFrameIdx],
                                ptScene->atShadowBG[uFrameIdx]
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
                            .tShader        = ptScene->sbtShadowShaders[ptScene->sbuShadowForwardDrawables[i]],
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
                                ptScene->atSceneBindGroups[uFrameIdx],
                                ptScene->atShadowBG[uFrameIdx]
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
                                    ptScene->atSceneBindGroups[uFrameIdx],
                                    ptScene->atShadowBG[uFrameIdx]
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
                                .tShader        = ptScene->sbtShadowShaders[ptScene->sbuShadowForwardDrawables[i]],
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
                                    ptScene->atSceneBindGroups[uFrameIdx],
                                    ptScene->atShadowBG[uFrameIdx]
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
                            ptScene->atSceneBindGroups[uFrameIdx],
                            ptScene->atShadowBG[uFrameIdx]
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
                        .tShader        = ptScene->sbtShadowShaders[ptScene->sbuShadowForwardDrawables[i]],
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
                            ptScene->atSceneBindGroups[uFrameIdx],
                            ptScene->atShadowBG[uFrameIdx]
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

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_generate_cascaded_shadow_map(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer, plScene* ptScene, uint32_t uViewHandle, uint32_t uProbeIndex, plCamera* ptSceneCamera, plCSMInfo tInfo, plDrawList3D* ptDrawlist)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    int aiConstantData[] = {0, 0};
    plShaderHandle tShadowShader = gptShaderVariant->get_shader("shadow", NULL, aiConstantData, aiConstantData, &gptData->tDepthRenderPassLayout);

    const uint32_t uLightCount = pl_sb_size(ptScene->sbtDirectionLights);

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

    float fShadowFarZ = pl_min(ptSceneCamera->fFarZ, gptData->tRuntimeOptions.fMaxShadowRange);

    const uint32_t uAtlasRectCount = pl_sb_size(ptScene->sbtShadowRects);
    for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uRectIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];

        // check if light applies
        if(ptData->tType != PL_LIGHT_TYPE_DIRECTIONAL || ptData->uViewIndex != uViewHandle || ptData->bAltMode != tInfo.bAltMode || ptData->uProbeIndex != uProbeIndex)
        {
            continue;
        }
        plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptData->uLightIndex].tEntity);


        int iShadowIndex = ptScene->sbtDirectionLightData[ptData->uLightIndex].iShadowIndex;
        if(tInfo.bAltMode)
            iShadowIndex += (int)uViewHandle;
        plGpuDirectionLightShadow* ptShadowData = &tInfo.sbtDLightShadowData[iShadowIndex];

        ptShadowData->iShadowMapTexIdx = ptScene->uShadowAtlasIndex;
        ptShadowData->fFactor          = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fXOffset         = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
        ptShadowData->fYOffset         = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

        plMat4 atCamViewProjs[PL_MAX_SHADOW_CASCADES] = {0};
        float fLastSplitDist = ptSceneCamera->fNearZ;
        const plVec3 tDirection = pl_norm_vec3(ptLight->tDirection);
        const uint32_t uCascadeCount = tInfo.bAltMode ? 1 : ptLight->uCascadeCount; // probe only needs single cascade

        float afCascadeSplits[4] = {
            tInfo.bAltMode ? 1.0f : ptLight->afCascadeSplits[0], // use whole frustum for environment probes
            ptLight->afCascadeSplits[1],
            ptLight->afCascadeSplits[2],
            ptLight->afCascadeSplits[3]
        };

        if(!tInfo.bAltMode && ptLight->fShadowLambda > 0.0f)
        {
            for(uint32_t i = 1; i <= uCascadeCount; ++i)
            {
                float p = (float)i / (float)uCascadeCount;

                float logSplit =
                    ptSceneCamera->fNearZ * powf(fShadowFarZ / ptSceneCamera->fNearZ, p);

                float linSplit =
                    ptSceneCamera->fNearZ + (fShadowFarZ - ptSceneCamera->fNearZ) * p;

                float di =
                    ptLight->fShadowLambda * logSplit +
                    (1.0f - ptLight->fShadowLambda) * linSplit;

                afCascadeSplits[i - 1] = (di - ptSceneCamera->fNearZ)/(ptSceneCamera->fFarZ - ptSceneCamera->fNearZ);
                ptLight->afCascadeSplits[i - 1] = afCascadeSplits[i - 1];
            }
        }

        //-------------------------------------------------------------------------
        // stable light basis from direction only
        //-------------------------------------------------------------------------
        plVec3 tLightForward = tDirection;
        plVec3 tWorldUp = {0.0f, 1.0f, 0.0f};

        if(fabsf(pl_dot_vec3(tLightForward, tWorldUp)) > 0.99f)
            tWorldUp = (plVec3){1.0f, 0.0f, 0.0f};

        plVec3 tLightRight = pl_norm_vec3(pl_cross_vec3(tWorldUp, tLightForward));
        plVec3 tLightUp    = pl_cross_vec3(tLightForward, tLightRight);

        // world -> light rotation only
        plMat4 tStableLightView = pl_identity_mat4();
        tStableLightView.col[0].x = tLightRight.x;
        tStableLightView.col[1].x = tLightRight.y;
        tStableLightView.col[2].x = tLightRight.z;
        tStableLightView.col[3].x = 0.0f;

        tStableLightView.col[0].y = tLightUp.x;
        tStableLightView.col[1].y = tLightUp.y;
        tStableLightView.col[2].y = tLightUp.z;
        tStableLightView.col[3].y = 0.0f;

        tStableLightView.col[0].z = tLightForward.x;
        tStableLightView.col[1].z = tLightForward.y;
        tStableLightView.col[2].z = tLightForward.z;
        tStableLightView.col[3].z = 0.0f;

        tStableLightView.col[0].w = 0.0f;
        tStableLightView.col[1].w = 0.0f;
        tStableLightView.col[2].w = 0.0f;
        tStableLightView.col[3].w = 1.0f;

        // inverse of rotation-only matrix = transpose for orthonormal basis
        plMat4 tStableLightViewInv = pl_identity_mat4();
        tStableLightViewInv.col[0].x = tLightRight.x;
        tStableLightViewInv.col[0].y = tLightRight.y;
        tStableLightViewInv.col[0].z = tLightRight.z;
        tStableLightViewInv.col[0].w = 0.0f;

        tStableLightViewInv.col[1].x = tLightUp.x;
        tStableLightViewInv.col[1].y = tLightUp.y;
        tStableLightViewInv.col[1].z = tLightUp.z;
        tStableLightViewInv.col[1].w = 0.0f;

        tStableLightViewInv.col[2].x = tLightForward.x;
        tStableLightViewInv.col[2].y = tLightForward.y;
        tStableLightViewInv.col[2].z = tLightForward.z;
        tStableLightViewInv.col[2].w = 0.0f;

        tStableLightViewInv.col[3].x = 0.0f;
        tStableLightViewInv.col[3].y = 0.0f;
        tStableLightViewInv.col[3].z = 0.0f;
        tStableLightViewInv.col[3].w = 1.0f;

        for(uint32_t uCascade = 0; uCascade < uCascadeCount; uCascade++)
        {
            float fSplitDist = ptSceneCamera->fNearZ + afCascadeSplits[uCascade] * (ptSceneCamera->fFarZ - ptSceneCamera->fNearZ);

            // scene camera space
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

            // convert to world space
            plVec3 atWorldSpaceCorners[8] = {0};
            const plMat4 tCameraInversion = pl_mat4_invert(&ptSceneCamera->tViewMat);
            for(uint32_t i = 0; i < 8; i++)
            {
                plVec4 tInvCorner = pl_mul_mat4_vec4(&tCameraInversion, (plVec4){ .xyz = atCameraCorners2[i], .w = 1.0f });
                atWorldSpaceCorners[i] = tInvCorner.xyz;
            }

            // find frustum slice center in world space
            plVec3 tFrustumCenter = {0};
            for(uint32_t i = 0; i < 8; i++)
                tFrustumCenter = pl_add_vec3(tFrustumCenter, atWorldSpaceCorners[i]);
            tFrustumCenter = pl_mul_vec3_scalarf(tFrustumCenter, 1.0f / 8.0f);

            // transform frustum center into stable light space
            plVec4 tFrustumCenterLS4 = pl_mul_mat4_vec4(&tStableLightView, (plVec4){ .xyz = tFrustumCenter, .w = 1.0f });
            plVec3 tFrustumCenterLS = tFrustumCenterLS4.xyz;

            // transform slice corners into stable light space
            float fZMin =  FLT_MAX;
            float fZMax = -FLT_MAX;
            for(uint32_t i = 0; i < 8; i++)
            {
                plVec4 tCornerLS = pl_mul_mat4_vec4(&tStableLightView, (plVec4){ .xyz = atWorldSpaceCorners[i], .w = 1.0f });
                atCameraCorners2[i] = tCornerLS.xyz;

                fZMin = pl_min(fZMin, atCameraCorners2[i].z);
                fZMax = pl_max(fZMax, atCameraCorners2[i].z);
            }

            // stable radius from world-space slice sphere
            float fRadius = 0.0f;
            for(uint32_t i = 0; i < 8; i++)
            {
                plVec3 tDiff = pl_sub_vec3(atWorldSpaceCorners[i], tFrustumCenter);
                float fDist = pl_length_vec3(tDiff);
                fRadius = pl_max(fRadius, fDist);
            }

            // quantize radius a bit to reduce tiny fluctuations
            fRadius = ceilf(fRadius * 16.0f) / 16.0f;

            // final stable ortho dimension
            const float fDim = fRadius * 2.0f;

            // texel snapping in stable light space
            const float fUnitPerTexel = fDim / (float)ptLight->uShadowResolution;
            const float fCenterX = fUnitPerTexel * roundf(tFrustumCenterLS.x / fUnitPerTexel);
            const float fCenterY = fUnitPerTexel * roundf(tFrustumCenterLS.y / fUnitPerTexel);
            const float fCenterZ = 0.5f * (fZMin + fZMax);

            // optional z padding for off-frustum casters
            const float fDepthPadding = 100.0f; // TODO: make option
            const float fNearZ = fZMax + fDepthPadding;
            const float fFarZ  = fZMin - fDepthPadding;

            // reconstruct snapped world-space center from stable light space
            plVec3 tSnappedCenterLS = {
                fCenterX,
                fCenterY,
                fCenterZ
            };

            plVec4 tSnappedCenterWS4 = pl_mul_mat4_vec4(&tStableLightViewInv, (plVec4){ .xyz = tSnappedCenterLS, .w = 1.0f });
            plVec3 tSnappedCenterWS = tSnappedCenterWS4.xyz;

            // final shadow camera
            plCamera tShadowCamera = {
                .tType = PL_CAMERA_TYPE_ORTHOGRAPHIC_REVERSE_Z
            };

            tShadowCamera.tPosDouble = pl__to_double_vec(tSnappedCenterWS);
            tShadowCamera.tPos = tSnappedCenterWS;
            tShadowCamera.fWidth  = fDim;
            tShadowCamera.fHeight = fDim;
            tShadowCamera.fNearZ  = fNearZ;
            tShadowCamera.fFarZ   = fFarZ;

            gptCamera->update(&tShadowCamera);

            // direct stable view matrix construction (no look_at)
            tShadowCamera.tViewMat = tStableLightView;
            tShadowCamera.tViewMat.col[3].x = -pl_dot_vec3(tLightRight,   tSnappedCenterWS);
            tShadowCamera.tViewMat.col[3].y = -pl_dot_vec3(tLightUp,      tSnappedCenterWS);
            tShadowCamera.tViewMat.col[3].z = -pl_dot_vec3(tLightForward, tSnappedCenterWS);

            

            // if update() overwrites tViewMat in your camera system, move the
            // tViewMat assignment to after update() and then rebuild viewProj here.

            atCamViewProjs[uCascade] = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            ptShadowData->viewProjMat[uCascade] = atCamViewProjs[uCascade];
            fLastSplitDist = fSplitDist;

            // copy data to GPU buffer
            char* pcBufferStart = gptGfx->get_buffer(ptDevice, tInfo.tDShadowCameraBuffer)->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[iShadowIndex * sizeof(plMat4) * PL_MAX_SHADOW_CASCADES + uCascade * sizeof(plMat4)], &atCamViewProjs[uCascade], sizeof(plMat4));

            if(ptScene->ptTerrain)
            {
                const plMat4 tMVP = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);

                plScissor tScissor = {
                        .iOffsetX = (int)(ptRect->iX + uCascade * ptLight->uShadowResolution),
                        .iOffsetY = ptRect->iY,
                        .uWidth  = ptLight->uShadowResolution,
                        .uHeight = ptLight->uShadowResolution,
                    };
                
                plRenderViewport tViewport = {
                    .fX = (float)(ptRect->iX + uCascade * ptLight->uShadowResolution),
                    .fY = (float)ptRect->iY,
                    .fWidth  = (float)ptLight->uShadowResolution,
                    .fHeight = (float)ptLight->uShadowResolution,
                    .fMaxDepth = 1.0f
                };
                gptGfx->set_viewport(ptEncoder, &tViewport);
                gptGfx->set_scissor_region(ptEncoder, &tScissor);
                gptGfx->set_depth_bias(ptEncoder, gptData->tRuntimeOptions.fTerrainShadowConstantDepthBias, 0.0f, gptData->tRuntimeOptions.fTerrainShadowSlopeDepthBias);
                gptGfx->bind_shader(ptEncoder, ptScene->tTerrainShadowShader);
                gptGfx->bind_vertex_buffer(ptEncoder, ptScene->ptTerrain->tVertexBuffer);
                plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], tInfo.tBindGroup};

                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                // ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                // ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                // ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)uCascade + iShadowIndex;

                gptGfx->bind_graphics_bind_groups(
                    ptEncoder,
                    ptScene->tTerrainShadowShader,
                    0, 2,
                    atBindGroups,
                    1, &tDynamicBinding
                );


                for(uint32_t i = 0; i < pl_sb_size(ptScene->ptTerrain->sbtChunkFiles); i++)
                    pl__render_chunk_shadow(ptScene, ptScene->ptTerrain, &tShadowCamera, ptEncoder, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, &tMVP, 0);
            }
        }

        // TODO: rework to not waste so much space (don't use max cascades as stride)

        // // copy data to GPU buffer
        // char* pcBufferStart = gptGfx->get_buffer(ptDevice, tInfo.tDShadowCameraBuffer)->tMemoryAllocation.pHostMapped;
        // memcpy(&pcBufferStart[iShadowIndex * sizeof(plMat4) * PL_MAX_SHADOW_CASCADES], atCamViewProjs, sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);

        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, tInfo.tDLightShadowDataBuffer);
        memcpy(&ptDShadowDataBuffer->tMemoryAllocation.pHostMapped[iShadowIndex * sizeof(plGpuDirectionLightShadow)], ptShadowData, sizeof(plGpuDirectionLightShadow));
    }

    // const uint32_t uIndexingOffset = uInitialOffset / (sizeof(plMat4) * PL_MAX_SHADOW_CASCADES);

    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbuShadowDeferredDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbuShadowForwardDrawables);

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uRectIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];

        // check if light applies
        if(ptData->tType != PL_LIGHT_TYPE_DIRECTIONAL || ptData->uViewIndex != uViewHandle || ptData->bAltMode != tInfo.bAltMode || ptData->uProbeIndex != uProbeIndex)
        {
            continue;
        }

        const plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptData->uLightIndex].tEntity);


        int iShadowIndex = ptScene->sbtDirectionLightData[ptData->uLightIndex].iShadowIndex;
        if(tInfo.bAltMode)
            iShadowIndex += (int)uViewHandle;

        const uint32_t uCascadeCount = tInfo.bAltMode ? 1 : ptLight->uCascadeCount; // probe only needs single cascade

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
                    ptDynamicData->iIndex = iShadowIndex;

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
                            ptScene->atSceneBindGroups[uFrameIdx],
                            tInfo.tBindGroup
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
                    ptDynamicData->iIndex = iShadowIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = ptScene->sbtShadowShaders[ptScene->sbuShadowForwardDrawables[i]],
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
                            ptScene->atSceneBindGroups[uFrameIdx],
                            tInfo.tBindGroup
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
                        ptDynamicData->iIndex = (int)uCascade + iShadowIndex;

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
                                ptScene->atSceneBindGroups[uFrameIdx],
                                tInfo.tBindGroup
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
                        ptDynamicData->iIndex = (int)uCascade + iShadowIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader        = ptScene->sbtShadowShaders[ptScene->sbuShadowForwardDrawables[i]],
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
                                ptScene->atSceneBindGroups[uFrameIdx],
                                tInfo.tBindGroup
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
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] misc.
//-----------------------------------------------------------------------------

static uint64_t
pl_renderer__add_material_to_scene(plScene* ptScene, plEntity tMaterial)
{
    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();

    plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, tMaterialComponentType, tMaterial);

    // see if material already exists
    if(!pl_hm_has_key(&ptScene->tMaterialHashmap, tMaterial.uData))
    {
        uint64_t uMaterialIndex = pl_hm_get_free_index(&ptScene->tMaterialHashmap);
        if(uMaterialIndex == PL_DS_HASH_INVALID)
        {
            uMaterialIndex = pl_sb_size(ptScene->sbtMaterialNodes);
            pl_sb_add(ptScene->sbtMaterialNodes);
        }
        ptScene->sbtMaterialNodes[uMaterialIndex] = gptFreeList->get_node(&ptScene->tMaterialFreeList, sizeof(plGpuMaterial));

        plGpuMaterial tGPUMaterial = {0};

        tGPUMaterial.fMetallicFactor           = ptMaterial->fMetalness;
        tGPUMaterial.fRoughnessFactor          = ptMaterial->fRoughness;
        tGPUMaterial.tBaseColorFactor          = ptMaterial->tBaseColor;
        tGPUMaterial.tEmissiveFactor           = ptMaterial->tEmissiveColor.rgb;
        tGPUMaterial.fAlphaCutoff              = ptMaterial->fAlphaCutoff;
        tGPUMaterial.fClearcoatFactor          = ptMaterial->fClearcoat;
        tGPUMaterial.fClearcoatRoughnessFactor = ptMaterial->fClearcoatRoughness;
        tGPUMaterial.fSheenRoughnessFactor     = ptMaterial->fSheenRoughness;
        tGPUMaterial.fNormalMapStrength        = ptMaterial->fNormalMapStrength;
        tGPUMaterial.fEmissiveStrength         = ptMaterial->fEmissiveStrength;
        tGPUMaterial.tSheenColorFactor         = ptMaterial->tSheenColor;
        tGPUMaterial.fIridescenceFactor        = ptMaterial->fIridescenceFactor;
        tGPUMaterial.fIridescenceIor           = ptMaterial->fIridescenceIor;
        tGPUMaterial.fIridescenceThicknessMin  = ptMaterial->fIridescenceThicknessMin;
        tGPUMaterial.fIridescenceThicknessMax  = ptMaterial->fIridescenceThicknessMax;
        tGPUMaterial.tAnisotropy.x             = cosf(ptMaterial->fAnisotropyRotation);
        tGPUMaterial.tAnisotropy.y             = sinf(ptMaterial->fAnisotropyRotation);
        tGPUMaterial.tAnisotropy.z             = ptMaterial->fAnisotropyStrength;
        tGPUMaterial.fOcclusionStrength        = ptMaterial->fOcclusionStrength;
        tGPUMaterial.tAlphaMode                = ptMaterial->tAlphaMode;
        tGPUMaterial.fTransmissionFactor       = ptMaterial->fTransmissionFactor;
        tGPUMaterial.fThickness                = ptMaterial->fThickness;
        tGPUMaterial.fAttenuationDistance      = ptMaterial->fAttenuationDistance;
        tGPUMaterial.tAttenuationColor         = ptMaterial->tAttenuationColor;
        tGPUMaterial.fDispersion               = ptMaterial->fDispersion;
        tGPUMaterial.fIor                      = ptMaterial->fIor;
        tGPUMaterial.fDiffuseTransmission      = ptMaterial->fDiffuseTransmission;
        tGPUMaterial.tDiffuseTransmissionColor = ptMaterial->tDiffuseTransmissionColor;

        const int iDummyIndex = (int)pl__renderer_get_bindless_texture_index(ptScene, gptData->tDummyTexture);
        for(uint32_t uTextureIndex = 0; uTextureIndex < PL_TEXTURE_SLOT_COUNT; uTextureIndex++)
        {
            tGPUMaterial.aiTextureUVSet[uTextureIndex] = (int)ptMaterial->atTextureMaps[uTextureIndex].uUVSet;
            tGPUMaterial.atTextureTransforms[uTextureIndex] = ptMaterial->atTextureMaps[uTextureIndex].tTransform;
            if(gptResource->is_valid(ptMaterial->atTextureMaps[uTextureIndex].tResource))
            {
                plTextureHandle tValidTexture = gptResource->get_texture(ptMaterial->atTextureMaps[uTextureIndex].tResource);
                tGPUMaterial.aiTextureIndices[uTextureIndex] = (int)pl__renderer_get_bindless_texture_index(ptScene, tValidTexture);
            }
            else
                tGPUMaterial.aiTextureIndices[uTextureIndex] = iDummyIndex;
        }

        ptScene->uMaterialDirtyValue = gptStage->queue_buffer_upload(ptScene->tMaterialDataBuffer,
            ptScene->sbtMaterialNodes[uMaterialIndex]->uOffset,
            &tGPUMaterial,
            sizeof(plGpuMaterial));

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_SHEEN)
            ptScene->tFlags |= PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED;

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_TRANSMISSION || ptMaterial->tFlags & PL_MATERIAL_FLAG_VOLUME || ptMaterial->tFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
            ptScene->tFlags |= PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED;
        pl_hm_insert(&ptScene->tMaterialHashmap, tMaterial.uData, uMaterialIndex);
        return uMaterialIndex;
    }
    return pl_hm_lookup(&ptScene->tMaterialHashmap, tMaterial.uData);
}

static bool
pl__renderer_add_drawable_data_to_global_buffer(plScene* ptScene, uint32_t uDrawableIndex)
{

    pl_sb_reset(ptScene->sbuIndexBuffer);
    pl_sb_reset(ptScene->sbtVertexPosBuffer);
    pl_sb_reset(ptScene->sbtVertexDataBuffer);
    pl_sb_reset(ptScene->sbtSkinVertexDataBuffer);

    plEntity tEntity = ptScene->sbtDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent* ptObject   = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
    plMeshComponent*   ptMesh     = gptECS->get_component(ptScene->ptComponentLibrary, gptMesh->get_ecs_type_key_mesh(), ptObject->tMesh);

    const uint32_t uIndexCount = (uint32_t)ptMesh->szIndexCount;
    const uint32_t uVertexCount = (uint32_t)ptMesh->szVertexCount;

    // stride within storage buffer
    uint32_t uStride = 0;
    uint32_t uSkinStride = 0;

    // calculate vertex stream mask based on provided data
    if(ptMesh->ptVertexNormals)               { uStride += 1; }
    if(ptMesh->ptVertexTangents)              { uStride += 1; }
    if(ptMesh->ptVertexColors[0])             { uStride += 1; }
    if(ptMesh->ptVertexColors[1])             { uStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[0]) { uStride += 1; }

    uint64_t ulVertexStreamMask = 0;

    // calculate vertex stream mask based on provided data
    if(ptMesh->ptVertexPositions)  { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_POSITION; }
    if(ptMesh->ptVertexNormals)    { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(ptMesh->ptVertexTangents)   { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(ptMesh->ptVertexWeights[0]) { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(ptMesh->ptVertexWeights[1]) { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(ptMesh->ptVertexJoints[0])  { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(ptMesh->ptVertexJoints[1])  { uSkinStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

    plFreeListNode* ptIndexBufferNode = gptFreeList->get_node(&ptScene->tIndexBufferFreeList, uIndexCount * sizeof(uint32_t));
    plFreeListNode* ptVertexBufferNode = gptFreeList->get_node(&ptScene->tVertexBufferFreeList, uVertexCount * sizeof(plVec3));
    plFreeListNode* ptVertexDataBufferNode = gptFreeList->get_node(&ptScene->tStorageBufferFreeList, uStride * uVertexCount * sizeof(plVec4));
    plFreeListNode* ptSkinVertexDataBufferNode = NULL;
    if(ptMesh->tSkinComponent.uIndex != UINT32_MAX)
        ptSkinVertexDataBufferNode = gptFreeList->get_node(&ptScene->tStorageBufferFreeList, uSkinStride * uVertexCount * sizeof(plVec4));

    bool bResizeNeeded = false;
    if(ptIndexBufferNode == NULL)
    {
        bResizeNeeded = true;
    }
    if(ptVertexBufferNode == NULL)
    {
        bResizeNeeded = true;
    }
    if(ptVertexDataBufferNode == NULL)
    {
        bResizeNeeded = true;
    }
    if(ptSkinVertexDataBufferNode == NULL && ptMesh->tSkinComponent.uIndex != UINT32_MAX)
    {
        bResizeNeeded = true;
    }

    if(bResizeNeeded)
    {
        if(ptIndexBufferNode) gptFreeList->return_node(&ptScene->tIndexBufferFreeList, ptIndexBufferNode);
        if(ptVertexBufferNode) gptFreeList->return_node(&ptScene->tVertexBufferFreeList, ptVertexBufferNode);
        if(ptVertexDataBufferNode) gptFreeList->return_node(&ptScene->tStorageBufferFreeList, ptVertexDataBufferNode);
        if(ptSkinVertexDataBufferNode) gptFreeList->return_node(&ptScene->tStorageBufferFreeList, ptSkinVertexDataBufferNode);
        return false;
    }


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
        ptScene->sbtVertexDataBuffer[i * uStride].x = ptNormal->x;
        ptScene->sbtVertexDataBuffer[i * uStride].y = ptNormal->y;
        ptScene->sbtVertexDataBuffer[i * uStride].z = ptNormal->z;
        ptScene->sbtVertexDataBuffer[i * uStride].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = ptMesh->ptVertexTangents ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->ptVertexTangents[i];
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // texture coordinates 0
    for(uint32_t i = 0; i < 2; i+=2)
    {
        const uint32_t uVertexTexCount0 = ptMesh->ptVertexTextureCoordinates[i] ? uVertexCount : 0;
        const uint32_t uVertexTexCount1 = ptMesh->ptVertexTextureCoordinates[i + 1] ? uVertexCount : 0;

        if(uVertexTexCount1 > 0)
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates0 = &(ptMesh->ptVertexTextureCoordinates[i])[j];
                const plVec2* ptTextureCoordinates1 = &(ptMesh->ptVertexTextureCoordinates[i + 1])[j];
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].x = ptTextureCoordinates0->u;
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].y = ptTextureCoordinates0->v;
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].z = ptTextureCoordinates1->u;
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].w = ptTextureCoordinates1->v;
            }
        }
        else
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates = &(ptMesh->ptVertexTextureCoordinates[i])[j];
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].x = ptTextureCoordinates->u;
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].y = ptTextureCoordinates->v;
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].z = 0.0f;
                ptScene->sbtVertexDataBuffer[j * uStride + uOffset].w = 0.0f;
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
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount0 > 0)
        uOffset += 1;

    const uint32_t uVertexColorCount1 = ptMesh->ptVertexColors[1] ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexColorCount1; i++)
    {
        const plVec4* ptColor = &ptMesh->ptVertexColors[1][i];
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount1 > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    const uint32_t uVertexPosStartIndex  = (uint32_t)(ptVertexBufferNode->uOffset / sizeof(plVec3));

    // add index buffer data
    pl_sb_add_n(ptScene->sbuIndexBuffer, uIndexCount);
    for(uint32_t j = 0; j < uIndexCount; j++)
        ptScene->sbuIndexBuffer[j] = uVertexPosStartIndex + ptMesh->puIndices[j];

    gptStage->stage_buffer_upload(ptScene->tIndexBuffer, ptIndexBufferNode->uOffset, ptScene->sbuIndexBuffer, uIndexCount * sizeof(uint32_t));
    gptStage->stage_buffer_upload(ptScene->tVertexBuffer, ptVertexBufferNode->uOffset, ptMesh->ptVertexPositions, sizeof(plVec3) * uVertexCount);
    gptStage->stage_buffer_upload(ptScene->tStorageBuffer, ptVertexDataBufferNode->uOffset, ptScene->sbtVertexDataBuffer, sizeof(plVec4) * uVertexCount * uStride);
    
    ptScene->sbtDrawables[uDrawableIndex].uIndexCount   = uIndexCount;
    ptScene->sbtDrawables[uDrawableIndex].uVertexCount  = uVertexCount;
    ptScene->sbtDrawables[uDrawableIndex].uIndexOffset  = (uint32_t)(ptIndexBufferNode->uOffset / sizeof(uint32_t));
    ptScene->sbtDrawables[uDrawableIndex].uVertexOffset = (uint32_t)(ptVertexBufferNode->uOffset / sizeof(plVec3));
    ptScene->sbtDrawables[uDrawableIndex].uDataOffset   = (uint32_t)(ptVertexDataBufferNode->uOffset / sizeof(plVec4));

    if(ptMesh->tSkinComponent.uIndex != UINT32_MAX)
    {

        // current attribute offset
        uOffset = 0;

        pl_sb_add_n(ptScene->sbtSkinVertexDataBuffer, uSkinStride * uVertexCount);

        // positions
        const uint32_t uVertexPositionCount = uVertexCount;
        for(uint32_t i = 0; i < uVertexPositionCount; i++)
        {
            const plVec3* ptPosition = &ptMesh->ptVertexPositions[i];
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride].x = ptPosition->x;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride].y = ptPosition->y;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride].z = ptPosition->z;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride].w = 1.0f;
        }

        if(uVertexPositionCount > 0)
            uOffset += 1;

        // normals
        // const uint32_t uVertexNormalCount = ptMesh->ptVertexNormals ? uVertexCount : 0;
        for(uint32_t i = 0; i < uVertexNormalCount; i++)
        {
            ptMesh->ptVertexNormals[i] = pl_norm_vec3(ptMesh->ptVertexNormals[i]);
            const plVec3* ptNormal = &ptMesh->ptVertexNormals[i];
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].x = ptNormal->x;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].y = ptNormal->y;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].z = ptNormal->z;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].w = 0.0f;
        }

        if(uVertexNormalCount > 0)
            uOffset += 1;

        // tangents
        // const uint32_t uVertexTangentCount = ptMesh->ptVertexTangents ? uVertexCount : 0;
        for(uint32_t i = 0; i < uVertexTangentCount; i++)
        {
            const plVec4* ptTangent = &ptMesh->ptVertexTangents[i];
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].x = ptTangent->x;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].y = ptTangent->y;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].z = ptTangent->z;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].w = ptTangent->w;
        }

        if(uVertexTangentCount > 0)
            uOffset += 1;

        // joints 0
        const uint32_t uVertexJoint0Count = ptMesh->ptVertexJoints[0] ? uVertexCount : 0;
        for(uint32_t i = 0; i < uVertexJoint0Count; i++)
        {
            const plVec4* ptJoint = &ptMesh->ptVertexJoints[0][i];
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].x = ptJoint->x;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].y = ptJoint->y;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].z = ptJoint->z;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].w = ptJoint->w;
        }

        if(uVertexJoint0Count > 0)
            uOffset += 1;

        // weights 0
        const uint32_t uVertexWeights0Count = ptMesh->ptVertexWeights[0] ? uVertexCount : 0;
        for(uint32_t i = 0; i < uVertexWeights0Count; i++)
        {
            const plVec4* ptWeight = &ptMesh->ptVertexWeights[0][i];
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].x = ptWeight->x;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].y = ptWeight->y;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].z = ptWeight->z;
            ptScene->sbtSkinVertexDataBuffer[i * uSkinStride + uOffset].w = ptWeight->w;
        }

        if(uVertexWeights0Count > 0)
            uOffset += 1;

        PL_ASSERT(uOffset == uSkinStride && "sanity check");

        // stride within storage buffer
        uint32_t uDestStride = 0;

        // calculate vertex stream mask based on provided data
        if(ptMesh->ptVertexNormals)               { uDestStride += 1; }
        if(ptMesh->ptVertexTangents)              { uDestStride += 1; }
        if(ptMesh->ptVertexColors[0])             { uDestStride += 1; }
        if(ptMesh->ptVertexColors[1])             { uDestStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[0]) { uDestStride += 1; }

        // const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtSkinVertexDataBuffer);

        gptStage->stage_buffer_upload(ptScene->tStorageBuffer, ptSkinVertexDataBufferNode->uOffset, ptScene->sbtSkinVertexDataBuffer, sizeof(plVec4) * uVertexCount * uSkinStride);

        plSkinData tSkinData = {
            .tEntity           = ptMesh->tSkinComponent,
            .uVertexCount      = uVertexCount,
            .iSourceDataOffset = (int)(ptSkinVertexDataBufferNode->uOffset / sizeof(plVec4)),
            .iDestDataOffset   = ptScene->sbtDrawables[uDrawableIndex].uDataOffset,
            .iDestVertexOffset = ptScene->sbtDrawables[uDrawableIndex].uVertexOffset
        };

        plSkinComponent* ptSkinComponent = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tSkinComponentType, ptMesh->tSkinComponent);
        pl_sb_reset(ptSkinComponent->sbtTextureData);
        pl_sb_resize(ptSkinComponent->sbtTextureData, pl_sb_size(ptSkinComponent->sbtJoints) * 8);
        tSkinData.ptFreeListNode = gptFreeList->get_node(&ptScene->tSkinBufferFreeList, pl_sb_size(ptSkinComponent->sbtJoints) * 8 * sizeof(plMat4));

        int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uSkinStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
        tSkinData.tShader = gptShaderVariant->get_compute_shader("skinning", aiSpecializationData);

        tSkinData.tObjectEntity = tEntity;
        pl_temp_allocator_reset(&gptData->tTempAllocator);
        ptScene->sbtDrawables[uDrawableIndex].uSkinIndex = pl_sb_size(ptScene->sbtSkinData);
        pl_sb_push(ptScene->sbtSkinData, tSkinData);
        ptScene->sbtDrawableResources[uDrawableIndex].ptSkinBufferNode = ptSkinVertexDataBufferNode;
    }
    ptScene->sbtDrawableResources[uDrawableIndex].ptIndexBufferNode = ptIndexBufferNode;
    ptScene->sbtDrawableResources[uDrawableIndex].ptVertexBufferNode = ptVertexBufferNode;
    ptScene->sbtDrawableResources[uDrawableIndex].ptDataBufferNode = ptVertexDataBufferNode;
    gptStage->flush();

    if(ptScene->sbtDrawables[uDrawableIndex].uIndexCount == 0) // non-indexed drawables
    {
        ptScene->sbtDrawables[uDrawableIndex].uTriangleCount       = ptScene->sbtDrawables[uDrawableIndex].uVertexCount / 3;
        ptScene->sbtDrawables[uDrawableIndex].uStaticVertexOffset  = ptScene->sbtDrawables[uDrawableIndex].uVertexOffset;
        ptScene->sbtDrawables[uDrawableIndex].uDynamicVertexOffset = 0;
    }
    else // indexed drawables
    {
        ptScene->sbtDrawables[uDrawableIndex].uTriangleCount       = ptScene->sbtDrawables[uDrawableIndex].uIndexCount / 3;
        ptScene->sbtDrawables[uDrawableIndex].uStaticVertexOffset  = 0;
        ptScene->sbtDrawables[uDrawableIndex].uDynamicVertexOffset = ptScene->sbtDrawables[uDrawableIndex].uVertexOffset;
    }

    return true;
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
        PL_ASSERT(ptScene->uTextureIndexCount < PL_MAX_BINDLESS_TEXTURES);
        ulValue = ptScene->uTextureIndexCount++;

        // TODO: handle when greater than 4096
    }
    pl_hm_insert(&ptScene->tTextureIndexHashmap, tTexture.uData, ulValue);
    
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
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atSceneBindGroups[i], &tGlobalBindGroupData);

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
    pl_hm_insert(&ptScene->tCubeTextureIndexHashmap, tTexture.uData, ulValue);
    
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
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atSceneBindGroups[i], &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}

static void
pl__renderer_return_bindless_texture_index(plScene* ptScene, plTextureHandle tTexture)
{
    uint64_t uIndex = 0;
    if(pl_hm_has_key_ex(&ptScene->tTextureIndexHashmap, tTexture.uData, &uIndex))
    {
        pl_hm_remove(&ptScene->tTextureIndexHashmap, tTexture.uData);
    }
}

static void
pl__renderer_return_bindless_cube_texture_index(plScene* ptScene, plTextureHandle tTexture)
{
    uint64_t uIndex = 0;
    if(pl_hm_has_key_ex(&ptScene->tCubeTextureIndexHashmap, tTexture.uData, &uIndex))
    {
        pl_hm_remove(&ptScene->tCubeTextureIndexHashmap, tTexture.uData);
    }
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
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = 4096,
        .pcDebugName = "global buffer"
    };

    const plBufferDesc atViewBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_UNIFORM,
        .szByteSize = sizeof(plGpuViewData),
        .pcDebugName = "probe view buffer"
    };

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGpuDirectionLightShadow),
        .pcDebugName = "shadow data buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    tProbeData.tDShadowCameraBuffers   = pl__renderer_create_staging_buffer(&atCameraBuffersDesc, "directional shadow camera buffer", 0);
    tProbeData.tDLightShadowDataBuffer = pl__renderer_create_staging_buffer(&atLightShadowDataBufferDesc, "directional shadow buffer", 0);

    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };
    tProbeData.tDShadowBG = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);

    const plBindGroupUpdateBufferData atBufferData1[] = 
    {
        {
            .tBuffer       = tProbeData.tDShadowCameraBuffers,
            .uSlot         = 0,
            .szBufferRange = atCameraBuffersDesc.szByteSize
        },
        {
            .tBuffer       = ptScene->atInstanceBuffer[0],
            .uSlot         = 1,
            .szBufferRange = sizeof(plShadowInstanceBufferData) * 10000
        }
    };

    plBindGroupUpdateData tDShadowBGData = {
        .uBufferCount = 2,
        .atBufferBindings = atBufferData1
    };

    gptGfx->update_bind_group(gptData->ptDevice, tProbeData.tDShadowBG, &tDShadowBGData);

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

    // buffers
    tProbeData.tView2Buffer = pl__renderer_create_staging_buffer(&atView2BuffersDesc, "scene", 0);
    tProbeData.tViewBuffer = pl__renderer_create_staging_buffer(&atViewBuffersDesc, "view buffer", 0);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

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

    const plRenderPassDesc tTransparentRenderPassDesc = {
        .tLayout = gptData->tTransparentRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_LOAD,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_LOAD,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = tProbeData.tTargetSize.x, .y = tProbeData.tTargetSize.y}
    };


    for(uint32_t uFace = 0; uFace < 6; uFace++)
    {
        tProbeData.atRenderPasses[uFace] = gptGfx->create_render_pass(gptData->ptDevice, &tRenderPassDesc, atAttachmentSets[uFace]);
        tProbeData.atTransparentRenderPasses[uFace] = gptGfx->create_render_pass(gptData->ptDevice, &tTransparentRenderPassDesc, atAttachmentSets[uFace]);
    }

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
    tProbeData.tGGXEnvTexture = pl__renderer_create_texture(&tTextureDesc, "ggx texture", 0, PL_TEXTURE_USAGE_SAMPLED);

    tProbeData.uLambertianEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, tProbeData.tLambertianEnvTexture);
    tProbeData.uGGXEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, tProbeData.tGGXEnvTexture);
    tProbeData.iMips = (int)tTextureDesc.uMips;
    
    plObjectComponent* ptProbeObj = gptECS->add_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tProbeHandle);
    ptProbeObj->tMesh = ptScene->tProbeMesh;
    ptProbeObj->tTransform = tProbeHandle;

    const plBindGroupDesc tGBufferFillBG1Desc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("gbuffer_fill", 1),
        .pcDebugName = "gbuffer fill bg1"
    };

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tViewBGLayout,
        .pcDebugName = "probe scene bg"
    };

    // create g-buffer fill bind group 1
    const plBindGroupUpdateBufferData tGBufferFillBG1BufferData = {
        .tBuffer       = tProbeData.tView2Buffer,
        .uSlot         = 0,
        .szBufferRange = sizeof(plGpuViewData) * 6
    };

    const plBindGroupUpdateData tGBufferFillBG1Data = {
        .uBufferCount     = 1,
        .atBufferBindings = &tGBufferFillBG1BufferData
    };

    tProbeData.tGBufferBG = gptGfx->create_bind_group(gptData->ptDevice, &tGBufferFillBG1Desc);
    gptGfx->update_bind_group(gptData->ptDevice, tProbeData.tGBufferBG, &tGBufferFillBG1Data);

    const plBindGroupUpdateBufferData tViewBGBufferData[] = 
    {
        { .uSlot = 0, .tBuffer = tProbeData.tViewBuffer,            .szBufferRange = sizeof(plGpuViewData) },
        { .uSlot = 1, .tBuffer = tProbeData.tView2Buffer,           .szBufferRange = sizeof(plGpuViewData) * 6 },
        { .uSlot = 2, .tBuffer = ptScene->atPointLightBuffer[0],            .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightBuffer[0])->tDesc.szByteSize},
        { .uSlot = 3, .tBuffer = ptScene->atSpotLightBuffer[0],            .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightBuffer[0])->tDesc.szByteSize},
        { .uSlot = 4, .tBuffer = ptScene->atDirectionLightBuffer[0],            .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atDirectionLightBuffer[0])->tDesc.szByteSize},
        { .uSlot = 5, .tBuffer = tProbeData.tDLightShadowDataBuffer, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, tProbeData.tDLightShadowDataBuffer)->tDesc.szByteSize},
        { .uSlot = 6, .tBuffer = ptScene->atPointLightShadowDataBuffer[0],  .szBufferRange =  gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightShadowDataBuffer[0])->tDesc.szByteSize},
        { .uSlot = 7, .tBuffer = ptScene->atSpotLightShadowDataBuffer[0],  .szBufferRange =  gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightShadowDataBuffer[0])->tDesc.szByteSize},
        { .uSlot = 8, .tBuffer = ptScene->tGPUProbeDataBuffers,    .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tGPUProbeDataBuffers)->tDesc.szByteSize},
    };

    const plBindGroupUpdateData tViewBGData = {
        .uBufferCount      = 9,
        .atBufferBindings  = tViewBGBufferData
    };
    tProbeData.tViewBG = gptGfx->create_bind_group(gptData->ptDevice, &tViewBGDesc);
    gptGfx->update_bind_group(gptData->ptDevice, tProbeData.tViewBG, &tViewBGData);

    pl_sb_push(ptScene->sbtProbeData, tProbeData);
};

static void
pl__renderer_create_environment_map_from_texture(plScene* ptScene, plEnvironmentProbeData* ptProbe)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    
    plComputeShaderHandle tCubeFilterSpecularShader = gptShaderVariant->get_compute_shader("cube_filter_specular", NULL);
    plComputeShaderHandle tCubeFilterDiffuseShader = gptShaderVariant->get_compute_shader("cube_filter_diffuse", NULL);
    plComputeShaderHandle tCubeFilterSheenShader = gptShaderVariant->get_compute_shader("cube_filter_sheen", NULL);

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
    
    plBindGroupHandle atFullBindGroupHandles[2] = {tCubeFilterBGSet0, tCubeFilterBGSet1};

    {

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 3,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 2
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 2");
        const plBeginCommandInfo tBeginInfo0 = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo0);
        gptGfx->push_debug_group(ptCommandBuffer, "Env Filtering", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

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

        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tCubeFilterDiffuseShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
        gptGfx->bind_compute_shader(ptComputeEncoder, tCubeFilterDiffuseShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);

        gptGfx->pop_debug_group(ptCommandBuffer);
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
                .uGroupCountZ     = 3,
                .uThreadPerGroupX = 16,
                .uThreadPerGroupY = 16,
                .uThreadPerGroupZ = 2
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

    // sheen
    if(ptScene->tFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED)
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

        plTexture* ptEnvTexture = gptGfx->get_texture(ptDevice, ptProbe->tSheenEnvTexture);
        for(int i = ptEnvTexture->tDesc.uMips - 1; i != -1; i--)
        {
            int currentWidth = iResolution >> i;

            if(currentWidth < 16)
                continue;

            // const size_t uCurrentFaceSize = (size_t)currentWidth * (size_t)currentWidth * 4 * sizeof(float);

            const plDispatch tDispach = {
                .uGroupCountX     = (uint32_t)currentWidth / 16,
                .uGroupCountY     = (uint32_t)currentWidth / 16,
                .uGroupCountZ     = 3,
                .uThreadPerGroupX = 16,
                .uThreadPerGroupY = 16,
                .uThreadPerGroupZ = 2
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
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, tCubeFilterSheenShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptComputeEncoder, tCubeFilterSheenShader);
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
                gptGfx->copy_buffer_to_texture(ptBlitEncoder, ptScene->atFilterWorkingBuffers[j], ptProbe->tSheenEnvTexture, 1, &tBufferImageCopy);
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

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_add_skybox_drawable(plScene* ptScene)
{
    plFreeListNode* ptIndexBufferNode = gptFreeList->get_node(&ptScene->tIndexBufferFreeList, 36 * sizeof(uint32_t));
    plFreeListNode* ptVertexBufferNode = gptFreeList->get_node(&ptScene->tVertexBufferFreeList, 8 * sizeof(plVec3));

    const uint32_t uStartIndex = (uint32_t)(ptVertexBufferNode->uOffset / sizeof(plVec3));

    uint32_t auIndexBuffer[] = {
        uStartIndex + 0,
        uStartIndex + 2,
        uStartIndex + 1,
        uStartIndex + 2,
        uStartIndex + 3,
        uStartIndex + 1,
        uStartIndex + 1,
        uStartIndex + 3,
        uStartIndex + 5,
        uStartIndex + 3,
        uStartIndex + 7,
        uStartIndex + 5,
        uStartIndex + 2,
        uStartIndex + 6,
        uStartIndex + 3,
        uStartIndex + 3,
        uStartIndex + 6,
        uStartIndex + 7,
        uStartIndex + 4,
        uStartIndex + 5,
        uStartIndex + 7,
        uStartIndex + 4,
        uStartIndex + 7,
        uStartIndex + 6,
        uStartIndex + 0,
        uStartIndex + 4,
        uStartIndex + 2,
        uStartIndex + 2,
        uStartIndex + 4,
        uStartIndex + 6,
        uStartIndex + 0,
        uStartIndex + 1,
        uStartIndex + 4,
        uStartIndex + 1,
        uStartIndex + 5,
        uStartIndex + 4,
    };

    const float fCubeSide = 1.0f;
    plVec3 atVertexBuffer[] = {
        {-fCubeSide, -fCubeSide, -fCubeSide},
        { fCubeSide, -fCubeSide, -fCubeSide},
        {-fCubeSide,  fCubeSide, -fCubeSide},
        { fCubeSide,  fCubeSide, -fCubeSide},
        {-fCubeSide, -fCubeSide,  fCubeSide},
        { fCubeSide, -fCubeSide,  fCubeSide},
        {-fCubeSide,  fCubeSide,  fCubeSide},
        { fCubeSide,  fCubeSide,  fCubeSide}
    };

    const plDrawable tDrawable = {
        .uIndexCount     = 36,
        .uVertexCount    = 8,
        .uIndexOffset    = (uint32_t)(ptIndexBufferNode->uOffset / sizeof(uint32_t)),
        .uVertexOffset   = uStartIndex,
        .uDataOffset     = pl_sb_size(ptScene->sbtVertexDataBuffer),
        .uTransformIndex = ptScene->uNextTransformIndex++
    };
    ptScene->tSkyboxDrawable = tDrawable;

    gptStage->stage_buffer_upload(ptScene->tIndexBuffer, ptIndexBufferNode->uOffset, auIndexBuffer, 36 * sizeof(uint32_t));
    gptStage->stage_buffer_upload(ptScene->tVertexBuffer, ptVertexBufferNode->uOffset, atVertexBuffer, sizeof(plVec3) * 8);
    gptStage->flush();
}

typedef struct _plGbufferFillPassInfo
{
    uint32_t*         sbuVisibleDeferredEntities;
    uint32_t          uGlobalIndex;
    plBindGroupHandle tBG2;
    plDrawArea*       ptArea;
    plShaderHandle*   sbtShaders;
} plGbufferFillPassInfo;

static void
pl__render_view_gbuffer_fill_pass(plScene* ptScene, plRenderEncoder* ptSceneEncoder, plGbufferFillPassInfo* ptInfo)
{
    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    gptGfx->push_render_debug_group(ptSceneEncoder, "G-Buffer Fill", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});
    gptGfx->set_depth_bias(ptSceneEncoder, 0.0f, 0.0f, 0.0f);

    const uint32_t uVisibleDeferredDrawCount = pl_sb_size(ptInfo->sbuVisibleDeferredEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleDeferredDrawCount);

    for(uint32_t i = 0; i < uVisibleDeferredDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptInfo->sbuVisibleDeferredEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plGpuDynData* ptDynamicData = (plGpuDynData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
            ptDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = ptInfo->sbtShaders[ptInfo->sbuVisibleDeferredEntities[i]],
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = tDrawable.tIndexBuffer,
                .uIndexOffset   = tDrawable.uIndexOffset,
                .uTriangleCount = tDrawable.uTriangleCount,
                .uVertexOffset  = tDrawable.uStaticVertexOffset,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    ptInfo->tBG2
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = tDrawable.uTransformIndex,
                .uInstanceCount  = 1
            });
        }
    }

    gptGfx->draw_stream(ptSceneEncoder, 1, ptInfo->ptArea);

    gptGfx->pop_render_debug_group(ptSceneEncoder);
}

typedef struct _plDeferredLightingPassInfo
{
    uint32_t uGlobalIndex;
    uint32_t uProbe;
    plBindGroupHandle tBG2;
    bool bProbe;
    plDrawArea* ptArea;
} plDeferredLightingPassInfo;

static void
pl__render_view_deferred_lighting_pass(plScene* ptScene, plRenderEncoder* ptSceneEncoder, plBindGroupHandle tViewBG, plDeferredLightingPassInfo* ptInfo)
{
    gptGfx->push_render_debug_group(ptSceneEncoder, "Deferred Lighting", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    gptGfx->push_render_debug_group(ptSceneEncoder, "Lights", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    const uint32_t uPointLightCount = pl_sb_size(ptScene->sbtPointLightData);
    const uint32_t uSpotLightCount = pl_sb_size(ptScene->sbtSpotLightData);
    const uint32_t uDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLightData);

    if(gptData->tRuntimeOptions.bPunctualLighting)
    {
        gptGfx->reset_draw_stream(ptStream, uPointLightCount + uSpotLightCount);
        for(uint32_t uLightIndex = 0; uLightIndex < uPointLightCount; uLightIndex++)
        {
            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptLightingDynamicData->iLightIndex = (int)uLightIndex;
            
            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader = ptScene->tPointLightingShader,
                .auDynamicBuffers = {
                    tLightingDynamicData.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexOffset   = ptScene->tUnitSphereDrawable.uIndexOffset,
                .uTriangleCount = ptScene->tUnitSphereDrawable.uTriangleCount,
                .uVertexOffset  = ptScene->tUnitSphereDrawable.uVertexOffset,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tViewBG,
                    ptInfo->tBG2
                },
                .auDynamicBufferOffsets = {
                    tLightingDynamicData.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount  = 1
            });
        }

        for(uint32_t uLightIndex = 0; uLightIndex < uSpotLightCount; uLightIndex++)
        {

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptLightingDynamicData->iLightIndex = (int)uLightIndex;
            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader = ptScene->tSpotLightingShader,
                .auDynamicBuffers = {
                    tLightingDynamicData.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexOffset   = ptScene->tUnitSphereDrawable.uIndexOffset,
                .uTriangleCount = ptScene->tUnitSphereDrawable.uTriangleCount,
                .uVertexOffset  = ptScene->tUnitSphereDrawable.uVertexOffset,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tViewBG,
                    ptInfo->tBG2
                },
                .auDynamicBufferOffsets = {
                    tLightingDynamicData.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount  = 1
            });
        }

        gptGfx->draw_stream(ptSceneEncoder, 1, ptInfo->ptArea);
        gptGfx->reset_draw_stream(ptStream, uDirectionLightCount);
        for(uint32_t uLightIndex = 0; uLightIndex < uDirectionLightCount; uLightIndex++)
        {

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptLightingDynamicData->iLightIndex = (int)uLightIndex;
            ptLightingDynamicData->iProbe = (int)ptInfo->bProbe;
            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader = ptScene->tDirectionalLightingShader,
                .auDynamicBuffers = {
                    tLightingDynamicData.uBufferHandle
                },
                .uIndexOffset   = 0,
                .uTriangleCount = 1,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tViewBG,
                    ptInfo->tBG2
                },
                .auDynamicBufferOffsets = {
                    tLightingDynamicData.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount  = 1
            });
        }
        gptGfx->draw_stream(ptSceneEncoder, 1, ptInfo->ptArea);
    }
    
    gptGfx->pop_render_debug_group(ptSceneEncoder);

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);

    if(uProbeCount > 0 && !ptInfo->bProbe && gptData->tRuntimeOptions.bImageBasedLighting)
    {
        gptGfx->push_render_debug_group(ptSceneEncoder, "Probes", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});
        gptGfx->reset_draw_stream(ptStream, 1);
        plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
        plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
        ptLightingDynamicData->uGlobalIndex = 0;
        ptLightingDynamicData->iLightIndex = -1;
        ptLightingDynamicData->iProbeCount = pl_sb_size(ptScene->sbtProbeData);

        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader = ptScene->tProbeLightingShader,
            .auDynamicBuffers = {
                tLightingDynamicData.uBufferHandle
            },
            .uIndexOffset   = 0,
            .uTriangleCount = 1,
            .atBindGroups = {
                ptScene->atSceneBindGroups[uFrameIdx],
                tViewBG,
                ptInfo->tBG2
            },
            .auDynamicBufferOffsets = {
                tLightingDynamicData.uByteOffset
            },
            .uInstanceOffset = 0,
            .uInstanceCount  = 1
        });
        gptGfx->draw_stream(ptSceneEncoder, 1, ptInfo->ptArea);
        gptGfx->pop_render_debug_group(ptSceneEncoder);
    }
    gptGfx->pop_render_debug_group(ptSceneEncoder);
}

static void
pl__render_view_deferred_lighting_debug_pass(plView* ptView, plRenderEncoder* ptSceneEncoder, plBindGroupHandle tViewBG)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plScene*       ptScene   = ptView->ptParentScene;

    const plVec2 tDimensions = ptView->tTargetSize;

    plDrawArea tArea = {
        .ptDrawStream = ptStream,
        .atScissors = 
        {
            {
                .uWidth  = (uint32_t)tDimensions.x,
                .uHeight = (uint32_t)tDimensions.y,
            }
        },
        .atViewports =
        {
            {
                .fWidth  = tDimensions.x,
                .fHeight = tDimensions.y,
                .fMaxDepth = 1.0f
            }
        }
    };

    gptGfx->reset_draw_stream(ptStream, 1);
    plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
    plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
    ptLightingDynamicData->uGlobalIndex = 0;
    ptLightingDynamicData->iLightIndex = -1;

    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
    {
        .tShader = ptScene->tDirectionalLightingShader,
        .auDynamicBuffers = {
            tLightingDynamicData.uBufferHandle
        },
        .uIndexOffset   = 0,
        .uTriangleCount = 1,
        .atBindGroups = {
            ptScene->atSceneBindGroups[uFrameIdx],
            tViewBG,
            ptView->tLightingBindGroup
        },
        .auDynamicBufferOffsets = {
            tLightingDynamicData.uByteOffset
        },
        .uInstanceOffset = 0,
        .uInstanceCount  = 1
    });
    gptGfx->draw_stream(ptSceneEncoder, 1, &tArea);
}

static void
pl__render_view_skybox_pass(plScene* ptScene, plRenderEncoder* ptSceneEncoder, plBindGroupHandle tViewBG, plMat4* ptTransform, plDrawArea* ptArea, uint32_t uFace)
{
    gptGfx->push_render_debug_group(ptSceneEncoder, "Skybox", (plVec4){0.33f, 0.02f, 0.80f, 1.0f});

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    plDynamicBinding tSkyboxDynamicData = pl__allocate_dynamic_data(ptDevice);
    plGpuDynSkybox* ptSkyboxDynamicData = (plGpuDynSkybox*)tSkyboxDynamicData.pcData;
    ptSkyboxDynamicData->tModel = *ptTransform;
    ptSkyboxDynamicData->uGlobalIndex = uFace;

    gptGfx->reset_draw_stream(ptStream, 1);
    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
    {
        .tShader        = gptShaderVariant->get_shader("skybox", NULL, NULL, NULL, &gptData->tRenderPassLayout),
        .auDynamicBuffers = {
            tSkyboxDynamicData.uBufferHandle
        },
        .atVertexBuffers = {
            ptScene->tVertexBuffer,
        },
        .tIndexBuffer   = ptScene->tIndexBuffer,
        .uIndexOffset   = ptScene->tSkyboxDrawable.uIndexOffset,
        .uTriangleCount = ptScene->tSkyboxDrawable.uIndexCount / 3,
        .atBindGroups = {
            ptScene->atSceneBindGroups[uFrameIdx],
            tViewBG,
            ptScene->tSkyboxBindGroup
        },
        .auDynamicBufferOffsets = {
            tSkyboxDynamicData.uByteOffset
        },
        .uInstanceOffset = 0,
        .uInstanceCount  = 1
    });
    gptGfx->draw_stream(ptSceneEncoder, 1, ptArea);

    gptGfx->pop_render_debug_group(ptSceneEncoder);
}

typedef struct _plForwardPassInfo
{
    uint32_t* sbuVisibleEntities;
    uint32_t uGlobalIndex;
    plShaderHandle* sbtShaders;
    plDrawArea* ptArea;
    bool bProbe;
} plForwardPassInfo;

static void
pl__render_view_forward_pass(plScene* ptScene, plRenderEncoder* ptSceneEncoder, plBindGroupHandle tViewBG, plForwardPassInfo* ptInfo)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    const uint32_t uVisibleForwardDrawCount = pl_sb_size(ptInfo->sbuVisibleEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleForwardDrawCount);

    for(uint32_t i = 0; i < uVisibleForwardDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptInfo->sbuVisibleEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            plGpuDynForwardData* ptDynamicData = (plGpuDynForwardData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
            ptDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptDynamicData->iPointLightCount = pl_sb_size(ptScene->sbtPointLights);
            ptDynamicData->iSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
            ptDynamicData->iDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);
            ptDynamicData->iProbeCount = pl_sb_size(ptScene->sbtProbeData);
            ptDynamicData->iProbe = (int)ptInfo->bProbe;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = ptInfo->sbtShaders[ptInfo->sbuVisibleEntities[i]],
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = tDrawable.tIndexBuffer,
                .uIndexOffset   = tDrawable.uIndexOffset,
                .uTriangleCount = tDrawable.uTriangleCount,
                .uVertexOffset  = tDrawable.uStaticVertexOffset,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tViewBG
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = tDrawable.uTransformIndex,
                .uInstanceCount = tDrawable.uInstanceCount
            });
        }
    }
    gptGfx->draw_stream(ptSceneEncoder, 1, ptInfo->ptArea);
}


static plCommandBuffer*
pl__render_view_full_screen_blit(plView* ptView, const plBeginCommandInfo* ptSceneBeginInfo)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();

    const plBeginCommandInfo tSceneBlitBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "main scene blit");
    gptGfx->begin_command_recording(ptSceneCmdBuffer, ptSceneBeginInfo);

    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptSceneCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    plImageCopy tFrameCopy = {

        .uSourceExtentX = (uint32_t)ptView->tTargetSize.x,
        .uSourceExtentY = (uint32_t)ptView->tTargetSize.y,
        .uSourceExtentZ = 1,
        .uSourceLayerCount = 1,
        .tSourceImageUsage = PL_TEXTURE_USAGE_SAMPLED,
        .tDestinationImageUsage = PL_TEXTURE_USAGE_SAMPLED,
        .uDestinationLayerCount = 1,
    };
    gptGfx->copy_texture(ptBlitEncoder, ptView->tRawOutputTexture, ptView->tTransmissionTexture, 1, &tFrameCopy);
    gptGfx->generate_mipmaps(ptBlitEncoder, ptView->tTransmissionTexture);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    return ptSceneCmdBuffer;
}

static void
pl__render_view_transmission_pass(plView* ptView, plRenderEncoder* ptSceneEncoder, plBindGroupHandle tViewBG)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plScene*       ptScene   = ptView->ptParentScene;

    const plVec2 tDimensions = ptView->tTargetSize;
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    plDrawArea tArea = {
        .ptDrawStream = ptStream,
        .atScissors = 
        {
            {
                .uWidth  = (uint32_t)tDimensions.x,
                .uHeight = (uint32_t)tDimensions.y,
            }
        },
        .atViewports =
        {
            {
                .fWidth  = tDimensions.x,
                .fHeight = tDimensions.y,
                .fMaxDepth = 1.0f
            }
        }
    };


    gptGfx->push_render_debug_group(ptSceneEncoder, "Transmission Pass", (plVec4){0.33f, 0.20f, 0.10f, 1.0f});

    const uint32_t uVisibleTransmissionDrawCount = pl_sb_size(ptView->sbuVisibleTransmissionEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleTransmissionDrawCount);
    for(uint32_t i = 0; i < uVisibleTransmissionDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbuVisibleTransmissionEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            plGpuDynForwardData* ptDynamicData = (plGpuDynForwardData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
            ptDynamicData->uGlobalIndex = 0;
            ptDynamicData->iPointLightCount = pl_sb_size(ptScene->sbtPointLights);
            ptDynamicData->iSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
            ptDynamicData->iDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);
            ptDynamicData->iProbeCount = pl_sb_size(ptScene->sbtProbeData);

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = ptScene->sbtRegularShaders[ptView->sbuVisibleTransmissionEntities[i]],
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = tDrawable.tIndexBuffer,
                .uIndexOffset   = tDrawable.uIndexOffset,
                .uTriangleCount = tDrawable.uTriangleCount,
                .uVertexOffset  = tDrawable.uStaticVertexOffset,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tViewBG
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = tDrawable.uTransformIndex,
                .uInstanceCount = tDrawable.uInstanceCount
            });
        }
    }
    gptGfx->draw_stream(ptSceneEncoder, 1, &tArea);
    gptGfx->pop_render_debug_group(ptSceneEncoder);
}

static void
pl__render_view_grid_pass(plView* ptView, plRenderEncoder* ptSceneEncoder, plCamera* ptCamera)
{
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    const plVec2 tDimensions = ptView->tTargetSize;
    plDrawArea tArea = {
        .ptDrawStream = ptStream,
        .atScissors = 
        {
            {
                .uWidth  = (uint32_t)tDimensions.x,
                .uHeight = (uint32_t)tDimensions.y,
            }
        },
        .atViewports =
        {
            {
                .fWidth  = tDimensions.x,
                .fHeight = tDimensions.y,
                .fMaxDepth = 1.0f
            }
        }
    };

    ptView->bShowGrid = false; // resubmitted every frame

    plShaderHandle tGridShader = gptShaderVariant->get_shader("grid", NULL, NULL, NULL, &gptData->tTransparentRenderPassLayout);
    gptGfx->bind_shader(ptSceneEncoder, tGridShader);

    plDynamicBinding tGridDynamicBinding = pl__allocate_dynamic_data(ptDevice);
    plGpuDynGrid* ptGridDynamicData = (plGpuDynGrid*)tGridDynamicBinding.pcData;
    const float fGridFactor = pl_squaref(ptCamera->fFarZ) - pl_squaref(ptCamera->tPos.y);
    ptGridDynamicData->fGridSize = fGridFactor > 0.0f ? sqrtf(fGridFactor) : 100.0f;
    ptGridDynamicData->fGridCellSize = gptData->tRuntimeOptions.fGridCellSize;
    ptGridDynamicData->fGridMinPixelsBetweenCells = gptData->tRuntimeOptions.fGridMinPixelsBetweenCells;
    ptGridDynamicData->tGridColorThin = gptData->tRuntimeOptions.tGridColorThin;
    ptGridDynamicData->tGridColorThick = gptData->tRuntimeOptions.tGridColorThick;
    ptGridDynamicData->tViewDirection.xyz = ptCamera->_tForwardVec;
    ptGridDynamicData->fCameraXPos = ptCamera->tPos.x;
    ptGridDynamicData->fCameraZPos = ptCamera->tPos.z;
    ptGridDynamicData->tCameraViewProjection = tMVP;
    
    gptGfx->bind_graphics_bind_groups(ptSceneEncoder, tGridShader, 0, 0, NULL, 1, &tGridDynamicBinding);

    gptGfx->set_scissor_region(ptSceneEncoder, tArea.atScissors);
    gptGfx->set_viewport(ptSceneEncoder, tArea.atViewports);

    plDraw tGridDraw = {
        .uVertexCount   = 6,
        .uInstanceCount = 1,
    };
    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw(ptSceneEncoder, 1, &tGridDraw);
}

static void
pl__render_view_debug_pass(plView* ptView, plCamera* ptCamera, plCamera* ptCullCamera)
{
    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plScene* ptScene = ptView->ptParentScene;
    const plVec2 tDimensions = ptView->tTargetSize;

    // bounding boxes
    const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlinedEntities);
    if(uOutlineDrawableCount > 0 && gptData->tRuntimeOptions.bShowSelectedBoundingBox)
    {
        const plVec4 tOutlineColor = (plVec4){0.0f, (float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 1.0f};
        for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtOutlinedEntities[i]);
            gptDraw->add_3d_aabb(ptView->pt3DSelectionDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tOutlineColor), .fThickness = 0.01f});
            
        }
    }

    if(gptData->tRuntimeOptions.bShowOrigin)
    {
        const plMat4 tTransform = pl_identity_mat4();
        gptDraw->add_3d_transform(ptView->pt3DDrawList, &tTransform, 10.0f, (plDrawLineOptions){.fThickness = 0.02f});
    }

    if(ptCullCamera && ptCullCamera != ptCamera)
    {
        const plDrawFrustumDesc tFrustumDesc = {
            .fAspectRatio = ptCullCamera->fAspectRatio,
            .fFarZ        = ptCullCamera->fFarZ,
            .fNearZ       = ptCullCamera->fNearZ,
            .fYFov        = ptCullCamera->fFieldOfView
        };
        gptDraw->add_3d_frustum(ptView->pt3DSelectionDrawList, &ptCullCamera->tTransformMat, tFrustumDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
    }

    const plBeginCommandInfo tPostBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "tonemap");
    gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

    plRenderEncoder* ptSceneEncoder = gptGfx->begin_render_pass(ptPostCmdBuffer, ptView->tFinalRenderPass, NULL);
    gptGfx->set_depth_bias(ptSceneEncoder, 0.0f, 0.0f, 0.0f);
    gptDraw->submit_3d_drawlist(ptView->pt3DDrawList, ptSceneEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST, 1);
    gptDraw->submit_3d_drawlist(ptView->pt3DSelectionDrawList, ptSceneEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST, 1);
    gptGfx->end_render_pass(ptSceneEncoder);

    gptGfx->end_command_recording(ptPostCmdBuffer);

    const plSubmitInfo tPostSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
    gptGfx->return_command_buffer(ptPostCmdBuffer);
}

static void
pl__render_view_pick_pass(plView* ptView, plBindGroupHandle tViewBG, plCommandBuffer* ptSceneCmdBuffer)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Picking Submission");

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plScene*       ptScene   = ptView->ptParentScene;

    const plVec2 tDimensions = ptView->tTargetSize;

    plDrawArea tArea = {
        .ptDrawStream = ptStream,
        .atScissors = 
        {
            {
                .uWidth  = (uint32_t)tDimensions.x,
                .uHeight = (uint32_t)tDimensions.y,
            }
        },
        .atViewports =
        {
            {
                .fWidth  = tDimensions.x,
                .fHeight = tDimensions.y,
                .fMaxDepth = 1.0f
            }
        }
    };

    plShaderHandle tPickShader = gptShaderVariant->get_shader("picking", NULL, NULL, NULL, &gptData->tPickRenderPassLayout);

    plBuffer* ptPickBuffer = gptGfx->get_buffer(ptDevice, ptView->atPickBuffer[uFrameIdx]);
    // memset(ptPickBuffer->tMemoryAllocation.pHostMapped, 0, sizeof(uint32_t) * 2);

    ptView->auHoverResultProcessing[uFrameIdx] = true;
    ptView->auHoverResultReady[uFrameIdx] = false;
    ptView->bRequestHoverCheck = false;

    plRenderEncoder* ptPickEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tPickRenderPass, NULL);
    gptGfx->set_depth_bias(ptPickEncoder, 0.0f, 0.0f, 0.0f);
    gptGfx->bind_shader(ptPickEncoder, tPickShader);
    gptGfx->bind_vertex_buffer(ptPickEncoder, ptScene->tVertexBuffer);

    gptGfx->set_scissor_region(ptPickEncoder, tArea.atScissors);
    gptGfx->set_viewport(ptPickEncoder, tArea.atViewports);

    plBindGroupHandle atBindGroups[2] = {tViewBG, ptView->atPickBindGroup[uFrameIdx]};
    gptGfx->bind_graphics_bind_groups(ptPickEncoder, tPickShader, 0, 2, atBindGroups, 0, NULL);

    const uint32_t uVisibleDrawCount = pl_sb_size(ptView->sbtVisibleDrawables);
    *gptData->pdDrawCalls += (double)uVisibleDrawCount;

    plVec2 tMousePos = gptIOI->get_mouse_pos();
    tMousePos = pl_sub_vec2(tMousePos, ptView->tHoverOffset);
    tMousePos = pl_div_vec2(tMousePos, ptView->tHoverWindowRatio);
    
    for(uint32_t i = 0; i < uVisibleDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbtVisibleDrawables[i]];

        uint32_t uId = tDrawable.tEntity.uIndex;
        
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
        
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        plGpuDynPick* ptDynamicData = (plGpuDynPick*)tDynamicBinding.pcData;
        
        ptDynamicData->uID = uId;
        ptDynamicData->tModel = ptTransform->tWorld;
        ptDynamicData->tMousePos.xy = tMousePos;
        
        gptGfx->bind_graphics_bind_groups(ptPickEncoder, tPickShader, 0, 0, NULL, 1, &tDynamicBinding);

        if(tDrawable.uIndexCount > 0)
        {
            plDrawIndex tDraw = {
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexCount    = tDrawable.uIndexCount,
                .uIndexStart    = tDrawable.uIndexOffset,
                .uInstanceCount = 1
            };
            gptGfx->draw_indexed(ptPickEncoder, 1, &tDraw);
        }
        else
        {
            plDraw tDraw = {
                .uVertexStart   = tDrawable.uVertexOffset,
                .uInstanceCount = 1,
                .uVertexCount   = tDrawable.uVertexCount
            };
            gptGfx->draw(ptPickEncoder, 1, &tDraw);
        }
    }
    gptGfx->end_render_pass(ptPickEncoder);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__render_view_uv_pass(plView* ptView)
{
    // for convience
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice* ptDevice  = gptData->ptDevice;

    const plBeginCommandInfo tUVBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptUVCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "uv");
    gptGfx->begin_command_recording(ptUVCmdBuffer, &tUVBeginInfo);

    plRenderEncoder* ptUVEncoder = gptGfx->begin_render_pass(ptUVCmdBuffer, ptView->tUVRenderPass, NULL);
    gptGfx->push_render_debug_group(ptUVEncoder, "UV Map", (plVec4){0.33f, 0.72f, 0.10f, 1.0f});

    // submit nonindexed draw using basic API
    plShaderHandle tUVShader = gptShaderVariant->get_shader("uvmap", NULL, NULL, NULL, &gptData->tUVRenderPassLayout);
    gptGfx->bind_shader(ptUVEncoder, tUVShader);

    plTexture* ptTargetTexture = gptGfx->get_texture(ptDevice, ptView->tFinalTexture);

    plScissor tUVScissor = {
        .uWidth  = (uint32_t)ptTargetTexture->tDesc.tDimensions.x,
        .uHeight = (uint32_t)ptTargetTexture->tDesc.tDimensions.y
    };

    plRenderViewport tUVViewport = {
        .fWidth = ptTargetTexture->tDesc.tDimensions.x,
        .fHeight = ptTargetTexture->tDesc.tDimensions.y,
        .fMaxDepth = 1.0f
    };

    gptGfx->set_scissor_region(ptUVEncoder, &tUVScissor);
    gptGfx->set_viewport(ptUVEncoder, &tUVViewport);

    plDraw tDraw = {
        .uVertexCount   = 3,
        .uInstanceCount = 1,
    };
    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw(ptUVEncoder, 1, &tDraw);

    // end render pass
    gptGfx->pop_render_debug_group(ptUVEncoder);
    gptGfx->end_render_pass(ptUVEncoder);

    gptGfx->end_command_recording(ptUVCmdBuffer);

    const plSubmitInfo tUVSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptUVCmdBuffer, &tUVSubmitInfo);
    gptGfx->return_command_buffer(ptUVCmdBuffer);
}

static void
pl__render_view_jfa_pass(plView* ptView)
{
    // for convience
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice* ptDevice  = gptData->ptDevice;
    plScene* ptScene   = ptView->ptParentScene;
    const plVec2 tDimensions = ptView->tTargetSize;

    // find next power of 2
    uint32_t uJumpDistance = 1;
    uint32_t uHalfWidth = gptData->tRuntimeOptions.uOutlineWidth / 2;
    if (uHalfWidth && !(uHalfWidth & (uHalfWidth - 1))) 
        uJumpDistance = uHalfWidth;
    while(uJumpDistance < uHalfWidth)
        uJumpDistance <<= 1;

    // calculate number of jumps necessary
    uint32_t uJumpSteps = 0;
    while(uJumpDistance)
    {
        uJumpSteps++;
        uJumpDistance >>= 1;
    }

    float fJumpDistance = (float)uHalfWidth;
    if(pl_sb_size(ptScene->sbtOutlinedEntities) == 0)
        uJumpSteps = 1;

    const plDispatch tDispach = {
        .uGroupCountX     = (uint32_t)ceilf(tDimensions.x / 8.0f),
        .uGroupCountY     = (uint32_t)ceilf(tDimensions.y / 8.0f),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };

    plComputeShaderHandle tJFAShader = gptShaderVariant->get_compute_shader("jumpfloodalgo", NULL);
    for(uint32_t i = 0; i < uJumpSteps; i++)
    {
        const plBeginCommandInfo tJumpBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };

        plCommandBuffer* ptJumpCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "JFA");
        gptGfx->begin_command_recording(ptJumpCmdBuffer, &tJumpBeginInfo);

        // begin main renderpass (directly to swapchain)
        plComputeEncoder* ptJumpEncoder = gptGfx->begin_compute_pass(ptJumpCmdBuffer, NULL);
        gptGfx->push_compute_debug_group(ptJumpEncoder, "JFA", (plVec4){0.73f, 0.02f, 0.80f, 1.0f});
        gptGfx->pipeline_barrier_compute(ptJumpEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

        ptView->uLastUVIndex = (i % 2 == 0) ? 1 : 0;

        // submit nonindexed draw using basic API
        gptGfx->bind_compute_shader(ptJumpEncoder, tJFAShader);

        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        plVec4* ptJumpDistance = (plVec4*)tDynamicBinding.pcData;
        ptJumpDistance->x = tDimensions.x;
        ptJumpDistance->y = tDimensions.y;
        ptJumpDistance->z = fJumpDistance;

        gptGfx->bind_compute_bind_groups(ptJumpEncoder, tJFAShader, 0, 1, &ptView->atJFABindGroups[i % 2], 1, &tDynamicBinding);
        gptGfx->dispatch(ptJumpEncoder, 1, &tDispach);

        // end render pass
        gptGfx->pipeline_barrier_compute(ptJumpEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->pop_compute_debug_group(ptJumpEncoder);
        gptGfx->end_compute_pass(ptJumpEncoder);

        // end recording
        gptGfx->end_command_recording(ptJumpCmdBuffer);

        const plSubmitInfo tJumpSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()},
        };
        gptGfx->submit_command_buffer(ptJumpCmdBuffer, &tJumpSubmitInfo);
        gptGfx->return_command_buffer(ptJumpCmdBuffer);

        fJumpDistance = fJumpDistance / 2.0f;
        if(fJumpDistance < 1.0f)
            fJumpDistance = 1.0f;
    }
}

static void
pl__render_view_outline_pass(plView* ptView, plCamera* ptCamera)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    const plBeginCommandInfo tOutlineBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "Outline");
    gptGfx->begin_command_recording(ptCommandBuffer, &tOutlineBeginInfo);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plScene*      ptScene    = ptView->ptParentScene;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plVec2 tDimensions = ptView->tTargetSize;

    plDrawArea tArea = {
        .atScissors = 
        {
            {
                .uWidth  = (uint32_t)tDimensions.x,
                .uHeight = (uint32_t)tDimensions.y,
            }
        },
        .atViewports =
        {
            {
                .fWidth  = tDimensions.x,
                .fHeight = tDimensions.y,
                .fMaxDepth = 1.0f
            }
        }
    };

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tPostProcessRenderPass, NULL);
    gptGfx->push_render_debug_group(ptEncoder, "Outline", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

    plDraw tDraw = {
        .uInstanceCount = 1,
        .uVertexCount   = 3
    };

    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

    plGpuDynPost* ptDynamicData = (plGpuDynPost*)tDynamicBinding.pcData;
    const plVec4 tOutlineColor = (plVec4){(float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 0.0f, 1.0f};
    ptDynamicData->fTargetWidth = (float)gptData->tRuntimeOptions.uOutlineWidth * tOutlineColor.r + 1.0f;
    ptDynamicData->tOutlineColor = tOutlineColor;
    plVec2 tUVScale = {0};
    pl_renderer_get_view_texture(ptView, &tUVScale);
    ptDynamicData->fXScale = tUVScale.x;
    ptDynamicData->fYScale = tUVScale.y;

    plShaderHandle tTonemapShader = gptShaderVariant->get_shader("jumpfloodalgo2", NULL, NULL, NULL, &gptData->tPostProcessRenderPassLayout);
    gptGfx->bind_shader(ptEncoder, tTonemapShader);
    plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atOutlineBG[ptView->uLastUVIndex]};
    gptGfx->bind_graphics_bind_groups(ptEncoder, tTonemapShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
    gptGfx->set_scissor_region(ptEncoder, tArea.atScissors);
    gptGfx->set_viewport(ptEncoder, tArea.atViewports);

    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw(ptEncoder, 1, &tDraw);

    // gptDraw->submit_3d_drawlist(ptView->pt3DGizmoDrawList, ptEncoder, tDimensions.x, tDimensions.y, ptMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);
    gptDraw->submit_3d_drawlist(ptView->pt3DGizmoDrawList, ptEncoder, tDimensions.x, tDimensions.y, &tMVP, 0, 1);
    // gptDraw->submit_3d_drawlist(ptView->pt3DDrawList, ptEncoder, tDimensions.x, tDimensions.y, ptMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST, 1);
    // gptDraw->submit_3d_drawlist(ptView->pt3DSelectionDrawList, ptEncoder, tDimensions.x, tDimensions.y, ptMVP, 0, 1);

    gptGfx->pop_render_debug_group(ptEncoder);
    gptGfx->end_render_pass(ptEncoder);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tOutlineSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tOutlineSubmitInfo);
    gptGfx->return_command_buffer(ptCommandBuffer);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__render_view_bloom_pass(plView* ptView)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "bloom");

    // for convience
    // const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;

    for(uint32_t i = 0; i < gptData->tRuntimeOptions.uBloomChainLength - 1; i++)
    {

        const int sourceMip = i;
        const int targetMip = i + 1;

        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("bloom_downsample", 0),
            .pcDebugName = "temp bind group c0"
        };
        plBindGroupHandle tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

        const plBindGroupUpdateTextureData tTonemapTextureData[] = 
        {
            { // target
                .tTexture      = ptView->sbtBloomDownChain[targetMip],
                .uSlot         = 0,
                .tType         = PL_TEXTURE_BINDING_TYPE_STORAGE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE,
            },
            { // source
                .tTexture      = i == 0 ? ptView->tFinalTexture : ptView->sbtBloomDownChain[sourceMip],
                .uSlot         = 1,
                .tType         = PL_TEXTURE_BINDING_TYPE_SAMPLED,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
            }
        };

        plBindGroupUpdateSamplerData tGlobalSamplerData[] = {
            { .uSlot = 2, .tSampler = gptData->tSamplerLinearClamp }
        };

        const plBindGroupUpdateData tTonemapBGData = {
            .uTextureCount = 2,
            .atTextureBindings = tTonemapTextureData,
            .uSamplerCount = 1,
            .atSamplerBindings = tGlobalSamplerData
        };
        gptGfx->update_bind_group(gptData->ptDevice, tTonemapBG, &tTonemapBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tTonemapBG);

        const plBeginCommandInfo tPostBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "bloom_downsample");
        gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

        plBlitEncoder* ptTonemapPrepEncoder0 = gptGfx->begin_blit_pass(ptPostCmdBuffer);
        gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[0].tTexture, PL_TEXTURE_USAGE_STORAGE, PL_TEXTURE_USAGE_SAMPLED);
        gptGfx->end_blit_pass(ptTonemapPrepEncoder0);

        plComputeEncoder* ptPostEncoder = gptGfx->begin_compute_pass(ptPostCmdBuffer, NULL);
        gptGfx->push_compute_debug_group(ptPostEncoder, "bloom_downsample", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
        gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

        plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        plGpuDynBloomData* ptTonemapData = (plGpuDynBloomData*)tTonemapDynamicBinding.pcData;
        ptTonemapData->iMipLevel = i;

        plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("bloom_downsample", NULL);
        gptGfx->bind_compute_shader(ptPostEncoder, tTonemapShader);
        gptGfx->bind_compute_bind_groups(ptPostEncoder, tTonemapShader, 0, 1, &tTonemapBG, 1, &tTonemapDynamicBinding);

        plDispatch tTonemapDispatch = {
            .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
            .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptPostEncoder, 1, &tTonemapDispatch);

        gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->pop_compute_debug_group(ptPostEncoder);
        gptGfx->end_compute_pass(ptPostEncoder);

        plBlitEncoder* ptTonemapPrepEncoder1 = gptGfx->begin_blit_pass(ptPostCmdBuffer);
        gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[0].tTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
        gptGfx->end_blit_pass(ptTonemapPrepEncoder1);

        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
        gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
        gptGfx->return_command_buffer(ptPostCmdBuffer);
    }

    for(uint32_t i = 0; i < gptData->tRuntimeOptions.uBloomChainLength - 1; i++)
    {

        const int targetMip = gptData->tRuntimeOptions.uBloomChainLength - 2 - i;
        const int sourceMip = targetMip + 1;

        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("bloom_upsample", 0),
            .pcDebugName = "temp bind group c0"
        };
        plBindGroupHandle tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

        const plBindGroupUpdateTextureData tTonemapTextureData[] = 
        {
            { // target
                .tTexture      = ptView->sbtBloomUpChain[targetMip],
                .uSlot         = 0,
                .tType         = PL_TEXTURE_BINDING_TYPE_STORAGE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE,
            },
            { // target previous mip
                .tTexture      = ptView->sbtBloomUpChain[sourceMip],
                .uSlot         = 1,
                .tType         = PL_TEXTURE_BINDING_TYPE_SAMPLED,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
            },
            { // source
                .tTexture      = ptView->sbtBloomDownChain[sourceMip],
                .uSlot         = 2,
                .tType         = PL_TEXTURE_BINDING_TYPE_SAMPLED,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
            }
        };

        plBindGroupUpdateSamplerData tGlobalSamplerData[] = {
            { .uSlot = 3, .tSampler = gptData->tSamplerLinearClamp }
        };

        const plBindGroupUpdateData tTonemapBGData = {
            .uTextureCount = 3,
            .atTextureBindings = tTonemapTextureData,
            .uSamplerCount = 1,
            .atSamplerBindings = tGlobalSamplerData
        };
        gptGfx->update_bind_group(gptData->ptDevice, tTonemapBG, &tTonemapBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tTonemapBG);

        const plBeginCommandInfo tPostBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "bloom_upsample");
        gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

        plBlitEncoder* ptTonemapPrepEncoder0 = gptGfx->begin_blit_pass(ptPostCmdBuffer);

        gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[0].tTexture, PL_TEXTURE_USAGE_STORAGE, PL_TEXTURE_USAGE_SAMPLED);
        
        gptGfx->end_blit_pass(ptTonemapPrepEncoder0);

        plComputeEncoder* ptPostEncoder = gptGfx->begin_compute_pass(ptPostCmdBuffer, NULL);
        gptGfx->push_compute_debug_group(ptPostEncoder, "upscale", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
        gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

        plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        plGpuDynBloomData* ptTonemapData = (plGpuDynBloomData*)tTonemapDynamicBinding.pcData;
        ptTonemapData->bloomStrength = gptData->tRuntimeOptions.fBloomStrength;
        ptTonemapData->blurRadius = gptData->tRuntimeOptions.fBloomRadius;
        ptTonemapData->isLowestMip = i == 0 ? 1 : 0;

        plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("bloom_upsample", NULL);
        gptGfx->bind_compute_shader(ptPostEncoder, tTonemapShader);
        gptGfx->bind_compute_bind_groups(ptPostEncoder, tTonemapShader, 0, 1, &tTonemapBG, 1, &tTonemapDynamicBinding);

        plDispatch tTonemapDispatch = {
            .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
            .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptPostEncoder, 1, &tTonemapDispatch);

        gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->pop_compute_debug_group(ptPostEncoder);
        gptGfx->end_compute_pass(ptPostEncoder);

        plBlitEncoder* ptTonemapPrepEncoder1 = gptGfx->begin_blit_pass(ptPostCmdBuffer);
        // gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[1].tTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
        gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[0].tTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
        gptGfx->end_blit_pass(ptTonemapPrepEncoder1);

        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
        gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
        gptGfx->return_command_buffer(ptPostCmdBuffer);
    }

    {
        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("bloom_apply", 0),
            .pcDebugName = "temp bind group c0"
        };
        plBindGroupHandle tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

        const plBindGroupUpdateTextureData tTonemapTextureData[] = 
        {
            { // target
                .tTexture      = ptView->tFinalTexture,
                .uSlot         = 0,
                .tType         = PL_TEXTURE_BINDING_TYPE_STORAGE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE,
            },
            { // target previous mip
                .tTexture      = ptView->sbtBloomUpChain[0],
                .uSlot         = 1,
                .tType         = PL_TEXTURE_BINDING_TYPE_SAMPLED,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
            }
        };

        plBindGroupUpdateSamplerData tGlobalSamplerData[] = {
            { .uSlot = 2, .tSampler = gptData->tSamplerLinearClamp }
        };

        const plBindGroupUpdateData tTonemapBGData = {
            .uTextureCount = 2,
            .atTextureBindings = tTonemapTextureData,
            .uSamplerCount = 1,
            .atSamplerBindings = tGlobalSamplerData
        };
        gptGfx->update_bind_group(gptData->ptDevice, tTonemapBG, &tTonemapBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tTonemapBG);

        const plBeginCommandInfo tPostBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "bloom_apply");
        gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

        plBlitEncoder* ptTonemapPrepEncoder0 = gptGfx->begin_blit_pass(ptPostCmdBuffer);

        gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[0].tTexture, PL_TEXTURE_USAGE_STORAGE, PL_TEXTURE_USAGE_SAMPLED);
        
        // if(i > 0)
            
        gptGfx->end_blit_pass(ptTonemapPrepEncoder0);

        plComputeEncoder* ptPostEncoder = gptGfx->begin_compute_pass(ptPostCmdBuffer, NULL);
        gptGfx->push_compute_debug_group(ptPostEncoder, "bloom_apply", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
        gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

        plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        plGpuDynBloomData* ptTonemapData = (plGpuDynBloomData*)tTonemapDynamicBinding.pcData;
        ptTonemapData->bloomStrength = gptData->tRuntimeOptions.fBloomStrength;
        ptTonemapData->blurRadius = gptData->tRuntimeOptions.fBloomRadius;

        plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("bloom_apply", NULL);
        gptGfx->bind_compute_shader(ptPostEncoder, tTonemapShader);
        gptGfx->bind_compute_bind_groups(ptPostEncoder, tTonemapShader, 0, 1, &tTonemapBG, 1, &tTonemapDynamicBinding);

        plDispatch tTonemapDispatch = {
            .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
            .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptPostEncoder, 1, &tTonemapDispatch);

        gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->pop_compute_debug_group(ptPostEncoder);
        gptGfx->end_compute_pass(ptPostEncoder);

        plBlitEncoder* ptTonemapPrepEncoder1 = gptGfx->begin_blit_pass(ptPostCmdBuffer);
        // gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[1].tTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
        gptGfx->set_texture_usage(ptTonemapPrepEncoder0, tTonemapTextureData[0].tTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
        gptGfx->end_blit_pass(ptTonemapPrepEncoder1);

        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
        gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
        gptGfx->return_command_buffer(ptPostCmdBuffer);
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__render_view_tonemap_pass(plView* ptView)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;


    const plBeginCommandInfo tPostBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "tonemap");
    gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

    plBlitEncoder* ptTonemapPrepEncoder0 = gptGfx->begin_blit_pass(ptPostCmdBuffer);
    gptGfx->set_texture_usage(ptTonemapPrepEncoder0, ptView->tFinalTexture, PL_TEXTURE_USAGE_STORAGE, PL_TEXTURE_USAGE_SAMPLED);
    gptGfx->end_blit_pass(ptTonemapPrepEncoder0);

    plComputeEncoder* ptPostEncoder = gptGfx->begin_compute_pass(ptPostCmdBuffer, NULL);
    gptGfx->push_compute_debug_group(ptPostEncoder, "Tonemap Compute", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
    gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

    plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice);
    plGpuDynTonemap* ptTonemapData = (plGpuDynTonemap*)tTonemapDynamicBinding.pcData;
    ptTonemapData->iMode = gptData->tRuntimeOptions.tTonemapMode;
    ptTonemapData->fExposure = gptData->tRuntimeOptions.fExposure;
    ptTonemapData->fBrightness = gptData->tRuntimeOptions.fBrightness;
    ptTonemapData->fContrast = gptData->tRuntimeOptions.fContrast;
    ptTonemapData->fSaturation = gptData->tRuntimeOptions.fSaturation;

    plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("tonemap", NULL);
    gptGfx->bind_compute_shader(ptPostEncoder, tTonemapShader);
    gptGfx->bind_compute_bind_groups(ptPostEncoder, tTonemapShader, 0, 1, &ptView->tTonemapBG, 1, &tTonemapDynamicBinding);

    plDispatch tTonemapDispatch = {
        .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
        .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptPostEncoder, 1, &tTonemapDispatch);

    gptGfx->pipeline_barrier_compute(ptPostEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
    gptGfx->pop_compute_debug_group(ptPostEncoder);
    gptGfx->end_compute_pass(ptPostEncoder);

    plBlitEncoder* ptTonemapPrepEncoder1 = gptGfx->begin_blit_pass(ptPostCmdBuffer);
    gptGfx->set_texture_usage(ptTonemapPrepEncoder1, ptView->tFinalTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
    gptGfx->end_blit_pass(ptTonemapPrepEncoder1);

    gptGfx->end_command_recording(ptPostCmdBuffer);

    const plSubmitInfo tPostSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
    gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
    gptGfx->return_command_buffer(ptPostCmdBuffer);
}

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

    const plBindGroupUpdateSamplerData tSkyboxBG1SamplerData = {
        .tSampler = gptData->tSamplerLinearRepeat,
        .uSlot    = 1
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
                .tPosDouble   = {(double)ptProbeTransform->tTranslation.x, (double)ptProbeTransform->tTranslation.y, (double)ptProbeTransform->tTranslation.z},
                .fNearZ       = 0.26f,
                .fFarZ        = ptProbeComp->fRange,
                .fFieldOfView = PL_PI_2,
                .fAspectRatio = 1.0f,
                .fRoll        = atPitchYawRoll[uFace].z
            };
            gptCamera->set_pitch_yaw(&atEnvironmentCamera[uFace], atPitchYawRoll[uFace].x, atPitchYawRoll[uFace].y);
            gptCamera->update(&atEnvironmentCamera[uFace]);
        }

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
            pl_sb_reset(ptProbe->sbuVisibleTransmissionEntities[uFace]);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                {
                    if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                        pl_sb_push(ptProbe->sbuVisibleDeferredEntities[uFace], uDrawableIndex);
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                        pl_sb_push(ptProbe->sbuVisibleForwardEntities[uFace], uDrawableIndex);
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
                        pl_sb_push(ptProbe->sbuVisibleTransmissionEntities[uFace], uDrawableIndex);
                }
            }

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            plGbufferFillPassInfo tGbufferFillPassInfo = {
                .sbuVisibleDeferredEntities = ptProbe->sbuVisibleDeferredEntities[uFace],
                .uGlobalIndex = uFace,
                .tBG2 = ptProbe->tGBufferBG,
                .ptArea = &tArea,
                .sbtShaders = ptScene->sbtProbeShaders
            };
            pl__render_view_gbuffer_fill_pass(ptScene, ptProbeEncoder, &tGbufferFillPassInfo);
            

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            gptGfx->next_subpass(ptProbeEncoder, NULL);

            plDeferredLightingPassInfo tDeferredLightingPassInfo = {
                .uGlobalIndex = uFace,
                .uProbe = uProbeIndex,
                .tBG2 = ptProbe->atLightingBindGroup[uFace],
                .ptArea = &tArea,
                .bProbe = true
            };
            pl__render_view_deferred_lighting_pass(ptScene, ptProbeEncoder, ptProbe->tViewBG, &tDeferredLightingPassInfo);

            gptGfx->next_subpass(ptProbeEncoder, NULL);

            if(ptScene->tSkyboxTexture.uIndex != 0 && ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY)
            {
                plMat4 tTransformMat = pl_mat4_translate_vec3(ptProbeTransform->tTranslation);
                pl__render_view_skybox_pass(ptScene, ptProbeEncoder, ptProbe->tViewBG, &tTransformMat, &tArea, uFace);  
            }

            plForwardPassInfo tForwardPassInfo = {
                .sbuVisibleEntities = ptProbe->sbuVisibleForwardEntities[uFace],
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true,
                .sbtShaders = ptScene->sbtProbeShaders
            };
            pl__render_view_forward_pass(ptScene, ptProbeEncoder, ptProbe->tViewBG, &tForwardPassInfo);

            plForwardPassInfo tForwardPassInfo2 = {
                .sbuVisibleEntities = ptProbe->sbuVisibleTransmissionEntities[uFace],
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true,
                .sbtShaders = ptScene->sbtProbeShaders
            };
            pl__render_view_forward_pass(ptScene, ptProbeEncoder, ptProbe->tViewBG, &tForwardPassInfo2);
    
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
