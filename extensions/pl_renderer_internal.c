/*
   pl_renderer_internal.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] job system tasks
// [SECTION] scene render helpers
// [SECTION] water simulation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] job system tasks
//-----------------------------------------------------------------------------

static inline plCloudShadowGrid
pl_cloud_shadow_grid(plVec3 tDesiredCenter, plVec3 tHalfSize, const plVec3 atAxes[3], uint32_t uResolution, uint32_t uSlices)
{
    plCloudShadowGrid tGrid = {0};
    for(uint32_t i = 0; i < 3; i++)
    {
        const plVec3 tAxis = atAxes[i];
        const float fCount = (float)(i == 2 ? uSlices : uResolution);
        const float fRequired = fabsf(tAxis.x) * tHalfSize.x + fabsf(tAxis.y) * tHalfSize.y + fabsf(tAxis.z) * tHalfSize.z;
        // Stable size buckets prevent curvature adjustments from continuously
        // changing texel pitch. Reserve half a cell for center rounding.
        const float fExtent = exp2f(ceilf(log2f(fmaxf(fRequired, 0.001f) / (1.0f - 1.0f / fCount))));
        const float fPitch = 2.0f * fExtent / fCount;
        const float fProjected = tDesiredCenter.x * tAxis.x + tDesiredCenter.y * tAxis.y + tDesiredCenter.z * tAxis.z;
        const float fSnapped = floorf(fProjected / fPitch + 0.5f) * fPitch;
        tGrid.tCenter.x += tAxis.x * fSnapped;
        tGrid.tCenter.y += tAxis.y * fSnapped;
        tGrid.tCenter.z += tAxis.z * fSnapped;
        tGrid.atAxes[i].xyz = tAxis;
        tGrid.atAxes[i].w = fExtent;
    }
    return tGrid;
}

static inline plVec3d
pl__to_double_vec(plVec3 tVec)
{
    return (plVec3d){(double)tVec.x, (double)tVec.y, (double)tVec.z};
}

static plPlane
pl_make_plane_from_point_normal(plVec3 tPoint, plVec3 tNormal)
{
    tNormal = pl_norm_vec3(tNormal);

    plPlane tPlane = {0};
    tPlane.tDirection = tNormal;
    tPlane.fOffset = pl_dot_vec3(tNormal, tPoint);
    return tPlane;
}

static void
pl__camera_build_orthographic_frustum(const plCamera* ptCamera, plFrustum* ptFrustum)
{
    plVec3 C = ptCamera->tPositionF;
    plVec3 F = pl_norm_vec3(ptCamera->tForwardVec);
    plVec3 R = pl_norm_vec3(ptCamera->tRightVec);
    plVec3 U = pl_norm_vec3(ptCamera->tUpVec);

    float fHalfWidth  = ptCamera->fWidth  * 0.5f;
    float fHalfHeight = ptCamera->fHeight * 0.5f;

    plVec3 LeftPoint   = pl_sub_vec3(C, pl_mul_vec3_scalarf(R, fHalfWidth));
    plVec3 RightPoint  = pl_add_vec3(C, pl_mul_vec3_scalarf(R, fHalfWidth));
    plVec3 TopPoint    = pl_add_vec3(C, pl_mul_vec3_scalarf(U, fHalfHeight));
    plVec3 BottomPoint = pl_sub_vec3(C, pl_mul_vec3_scalarf(U, fHalfHeight));

    plVec3 NearPoint = pl_add_vec3(C, pl_mul_vec3_scalarf(F, ptCamera->fNearZ));
    plVec3 FarPoint  = pl_add_vec3(C, pl_mul_vec3_scalarf(F, ptCamera->fFarZ));

    // Inward-facing normals using your working perspective convention.
    plVec3 NLeft   = R;
    plVec3 NRight  = pl_mul_vec3_scalarf(R, -1.0f);

    // These are the important ones relative to my previous version:
    // top should point inward/down, bottom should point inward/up.
    //
    // If your perspective top/bottom needed flipping, this is the orthographic
    // equivalent that should match it.
    plVec3 NTop    = U;
    plVec3 NBottom = pl_mul_vec3_scalarf(U, -1.0f);

    plVec3 NNear = F;
    plVec3 NFar  = pl_mul_vec3_scalarf(F, -1.0f);

    ptFrustum->atPlanes[0] = pl_make_plane_from_point_normal(LeftPoint,   NLeft);
    ptFrustum->atPlanes[1] = pl_make_plane_from_point_normal(RightPoint,  NRight);
    ptFrustum->atPlanes[2] = pl_make_plane_from_point_normal(TopPoint,    NTop);
    ptFrustum->atPlanes[3] = pl_make_plane_from_point_normal(BottomPoint, NBottom);
    ptFrustum->atPlanes[4] = pl_make_plane_from_point_normal(NearPoint,   NNear);
    ptFrustum->atPlanes[5] = pl_make_plane_from_point_normal(FarPoint,    NFar);
}

static void
pl__camera_build_perspective_frustum(const plCamera* ptCamera, plFrustum* ptFrustum)
{
    plVec3 C = ptCamera->tPositionF;
    plVec3 F = pl_norm_vec3(ptCamera->tForwardVec);
    plVec3 R = pl_norm_vec3(ptCamera->tRightVec);
    plVec3 U = pl_norm_vec3(ptCamera->tUpVec);

    float fTanY = tanf(ptCamera->fYFov * 0.5f);
    float fTanX = fTanY * ptCamera->fAspectRatio;

    // Direction to frustum edge centers.
    plVec3 LDir = pl_norm_vec3(pl_sub_vec3(F, pl_mul_vec3_scalarf(R, fTanX)));
    plVec3 RDir = pl_norm_vec3(pl_add_vec3(F, pl_mul_vec3_scalarf(R, fTanX)));
    plVec3 TDir = pl_norm_vec3(pl_add_vec3(F, pl_mul_vec3_scalarf(U, fTanY)));
    plVec3 BDir = pl_norm_vec3(pl_sub_vec3(F, pl_mul_vec3_scalarf(U, fTanY)));

    // Side plane normals, pointing inward.
    //
    // These assume:
    //   R = camera right
    //   U = camera up
    //   F = camera forward
    //
    // and inside test is dot(n, p) + d >= 0.
    plVec3 NLeft   = pl_norm_vec3(pl_cross_vec3(LDir, U));
    plVec3 NRight  = pl_norm_vec3(pl_cross_vec3(U, RDir));
    plVec3 NTop    = pl_norm_vec3(pl_cross_vec3(R, TDir));
    plVec3 NBottom = pl_norm_vec3(pl_cross_vec3(BDir, R));

    plVec3 NearPoint = pl_add_vec3(C, pl_mul_vec3_scalarf(F, ptCamera->fNearZ));
    plVec3 FarPoint  = pl_add_vec3(C, pl_mul_vec3_scalarf(F, ptCamera->fFarZ));

    plVec3 NNear = F;
    plVec3 NFar  = pl_mul_vec3_scalarf(F, -1.0f);

    NTop = pl_mul_vec3_scalarf(NTop, -1.0f);
    NBottom = pl_mul_vec3_scalarf(NBottom, -1.0f);
    ptFrustum->atPlanes[0] = pl_make_plane_from_point_normal(C,         NLeft);
    ptFrustum->atPlanes[1] = pl_make_plane_from_point_normal(C,         NRight);
    ptFrustum->atPlanes[2] = pl_make_plane_from_point_normal(C,         NTop);
    ptFrustum->atPlanes[3] = pl_make_plane_from_point_normal(C,         NBottom);
    ptFrustum->atPlanes[4] = pl_make_plane_from_point_normal(NearPoint, NNear);
    ptFrustum->atPlanes[5] = pl_make_plane_from_point_normal(FarPoint,  NFar);
}

static void
pl__renderer_cull_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plCullData* ptCullData = pData;
    plScene* ptScene = ptCullData->ptScene;
    uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[tInvoData.uGlobalIndex];
    plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
    plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
    plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);
    ptDrawable->bCulled = true;

    if(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE)
    {
        if(ptDrawable->uInstanceCount == 1) // ignore instanced
        {
            if(gptGjk->pen(pl_gjk_support_aabb, &ptObject->tAABB, pl_gjk_support_frustum, &ptCullData->tFrustum, NULL))
            {
                ptDrawable->bCulled = false;
            }
        }
        else
        {
            ptDrawable->bCulled = false;
        }
    }
}

static void
pl__renderer_cull_point_light_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plCullData* ptCullData = pData;
    plScene* ptScene = ptCullData->ptScene;
    uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[tInvoData.uGlobalIndex];
    plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
    plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
    plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);
    ptDrawable->bCulled = true;

    if(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE)
    {
        if(ptDrawable->uInstanceCount == 1) // ignore instanced
        {
            if(gptGjk->pen(pl_gjk_support_aabb, &ptObject->tAABB, pl_gjk_support_sphere, &ptCullData->tSphere, NULL))
            {
                ptDrawable->bCulled = false;
            }
        }
        else
        {
            ptDrawable->bCulled = false;
        }
    }
}

static void
pl__renderer_cull_spot_light_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plCullData* ptCullData = pData;
    plScene* ptScene = ptCullData->ptScene;
    uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[tInvoData.uGlobalIndex];
    plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
    plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
    plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);
    ptDrawable->bCulled = true;

    if(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE)
    {
        if(ptDrawable->uInstanceCount == 1) // ignore instanced
        {
            if(gptGjk->pen(pl_gjk_support_aabb, &ptObject->tAABB, pl_gjk_support_cone, &ptCullData->tCone, NULL))
            {
                ptDrawable->bCulled = false;
            }
        }
        else
        {
            ptDrawable->bCulled = false;
        }
    }
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

        const plPassResources tPassResources = {
            .atBuffers = {
                { .tHandle = ptScene->tStorageBuffer, .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->tVertexBuffer,  .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atDynamicSkinBuffer[gptGfx->get_current_frame_index()], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE }
            }
        };

        gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->push_debug_group(ptCommandBuffer, "Skinning Compute", (plVec4){1.0f, 0.0f, 1.0f, 1.0f});
        const plEcsTypeKey tTransformComponentType = gptTransform->get_ecs_type_key_transform();
        for(uint32_t i = 0; i < uSkinCount; i++)
        {
            if(!ptScene->sbtSkinData[i].bActive)
                continue;

            PL_RENDERER_STAT("renderer / skinning dispatches", 1);
            PL_RENDERER_STAT("renderer / skinned vertices", ptScene->sbtSkinData[i].uVertexCount);
                
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynSkinData));
            plGpuDynSkinData* ptDynamicData = (plGpuDynSkinData*)tDynamicBinding.pcData;
            ptDynamicData->iSourceDataOffset = ptScene->sbtSkinData[i].iSourceDataOffset;
            ptDynamicData->iDestDataOffset = ptScene->sbtSkinData[i].iDestDataOffset;
            ptDynamicData->iDestVertexOffset = ptScene->sbtSkinData[i].iDestVertexOffset;
            ptDynamicData->uMaxSize = ptScene->sbtSkinData[i].uVertexCount;
            ptDynamicData->uMatrixOffset = (uint32_t)ptScene->sbtSkinData[i].ptFreeListNode->uOffset / sizeof(plMat4);

            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtSkinData[i].tObjectEntity);
            plTransformComponent* ptObjectTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);

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
            gptGfx->bind_compute_bind_groups(ptCommandBuffer, ptScene->tSkinningShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptCommandBuffer, ptScene->tSkinningShader);
            gptGfx->dispatch(ptCommandBuffer, 1, &tDispach);
        }
        gptGfx->pop_debug_group(ptCommandBuffer);
        gptGfx->end_compute_pass(ptCommandBuffer);
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

    const uint32_t uProbeCount = gptEcs->get_components(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, (void**)&ptProbes, NULL);
    uint32_t uLightCount = pl_sb_size(ptScene->sbtPointLights);

    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[uLightIndex].tEntity);

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
            .tType       = ptLight->tType
        };
        pl_sb_push(ptScene->sbtShadowRectData, tPackData);

    }

    uLightCount = pl_sb_size(ptScene->sbtSpotLights);
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[uLightIndex].tEntity);

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
            .tType       = ptLight->tType
        };
        pl_sb_push(ptScene->sbtShadowRectData, tPackData);
    }

    const uint32_t uRectCount = pl_sb_size(ptScene->sbtShadowRects);
    gptRect->pack(ptScene->uShadowAtlasResolution, ptScene->uShadowAtlasResolution, ptScene->sbtShadowRects, uRectCount);

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

    // ptScene->tSunPackRect = pl_sb_pop(ptScene->sbtShadowRects);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return bPacked;
}

static bool
pl__renderer_pack_view_shadow_atlas(plScene* ptScene)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    
    pl_sb_reset(ptScene->sbtShadowViewRects);

    plEnvironmentProbeComponent* ptProbes = NULL;

    const uint32_t uProbeCount = gptEcs->get_components(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, (void**)&ptProbes, NULL);
    uint32_t uLightCount = pl_sb_size(ptScene->sbtDirectionLights);

    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    const plPackRect tSunPackRect = {
        .iWidth  = (int)ptSettings->tShadows.uShadowResolution * (int)ptSettings->tShadows.uShadowCascadeCount,
        .iHeight = (int)ptSettings->tShadows.uShadowResolution,
        .iId     = 0
    };
    pl_sb_push(ptScene->sbtShadowViewRects, tSunPackRect);

    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[uLightIndex].tEntity);

        // skip light if it doesn't cast shadows
        if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
        {
            continue;
        }
        const plPackRect tPackRect = {
            .iWidth  = (int)(ptLight->uShadowResolution),
            .iHeight = (int)ptLight->uShadowResolution,
            .iId     = (int)uLightIndex
        };
        pl_sb_push(ptScene->sbtShadowViewRects, tPackRect);
    }

    // pack rects
    const uint32_t uRectCount = pl_sb_size(ptScene->sbtShadowViewRects);
    gptRect->pack(ptScene->uSunShadowAtlasResolution, ptScene->uSunShadowAtlasResolution, ptScene->sbtShadowViewRects, uRectCount);

    // ensure rects are packed
    bool bPacked = true;
    for(uint32_t i = 0; i < uRectCount; i++)
    {
        if(!ptScene->sbtShadowViewRects[i].iWasPacked)
        {
            bPacked = false;
            PL_ASSERT(false && "Shadow sun atlas too small");
            break;
        }
    }

    // ptScene->tSunPackRect = pl_sb_pop(ptScene->sbtShadowRects);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return bPacked;
}

static void
pl__renderer_generate_shadow_maps(plCommandBuffer* ptCommandBuffer, plScene* ptScene, const plCamera** atCameras, uint32_t uCameraCount)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    const uint32_t uLightCount = pl_sb_size(ptScene->sbtShadowRects);
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];

        plLightComponent* ptLight = NULL;
        if(ptData->tType == PL_LIGHT_TYPE_POINT)     ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_SPOT) ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[ptData->uLightIndex].tEntity);

        if(ptData->tType == PL_LIGHT_TYPE_DIRECTIONAL)
            continue;

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {

            plGpuPointLightShadow* ptShadowData = &ptScene->sbtPointLightShadowData[ptScene->sbtPointLightData[ptData->uLightIndex].iShadowIndex];
            ptShadowData->iShadowMapTexIdx = ptScene->uShadowAtlasIndex;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 atCamViewProjs[6] = {0};

            plCamera tShadowCamera = {0};
            gptCamera->init(&tShadowCamera);
            tShadowCamera.eProjectionType = PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            tShadowCamera.eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
            tShadowCamera.tPosition   = (plVec3d){(double)ptLight->tPosition.x, (double)ptLight->tPosition.y, (double)ptLight->tPosition.z};
            tShadowCamera.fNearZ       = ptLight->fRadius;
            tShadowCamera.fFarZ        = ptLight->fRange;
            tShadowCamera.fYFov        = PL_PI_2;
            tShadowCamera.fAspectRatio = 1.0f;

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

                gptCamera->set_euler(&tShadowCamera, atPitchYaw[i].x, atPitchYaw[i].y, 0.0f);
                gptCamera->update(&tShadowCamera);
                atCamViewProjs[i] = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
                ptShadowData->viewProjMat[i] = atCamViewProjs[i];
            }

            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[ptScene->sbtPointLights[ptData->uLightIndex].ptShadowBufferOffset->uOffset], atCamViewProjs, sizeof(plMat4) * 6);
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {
            plGpuSpotLightShadow* ptShadowData = &ptScene->sbtSpotLightShadowData[ptScene->sbtSpotLightData[ptData->uLightIndex].iShadowIndex];
            ptShadowData->iShadowMapTexIdx = ptScene->uShadowAtlasIndex;
            ptShadowData->fFactor = (float)ptLight->uShadowResolution / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fXOffset = (float)ptRect->iX / (float)ptScene->uShadowAtlasResolution;
            ptShadowData->fYOffset = (float)ptRect->iY / (float)ptScene->uShadowAtlasResolution;

            plMat4 tCamViewProjs = {0};

            plCamera tShadowCamera = {0};
            gptCamera->init(&tShadowCamera);
            tShadowCamera.eProjectionType = PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            tShadowCamera.eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
            tShadowCamera.tPosition   = (plVec3d){(double)ptLight->tPosition.x, (double)ptLight->tPosition.y, (double)ptLight->tPosition.z};
            tShadowCamera.fNearZ       = ptLight->fRadius;
            tShadowCamera.fFarZ        = ptLight->fRange;
            tShadowCamera.fYFov        = ptLight->fOuterConeAngle * 2.0f;
            tShadowCamera.fAspectRatio = 1.0f;

            plVec3 tDirection = pl_norm_vec3(ptLight->tDirection);
            gptCamera->look_at(&tShadowCamera, pl__to_double_vec(ptLight->tPosition), pl__to_double_vec(pl_add_vec3(ptLight->tPosition, tDirection)), (plVec3){0.0f, 1.0f, 0.0f});
            gptCamera->update(&tShadowCamera);
            tCamViewProjs = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            ptShadowData->viewProjMat = tCamViewProjs;
            
            char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptScene->atShadowCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[ptScene->sbtSpotLights[ptData->uLightIndex].ptShadowBufferOffset->uOffset], &tCamViewProjs, sizeof(plMat4));
        }    
    }

    const plEcsTypeKey tTransformComponentType =gptTransform->get_ecs_type_key_transform();
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);

    // uint32_t uCameraBufferIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];


        plLightComponent* ptLight = NULL;
        if(ptData->tType == PL_LIGHT_TYPE_POINT)     ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_SPOT) ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[ptData->uLightIndex].tEntity);

        if(ptData->tType == PL_LIGHT_TYPE_DIRECTIONAL)
            continue;

        bool bVisibleToAnyCamera = false;

        for(uint32_t uCameraIndex = 0; uCameraIndex < uCameraCount; uCameraIndex++)
        {
            const plCamera* ptCamera = atCameras[uCameraIndex];

            if(ptLight->tType == PL_LIGHT_TYPE_POINT)
            {
                plSphere tSphere = {
                    .fRadius = ptLight->fRange,
                    .tCenter = ptLight->tPosition
                };

                plFrustum tFrustum = {0};
                if(ptCamera->eProjectionType == PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
                    pl__camera_build_perspective_frustum(ptCamera, &tFrustum);
                else
                    pl__camera_build_orthographic_frustum(ptCamera, &tFrustum);

                if(gptGjk->pen(pl_gjk_support_frustum, &tFrustum, pl_gjk_support_sphere, &tSphere, NULL))
                {
                    bVisibleToAnyCamera = true;
                }
            }

            else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
            {
                plCone tCone = {
                    .tTipPos = ptLight->tPosition,
                    .fRadius = tanf(ptLight->fOuterConeAngle) * ptLight->fRange,
                    .tBasePos = pl_add_vec3(ptLight->tPosition, pl_mul_vec3_scalarf(ptLight->tDirection, ptLight->fRange))
                };

                plFrustum tFrustum = {0};
                if(ptCamera->eProjectionType == PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
                    pl__camera_build_perspective_frustum(ptCamera, &tFrustum);
                else
                    pl__camera_build_orthographic_frustum(ptCamera, &tFrustum);

                if(gptGjk->pen(pl_gjk_support_frustum, &tFrustum, pl_gjk_support_cone, &tCone, NULL))
                {
                    bVisibleToAnyCamera = true;
                }
            }
        }

        if(!bVisibleToAnyCamera)
            continue;

        plAtomicCounter* ptCullCounter = NULL;
        pl_sb_reset(ptScene->sbtVisibleDrawables0);
        pl_sb_reset(ptScene->sbtVisibleDrawables1);

        plCullData tCullData = {
            .ptScene      = ptScene,
            .tSphere      = {
                .fRadius = ptLight->fRange,
                .tCenter = ptLight->tPosition
            },
            .tCone = {
                .tTipPos = ptLight->tPosition,
                .fRadius = tanf(ptLight->fOuterConeAngle) * ptLight->fRange,
                .tBasePos = pl_add_vec3(ptLight->tPosition, pl_mul_vec3_scalarf(ptLight->tDirection, ptLight->fRange))
            }
        };

        plJobDesc tJobDesc = {
            .pData = &tCullData
        };
        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
            tJobDesc.task = pl__renderer_cull_point_light_job;
        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
            tJobDesc.task = pl__renderer_cull_spot_light_job;

        gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
        gptJob->wait_for_counter(ptCullCounter);

        pl_sb_reserve(ptScene->sbtVisibleDrawables0, uDrawableCount);
        pl_sb_reserve(ptScene->sbtVisibleDrawables1, uDrawableCount);
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
            plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
            if(!ptDrawable->bCulled)
            {
                plVisibleDrawable tVisibleDrawable = {
                    .uDrawableIndex = uDrawableIndex
                };
                if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_CASTS_SHADOW)
                {
                    if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                    {
                        pl_sb_push(ptScene->sbtVisibleDrawables1, tVisibleDrawable);
                    }
                    else
                    {
                        pl_sb_push(ptScene->sbtVisibleDrawables0, tVisibleDrawable);
                    }
                }
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables0);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables1);

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {

            const uint32_t uCameraBufferIndex = (uint32_t)ptScene->sbtPointLights[ptData->uLightIndex].ptShadowBufferOffset->uOffset / sizeof(plMat4);

            if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptCommandBuffer, ptSettings->tShadows.fConstantDepthBias, 0.0f, ptSettings->tShadows.fSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleOpaqueDrawCount);
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                    ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                    ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = ptScene->tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = ptScene->tIndexBuffer,
                        .uIndexOffset         = ptDrawable->uIndexOffset,
                        .uTriangleCount       = ptDrawable->uTriangleCount,
                        .uVertexOffset        = 0,
                        .atBindGroups = {
                            ptScene->atSceneBindGroups[uFrameIdx],
                            ptScene->atShadowBG[uFrameIdx]
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = ptDrawable->uInstanceIndex,
                        .uInstanceCount = 6 * ptDrawable->uInstanceCount
                    });
                }

                *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleTransparentDrawCount);
                for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                {
                    plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                    ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                    ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = ptScene->tShadowAlphaShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer         = ptScene->tIndexBuffer,
                        .uIndexOffset         = ptDrawable->uIndexOffset,
                        .uTriangleCount       = ptDrawable->uTriangleCount,
                        .uVertexOffset        = 0,
                        .atBindGroups = {
                            ptScene->atSceneBindGroups[uFrameIdx],
                            ptScene->atShadowBG[uFrameIdx]
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = ptDrawable->uInstanceIndex,
                        .uInstanceCount = 6 * ptDrawable->uInstanceCount
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

                gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
            }
            else
            {
                for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
                {

                    gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                    gptGfx->set_depth_bias(ptCommandBuffer, ptSettings->tShadows.fConstantDepthBias, 0.0f, ptSettings->tShadows.fSlopeDepthBias);
                    *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                    PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleOpaqueDrawCount);
                    for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                    {
                        plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                        ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                        ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCameraBufferIndex + uFaceIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader         = ptScene->tShadowShader,
                            .auDynamicBuffers = {
                                tDynamicBinding.uBufferHandle
                            },
                            .atVertexBuffers = {
                                ptScene->tVertexBuffer,
                            },
                            .tIndexBuffer         = ptScene->tIndexBuffer,
                            .uIndexOffset         = ptDrawable->uIndexOffset,
                            .uTriangleCount       = ptDrawable->uTriangleCount,
                            .uVertexOffset        = 0,
                            .atBindGroups = {
                                ptScene->atSceneBindGroups[uFrameIdx],
                                ptScene->atShadowBG[uFrameIdx]
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = ptDrawable->uInstanceIndex,
                            .uInstanceCount = ptDrawable->uInstanceCount
                        });
                    }

                    *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                    PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleTransparentDrawCount);
                    for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                    {
                        plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                        ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                        ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCameraBufferIndex + uFaceIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader        = ptScene->tShadowAlphaShader,
                            .auDynamicBuffers = {
                                tDynamicBinding.uBufferHandle
                            },
                            .atVertexBuffers = {
                                ptScene->tVertexBuffer,
                            },
                            .tIndexBuffer   = ptScene->tIndexBuffer,
                            .uIndexOffset   = ptDrawable->uIndexOffset,
                            .uTriangleCount = ptDrawable->uTriangleCount,
                            .uVertexOffset  = 0,
                            .atBindGroups = {
                                ptScene->atSceneBindGroups[uFrameIdx],
                                ptScene->atShadowBG[uFrameIdx]
                            },
                            .auDynamicBufferOffsets = {
                                tDynamicBinding.uByteOffset
                            },
                            .uInstanceOffset = ptDrawable->uInstanceIndex,
                            .uInstanceCount = ptDrawable->uInstanceCount
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

                    gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
                }
            }
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {

            const uint32_t uCameraBufferIndex = (uint32_t)ptScene->sbtSpotLights[ptData->uLightIndex].ptShadowBufferOffset->uOffset / sizeof(plMat4);

            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptCommandBuffer, ptSettings->tShadows.fConstantDepthBias, 0.0f, ptSettings->tShadows.fSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleOpaqueDrawCount);
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                ptDynamicData->iIndex = (int)uCameraBufferIndex;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader         = ptScene->tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer   = ptScene->tIndexBuffer,
                    .uIndexOffset   = ptDrawable->uIndexOffset,
                    .uTriangleCount = ptDrawable->uTriangleCount,
                    .uVertexOffset  = 0,
                    .atBindGroups = {
                        ptScene->atSceneBindGroups[uFrameIdx],
                        ptScene->atShadowBG[uFrameIdx]
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = ptDrawable->uInstanceIndex,
                    .uInstanceCount = ptDrawable->uInstanceCount
                });
            }

            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleTransparentDrawCount);
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                ptDynamicData->iIndex = (int)uCameraBufferIndex;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = ptScene->tShadowAlphaShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer   = ptScene->tIndexBuffer,
                    .uIndexOffset   = ptDrawable->uIndexOffset,
                    .uTriangleCount = ptDrawable->uTriangleCount,
                    .uVertexOffset  = 0,
                    .atBindGroups = {
                        ptScene->atSceneBindGroups[uFrameIdx],
                        ptScene->atShadowBG[uFrameIdx]
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = ptDrawable->uInstanceIndex,
                    .uInstanceCount = ptDrawable->uInstanceCount
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

            gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_generate_direction_view_map(plCommandBuffer* ptCommandBuffer, plScene* ptScene, const plCamera* ptSceneCamera, plCSMInfo tInfo)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // for convience
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    const float g = 1.0f / tanf(ptSceneCamera->fYFov / 2.0f);
    const float s = ptSceneCamera->fAspectRatio;

    float fShadowFarZ = pl_min(ptSceneCamera->fFarZ, ptSettings->tShadows.fMaxShadowRange);

    // common calculations
    const float fFarClip = fShadowFarZ;
    const float fNearClip = ptSceneCamera->fNearZ;
    const float fClipRange = fFarClip - fNearClip;

    const float fMinZ = fNearClip;
    const float fMaxZ = fNearClip + fClipRange;

    const float fRange = fMaxZ - fMinZ;
    const float fRatio = fMaxZ / fMinZ;

    // TODO: we shouldn't have to check all rects, optimize this

    

    const uint32_t uAtlasRectCount = pl_sb_size(ptScene->sbtShadowViewRects);
    for(uint32_t uRectIndex = 1; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowViewRects[uRectIndex];

        plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptRect->iId].tEntity);

        int iShadowIndex = ptScene->sbtDirectionLightData[ptRect->iId].iShadowIndex;
        plGpuDirectionLightShadow* ptShadowData = &tInfo.sbtDLightShadowData[iShadowIndex];

        ptShadowData->iShadowMapTexIdx = ptScene->uSunShadowAtlasIndex;
        ptShadowData->fFactor          = (float)ptLight->uShadowResolution / (float)ptScene->uSunShadowAtlasResolution;
        ptShadowData->fXOffset         = (float)ptRect->iX / (float)ptScene->uSunShadowAtlasResolution;
        ptShadowData->fYOffset         = (float)ptRect->iY / (float)ptScene->uSunShadowAtlasResolution;

        plMat4 tCamViewProj = {0};
        float fLastSplitDist = ptSceneCamera->fNearZ;
        const plVec3 tDirection = pl_norm_vec3(ptLight->tDirection);

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

        float fSplitDist = ptSceneCamera->fNearZ + (fShadowFarZ - ptSceneCamera->fNearZ);

        // scene camera space
        plVec3 atCameraCorners2[] = {
            {  fLastSplitDist * s / g, -fLastSplitDist / g, fLastSplitDist },
            {  fLastSplitDist * s / g,  fLastSplitDist / g, fLastSplitDist },
            { -fLastSplitDist * s / g,  fLastSplitDist / g, fLastSplitDist },
            { -fLastSplitDist * s / g, -fLastSplitDist / g, fLastSplitDist },
            {  fSplitDist * s / g, -fSplitDist / g, fSplitDist },
            {  fSplitDist * s / g,  fSplitDist / g, fSplitDist },
            { -fSplitDist * s / g,  fSplitDist / g, fSplitDist },
            { -fSplitDist * s / g, -fSplitDist / g, fSplitDist },
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
        const float fDepthPadding = 500.0f; // TODO: make option
        const float fNearZ = fZMin - fDepthPadding;
        const float fFarZ  = fZMax + fDepthPadding;

        // reconstruct snapped world-space center from stable light space
        plVec3 tSnappedCenterLS = {
            fCenterX,
            fCenterY,
            fCenterZ
        };

        plVec4 tSnappedCenterWS4 = pl_mul_mat4_vec4(&tStableLightViewInv, (plVec4){ .xyz = tSnappedCenterLS, .w = 1.0f });
        plVec3 tSnappedCenterWS = tSnappedCenterWS4.xyz;

        // final shadow camera
        plCamera tShadowCamera = {0};
        gptCamera->init(&tShadowCamera);
        tShadowCamera.eProjectionType = PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
        tShadowCamera.eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
        tShadowCamera.tPosition = pl__to_double_vec(tSnappedCenterWS);
        tShadowCamera.tPositionF = tSnappedCenterWS;
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

        tCamViewProj = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
        tShadowCamera.tViewProjMat = tCamViewProj;
        ptShadowData->viewProjMat = tCamViewProj;
        fLastSplitDist = fSplitDist;

        // copy data to GPU buffer
        char* pcBufferStart = gptGfx->get_buffer(ptDevice, tInfo.tDShadowCameraBuffer)->tMemoryAllocation.pHostMapped;
        memcpy(&pcBufferStart[iShadowIndex * sizeof(plMat4)], &tCamViewProj, sizeof(plMat4));

        if(ptScene->ptTerrain)
        {

            plTerrainComponent* ptTerrainComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tTerrainComponentType, ptScene->tTerrain);

            plScissor tScissor = {
                    .iOffsetX = (int)(ptRect->iX),
                    .iOffsetY = ptRect->iY,
                    .uWidth  = ptLight->uShadowResolution,
                    .uHeight = ptLight->uShadowResolution,
                };
            
            plRenderViewport tViewport = {
                .fX = (float)(ptRect->iX),
                .fY = (float)ptRect->iY,
                .fWidth  = (float)ptLight->uShadowResolution,
                .fHeight = (float)ptLight->uShadowResolution,
                .fMaxDepth = 1.0f
            };
            gptGfx->set_viewport(ptCommandBuffer, &tViewport);
            gptGfx->set_scissor_region(ptCommandBuffer, &tScissor);
            gptGfx->set_depth_bias(ptCommandBuffer, ptTerrainComp->fTerrainShadowConstantDepthBias, 0.0f, ptTerrainComp->fTerrainShadowSlopeDepthBias);
            gptGfx->bind_shader(ptCommandBuffer, ptScene->tTerrainShadowShader);
            gptGfx->bind_vertex_buffer(ptCommandBuffer, ptScene->ptTerrain->tVertexBuffer);
            plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], tInfo.tBindGroup};

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

            plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
            ptDynamicData->iIndex = (int)iShadowIndex;

            gptGfx->bind_graphics_bind_groups(
                ptCommandBuffer,
                ptScene->tTerrainShadowShader,
                0, 2,
                atBindGroups,
                1, &tDynamicBinding
            );


            for(uint32_t i = 0; i < pl_sb_size(ptScene->ptTerrain->sbtChunkFiles); i++)
                pl__render_chunk_shadow(ptScene, ptScene->ptTerrain, &tShadowCamera, ptCommandBuffer, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, 0, (float)ptLight->uShadowResolution, (float)ptLight->uShadowResolution);
        }

        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, tInfo.tDLightShadowDataBuffer);
        memcpy(&ptDShadowDataBuffer->tMemoryAllocation.pHostMapped[iShadowIndex * sizeof(plGpuDirectionLightShadow)], ptShadowData, sizeof(plGpuDirectionLightShadow));
    }

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);

    const plEcsTypeKey tTransformComponentType =gptTransform->get_ecs_type_key_transform();

    for(uint32_t uRectIndex = 1; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowViewRects[uRectIndex];

        const plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptRect->iId].tEntity);


        int iShadowIndex = ptScene->sbtDirectionLightData[ptRect->iId].iShadowIndex;

        plAtomicCounter* ptCullCounter = NULL;
        pl_sb_reset(ptScene->sbtVisibleDrawables0);
        pl_sb_reset(ptScene->sbtVisibleDrawables1);
        plCullData tCullData = {
            .ptScene      = ptScene,
            .tSphere = {
                .fRadius = ptSceneCamera->fFarZ - ptSceneCamera->fNearZ,
                .tCenter = pl_add_vec3(pl_mul_vec3_scalarf(ptSceneCamera->tForwardVec, 0.6f * (ptSceneCamera->fFarZ - ptSceneCamera->fNearZ)), ptSceneCamera->tPositionF),
            }
        };

        plJobDesc tJobDesc = {
            .pData = &tCullData,
            .task = pl__renderer_cull_point_light_job,
        };

        gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
        gptJob->wait_for_counter(ptCullCounter);

        pl_sb_reserve(ptScene->sbtVisibleDrawables0, uDrawableCount);
        pl_sb_reserve(ptScene->sbtVisibleDrawables1, uDrawableCount);

        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
            plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
            if(!ptDrawable->bCulled)
            {
                plVisibleDrawable tVisibleDrawable = {
                    .uDrawableIndex = uDrawableIndex
                };
                if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_CASTS_SHADOW)
                {
                    if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                    {
                        pl_sb_push(ptScene->sbtVisibleDrawables1, tVisibleDrawable);
                    }
                    else
                    {
                        pl_sb_push(ptScene->sbtVisibleDrawables0, tVisibleDrawable);
                    }
                }
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables0);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables1);

        gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
        gptGfx->set_depth_bias(ptCommandBuffer, ptSettings->tShadows.fConstantDepthBias, 0.0f, ptSettings->tShadows.fSlopeDepthBias);
        *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
        PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleOpaqueDrawCount);
        for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
        {
            plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
            plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
            ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
            ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
            ptDynamicData->iIndex = (int)iShadowIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader         = ptScene->tShadowShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexOffset   = ptDrawable->uIndexOffset,
                .uTriangleCount = ptDrawable->uTriangleCount,
                .uVertexOffset  = 0,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tInfo.tBindGroup
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = ptDrawable->uInstanceIndex,
                .uInstanceCount = ptDrawable->uInstanceCount
            });
        }

        *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
        PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleTransparentDrawCount);
        for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
        {
            plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
            plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
            ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
            ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
            ptDynamicData->iIndex = (int)iShadowIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = ptScene->tShadowAlphaShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexOffset   = ptDrawable->uIndexOffset,
                .uTriangleCount = ptDrawable->uTriangleCount,
                .uVertexOffset  = 0,
                .atBindGroups = {
                    ptScene->atSceneBindGroups[uFrameIdx],
                    tInfo.tBindGroup
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = ptDrawable->uInstanceIndex,
                .uInstanceCount = ptDrawable->uInstanceCount
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
                }
            },
            .atViewports = {
                {
                    .fX = (float)(ptRect->iX),
                    .fY = (float)ptRect->iY,
                    .fWidth  = (float)ptLight->uShadowResolution,
                    .fHeight = (float)ptLight->uShadowResolution,
                    .fMaxDepth = 1.0f
                }
            }
        };

        gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_generate_sun_shadow_map(plCommandBuffer* ptCommandBuffer, plScene* ptScene, plGpuViewData* ptViewData, plBufferHandle tViewBuffer, plBufferHandle tSunCameraBuffer, const plCamera* ptSceneCamera, plBindGroupHandle tBindGroup, bool bProbe)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // More resolution for nearby receivers (the last cascade still reaches fShadowFarZ).
    const float fShadowLambda = 0.95f;
    uint32_t uShadowResolution = (uint32_t)ptScene->sbtShadowViewRects[0].iHeight;
    int iX = ptScene->sbtShadowViewRects[0].iX;
    int iY = ptScene->sbtShadowViewRects[0].iY;
    const uint32_t uCascadeCount = bProbe ? 1 : ptScene->tSceneData.iCascadeCount;

    // for convience
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    const float g = 1.0f / tanf(ptSceneCamera->fYFov / 2.0f);
    const float s = ptSceneCamera->fAspectRatio;

    // This distance limits RECEIVERS. Off-camera casters are covered separately below.
    const float fShadowFarZ = pl_max(ptSceneCamera->fNearZ + 0.01f, pl_min(ptSceneCamera->fFarZ, ptSettings->tShadows.fMaxShadowRange));

    // Conservative broad-phase bounds for all four directional-light cascades.
    // This replaces the camera-centered sphere later used to cull drawables.
    plSphere tSunCasterCullSphere = {0};

    // const uint32_t uAtlasRectCount = pl_sb_size(ptScene->sbtShadowRects);
    // for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {

        plMat4 atCamViewProjs[4] = {0};
        float fLastSplitDist = ptSceneCamera->fNearZ;
        const plMat4 tCameraInversion = pl_mat4_invert(&ptSceneCamera->tViewMat);
        const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);

        plVec3 tCullMinLS = { FLT_MAX, FLT_MAX, FLT_MAX };
        plVec3 tCullMaxLS = {-FLT_MAX,-FLT_MAX,-FLT_MAX };
        const plVec3 tDirection = pl_norm_vec3(ptScene->tSceneData.tDirection);

        float afCascadeSplits[4] = {
            bProbe ? 1.0f : ptViewData->afCascadeSplits.d[0],
            ptViewData->afCascadeSplits.d[1],
            ptViewData->afCascadeSplits.d[2],
            ptViewData->afCascadeSplits.d[3]
        };


        if(bProbe)
        {
            ptViewData->afCascadeSplits.d[0] = 1.0f;
        }
        else
        {
            for(uint32_t i = 1; i <= uCascadeCount; ++i)
            {
                float p = (float)i / (float)uCascadeCount;
                float logSplit = ptSceneCamera->fNearZ * powf(fShadowFarZ / ptSceneCamera->fNearZ, p);
                float linSplit = ptSceneCamera->fNearZ + (fShadowFarZ - ptSceneCamera->fNearZ) * p;
                float di = fShadowLambda * logSplit + (1.0f - fShadowLambda) * linSplit;

                // Four-cascade terrain preset: preserve close-up resolution even
                // when the last cascade reaches many kilometers. Distances are
                // world units (meters in the terrain setup). For other counts,
                // retain the practical split scheme above.
                if(uCascadeCount == 4)
                {
                    const float afPreferred[4] = {50.0f, 250.0f, 1500.0f, fShadowFarZ};
                    const float afMaxFraction[4] = {0.25f, 0.50f, 0.80f, 1.0f};
                    di = pl_min(afPreferred[i - 1], ptSceneCamera->fNearZ + (fShadowFarZ - ptSceneCamera->fNearZ) * afMaxFraction[i - 1]);
                    di = pl_max(di, ptSceneCamera->fNearZ);
                }

                afCascadeSplits[i - 1] = (di - ptSceneCamera->fNearZ)/(ptSceneCamera->fFarZ - ptSceneCamera->fNearZ);
                ptViewData->afCascadeSplits.d[i - 1] = afCascadeSplits[i - 1];
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
                {  fLastSplitDist * s / g, -fLastSplitDist / g, fLastSplitDist },
                {  fLastSplitDist * s / g,  fLastSplitDist / g, fLastSplitDist },
                { -fLastSplitDist * s / g,  fLastSplitDist / g, fLastSplitDist },
                { -fLastSplitDist * s / g, -fLastSplitDist / g, fLastSplitDist },
                {  fSplitDist * s / g, -fSplitDist / g, fSplitDist },
                {  fSplitDist * s / g,  fSplitDist / g, fSplitDist },
                { -fSplitDist * s / g,  fSplitDist / g, fSplitDist },
                { -fSplitDist * s / g, -fSplitDist / g, fSplitDist },
            };

            // convert to world space
            plVec3 atWorldSpaceCorners[8] = {0};
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
            const float fUnitPerTexel = fDim / (float)uShadowResolution;
            const float fCenterX = fUnitPerTexel * roundf(tFrustumCenterLS.x / fUnitPerTexel);
            const float fCenterY = fUnitPerTexel * roundf(tFrustumCenterLS.y / fUnitPerTexel);
            const float fCenterZ = 0.5f * (fZMin + fZMax);

            //-------------------------------------------------------------------------
            // Find off-camera shadow casters by LIGHT-SPACE X/Y overlap, not by
            // the camera's viewing distance. Each terrain file root has an AABB.
            // Also include ordinary shadow-casting drawable world AABBs.
            // tDirection is assumed to be the direction the light travels (+Z),
            // so relevant casters extend toward SMALLER light-space Z.
            //-------------------------------------------------------------------------
            float fCasterMinZ = fZMin;
            const float fMinX = fCenterX - fRadius;
            const float fMaxX = fCenterX + fRadius;
            const float fMinY = fCenterY - fRadius;
            const float fMaxY = fCenterY + fRadius;

            const uint32_t uTerrainFileCount = ptScene->ptTerrain ? pl_sb_size(ptScene->ptTerrain->sbtChunkFiles) : 0;

            for(uint32_t i = 0; i < uTerrainFileCount + uDrawableCount; i++)
            {
                plVec3 tMinBound = {0};
                plVec3 tMaxBound = {0};

                if(i < uTerrainFileCount)
                {
                    plTerrainChunkFile* ptFile = &ptScene->ptTerrain->sbtChunkFiles[i].tFile;
                    if(ptFile->uChunkCount == 0 || !ptFile->atChunks)
                        continue;
                    // Conservative: one AABB for the whole file. The chunk tree
                    // may be used later for tighter bounds if needed.
                    tMinBound = ptFile->atChunks[0].tMinBound;
                    tMaxBound = ptFile->atChunks[0].tMaxBound;
                }
                else
                {
                    const uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i - uTerrainFileCount];
                    const plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
                    if(!(ptDrawable->tFlags & PL_DRAWABLE_FLAG_CASTS_SHADOW))
                        continue;

                    const plDrawableResources* ptResources = &ptScene->sbtDrawableResources[uDrawableIndex];
                    plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptResources->tEntity);
                    if(!ptObject || !(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE))
                        continue;

                    // Requires a conservative world-space AABB, including all
                    // instances for instanced drawables.
                    tMinBound = ptObject->tAABB.tMin;
                    tMaxBound = ptObject->tAABB.tMax;

                    uint32_t uInstances = pl_sb_size(ptResources->sbtEntities);
                    for(uint32_t j = 0; j < uInstances; j++)
                    {
                        
                        plEntity tInstancedEntity = ptResources->sbtEntities[j];
                        plObjectComponent* ptInstancedObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tInstancedEntity);

                        if(ptInstancedObject->tAABB.tMin.x < tMinBound.x) tMinBound.x = ptInstancedObject->tAABB.tMin.x;
                        if(ptInstancedObject->tAABB.tMin.y < tMinBound.y) tMinBound.y = ptInstancedObject->tAABB.tMin.y;
                        if(ptInstancedObject->tAABB.tMin.z < tMinBound.z) tMinBound.z = ptInstancedObject->tAABB.tMin.z;
                        if(ptInstancedObject->tAABB.tMax.x > tMaxBound.x) tMaxBound.x = ptInstancedObject->tAABB.tMax.x;
                        if(ptInstancedObject->tAABB.tMax.y > tMaxBound.y) tMaxBound.y = ptInstancedObject->tAABB.tMax.y;
                        if(ptInstancedObject->tAABB.tMax.z > tMaxBound.z) tMaxBound.z = ptInstancedObject->tAABB.tMax.z;

                    }
                }

                const plVec3 tBoxCenter = pl_mul_vec3_scalarf(pl_add_vec3(tMinBound, tMaxBound), 0.5f);
                const plVec3 tBoxExtent = pl_mul_vec3_scalarf(pl_sub_vec3(tMaxBound, tMinBound), 0.5f);
                const plVec4 tBoxCenterLS4 = pl_mul_mat4_vec4(&tStableLightView, (plVec4){ .xyz = tBoxCenter, .w = 1.0f });
                const plVec3 tBoxCenterLS = tBoxCenterLS4.xyz;

                // Project the AABB half-extents onto the light-space axes.
                const float fExtentX = fabsf(tLightRight.x) * tBoxExtent.x + fabsf(tLightRight.y) * tBoxExtent.y + fabsf(tLightRight.z) * tBoxExtent.z;
                const float fExtentY = fabsf(tLightUp.x) * tBoxExtent.x + fabsf(tLightUp.y) * tBoxExtent.y + fabsf(tLightUp.z) * tBoxExtent.z;
                const float fExtentZ = fabsf(tLightForward.x) * tBoxExtent.x + fabsf(tLightForward.y) * tBoxExtent.y + fabsf(tLightForward.z) * tBoxExtent.z;

                // No X/Y intersection => cannot cast into this cascade.
                if(tBoxCenterLS.x + fExtentX < fMinX ||
                   tBoxCenterLS.x - fExtentX > fMaxX ||
                   tBoxCenterLS.y + fExtentY < fMinY ||
                   tBoxCenterLS.y - fExtentY > fMaxY)
                    continue;

                // A caster wholly downstream of the receiver slice is irrelevant.
                if(tBoxCenterLS.z - fExtentZ > fZMax)
                    continue;

                fCasterMinZ = pl_min(fCasterMinZ, tBoxCenterLS.z - fExtentZ);
            }

            // Keep the receiver's far side and extend only toward casters.
            // Small safety pad replaces the fixed 500-unit blanket padding.
            const float fDepthPadding = 10.0f;
            const float fNearZ = pl_min(fZMin, fCasterMinZ) - fCenterZ - fDepthPadding;
            const float fFarZ  = fZMax - fCenterZ + fDepthPadding;

            // Union of all cascade volumes for subsequent drawable broad-phase.
            tCullMinLS.x = pl_min(tCullMinLS.x, fMinX);
            tCullMinLS.y = pl_min(tCullMinLS.y, fMinY);
            tCullMinLS.z = pl_min(tCullMinLS.z, fCenterZ + fNearZ);
            tCullMaxLS.x = pl_max(tCullMaxLS.x, fMaxX);
            tCullMaxLS.y = pl_max(tCullMaxLS.y, fMaxY);
            tCullMaxLS.z = pl_max(tCullMaxLS.z, fCenterZ + fFarZ);

            // reconstruct snapped world-space center from stable light space
            plVec3 tSnappedCenterLS = { fCenterX, fCenterY, fCenterZ};

            plVec4 tSnappedCenterWS4 = pl_mul_mat4_vec4(&tStableLightViewInv, (plVec4){ .xyz = tSnappedCenterLS, .w = 1.0f });
            plVec3 tSnappedCenterWS = tSnappedCenterWS4.xyz;

            // final shadow camera
            plCamera tShadowCamera = {0};
            gptCamera->init(&tShadowCamera);
            tShadowCamera.eProjectionType = PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
            tShadowCamera.eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
            tShadowCamera.tPosition = pl__to_double_vec(tSnappedCenterWS);
            tShadowCamera.tPositionF = tSnappedCenterWS;
            tShadowCamera.fWidth  = fDim;
            tShadowCamera.fHeight = fDim;
            tShadowCamera.fNearZ  = fNearZ;
            tShadowCamera.fFarZ   = fFarZ;
            tShadowCamera.fAspectRatio = 1.0f;

            // IMPORTANT: set the actual camera orientation before update(),
            // so the view matrix and the forward/right/up vectors used by
            // pl__render_chunk_shadow() agree. Using a double target also
            // avoids loss of the direction at large world coordinates.
            const plVec3d tEye = pl__to_double_vec(tSnappedCenterWS);
            const plVec3d tTarget = {
                tEye.x + 100.0 * (double)tLightForward.x,
                tEye.y + 100.0 * (double)tLightForward.y,
                tEye.z + 100.0 * (double)tLightForward.z
            };
            gptCamera->look_at(&tShadowCamera, tEye, tTarget, tLightUp);
            gptCamera->update(&tShadowCamera);

            atCamViewProjs[uCascade] = tShadowCamera.tViewProjMat;
            ptViewData->viewProjMat[uCascade] = atCamViewProjs[uCascade];
            fLastSplitDist = fSplitDist;

            char* pcBufferStart = gptGfx->get_buffer(ptDevice, tSunCameraBuffer)->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[uCascade * sizeof(plMat4)], &atCamViewProjs[uCascade], sizeof(plMat4));

            if(ptScene->ptTerrain)
            {
                plTerrainComponent* ptTerrainComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tTerrainComponentType, ptScene->tTerrain);
   
                plScissor tScissor = {
                        .iOffsetX = (int)(iX + uCascade * uShadowResolution),
                        .iOffsetY = iY,
                        .uWidth  = uShadowResolution,
                        .uHeight = uShadowResolution,
                    };
                
                plRenderViewport tViewport = {
                    .fX = (float)(iX + uCascade * uShadowResolution),
                    .fY = (float)iY,
                    .fWidth  = (float)uShadowResolution,
                    .fHeight = (float)uShadowResolution,
                    .fMaxDepth = 1.0f
                };
                gptGfx->set_viewport(ptCommandBuffer, &tViewport);
                gptGfx->set_scissor_region(ptCommandBuffer, &tScissor);
                gptGfx->set_depth_bias(ptCommandBuffer, ptTerrainComp->fTerrainShadowConstantDepthBias, 0.0f, ptTerrainComp->fTerrainShadowSlopeDepthBias);
                gptGfx->bind_shader(ptCommandBuffer, ptScene->tTerrainShadowShader);
                gptGfx->bind_vertex_buffer(ptCommandBuffer, ptScene->ptTerrain->tVertexBuffer);
                plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], tBindGroup};

                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iIndex = (int)uCascade;
                // ptDynamicData->iIndex = (int)uCascade + iShadowIndex;

                gptGfx->bind_graphics_bind_groups(
                    ptCommandBuffer,
                    ptScene->tTerrainShadowShader,
                    0, 2,
                    atBindGroups,
                    1, &tDynamicBinding
                );

                for(uint32_t i = 0; i < pl_sb_size(ptScene->ptTerrain->sbtChunkFiles); i++)
                    pl__render_chunk_shadow(ptScene, ptScene->ptTerrain, &tShadowCamera, ptCommandBuffer, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, 0, (float)uShadowResolution, (float)uShadowResolution);
            }
        }

        // Enclose the union of cascade light-space AABBs in one conservative
        // sphere. This includes upstream casters the camera-centered sphere
        // previously discarded. The broad-phase job can stay unchanged.
        const plVec3 tCullCenterLS = pl_mul_vec3_scalarf(pl_add_vec3(tCullMinLS, tCullMaxLS), 0.5f);
        const plVec3 tCullExtentLS = pl_mul_vec3_scalarf(pl_sub_vec3(tCullMaxLS, tCullMinLS), 0.5f);
        const plVec4 tCullCenterWS4 = pl_mul_mat4_vec4(&tStableLightViewInv, (plVec4){ .xyz = tCullCenterLS, .w = 1.0f });
        tSunCasterCullSphere.tCenter = tCullCenterWS4.xyz;
        tSunCasterCullSphere.fRadius = pl_length_vec3(tCullExtentLS) + 1.0f;
    }

    plBuffer* ptViewBuffer = gptGfx->get_buffer(ptDevice, tViewBuffer);
    memcpy(ptViewBuffer->tMemoryAllocation.pHostMapped, ptViewData, sizeof(plGpuViewData));

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);

    {

        plAtomicCounter* ptCullCounter = NULL;
        pl_sb_reset(ptScene->sbtVisibleDrawables0);
        pl_sb_reset(ptScene->sbtVisibleDrawables1);
        plCullData tCullData = {
            .ptScene      = ptScene,
            .tSphere = tSunCasterCullSphere
        };

        plJobDesc tJobDesc = {
            .pData = &tCullData,
            .task = pl__renderer_cull_point_light_job,
        };

        gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
        gptJob->wait_for_counter(ptCullCounter);

        pl_sb_reserve(ptScene->sbtVisibleDrawables0, uDrawableCount);
        pl_sb_reserve(ptScene->sbtVisibleDrawables1, uDrawableCount);

        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
            plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
            if(!ptDrawable->bCulled)
            {
                plVisibleDrawable tVisibleDrawable = {
                    .uDrawableIndex = uDrawableIndex
                };

                if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_CASTS_SHADOW)
                {
                    if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                    {
                        pl_sb_push(ptScene->sbtVisibleDrawables1, tVisibleDrawable);
                    }
                    else
                    {
                        pl_sb_push(ptScene->sbtVisibleDrawables0, tVisibleDrawable);
                    }
                }
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables0);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables1);


        if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT)
        {
            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptCommandBuffer, ptSettings->tShadows.fConstantDepthBias, 0.0f, ptSettings->tShadows.fSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleOpaqueDrawCount);
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                ptDynamicData->iIndex = 0;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader         = ptScene->tShadowShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer   = ptScene->tIndexBuffer,
                    .uIndexOffset   = ptDrawable->uIndexOffset,
                    .uTriangleCount = ptDrawable->uTriangleCount,
                    .uVertexOffset  = 0,
                    .atBindGroups = {
                        ptScene->atSceneBindGroups[uFrameIdx],
                        tBindGroup
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = ptDrawable->uInstanceIndex,
                    .uInstanceCount = uCascadeCount * ptDrawable->uInstanceCount
                });
            }

            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleTransparentDrawCount);
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                ptDynamicData->iIndex = 0;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader = ptScene->tShadowAlphaShader,
                    .auDynamicBuffers = {
                        tDynamicBinding.uBufferHandle
                    },
                    .atVertexBuffers = {
                        ptScene->tVertexBuffer,
                    },
                    .tIndexBuffer   = ptScene->tIndexBuffer,
                    .uIndexOffset   = ptDrawable->uIndexOffset,
                    .uTriangleCount = ptDrawable->uTriangleCount,
                    .uVertexOffset  = 0,
                    .atBindGroups = {
                        ptScene->atSceneBindGroups[uFrameIdx],
                        tBindGroup
                    },
                    .auDynamicBufferOffsets = {
                        tDynamicBinding.uByteOffset
                    },
                    .uInstanceOffset = ptDrawable->uInstanceIndex,
                    .uInstanceCount = uCascadeCount * ptDrawable->uInstanceCount
                });
            };
  
            plDrawArea tArea = 
            {
                .ptDrawStream = ptStream,
                .atScissors = {
                    {
                        .iOffsetX = (int)(iX + 0 * uShadowResolution),
                        .iOffsetY = iY,
                        .uWidth  = uShadowResolution,
                        .uHeight = uShadowResolution,
                    },
                    {
                        .iOffsetX = (int)(iX + 1 * uShadowResolution),
                        .iOffsetY = iY,
                        .uWidth  = uShadowResolution,
                        .uHeight = uShadowResolution,
                    },
                    {
                        .iOffsetX = (int)(iX + 2 * uShadowResolution),
                        .iOffsetY = iY,
                        .uWidth  = uShadowResolution,
                        .uHeight = uShadowResolution,
                    },
                    {
                        .iOffsetX = (int)(iX + 3 * uShadowResolution),
                        .iOffsetY = iY,
                        .uWidth  = uShadowResolution,
                        .uHeight = uShadowResolution,
                    }
                },
                .atViewports = {
                    {
                        .fX = (float)(iX + 0 * uShadowResolution),
                        .fY = (float)iY,
                        .fWidth  = (float)uShadowResolution,
                        .fHeight = (float)uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(iX + 1 * uShadowResolution),
                        .fY = (float)iY,
                        .fWidth  = uCascadeCount > 1 ? (float)uShadowResolution : 0.0f,
                        .fHeight = (float)uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(iX + 2 * uShadowResolution),
                        .fY = (float)iY,
                        .fWidth  = uCascadeCount > 2 ? (float)uShadowResolution : 0.0f,
                        .fHeight = (float)uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(iX + 3 * uShadowResolution),
                        .fY = (float)iY,
                        .fWidth  = uCascadeCount > 3 ? (float)uShadowResolution : 0.0f,
                        .fHeight = (float)uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                }
            };

            gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
        }
        else
        {
            for(uint32_t uCascade = 0; uCascade < uCascadeCount; uCascade++)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptCommandBuffer, ptSettings->tShadows.fConstantDepthBias, 0.0f, ptSettings->tShadows.fSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleOpaqueDrawCount);
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset    = ptDrawable->uDataOffset;
                    ptDynamicData->iVertexOffset  = ptDrawable->uVertexOffset;
                    ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCascade;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader         = ptScene->tShadowShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer   = ptScene->tIndexBuffer,
                        .uIndexOffset   = ptDrawable->uIndexOffset,
                        .uTriangleCount = ptDrawable->uTriangleCount,
                        .uVertexOffset  = 0,
                        .atBindGroups = {
                            ptScene->atSceneBindGroups[uFrameIdx],
                            tBindGroup
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = ptDrawable->uInstanceIndex,
                        .uInstanceCount = ptDrawable->uInstanceCount
                    });
                }

                *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
                PL_RENDERER_STAT("renderer / shadow drawable submissions", uVisibleTransparentDrawCount);
                for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
                {
                    plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));
                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
                    ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
                    ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCascade;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = ptScene->tShadowAlphaShader,
                        .auDynamicBuffers = {
                            tDynamicBinding.uBufferHandle
                        },
                        .atVertexBuffers = {
                            ptScene->tVertexBuffer,
                        },
                        .tIndexBuffer   = ptScene->tIndexBuffer,
                        .uIndexOffset   = ptDrawable->uIndexOffset,
                        .uTriangleCount = ptDrawable->uTriangleCount,
                        .uVertexOffset  = 0,
                        .atBindGroups = {
                            ptScene->atSceneBindGroups[uFrameIdx],
                            tBindGroup
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = ptDrawable->uInstanceIndex,
                        .uInstanceCount = ptDrawable->uInstanceCount
                    });
                };

                plDrawArea tArea = 
                {
                    .ptDrawStream = ptStream,
                    .atScissors = {
                        {
                            .iOffsetX = (int)(iX + uCascade * uShadowResolution),
                            .iOffsetY = iY,
                            .uWidth  = uShadowResolution,
                            .uHeight = uShadowResolution,
                        }
                    },
                    .atViewports = {
                        {
                            .fX = (float)(iX + uCascade * uShadowResolution),
                            .fY = (float)iY,
                            .fWidth  = (float)uShadowResolution,
                            .fHeight = (float)uShadowResolution,
                            .fMaxDepth = 1.0f
                        }
                    }
                };

                gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
            } 
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] misc.
//-----------------------------------------------------------------------------

static uint32_t
pl__renderer_probe_data_pack_index(plScene* ptScene, uint32_t uResolution)
{
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtProbeDataPacks); i++)
    {
        if(ptScene->sbtProbeDataPacks[i].uResolution == uResolution)
            return i;
    }

    plEnvironmentProbeDataPack tPack = {.uResolution = uResolution};

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {(float)uResolution, (float)uResolution, 1},
        .eFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 6,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_CUBE,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "albedo texture"
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = {(float)uResolution, (float)uResolution, 1},
        .eFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 6,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_CUBE,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = {(float)uResolution, (float)uResolution, 1},
        .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 6,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_CUBE,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "emissive texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {(float)uResolution, (float)uResolution, 1},
        .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 6,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_CUBE,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture probe"
    };

    gptStarter->create_texture(&tAlbedoTextureDesc, NULL, 0, &tPack.tAlbedoTexture);
    gptStarter->create_texture(&tNormalTextureDesc, NULL, 0, &tPack.tNormalTexture);
    gptStarter->create_texture(&tEmmissiveTexDesc, NULL, 0, &tPack.tAOMetalRoughnessTexture);
    gptStarter->create_texture(&tDepthTextureDesc, NULL, 0, &tPack.tDepthTexture);

    plTextureViewDesc tAlbedoTextureViewDesc = {
        .eFormat     = tAlbedoTextureDesc.eFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tPack.tAlbedoTexture,
        .eType       = PL_TEXTURE_TYPE_2D
    };

    plTextureViewDesc tNormalTextureViewDesc = {
        .eFormat     = tNormalTextureDesc.eFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tPack.tNormalTexture,
        .eType       = PL_TEXTURE_TYPE_2D,
        .pcDebugName = "gbuffer probe"
    };

    plTextureViewDesc tEmmissiveTexureViewDesc = {
        .eFormat     = tEmmissiveTexDesc.eFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tPack.tAOMetalRoughnessTexture,
        .eType       = PL_TEXTURE_TYPE_2D
    };

    plTextureViewDesc tDepthTextureViewDesc = {
        .eFormat     = tDepthTextureDesc.eFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = tPack.tDepthTexture,
        .eType       = PL_TEXTURE_TYPE_2D
    };

    // for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    // {

        for(uint32_t uFace = 0; uFace < 6; uFace++)
        {

            tAlbedoTextureViewDesc.uBaseLayer = uFace;
            tNormalTextureViewDesc.uBaseLayer = uFace;
            tEmmissiveTexureViewDesc.uBaseLayer = uFace;
            tDepthTextureViewDesc.uBaseLayer = uFace;

            tPack.atAlbedoTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tAlbedoTextureViewDesc);
            tPack.atNormalTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tNormalTextureViewDesc);
            tPack.atAOMetalRoughnessTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tEmmissiveTexureViewDesc);
            tPack.atDepthTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tDepthTextureViewDesc);
        }

    // }

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"),
        .pcDebugName = "lighting bind group"
    };
    
    for(uint32_t uFace = 0; uFace < 6; uFace++)
    {
        const plBindGroupUpdateData tBGData = {
            .atTextureBindings = {
                {
                    .tTexture = tPack.atAlbedoTextureViews[uFace],
                    .uSlot    = 0,
                    .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
                },
                {
                    .tTexture = tPack.atNormalTextureViews[uFace],
                    .uSlot    = 1,
                    .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
                },
                {
                    .tTexture = tPack.atAOMetalRoughnessTextureViews[uFace],
                    .uSlot    = 2,
                    .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
                },
                {
                    .tTexture = tPack.atDepthTextureViews[uFace],
                    .uSlot    = 3,
                    .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
                }
            }
        };
        tPack.atLightingBindGroup[uFace] = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);
        gptGfx->update_bind_group(gptData->ptDevice, tPack.atLightingBindGroup[uFace], &tBGData);
    }

    pl_sb_push(ptScene->sbtProbeDataPacks, tPack);
    return pl_sb_size(ptScene->sbtProbeDataPacks) - 1;
}

static uint32_t
pl__renderer_get_or_create_material_slot(plScene* ptScene, plAssetHandle tMaterial)
{
    if(!pl_hm_has_key(&ptScene->tMaterialHashmap, tMaterial.uData))
    {
        uint64_t uMaterialIndex = pl_hm_get_free_index(&ptScene->tMaterialHashmap);
        if(uMaterialIndex == PL_DS_HASH_INVALID)
        {
            uMaterialIndex = pl_sb_size(ptScene->sbtMaterialNodes);
            pl_sb_add(ptScene->sbtMaterialNodes);
        }
        pl_hm_insert(&ptScene->tMaterialHashmap, tMaterial.uData, uMaterialIndex);
        ptScene->sbtMaterialNodes[uMaterialIndex] = gptFreeList->get_node(&ptScene->tMaterialFreeList, sizeof(plGpuMaterial));
    }
    return (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, tMaterial.uData);
}

static uint32_t
pl__renderer_get_skin_slot(plScene* ptScene)
{
    uint32_t uNewIndex = UINT32_MAX;
    if(pl_sb_size(ptScene->sbuFreeSkinSlots) > 0)
    {
        uNewIndex = pl_sb_pop(ptScene->sbuFreeSkinSlots);
    }
    else
    {
        uNewIndex = pl_sb_size(ptScene->sbtSkinData);
        pl_sb_add(ptScene->sbtSkinData);
    }

    ptScene->sbtSkinData[uNewIndex].bActive = true;
    return uNewIndex;
}

static void
pl__renderer_return_skin_slot(plScene* ptScene, uint32_t uSlot)
{
    if(uSlot != UINT32_MAX)
    {
        ptScene->sbtSkinData[uSlot].bActive = false;
        pl_sb_push(ptScene->sbuFreeSkinSlots, uSlot);
    }
}

static uint32_t
pl__renderer_get_drawable_slot(plScene* ptScene)
{
    if(pl_sb_size(ptScene->sbuFreeDrawableSlots) > 0)
    {
        uint32_t uNewIndex = pl_sb_pop(ptScene->sbuFreeDrawableSlots);
        pl_sb_push(ptScene->sbuActiveDrawables, uNewIndex);
        memset(&ptScene->sbtDrawables[uNewIndex], 0, sizeof(plDrawable));
        memset(&ptScene->sbtDrawableResources[uNewIndex], 0, sizeof(plDrawableResources));
        return uNewIndex;
    }

    uint32_t uNewIndex = pl_sb_size(ptScene->sbtDrawables);

    pl_sb_add(ptScene->sbtDrawables);
    pl_sb_add(ptScene->sbtDrawableResources);

    pl_sb_push(ptScene->sbuActiveDrawables, uNewIndex);

    memset(&ptScene->sbtDrawables[uNewIndex], 0, sizeof(plDrawable));
    memset(&ptScene->sbtDrawableResources[uNewIndex], 0, sizeof(plDrawableResources));
    return uNewIndex;
}

static void
pl__renderer_return_drawable_slot(plScene* ptScene, uint32_t uSlot)
{
    uint32_t uActiveDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);
    for(uint32_t i = 0; i < uActiveDrawableCount; i++)
    {
        if(ptScene->sbuActiveDrawables[i] == uSlot)
        {
            pl_sb_del_swap(ptScene->sbuActiveDrawables, i);
            pl_sb_push(ptScene->sbuFreeDrawableSlots, uSlot);
            return;
        }
    }
    
}

static bool
pl__renderer_add_drawable_data_to_global_buffer(plScene* ptScene, uint32_t uDrawableIndex, uint32_t uSubmeshIndex)
{

    pl_sb_reset(ptScene->sbuIndexBuffer);
    pl_sb_reset(ptScene->sbtVertexPosBuffer);
    pl_sb_reset(ptScene->sbtVertexDataBuffer);
    pl_sb_reset(ptScene->sbtSkinVertexDataBuffer);

    plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
    plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
    plEntity tEntity = ptDrawableResources->tEntity;
    

    // get actual components
    plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
    plMesh* ptMainMesh = gptAsset->get_data(ptObject->tMesh);
    plSubmesh* ptMesh = &ptMainMesh->atSubmeshes[uSubmeshIndex];

    const uint32_t uIndexCount = (uint32_t)ptMesh->szIndexCount;
    const uint32_t uVertexCount = (uint32_t)ptMesh->szVertexCount;

    // stride within storage buffer
    uint32_t uStride = 0;
    uint32_t uSkinStride = 0;

    // calculate vertex stream mask based on provided data
    if(ptMesh->ptVertexNormals)               { uStride += 1; }
    if(ptMesh->ptVertexTangents)              { uStride += 1; }
    if(ptMesh->ptVertexColors)                { uStride += 1; }
    if(ptMesh->ptVertexTextureCoordinates[0]) { uStride += 1; }

    uint64_t uVertexStreamMask = 0;

    // calculate vertex stream mask based on provided data
    uSkinStride += 1;
    if(ptMesh->ptVertexNormals)    { uSkinStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(ptMesh->ptVertexTangents)   { uSkinStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(ptMesh->ptVertexWeights[0]) { uSkinStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(ptMesh->ptVertexWeights[1]) { uSkinStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(ptMesh->ptVertexJoints[0])  { uSkinStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(ptMesh->ptVertexJoints[1])  { uSkinStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

    plFreeListNode* ptIndexBufferNode = gptFreeList->get_node(&ptScene->tIndexBufferFreeList, uIndexCount * sizeof(uint32_t));
    plFreeListNode* ptVertexBufferNode = gptFreeList->get_node(&ptScene->tVertexBufferFreeList, uVertexCount * sizeof(plVec3));
    plFreeListNode* ptVertexDataBufferNode = NULL;
    if(uStride * uVertexCount > 0)
        ptVertexDataBufferNode = gptFreeList->get_node(&ptScene->tStorageBufferFreeList, uStride * uVertexCount * sizeof(plVec4));
    plFreeListNode* ptSkinVertexDataBufferNode = NULL;
    if(gptEcs->has_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), tEntity))
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
    if(uStride * uVertexCount > 0 && ptVertexDataBufferNode == NULL)
    {
        bResizeNeeded = true;
    }
    if(ptSkinVertexDataBufferNode == NULL && gptEcs->has_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), tEntity))
    {
        bResizeNeeded = true;
    }

    if(bResizeNeeded)
    {
        if(ptIndexBufferNode)          gptFreeList->return_node(&ptScene->tIndexBufferFreeList, ptIndexBufferNode);
        if(ptVertexBufferNode)         gptFreeList->return_node(&ptScene->tVertexBufferFreeList, ptVertexBufferNode);
        if(ptVertexDataBufferNode)     gptFreeList->return_node(&ptScene->tStorageBufferFreeList, ptVertexDataBufferNode);
        if(ptSkinVertexDataBufferNode) gptFreeList->return_node(&ptScene->tStorageBufferFreeList, ptSkinVertexDataBufferNode);
        return false;
    }

    // ptMesh->uVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_JOINTS_0;
    // ptMesh->uVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_JOINTS_1;
    // ptMesh->uVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0;
    // ptMesh->uVertexStreamMask &= ~PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1;

    pl_sb_add_n(ptScene->sbtVertexDataBuffer, uStride * uVertexCount);

    // current attribute offsetM
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
    const uint32_t uVertexColorCount0 = ptMesh->ptVertexColors ? uVertexCount : 0;
    for(uint32_t i = 0; i < uVertexColorCount0; i++)
    {
        const plVec4* ptColor = &ptMesh->ptVertexColors[i];
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount0 > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    const uint32_t uVertexPosStartIndex  = (uint32_t)(ptVertexBufferNode->uOffset / sizeof(plVec3));

    // add index buffer data
    pl_sb_add_n(ptScene->sbuIndexBuffer, uIndexCount);
    for(uint32_t j = 0; j < uIndexCount; j++)
        ptScene->sbuIndexBuffer[j] = uVertexPosStartIndex + ptMesh->puIndices[j];

    gptStage->stage_buffer_upload(ptScene->tIndexBuffer, ptIndexBufferNode->uOffset, ptScene->sbuIndexBuffer, uIndexCount * sizeof(uint32_t));
    gptStage->stage_buffer_upload(ptScene->tVertexBuffer, ptVertexBufferNode->uOffset, ptMesh->ptVertexPositions, sizeof(plVec3) * uVertexCount);
    if(ptVertexDataBufferNode)
        gptStage->stage_buffer_upload(ptScene->tStorageBuffer, ptVertexDataBufferNode->uOffset, ptScene->sbtVertexDataBuffer, sizeof(plVec4) * uVertexCount * uStride);
    
    ptDrawable->uIndexOffset  = (uint32_t)(ptIndexBufferNode->uOffset / sizeof(uint32_t));
    ptDrawable->uVertexOffset = uVertexPosStartIndex;

    if(ptVertexDataBufferNode)
        ptDrawable->uDataOffset = (uint32_t)(ptVertexDataBufferNode->uOffset / sizeof(plVec4));

    if(gptEcs->has_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), tEntity))
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
        if(ptMesh->ptVertexColors)                { uDestStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[0]) { uDestStride += 1; }

        // const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtSkinVertexDataBuffer);

        gptStage->stage_buffer_upload(ptScene->tStorageBuffer, ptSkinVertexDataBufferNode->uOffset, ptScene->sbtSkinVertexDataBuffer, sizeof(plVec4) * uVertexCount * uSkinStride);

        plSkinData tSkinData = {
            .tEntity           = tEntity,
            .uVertexCount      = uVertexCount,
            .iSourceDataOffset = (int)(ptSkinVertexDataBufferNode->uOffset / sizeof(plVec4)),
            .iDestDataOffset   = ptDrawable->uDataOffset,
            .iDestVertexOffset = ptDrawable->uVertexOffset
        };

        plSkinComponent* ptSkinComponent = gptEcs->get_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), tEntity);
        plSkin* ptSkin = gptAsset->get_data(ptSkinComponent->tSkin);
        tSkinData.ptFreeListNode = gptFreeList->get_node(&ptScene->tSkinBufferFreeList, ptSkin->uJointCount * 2 * sizeof(plMat4));

        tSkinData.tObjectEntity = tEntity;
        tSkinData.bActive = true;
        pl_temp_allocator_reset(&gptData->tTempAllocator);
        ptDrawableResources->uSkinIndex = pl__renderer_get_skin_slot(ptScene);
        ptScene->sbtSkinData[ptDrawableResources->uSkinIndex] = tSkinData;
        ptDrawableResources->ptSkinBufferNode = ptSkinVertexDataBufferNode;
    }
    ptDrawableResources->ptIndexBufferNode = ptIndexBufferNode;
    ptDrawableResources->ptVertexBufferNode = ptVertexBufferNode;
    ptDrawableResources->ptDataBufferNode = ptVertexDataBufferNode;
    gptStage->flush();

    ptDrawable->uTriangleCount = uIndexCount / 3;

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

    const plBindGroupUpdateData tGlobalBindGroupData = {
        .atTextureBindings = {
            {
                .tTexture = tTexture,
                .uSlot    = PL_MAX_BINDLESS_TEXTURE_SLOT,
                .uIndex   = (uint32_t)ulValue,
                .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        }
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

    const plBindGroupUpdateData tGlobalBindGroupData = {
        .atTextureBindings = {
            {
                .tTexture = tTexture,
                .uSlot    = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT,
                .uIndex   = (uint32_t)ulValue,
                .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        }
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
pl__renderer_probe_create_environment_map(plScene* ptScene, plEnvironmentProbeData* ptProbe, uint32_t uSampleCount)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();
    plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptProbe->uDataPackIndex];

    
    plComputeShaderHandle tCubeFilterSpecularShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cube_filter_specular", NULL);
    plComputeShaderHandle tCubeFilterDiffuseShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cube_filter_diffuse", NULL);
    plComputeShaderHandle tCubeFilterSheenShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cube_filter_sheen", NULL);

    plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptProbe->tRawOutputTexture);
    const int iResolution = (int)(ptTexture->tDesc.tDimensions.x);
    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);

    // copy to cube
    {

        #if 1

        gptStarter->get_readback_buffer(128000000, &gptData->tReadbackBuffer, "env readback");

        plBuffer* ptReadbackBuffer = gptGfx->get_buffer(ptDevice, gptData->tReadbackBuffer);

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "map to cube env");
        gptGfx->begin_command_recording(ptCommandBuffer);
        gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

        plBufferImageCopy tRegion = {
            .uImageWidth = (uint32_t)iResolution,
            .uImageHeight = (uint32_t)iResolution,
            .uImageDepth = 1,
            .uMipLevel = 0,
            .uLayerCount = 6,
            .szBufferOffset = 0
        };
        gptGfx->copy_texture_to_buffer(ptCommandBuffer, ptProbe->tRawOutputTexture, gptData->tReadbackBuffer, 1, &tRegion);

        gptGfx->end_compute_pass(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo33 = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {tSemHandle},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo33);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);

        plMipMapCpuDesc tMipDesc = {
            .pData        = ptReadbackBuffer->tMemoryAllocation.pHostMapped,
            .uWidth       = tRegion.uImageWidth,
            .uHeight      = tRegion.uImageHeight,
            .uLayers      = 6,
            .eFormat      = PL_FORMAT_R16G16B16A16_FLOAT,
            .tTextureType = PL_TEXTURE_TYPE_CUBE,
            // .tFilter      = PL_MIPMAP_FILTER_BOX,
            // .tColorSpace  = PL_MIPMAP_COLOR_SPACE_LINEAR,
            // .tContentType = PL_MIPMAP_CONTENT_COLOR,
            .uMipCount    = ptTexture->tDesc.uMips
        };

        plMipMapChain tChain = {0};
        gptImageOps->generate_mip_chain(&tMipDesc, &tChain);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "map to cube env");
        gptGfx->begin_command_recording(ptCommandBuffer);
        gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

        size_t uStageOffset = tChain.atLevels[0].szFaceStride * 6;
        for(uint32_t i = 1; i < tChain.uMipCount; i++)
        {
            // copy memory to mapped staging buffer
            memcpy(&ptReadbackBuffer->tMemoryAllocation.pHostMapped[uStageOffset], tChain.atLevels[i].pData, tChain.atLevels[i].szFaceStride * 6);
            

            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth    = tChain.atLevels[i].uWidth,
                .uImageHeight   = tChain.atLevels[i].uHeight,
                .uImageDepth    = 1,
                .uLayerCount    = 6,
                .szBufferOffset = uStageOffset,
                .uMipLevel = i,
            };
            uStageOffset += tChain.atLevels[i].szFaceStride * 6;

            gptGfx->copy_buffer_to_texture(ptCommandBuffer, gptData->tReadbackBuffer, ptProbe->tRawOutputTexture, 1, &tBufferImageCopy);
        }

        gptGfx->end_compute_pass(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {tSemHandle},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);

        // gptStarter->return_readback_buffer(&gptData->tReadbackBuffer);
        gptImageOps->free_mip_chain(&tChain);

        #endif

        #if 0

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "map to cube env");
        gptGfx->begin_command_recording(ptCommandBuffer);
        gptGfx->begin_compute_pass(ptCommandBuffer, NULL);
        gptGfx->generate_mipmaps(ptCommandBuffer, ptProbe->tRawOutputTexture);
        gptGfx->end_compute_pass(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
        #endif
    }

    // gptGfx->generate_mipmaps_old(gptData->ptDevice, ptProbe->tRawOutputTexture);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~common bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // source sampler & cube map
    const plBindGroupDesc tCubeFilterBGSet0Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_0"),
        .pcDebugName = "cube_filter_set_0"
    };
    plBindGroupHandle tCubeFilterBGSet0 = gptGfx->create_bind_group(ptDevice, &tCubeFilterBGSet0Desc);
    
    const plBindGroupUpdateData tCubeFilterBGSet0Data = {
        .atSamplerBindings = {
            {
                .tSampler = gptData->tSamplerLinearRepeat,
                .uSlot    = 0
            }
        },
        .atTextureBindings = {
            {
                .tTexture = ptProbe->tRawOutputTexture,
                .uSlot    = 1,
                .eType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        }
    };
    gptGfx->update_bind_group(ptDevice, tCubeFilterBGSet0, &tCubeFilterBGSet0Data);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tCubeFilterBGSet0);

    // specular & diffuse output buffers

    const plBindGroupDesc tCubeFilterBGSet1Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_1"),
        .pcDebugName = "cube_filter_set_1"
    };
    plBindGroupHandle tCubeFilterBGSet1 = gptGfx->create_bind_group(ptDevice, &tCubeFilterBGSet1Desc);

    const plBindGroupUpdateData tCubeFilterBGSet1Data = {
        .atBufferBindings = {
            { .uSlot = 0, .tBuffer = ptScene->atFilterWorkingBuffers[0], .szBufferRange = uFaceSize},
            { .uSlot = 1, .tBuffer = ptScene->atFilterWorkingBuffers[1], .szBufferRange = uFaceSize},
            { .uSlot = 2, .tBuffer = ptScene->atFilterWorkingBuffers[2], .szBufferRange = uFaceSize},
            { .uSlot = 3, .tBuffer = ptScene->atFilterWorkingBuffers[3], .szBufferRange = uFaceSize},
            { .uSlot = 4, .tBuffer = ptScene->atFilterWorkingBuffers[4], .szBufferRange = uFaceSize},
            { .uSlot = 5, .tBuffer = ptScene->atFilterWorkingBuffers[5], .szBufferRange = uFaceSize}
        }
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
        gptGfx->begin_command_recording(ptCommandBuffer);
        gptGfx->push_debug_group(ptCommandBuffer, "Env Filtering", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

        const plPassResources tPassResources = {
            .atBuffers = {
                { .tHandle = ptScene->atFilterWorkingBuffers[0], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[1], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[2], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[3], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[4], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[5], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[6], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE }
            }
        };

        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynFilterSpec));
        plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
        ptDynamicData->iResolution = iResolution;
        ptDynamicData->fRoughness = 0.0f;
        ptDynamicData->iSampleCount = (int)uSampleCount;
        ptDynamicData->iWidth = iResolution;
        ptDynamicData->fLodBias = 0.0f;
        ptDynamicData->iCurrentMipLevel = 0;

        gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);

        gptGfx->bind_compute_bind_groups(ptCommandBuffer, tCubeFilterDiffuseShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
        gptGfx->bind_compute_shader(ptCommandBuffer, tCubeFilterDiffuseShader);
        gptGfx->dispatch(ptCommandBuffer, 1, &tDispach);
        gptGfx->end_compute_pass(ptCommandBuffer);

        gptGfx->pop_debug_group(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo0 = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {tSemHandle},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
        gptGfx->return_command_buffer(ptCommandBuffer);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 3");
        gptGfx->begin_command_recording(ptCommandBuffer);
        gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

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
            gptGfx->copy_buffer_to_texture(ptCommandBuffer, ptScene->atFilterWorkingBuffers[i], ptProbe->tLambertianEnvTexture, 1, &tBufferImageCopy);
        }
        gptGfx->end_compute_pass(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);
        const plSubmitInfo tSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {tSemHandle},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }


    {

        const size_t uMaxFaceSize = (size_t)iResolution * (size_t)iResolution * 4 * sizeof(float);

        const plPassResources tInnerPassResources = {
            .atBuffers = {
                { .tHandle = ptScene->atFilterWorkingBuffers[0], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[1], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[2], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[3], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[4], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[5], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[6], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE }
            }
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
            gptGfx->begin_command_recording(ptCommandBuffer);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynFilterSpec));
            plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
            ptDynamicData->iResolution = iResolution;
            ptDynamicData->fRoughness = (float)i / (float)(ptEnvTexture->tDesc.uMips - 1);
            ptDynamicData->iSampleCount = i == 0 ? 1 : (int)uSampleCount;
            ptDynamicData->iWidth = currentWidth;
            ptDynamicData->fLodBias = 0.0f;
            ptDynamicData->iCurrentMipLevel = i;

            gptGfx->begin_compute_pass(ptCommandBuffer, &tInnerPassResources);
            gptGfx->bind_compute_bind_groups(ptCommandBuffer, tCubeFilterSpecularShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptCommandBuffer, tCubeFilterSpecularShader);
            gptGfx->dispatch(ptCommandBuffer, 1, &tDispach);
            gptGfx->end_compute_pass(ptCommandBuffer);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {tSemHandle},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
            gptGfx->return_command_buffer(ptCommandBuffer);

            ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 5");
            gptGfx->begin_command_recording(ptCommandBuffer);
            gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

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
                gptGfx->copy_buffer_to_texture(ptCommandBuffer, ptScene->atFilterWorkingBuffers[j], ptProbe->tGGXEnvTexture, 1, &tBufferImageCopy);
            }
            gptGfx->end_compute_pass(ptCommandBuffer);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo0 = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {tSemHandle},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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
    if(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED)
    {

        const size_t uMaxFaceSize = (size_t)iResolution * (size_t)iResolution * 4 * sizeof(float);

        const plPassResources tInnerPassResources = {
            .atBuffers = {
                { .tHandle = ptScene->atFilterWorkingBuffers[0], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[1], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[2], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[3], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[4], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[5], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = ptScene->atFilterWorkingBuffers[6], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE }
            }
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
            gptGfx->begin_command_recording(ptCommandBuffer);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynFilterSpec));
            plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
            ptDynamicData->iResolution = iResolution;
            ptDynamicData->fRoughness = (float)i / (float)(ptEnvTexture->tDesc.uMips - 1);
            ptDynamicData->iSampleCount = i == 0 ? 1 : (int)uSampleCount;
            ptDynamicData->iWidth = currentWidth;
            ptDynamicData->fLodBias = 0.0f;
            ptDynamicData->iCurrentMipLevel = i;

            gptGfx->begin_compute_pass(ptCommandBuffer, &tInnerPassResources);
            gptGfx->bind_compute_bind_groups(ptCommandBuffer, tCubeFilterSheenShader, 0, 2, atFullBindGroupHandles, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptCommandBuffer, tCubeFilterSheenShader);
            gptGfx->dispatch(ptCommandBuffer, 1, &tDispach);
            gptGfx->end_compute_pass(ptCommandBuffer);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {tSemHandle},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
            gptGfx->return_command_buffer(ptCommandBuffer);

            ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 5");
            gptGfx->begin_command_recording(ptCommandBuffer);
            gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

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
                gptGfx->copy_buffer_to_texture(ptCommandBuffer, ptScene->atFilterWorkingBuffers[j], ptProbe->tSheenEnvTexture, 1, &tBufferImageCopy);
            }
            gptGfx->end_compute_pass(ptCommandBuffer);
            gptGfx->end_command_recording(ptCommandBuffer);
            const plSubmitInfo tSubmitInfo0 = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {tSemHandle},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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

typedef struct _plGbufferFillPassInfo
{
    plRenderBucket    tBucket;
    plShaderHandle    tShader;
    uint32_t          uGlobalIndex;
    plBindGroupHandle tBG2;
    plDrawArea*       ptArea;
} plGbufferFillPassInfo;

static void
pl__render_view_gbuffer_fill_pass(plScene* ptScene, plCommandBuffer* ptCommandBuffer, plGbufferFillPassInfo* ptInfo)
{
    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;


    gptGfx->push_debug_group(ptCommandBuffer, "G-Buffer Fill Double Sided", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});
    gptGfx->set_depth_bias(ptCommandBuffer, 0.0f, 0.0f, 0.0f);

    const uint32_t uVisibleDeferredDoubleSidedDrawCount = pl_sb_size(ptScene->asbuRenderBuckets[ptInfo->tBucket]);
    gptGfx->reset_draw_stream(ptStream, uVisibleDeferredDoubleSidedDrawCount);

    for(uint32_t i = 0; i < uVisibleDeferredDoubleSidedDrawCount; i++)
    {
        plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->asbuRenderBuckets[ptInfo->tBucket][i]];
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynData));
        plGpuDynData* ptDynamicData = (plGpuDynData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
        ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
        ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
        ptDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;

        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader        = ptInfo->tShader,
            .auDynamicBuffers = {
                tDynamicBinding.uBufferHandle
            },
            .atVertexBuffers = {
                ptScene->tVertexBuffer,
            },
            .tIndexBuffer   = ptScene->tIndexBuffer,
            .uIndexOffset   = ptDrawable->uIndexOffset,
            .uTriangleCount = ptDrawable->uTriangleCount,
            .uVertexOffset  = 0,
            .atBindGroups = {
                ptScene->atSceneBindGroups[uFrameIdx],
                ptInfo->tBG2
            },
            .auDynamicBufferOffsets = {
                tDynamicBinding.uByteOffset
            },
            .uInstanceOffset = ptDrawable->uTransformIndex,
            .uInstanceCount  = ptDrawable->uInstanceCount
        });
    }

    gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
    gptGfx->pop_debug_group(ptCommandBuffer);
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
pl__render_view_deferred_lighting_pass(plScene* ptScene, plCommandBuffer* ptCommandBuffer, plBindGroupHandle tViewBG, plDeferredLightingPassInfo* ptInfo)
{
    gptGfx->push_debug_group(ptCommandBuffer, "Deferred Lighting", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    gptGfx->push_debug_group(ptCommandBuffer, "Lights", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    const uint32_t uPointLightCount = pl_sb_size(ptScene->sbtPointLightData);
    const uint32_t uSpotLightCount = pl_sb_size(ptScene->sbtSpotLightData);
    const uint32_t uDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLightData);

    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS)
    {
        if(!ptInfo->bProbe)
        {
            if(uPointLightCount + uSpotLightCount > 0)
            {
                gptGfx->reset_draw_stream(ptStream, 1);
                plDynamicBinding tBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
                plGpuDynDeferredLighting* ptData = (plGpuDynDeferredLighting*)tBinding.pcData;
                *ptData = (plGpuDynDeferredLighting){
                    .uGlobalIndex       = ptInfo->uGlobalIndex,
                    .iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS
                };
                pl_add_to_draw_stream(ptStream, (plDrawStreamData){
                    .tShader = ptScene->tClusteredLightingShader,
                    .auDynamicBuffers = {tBinding.uBufferHandle},
                    .auDynamicBufferOffsets = {tBinding.uByteOffset},
                    .atBindGroups = {ptScene->atSceneBindGroups[uFrameIdx], tViewBG, ptInfo->tBG2},
                    .uTriangleCount = 1,
                    .uInstanceCount = 1
                });
                gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
            }
        }
        else
        {
            // Probe faces have no cluster lists; retain light-volume draws for captures.
            gptGfx->reset_draw_stream(ptStream, uPointLightCount + uSpotLightCount);
            for(uint32_t uLightIndex = 0; uLightIndex < uPointLightCount; uLightIndex++)
            {
                plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
                plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
                ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
                ptLightingDynamicData->iLightIndex = (int)uLightIndex;
                ptLightingDynamicData->iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS;
                if(ptInfo->bProbe)
                    ptLightingDynamicData->iObjectShaderFlags |= PL_OBJECT_SHADER_FLAG_PROBE;

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

                plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
                plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
                ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
                ptLightingDynamicData->iLightIndex = (int)uLightIndex;
                ptLightingDynamicData->iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS;
                if(ptInfo->bProbe)
                    ptLightingDynamicData->iObjectShaderFlags |= PL_OBJECT_SHADER_FLAG_PROBE;

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
            gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
        }

        gptGfx->reset_draw_stream(ptStream, uDirectionLightCount);
        for(uint32_t uLightIndex = 0; uLightIndex < uDirectionLightCount; uLightIndex++)
        {

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptLightingDynamicData->iLightIndex = (int)uLightIndex;
            ptLightingDynamicData->iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS;
            if(ptInfo->bProbe)
                ptLightingDynamicData->iObjectShaderFlags |= PL_OBJECT_SHADER_FLAG_PROBE;
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
        gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);

        // sun
        gptGfx->reset_draw_stream(ptStream, 1);
        {

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptLightingDynamicData->iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS;
            if(ptInfo->bProbe)
                ptLightingDynamicData->iObjectShaderFlags |= PL_OBJECT_SHADER_FLAG_PROBE;
            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader = ptScene->tSunShader,
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
        gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
    }
    
    gptGfx->pop_debug_group(ptCommandBuffer);

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);

    if(uProbeCount > 0 && !ptInfo->bProbe && ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED)
    {
        gptGfx->push_debug_group(ptCommandBuffer, "Probes", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});
        gptGfx->reset_draw_stream(ptStream, 1);
        plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
        plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
        ptLightingDynamicData->uGlobalIndex = 0;
        ptLightingDynamicData->iLightIndex = -1;

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
        gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
        gptGfx->pop_debug_group(ptCommandBuffer);
    }
    gptGfx->pop_debug_group(ptCommandBuffer);
}

static void
pl__render_view_deferred_lighting_debug_pass(plView* ptView, plCommandBuffer* ptCommandBuffer, plBindGroupHandle tViewBG)
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
    plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
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
    gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
}

static void
pl__render_view_skybox_pass(plScene* ptScene, plCommandBuffer* ptCommandBuffer, plBindGroupHandle tViewBG, plMat4* ptTransform, plDrawArea* ptArea, uint32_t uFace)
{
    gptGfx->push_debug_group(ptCommandBuffer, "Skybox", (plVec4){0.33f, 0.02f, 0.80f, 1.0f});

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    plDynamicBinding tSkyboxDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynSkybox));
    plGpuDynSkybox* ptSkyboxDynamicData = (plGpuDynSkybox*)tSkyboxDynamicData.pcData;
    ptSkyboxDynamicData->tModel = *ptTransform;
    ptSkyboxDynamicData->uGlobalIndex = uFace;

    plShaderVariantDesc tSkyboxVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout
    };

    plShaderHandle tSkyboxShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "skybox", &tSkyboxVariant);

    gptGfx->reset_draw_stream(ptStream, 1);
    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
    {
        .tShader        = tSkyboxShader,
        .auDynamicBuffers = {
            tSkyboxDynamicData.uBufferHandle
        },
        .uIndexOffset   = 0,
        .uTriangleCount = 1,
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
    gptGfx->draw_stream(ptCommandBuffer, 1, ptArea);

    gptGfx->pop_debug_group(ptCommandBuffer);
}

static void
pl__render_view_forward_pass(plScene* ptScene, plCommandBuffer* ptCommandBuffer, plBindGroupHandle tViewBG, plForwardPassInfo* ptInfo)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    const uint32_t uVisibleForwardDoubleSidedDrawCount = pl_sb_size(ptScene->asbuRenderBuckets[ptInfo->tBucket]);
    gptGfx->reset_draw_stream(ptStream, uVisibleForwardDoubleSidedDrawCount);

    for(uint32_t i = 0; i < uVisibleForwardDoubleSidedDrawCount; i++)
    {
        plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->asbuRenderBuckets[ptInfo->tBucket][i]];
        if(ptInfo->iAlphaFilter != 0)
        {
            plDrawableResources* ptResources = &ptScene->sbtDrawableResources[ptScene->asbuRenderBuckets[ptInfo->tBucket][i]];
            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptResources->tEntity);
            plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
            plAssetHandle tMaterial = ptObject->atMaterialOverrides ? ptObject->atMaterialOverrides[ptResources->uSubmeshIndex - ptObject->uFirstSubmesh] : ptMesh->atSubmeshes[ptResources->uSubmeshIndex].tMaterial;
            const plMaterial* ptMaterial = gptAsset->get_data(tMaterial);
            const bool bBlend = ptMaterial->eAlphaMode == PL_MATERIAL_ALPHA_MODE_BLEND;
            if((ptInfo->iAlphaFilter == 1 && bBlend) || (ptInfo->iAlphaFilter == 2 && !bBlend))
                continue;
        }
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynForwardData));
        plGpuDynForwardData* ptDynamicData = (plGpuDynForwardData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
        ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
        ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
        ptDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
        ptDynamicData->iPointLightCount = pl_sb_size(ptScene->sbtPointLights);
        ptDynamicData->iSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
        ptDynamicData->iDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);
        ptDynamicData->iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS;
        if(ptInfo->bProbe)
            ptDynamicData->iObjectShaderFlags |= PL_OBJECT_SHADER_FLAG_PROBE;

        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader        = ptInfo->tShader,
            .auDynamicBuffers = {
                tDynamicBinding.uBufferHandle
            },
            .atVertexBuffers = {
                ptScene->tVertexBuffer,
            },
            .tIndexBuffer   = ptScene->tIndexBuffer,
            .uIndexOffset   = ptDrawable->uIndexOffset,
            .uTriangleCount = ptDrawable->uTriangleCount,
            .uVertexOffset  = 0,
            .atBindGroups = {
                ptScene->atSceneBindGroups[uFrameIdx],
                tViewBG
            },
            .auDynamicBufferOffsets = {
                tDynamicBinding.uByteOffset
            },
            .uInstanceOffset = ptDrawable->uTransformIndex,
            .uInstanceCount  = ptDrawable->uInstanceCount
        });
    }
    gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
}

static plCommandBuffer*
pl__render_view_full_screen_blit(plView* ptView)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();

    plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "main scene blit");
    gptGfx->begin_command_recording(ptSceneCmdBuffer);

    // Metal 4 does not implicitly order passes, even on the same queue.
    // Finish scene writes and previous snapshot readers before overwriting it.
    gptGfx->consumer_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_BLIT, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_compute_pass(ptSceneCmdBuffer, NULL);

    plImageCopy tFrameCopy = {
        .uSourceExtentX = (uint32_t)ptView->tTargetSize.x,
        .uSourceExtentY = (uint32_t)ptView->tTargetSize.y,
        .uSourceExtentZ = 1,
        .uSourceLayerCount = 1,
        .uDestinationLayerCount = 1,
    };
    gptGfx->copy_texture(ptSceneCmdBuffer, ptView->tRawOutputTexture, ptView->tTransmissionTexture, 1, &tFrameCopy);
    gptGfx->copy_texture(ptSceneCmdBuffer, ptView->tDepthTexture, ptView->tTransmissionDepthTexture, 1, &tFrameCopy);
    // Mipmap generation reads the level zero written by the color copy.
    gptGfx->intra_pass_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_BLIT, PL_PIPELINE_STAGE_BLIT, PL_BARRIER_SCOPE_ALL, NULL);
    gptGfx->generate_mipmaps(ptSceneCmdBuffer, ptView->tTransmissionTexture);
    // Publish both snapshots and all color mips before water samples them.
    gptGfx->producer_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_BLIT, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->end_compute_pass(ptSceneCmdBuffer);
    return ptSceneCmdBuffer;
}

static void
pl__render_view_transmission_pass(plView* ptView, plCommandBuffer* ptCommandBuffer, plBindGroupHandle tViewBG)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plScene*       ptScene   = ptView->ptParentScene;

    const plVec2 tDimensions = ptView->tTargetSize;
    const plEcsTypeKey tTransformComponentType =gptTransform->get_ecs_type_key_transform();

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


    gptGfx->push_debug_group(ptCommandBuffer, "Transmission Pass", (plVec4){0.33f, 0.20f, 0.10f, 1.0f});

    plShaderHandle tTransmissionShader = ptScene->bWireframe ? ptScene->tTransmissionWireframeShader : ptScene->tTransmissionShader;

    const uint32_t uVisibleTransmissionDrawCount = pl_sb_size(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_TRANSMISSION]);
    gptGfx->reset_draw_stream(ptStream, uVisibleTransmissionDrawCount);
    for(uint32_t i = 0; i < uVisibleTransmissionDrawCount; i++)
    {
        plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_TRANSMISSION][i]];
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynForwardData));
        plGpuDynForwardData* ptDynamicData = (plGpuDynForwardData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = ptDrawable->uDataOffset;
        ptDynamicData->iVertexOffset = ptDrawable->uVertexOffset;
        ptDynamicData->iMaterialIndex = ptDrawable->uMaterialIndex;
        ptDynamicData->uGlobalIndex = 0;
        ptDynamicData->iPointLightCount = pl_sb_size(ptScene->sbtPointLights);
        ptDynamicData->iSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
        ptDynamicData->iDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);

        ptDynamicData->iObjectShaderFlags = PL_OBJECT_SHADER_FLAG_SHADOWS;
        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader        = tTransmissionShader,
            .auDynamicBuffers = {
                tDynamicBinding.uBufferHandle
            },
            .atVertexBuffers = {
                ptScene->tVertexBuffer,
            },
            .tIndexBuffer   = ptScene->tIndexBuffer,
            .uIndexOffset   = ptDrawable->uIndexOffset,
            .uTriangleCount = ptDrawable->uTriangleCount,
            .uVertexOffset  = 0,
            .atBindGroups = {
                ptScene->atSceneBindGroups[uFrameIdx],
                tViewBG
            },
            .auDynamicBufferOffsets = {
                tDynamicBinding.uByteOffset
            },
            .uInstanceOffset = ptDrawable->uTransformIndex,
            .uInstanceCount  = ptDrawable->uInstanceCount
        });
    }
    gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
    gptGfx->pop_debug_group(ptCommandBuffer);
}

static void
pl__render_view_grid_pass(plView* ptView, plCommandBuffer* ptCommandBuffer, const plCamera* ptCamera)
{
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plScene* ptScene = ptView->ptParentScene;

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

    plShaderVariantDesc tGridVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout
    };

    plShaderHandle tGridShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "grid", &tGridVariant);
    gptGfx->bind_shader(ptCommandBuffer, tGridShader);

    plDynamicBinding tGridDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynGrid));
    plGpuDynGrid* ptGridDynamicData = (plGpuDynGrid*)tGridDynamicBinding.pcData;
    const float fGridFactor = pl_squaref(ptCamera->fFarZ) - pl_squaref(ptCamera->tPositionF.y);
    ptGridDynamicData->fGridSize = fGridFactor > 0.0f ? sqrtf(fGridFactor) : 100.0f;
    ptGridDynamicData->fGridCellSize = ptView->fGridCellSize;
    ptGridDynamicData->fGridMinPixelsBetweenCells = ptView->fGridMinPixelsBetweenCells;
    ptGridDynamicData->tGridColorThin = ptView->tGridColorThin;
    ptGridDynamicData->tGridColorThick = ptView->tGridColorThick;
    ptGridDynamicData->tViewDirection.xyz = ptCamera->tForwardVec;
    ptGridDynamicData->fCameraXPos = ptCamera->tPositionF.x;
    ptGridDynamicData->fCameraZPos = ptCamera->tPositionF.z;
    ptGridDynamicData->tCameraViewProjection = ptCamera->tViewProjMat;
    
    gptGfx->bind_graphics_bind_groups(ptCommandBuffer, tGridShader, 0, 0, NULL, 1, &tGridDynamicBinding);

    gptGfx->set_scissor_region(ptCommandBuffer, tArea.atScissors);
    gptGfx->set_viewport(ptCommandBuffer, tArea.atViewports);

    plDraw tGridDraw = {
        .uVertexCount   = 6,
        .uInstanceCount = 1,
    };
    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw(ptCommandBuffer, 1, &tGridDraw);
}

static void
pl__render_view_debug_pass(plView* ptView, const plCamera* ptCamera, const plCamera* ptCullCamera)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plScene* ptScene = ptView->ptParentScene;
    const plVec2 tDimensions = ptView->tTargetSize;

    plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "tonemap");
    gptGfx->begin_command_recording(ptPostCmdBuffer);

    plRenderInfo tRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->tFinalTexture,
                .eLoadOp        = PL_LOAD_OP_LOAD,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 0.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    // begin main renderpass (directly to swapchain)

    const plPassResources tResources = {
        .atTextures = {
            {
                .tHandle = ptView->tFinalTexture,
                .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX,
                .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                .eUsage   = PL_TEXTURE_USAGE_COLOR_ATTACHMENT
            },
            {
                .tHandle = ptView->tDepthTexture,
                .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX,
                .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                .eUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
            }
        }
    };

    gptGfx->begin_render_pass(ptPostCmdBuffer, &tRenderInfo, &tResources);
    gptGfx->set_depth_bias(ptPostCmdBuffer, 0.0f, 0.0f, 0.0f);

    plRenderAttachmentInfo tRenderAttachmentInfo = {
        .aeColorFormats = {
            PL_FORMAT_R16G16B16A16_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    gptDraw->submit_3d_drawlist(ptView->pt3DDrawList, ptPostCmdBuffer, tDimensions.x, tDimensions.y, &ptCamera->tViewProjMat, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST, 1, &tRenderAttachmentInfo);
    gptDraw->submit_3d_drawlist(ptView->pt3DSelectionDrawList, ptPostCmdBuffer, tDimensions.x, tDimensions.y, &ptCamera->tViewProjMat, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST, 1, &tRenderAttachmentInfo);
    gptGfx->end_render_pass(ptPostCmdBuffer);
    gptGfx->end_command_recording(ptPostCmdBuffer);

    const plSubmitInfo tPostSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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

    const plEcsTypeKey tTransformComponentType =gptTransform->get_ecs_type_key_transform();
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

    plShaderVariantDesc tPickingVariant = {
        .ptAttachmentInfo = &gptData->tPickRenderPassLayout
    };

    plShaderHandle tPickShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "picking", &tPickingVariant);

    plBuffer* ptPickBuffer = gptGfx->get_buffer(ptDevice, ptView->atPickBuffer[uFrameIdx]);
    // memset(ptPickBuffer->tMemoryAllocation.pHostMapped, 0, sizeof(uint32_t) * 2);

    ptView->auHoverResultProcessing[uFrameIdx] = true;
    ptView->auHoverResultReady[uFrameIdx] = false;
    ptView->bRequestHoverCheck = false;

    plRenderInfo tPickRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->tPickTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 0.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    gptGfx->begin_render_pass(ptSceneCmdBuffer, &tPickRenderInfo, NULL);
    gptGfx->set_depth_bias(ptSceneCmdBuffer, 0.0f, 0.0f, 0.0f);
    gptGfx->bind_shader(ptSceneCmdBuffer, tPickShader);
    gptGfx->bind_vertex_buffer(ptSceneCmdBuffer, ptScene->tVertexBuffer);

    gptGfx->set_scissor_region(ptSceneCmdBuffer, tArea.atScissors);
    gptGfx->set_viewport(ptSceneCmdBuffer, tArea.atViewports);

    plBindGroupHandle atBindGroups[2] = {tViewBG, ptView->atPickBindGroup[uFrameIdx]};
    gptGfx->bind_graphics_bind_groups(ptSceneCmdBuffer, tPickShader, 0, 2, atBindGroups, 0, NULL);

    const uint32_t uVisibleDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables);
    *gptData->pdDrawCalls += (double)uVisibleDrawCount;

    plVec2 tMousePos = gptIOI->get_mouse_pos();
    tMousePos = pl_sub_vec2(tMousePos, ptView->tHoverOffset);
    tMousePos = pl_div_vec2(tMousePos, ptView->tHoverWindowRatio);
    
    for(uint32_t i = 0; i < uVisibleDrawCount; i++)
    {
        plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbtVisibleDrawables[i]];
        plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[ptScene->sbtVisibleDrawables[i]];

        // base draw
        {
            uint32_t uId = ptDrawableResources->tEntity.uIndex;
            
            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);
            plTransformComponent* ptTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynPick));
            plGpuDynPick tDynPick = {
                .uID = uId,
                .tModel = ptTransform->tWorld,
                .tMousePos = {
                    .xy = tMousePos
                }
            };
            plGpuDynPick* ptDynamicData = (plGpuDynPick*)tDynamicBinding.pcData;
            memcpy(ptDynamicData, &tDynPick, sizeof(plGpuDynPick));
            
            gptGfx->bind_graphics_bind_groups(ptSceneCmdBuffer, tPickShader, 0, 0, NULL, 1, &tDynamicBinding);

            plDrawIndex tDraw = {
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexCount    = ptDrawable->uTriangleCount * 3,
                .uIndexStart    = ptDrawable->uIndexOffset,
                .uInstanceCount = 1
            };
            gptGfx->draw_indexed(ptSceneCmdBuffer, 1, &tDraw);
        }

        // instance draw
        for(uint32_t uInstanceIndex = 0; uInstanceIndex < pl_sb_size(ptDrawableResources->sbtEntities); uInstanceIndex++)
        {
            plObjectComponent* ptInstanceObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->sbtEntities[uInstanceIndex]);
            plTransformComponent* ptTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptInstanceObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynPick));
            plGpuDynPick* ptDynamicData = (plGpuDynPick*)tDynamicBinding.pcData;
            
            ptDynamicData->uID = ptDrawableResources->sbtEntities[uInstanceIndex].uIndex;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->tMousePos.xy = tMousePos;
            
            gptGfx->bind_graphics_bind_groups(ptSceneCmdBuffer, tPickShader, 0, 0, NULL, 1, &tDynamicBinding);

            plDrawIndex tDraw = {
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexCount    = ptDrawable->uTriangleCount * 3,
                .uIndexStart    = ptDrawable->uIndexOffset,
                .uInstance      = uInstanceIndex + 1,
                .uInstanceCount = 1
            };
            gptGfx->draw_indexed(ptSceneCmdBuffer, 1, &tDraw);
        }


    }
    gptGfx->end_render_pass(ptSceneCmdBuffer);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__render_view_uv_pass(plView* ptView)
{
    // for convience
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice* ptDevice  = gptData->ptDevice;
    plScene* ptScene = ptView->ptParentScene;

    plCommandBuffer* ptUVCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "uv");
    gptGfx->begin_command_recording(ptUVCmdBuffer);

    plRenderInfo tUVRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->atUVMaskTexture0,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 1.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    gptGfx->begin_render_pass(ptUVCmdBuffer, &tUVRenderInfo, NULL);
    gptGfx->push_debug_group(ptUVCmdBuffer, "UV Map", (plVec4){0.33f, 0.72f, 0.10f, 1.0f});

    // submit nonindexed draw using basic API

    plShaderVariantDesc tUVMapVariant = {
        .ptAttachmentInfo = &gptData->tUVRenderPassLayout
    };

    plShaderHandle tUVShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "uvmap", &tUVMapVariant);
    gptGfx->bind_shader(ptUVCmdBuffer, tUVShader);

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

    gptGfx->set_scissor_region(ptUVCmdBuffer, &tUVScissor);
    gptGfx->set_viewport(ptUVCmdBuffer, &tUVViewport);

    plDraw tDraw = {
        .uVertexCount   = 3,
        .uInstanceCount = 1,
    };
    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw(ptUVCmdBuffer, 1, &tDraw);

    // end render pass
    gptGfx->pop_debug_group(ptUVCmdBuffer);
    gptGfx->end_render_pass(ptUVCmdBuffer);
    gptGfx->end_command_recording(ptUVCmdBuffer);

    const plSubmitInfo tUVSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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
    uint32_t uHalfWidth = ptView->uOutlineWidth / 2;
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
    if(pl_sb_size(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_OUTLINE]) + pl_sb_size(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_OUTLINE]) + pl_sb_size(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_OUTLINE_TRANSMISSION]) == 0)
        uJumpSteps = 1;

    const plDispatch tDispach = {
        .uGroupCountX     = (uint32_t)ceilf(tDimensions.x / 8.0f),
        .uGroupCountY     = (uint32_t)ceilf(tDimensions.y / 8.0f),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };

    plComputeShaderHandle tJFAShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "jumpfloodalgo", NULL);
    for(uint32_t i = 0; i < uJumpSteps; i++)
    {
        plCommandBuffer* ptJumpCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "JFA");
        gptGfx->begin_command_recording(ptJumpCmdBuffer);

        // begin main renderpass (directly to swapchain)
        const plPassResources tResources = {
            .atTextures = {
                {
                    .tHandle = ptView->atUVMaskTexture0,
                    .eStages = PL_SHADER_STAGE_COMPUTE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                    .eUsage = PL_TEXTURE_USAGE_STORAGE
                },
                {
                    .tHandle = ptView->atUVMaskTexture1,
                    .eStages = PL_SHADER_STAGE_COMPUTE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                    .eUsage = PL_TEXTURE_USAGE_STORAGE
                }
            }
        };
        gptGfx->begin_compute_pass(ptJumpCmdBuffer, &tResources);
        gptGfx->push_debug_group(ptJumpCmdBuffer, "JFA", (plVec4){0.73f, 0.02f, 0.80f, 1.0f});

        ptView->uLastUVIndex = (i % 2 == 0) ? 1 : 0;

        // submit nonindexed draw using basic API
        gptGfx->bind_compute_shader(ptJumpCmdBuffer, tJFAShader);

        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plVec4));
        plVec4* ptJumpDistance = (plVec4*)tDynamicBinding.pcData;
        ptJumpDistance->x = tDimensions.x;
        ptJumpDistance->y = tDimensions.y;
        ptJumpDistance->z = fJumpDistance;

        gptGfx->bind_compute_bind_groups(ptJumpCmdBuffer, tJFAShader, 0, 1, &ptView->atJFABindGroups[i % 2], 1, &tDynamicBinding);
        gptGfx->dispatch(ptJumpCmdBuffer, 1, &tDispach);

        // end render pass
        gptGfx->pop_debug_group(ptJumpCmdBuffer);
        gptGfx->end_compute_pass(ptJumpCmdBuffer);

        // end recording
        gptGfx->end_command_recording(ptJumpCmdBuffer);

        const plSubmitInfo tJumpSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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
pl__render_view_outline_pass(plView* ptView, const plCamera* ptCamera)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "Outline");
    gptGfx->begin_command_recording(ptCommandBuffer);

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

    plRenderInfo tPostRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->tFinalTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        }
    };

    gptGfx->begin_render_pass(ptCommandBuffer, &tPostRenderInfo, NULL);
    gptGfx->push_debug_group(ptCommandBuffer, "Outline", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

    plDraw tDraw = {
        .uInstanceCount = 1,
        .uVertexCount   = 3
    };

    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynPost));

    plGpuDynPost* ptDynamicData = (plGpuDynPost*)tDynamicBinding.pcData;
    const plVec4 tOutlineColor = (plVec4){(float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 0.0f, 1.0f};
    ptDynamicData->fTargetWidth = (float)ptView->uOutlineWidth * tOutlineColor.r + 1.0f;
    ptDynamicData->tOutlineColor = tOutlineColor;
    plVec2 tUVScale = {0};
    pl_renderer_get_view_color_bind_group(ptView, &tUVScale);
    ptDynamicData->fXScale = tUVScale.x;
    ptDynamicData->fYScale = tUVScale.y;

    plShaderVariantDesc tTonemapVariant = {
        .ptAttachmentInfo = &gptData->tPostProcessRenderPassLayout
    };

    plShaderHandle tTonemapShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "jumpfloodalgo2", &tTonemapVariant);
    gptGfx->bind_shader(ptCommandBuffer, tTonemapShader);
    plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atOutlineBG[ptView->uLastUVIndex]};
    gptGfx->bind_graphics_bind_groups(ptCommandBuffer, tTonemapShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
    gptGfx->set_scissor_region(ptCommandBuffer, tArea.atScissors);
    gptGfx->set_viewport(ptCommandBuffer, tArea.atViewports);

    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw(ptCommandBuffer, 1, &tDraw);

    plRenderAttachmentInfo tRenderAttachmentInfo = {
        .aeColorFormats = { PL_FORMAT_R16G16B16A16_FLOAT}
    };
    gptDraw->submit_3d_drawlist(ptView->pt3DGizmoDrawList, ptCommandBuffer, tDimensions.x, tDimensions.y, &ptCamera->tViewProjMat, 0, 1, &tRenderAttachmentInfo);
    gptGfx->pop_debug_group(ptCommandBuffer);
    gptGfx->end_render_pass(ptCommandBuffer);

    // gptGfx->reset_roles(ptCommandBuffer);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tOutlineSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tOutlineSubmitInfo);
    gptGfx->return_command_buffer(ptCommandBuffer);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

// Extend the existing depth/normal buffer with opaque forward surfaces before screen-space effects.
static void
pl__render_view_forward_depth_normal_pass(plView* ptView, plCommandBuffer* ptCmd, plDrawArea* ptArea)
{
    plScene* ptScene = ptView->ptParentScene;
    const plRenderBucket atBuckets[] = {
        PL_RENDER_BUCKET_FORWARD,
        PL_RENDER_BUCKET_FORWARD_DOUBLE_SIDED,
        PL_RENDER_BUCKET_OUTLINE,
        PL_RENDER_BUCKET_OUTLINE_DOUBLE_SIDED,
        PL_RENDER_BUCKET_FOLIAGE,
        PL_RENDER_BUCKET_FOLIAGE_OUTLINE
    };
    gptGfx->push_debug_group(ptCmd, "Forward Depth/Normals", (plVec4){0.2f, 0.3f, 0.4f, 1.0f});
    for(uint32_t i = 0; i < 6; i++)
    {
        if(pl_sb_size(ptScene->asbuRenderBuckets[atBuckets[i]]) == 0)
            continue;

        const int iFoliage = i >= 4;
        plGraphicsState tState = {
            .bDepthWriteEnabled = true,
            .eDepthMode         = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .eCullMode          = (i == 0 || i == 2) ? PL_CULL_MODE_CULL_BACK : PL_CULL_MODE_NONE,
            .bWireframe         = iFoliage ? false : ptScene->bWireframe
        };
        plShaderVariantDesc tVariant = {
            .ptAttachmentInfo   = &gptData->tDeferredLightingRenderPassLayout,
            .ptGraphicsState    = &tState,
            .pFragmentConstants = &iFoliage
        };
        plForwardPassInfo tInfo = {
            .tBucket = atBuckets[i],
            .tShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "ao_forward", &tVariant),
            .ptArea = ptArea
        };
        pl__render_view_forward_pass(ptScene, ptCmd, ptView->atViewBG[gptGfx->get_current_frame_index()], &tInfo);
    }
    gptGfx->pop_debug_group(ptCmd);
}

static void
pl__renderer_release_ao(plView* ptView)
{
    for(uint32_t i = 0; i < 3; i++)
    {
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->atAO[i]))
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atAO[i]);
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->atAODepth[i]))
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atAODepth[i]);
        ptView->atAO[i] = (plTextureHandle){0};
        ptView->atAODepth[i] = (plTextureHandle){0};
    }
    ptView->tAOSize = (plVec2){0};
}

static void
pl__render_view_ao_pass(plView* ptView, plCommandBuffer* ptCmd)
{
    plScene* ptScene = ptView->ptParentScene;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
    const plRendererAOOptions* ptOptions = &ptSettings->tAO;
    const int iLevels = ptOptions->tMode == PL_RENDERER_AO_MSAO ? 3 : 1;
    PL_RENDERER_STAT("renderer / AO views", 1);
    PL_RENDERER_STAT("renderer / AO dispatches", iLevels * 2 + 1);
    PL_RENDERER_STAT("renderer / AO output pixels", (double)ptView->tTargetSize.x * ptView->tTargetSize.y);

    if(ptView->tAOSize.x != ptView->tTargetSize.x || ptView->tAOSize.y != ptView->tTargetSize.y)
        pl__renderer_release_ao(ptView);

    ptView->tAOSize = ptView->tTargetSize;
    for(int i = 0; i < iLevels; i++)
    {
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->atAO[i]))
            continue;
        const uint32_t uScale = 1u << i;
        plTextureDesc tDesc = {
            .tDimensions = {
                (float)(((uint32_t)ptView->tTargetSize.x + uScale - 1) / uScale),
                (float)(((uint32_t)ptView->tTargetSize.y + uScale - 1) / uScale),
                1
            },
            .eFormat     = PL_FORMAT_R32_FLOAT,
            .uLayers     = 1,
            .uMips       = 1,
            .eType       = PL_TEXTURE_TYPE_2D,
            .eUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
            .pcDebugName = "AO depth"
        };
        gptStarter->create_texture(&tDesc, NULL, 0, &ptView->atAODepth[i]);

        tDesc.eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
        tDesc.pcDebugName = "AO visibility";
        gptStarter->create_texture(&tDesc, NULL, 0, &ptView->atAO[i]);
    }

    gptGfx->push_debug_group(ptCmd, "Ambient Occlusion", (plVec4){0.2f, 0.3f, 0.4f, 1.0f});
    // Each level has a depth reduction and an AO evaluation; the last pass filters
    // and writes shared visibility before ambient lighting consumes it.
    for(int iPass = 0; iPass < iLevels * 2 + 1; iPass++)
    {
        const bool bApply = iPass == iLevels * 2;
        const bool bDepth = !bApply && iPass % 2 == 0;
        const int iLevel = bApply ? 0 : iPass / 2;
        const char* pcShader = bApply ? "ao_apply" : (bDepth ? "ao_depth" : "ao_evaluate");
        plTextureHandle tTarget = bApply ? ptView->tAOResult : (bDepth ? ptView->atAODepth[iLevel] : ptView->atAO[iLevel]);
        plTextureHandle tSource = bApply ? ptView->atAO[0] : (bDepth ? (iLevel == 0 ? ptView->tDepthTexture : ptView->atAODepth[iLevel - 1]) : ptView->atAODepth[iLevel]);
        plBindGroupDesc tBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "ao"),
            .pcDebugName = pcShader
        };
        plBindGroupHandle tBG = gptGfx->create_bind_group(gptData->ptDevice, &tBGDesc);
        plBindGroupUpdateData tBGData = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = tTarget},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = tSource},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = bApply ? ptView->atAO[iLevels == 3 ? 1 : 0] : ptView->tNormalTexture},
                {.uSlot = 3, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = bApply ? ptView->atAO[iLevels == 3 ? 2 : 0] : ptView->tNormalTexture}
            },
            .atSamplerBindings = {{.uSlot = 5, .tSampler = gptData->tSamplerLinearClamp}}
        };
        gptGfx->update_bind_group(gptData->ptDevice, tBG, &tBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tBG);

        plPassResources tResources = {
            .atTextures = {
                {.tHandle = tTarget, .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE, .eUsage = PL_TEXTURE_USAGE_STORAGE}
            }
        };
        gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_COMPUTE, PL_BARRIER_SCOPE_ALL);
        gptGfx->begin_compute_pass(ptCmd, &tResources);
        plDynamicBinding tDynamic = pl__allocate_dynamic_data(gptData->ptDevice, sizeof(plGpuDynAOData));
        plGpuDynAOData* ptGPU = (plGpuDynAOData*)tDynamic.pcData;
        *ptGPU = (plGpuDynAOData){
            .tProjection    = ptView->tViewData.tCameraProjection[0],
            .tProjectionInv = ptView->tViewData.tCameraProjectionInv[0],
            .tView          = ptView->tViewData.tCameraView[0],
            .tViewportSize  = ptView->tTargetSize,
            .iLevel         = iLevel,
            .iMode          = ptOptions->tMode,
            .iLevels        = iLevels,
            .fRadius        = pl_clampf(0.001f, ptOptions->fRadius, 1000.0f),
            .fStrength      = pl_clampf(0.0f, ptOptions->fStrength, 8.0f),
            .fBias          = pl_clampf(0.0f, ptOptions->fBias, 0.5f)
        };
        plComputeShaderHandle tShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, pcShader, NULL);
        gptGfx->bind_compute_shader(ptCmd, tShader);
        gptGfx->bind_compute_bind_groups(ptCmd, tShader, 0, 1, &tBG, 1, &tDynamic);
        const uint32_t uScale = 1u << iLevel;
        plDispatch tDispatch = {
            .uGroupCountX     = (((uint32_t)ptView->tTargetSize.x + uScale - 1) / uScale + 7) / 8,
            .uGroupCountY     = (((uint32_t)ptView->tTargetSize.y + uScale - 1) / uScale + 7) / 8,
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptCmd, 1, &tDispatch);
        gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_COMPUTE, PL_PIPELINE_STAGE_COMPUTE | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_TEXTURES);
        gptGfx->end_compute_pass(ptCmd);
    }
    gptGfx->pop_debug_group(ptCmd);
}

static bool
pl__renderer_ssgi_enabled(plView* ptView)
{
    plScene* ptScene = ptView->ptParentScene;
    const plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
    return ptSettings->tSSGI.bEnabled && ptSettings->tSSGI.fStrength > 0.0f && ptSettings->tSSGI.fRadius > 0.0f && ptScene->tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE && ptView->tTargetSize.x > 0 && ptView->tTargetSize.y > 0;
}

// Trace into a separate texture, then filter and add to HDR before outlines.
static void
pl__render_view_ssgi_pass(plView* ptView, plCommandBuffer* ptCmd)
{
    plDevice* ptDevice = gptData->ptDevice;
    plScene* ptScene = ptView->ptParentScene;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
    const plRendererSSGIOptions* ptOptions = &ptSettings->tSSGI;
    if(!pl__renderer_ssgi_enabled(ptView))
    {
        if(gptGfx->is_texture_valid(ptDevice, ptView->tSSGITexture))
        {
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->tSSGITexture);
            ptView->tSSGITexture = (plTextureHandle){0};
        }
        return;
    }
    if(!gptGfx->is_texture_valid(ptDevice, ptView->tSSGITexture))
    {
        plTextureDesc tDesc = {
            .tDimensions = gptGfx->get_texture(ptDevice, ptView->tRawOutputTexture)->tDesc.tDimensions,
            .eFormat     = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers     = 1, .uMips = 1, .eType = PL_TEXTURE_TYPE_2D,
            .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
            .pcDebugName = "SSGI indirect"
        };
        gptStarter->create_texture(&tDesc, NULL, 0, &ptView->tSSGITexture);
    }
    PL_RENDERER_STAT("renderer / SSGI views", 1);
    PL_RENDERER_STAT("renderer / SSGI output pixels", (double)ptView->tTargetSize.x * ptView->tTargetSize.y);
    plDynamicBinding tDynamic = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynSSGI));
    plGpuDynSSGI* ptData = (plGpuDynSSGI*)tDynamic.pcData;
    *ptData = (plGpuDynSSGI){
        .tProjection = ptView->tViewData.tCameraProjection[0],
        .tProjectionInv = ptView->tViewData.tCameraProjectionInv[0],
        .tView = ptView->tViewData.tCameraView[0],
        .tParameters = {
            fminf(fmaxf(ptOptions->fRadius, 0.01f), 1000.0f),
            fminf(fmaxf(ptOptions->fThickness, 0.001f), 10.0f),
            fminf(ptOptions->fStrength, 10.0f), 0.0f
        },
        .uWidth     = (uint32_t)ptView->tTargetSize.x,
        .uHeight    = (uint32_t)ptView->tTargetSize.y,
        .uRayCount  = ptOptions->uRayCount ? pl_min(ptOptions->uRayCount, 32u) : 8,
        .uStepCount = ptOptions->uStepCount ? pl_min(ptOptions->uStepCount, 64u) : 16
    };
    for(uint32_t uPass = 0; uPass < 2; uPass++)
    {
        const char* pcShader = uPass == 0 ? "ssgi_trace" : "ssgi_apply";
        plTextureHandle atTextures[] = {
            uPass == 0 ? ptView->tSSGITexture : ptView->tRawOutputTexture,
            uPass == 0 ? ptView->tRawOutputTexture : ptView->tSSGITexture,
            ptView->tDepthTexture,
            ptView->tNormalTexture,
            ptView->tAlbedoTexture,
            ptView->tAOMetalRoughnessTexture
        };
        plBindGroupDesc tBGDesc = {
            .ptPool = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "ssgi"),
            .pcDebugName = pcShader
        };
        plBindGroupHandle tBG = gptGfx->create_bind_group(ptDevice, &tBGDesc);
        plBindGroupUpdateData tUpdate = {
            .atSamplerBindings = {{.uSlot = 6, .tSampler = gptData->tSamplerLinearClamp}}
        };
        plPassResources tResources = {0};
        for(uint32_t i = 0; i < 6; i++)
        {
            tUpdate.atTextureBindings[i] = (plBindGroupUpdateTextureData){
                .tTexture = atTextures[i], .uSlot = i,
                .eType = i == 0 ? PL_TEXTURE_BINDING_TYPE_STORAGE : PL_TEXTURE_BINDING_TYPE_SAMPLED
            };
            tResources.atTextures[i] = (plPassTextureResource){
                .tHandle = atTextures[i], .eStages = PL_SHADER_STAGE_COMPUTE,
                .eAccess = i == 0 ? PL_PASS_RESOURCE_ACCESS_READ_WRITE : PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = i == 0 ? PL_TEXTURE_USAGE_STORAGE : PL_TEXTURE_USAGE_SAMPLED
            };
        }
        gptGfx->update_bind_group(ptDevice, tBG, &tUpdate);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tBG);
        gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
        gptGfx->begin_compute_pass(ptCmd, &tResources);
        gptGfx->push_debug_group(ptCmd, pcShader, (plVec4){0.2f, 0.6f, 0.3f, 1.0f});
        plComputeShaderHandle tShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, pcShader, NULL);
        gptGfx->bind_compute_shader(ptCmd, tShader);
        gptGfx->bind_compute_bind_groups(ptCmd, tShader, 0, 1, &tBG, 1, &tDynamic);
        plDispatch tDispatch = {
            .uGroupCountX = (ptData->uWidth + 7) / 8, .uGroupCountY = (ptData->uHeight + 7) / 8,
            .uGroupCountZ = 1, .uThreadPerGroupX = 8, .uThreadPerGroupY = 8, .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptCmd, 1, &tDispatch);
        gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
        gptGfx->pop_debug_group(ptCmd);
        gptGfx->end_compute_pass(ptCmd);
    }
}

static void
pl__render_view_bloom_pass(plView* ptView)
{
    PL_RENDERER_STAT("renderer / bloom views", 1);
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "bloom");

    // for convience
    // const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    for(uint32_t i = 0; i < ptSettings->tBloom.uChainLength - 1; i++)
    {

        const int sourceMip = i;
        const int targetMip = i + 1;

        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample"),
            .pcDebugName = "temp bind group c0"
        };
        plBindGroupHandle tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

        const plBindGroupUpdateData tTonemapBGData = {
            .atTextureBindings = {
                { // target
                    .tTexture      = ptView->sbtBloomDownChain[targetMip],
                    .uSlot         = 0,
                    .eType         = PL_TEXTURE_BINDING_TYPE_STORAGE
                },
                { // source
                    .tTexture      = i == 0 ? ptView->tFinalTexture : ptView->sbtBloomDownChain[sourceMip],
                    .uSlot         = 1,
                    .eType         = PL_TEXTURE_BINDING_TYPE_SAMPLED
                }
            },
            .atSamplerBindings = {
                { .uSlot = 2, .tSampler = gptData->tSamplerLinearClamp }
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, tTonemapBG, &tTonemapBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tTonemapBG);

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "bloom_downsample");
        gptGfx->begin_command_recording(ptPostCmdBuffer);

        const plPassResources tResources = {
            .atTextures = {
                {
                    .tHandle = tTonemapBGData.atTextureBindings[0].tTexture,
                    .eStages = PL_SHADER_STAGE_COMPUTE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                    .eUsage = PL_TEXTURE_USAGE_STORAGE
                }
            }
        };

        gptGfx->begin_compute_pass(ptPostCmdBuffer, &tResources);
        gptGfx->push_debug_group(ptPostCmdBuffer, "bloom_downsample", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});

        plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynBloomData));
        plGpuDynBloomData* ptTonemapData = (plGpuDynBloomData*)tTonemapDynamicBinding.pcData;
        ptTonemapData->iMipLevel = i;

        plComputeShaderHandle tTonemapShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "bloom_downsample", NULL);
        gptGfx->bind_compute_shader(ptPostCmdBuffer, tTonemapShader);
        gptGfx->bind_compute_bind_groups(ptPostCmdBuffer, tTonemapShader, 0, 1, &tTonemapBG, 1, &tTonemapDynamicBinding);

        plDispatch tTonemapDispatch = {
            .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
            .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptPostCmdBuffer, 1, &tTonemapDispatch);

        gptGfx->pop_debug_group(ptPostCmdBuffer);
        gptGfx->end_compute_pass(ptPostCmdBuffer);

        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
        gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
        gptGfx->return_command_buffer(ptPostCmdBuffer);
    }

    for(uint32_t i = 0; i < ptSettings->tBloom.uChainLength - 1; i++)
    {

        const int targetMip = ptSettings->tBloom.uChainLength - 2 - i;
        const int sourceMip = targetMip + 1;

        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_upsample"),
            .pcDebugName = "temp bind group c0"
        };
        plBindGroupHandle tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

        const plBindGroupUpdateData tTonemapBGData = {
            .atTextureBindings = {
                { // target
                    .tTexture      = ptView->sbtBloomUpChain[targetMip],
                    .uSlot         = 0,
                    .eType         = PL_TEXTURE_BINDING_TYPE_STORAGE
                },
                { // target previous mip
                    .tTexture      = ptView->sbtBloomUpChain[sourceMip],
                    .uSlot         = 1,
                    .eType         = PL_TEXTURE_BINDING_TYPE_SAMPLED
                },
                { // source
                    .tTexture      = ptView->sbtBloomDownChain[sourceMip],
                    .uSlot         = 2,
                    .eType         = PL_TEXTURE_BINDING_TYPE_SAMPLED
                }
            },
            .atSamplerBindings = {
                { .uSlot = 3, .tSampler = gptData->tSamplerLinearClamp }
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, tTonemapBG, &tTonemapBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tTonemapBG);

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "bloom_upsample");
        gptGfx->begin_command_recording(ptPostCmdBuffer);

        const plPassResources tResources = {
            .atTextures = {
                {
                    .tHandle = tTonemapBGData.atTextureBindings[0].tTexture,
                    .eStages = PL_SHADER_STAGE_COMPUTE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                    .eUsage = PL_TEXTURE_USAGE_STORAGE
                }
            }
        };

        gptGfx->begin_compute_pass(ptPostCmdBuffer, &tResources);
        gptGfx->push_debug_group(ptPostCmdBuffer, "upscale", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});

        plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynBloomData));
        plGpuDynBloomData* ptTonemapData = (plGpuDynBloomData*)tTonemapDynamicBinding.pcData;
        ptTonemapData->bloomStrength = ptSettings->tBloom.fStrength;
        ptTonemapData->blurRadius = ptSettings->tBloom.fRadius;
        ptTonemapData->isLowestMip = i == 0 ? 1 : 0;

        plComputeShaderHandle tTonemapShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "bloom_upsample", NULL);
        gptGfx->bind_compute_shader(ptPostCmdBuffer, tTonemapShader);
        gptGfx->bind_compute_bind_groups(ptPostCmdBuffer, tTonemapShader, 0, 1, &tTonemapBG, 1, &tTonemapDynamicBinding);

        plDispatch tTonemapDispatch = {
            .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
            .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptPostCmdBuffer, 1, &tTonemapDispatch);

        gptGfx->pop_debug_group(ptPostCmdBuffer);
        gptGfx->end_compute_pass(ptPostCmdBuffer);

        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample"),
            .pcDebugName = "temp bind group c0"
        };
        plBindGroupHandle tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

        const plBindGroupUpdateData tTonemapBGData = {
            .atTextureBindings = {
                { // target
                    .tTexture      = ptView->tFinalTexture,
                    .uSlot         = 0,
                    .eType         = PL_TEXTURE_BINDING_TYPE_STORAGE
                },
                { // target previous mip
                    .tTexture      = ptView->sbtBloomUpChain[0],
                    .uSlot         = 1,
                    .eType         = PL_TEXTURE_BINDING_TYPE_SAMPLED
                }
            },
            .atSamplerBindings = {
                { .uSlot = 2, .tSampler = gptData->tSamplerLinearClamp }
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, tTonemapBG, &tTonemapBGData);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tTonemapBG);

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "bloom_apply");
        gptGfx->begin_command_recording(ptPostCmdBuffer);

        const plPassResources tResources = {
            .atTextures = {
                {
                    .tHandle = tTonemapBGData.atTextureBindings[0].tTexture,
                    .eStages = PL_SHADER_STAGE_COMPUTE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                    .eUsage = PL_TEXTURE_USAGE_STORAGE
                }
            }
        };

        gptGfx->begin_compute_pass(ptPostCmdBuffer, &tResources);
        gptGfx->push_debug_group(ptPostCmdBuffer, "bloom_apply", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});

        plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynBloomData));
        plGpuDynBloomData* ptTonemapData = (plGpuDynBloomData*)tTonemapDynamicBinding.pcData;
        ptTonemapData->bloomStrength = ptSettings->tBloom.fStrength;
        ptTonemapData->blurRadius = ptSettings->tBloom.fRadius;

        plComputeShaderHandle tTonemapShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "bloom_apply", NULL);
        gptGfx->bind_compute_shader(ptPostCmdBuffer, tTonemapShader);
        gptGfx->bind_compute_bind_groups(ptPostCmdBuffer, tTonemapShader, 0, 1, &tTonemapBG, 1, &tTonemapDynamicBinding);

        plDispatch tTonemapDispatch = {
            .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
            .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };
        gptGfx->dispatch(ptPostCmdBuffer, 1, &tTonemapDispatch);

        gptGfx->pop_debug_group(ptPostCmdBuffer);
        gptGfx->end_compute_pass(ptPostCmdBuffer);

        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
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
pl__cloud_release_low_resolution(plView* ptView)
{
    pl__renderer_return_bindless_texture_index(ptView->ptParentScene, ptView->tCloudLowResTexture);
    if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->tCloudLowResTexture))
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tCloudLowResTexture);
    ptView->tCloudLowResTexture = (plTextureHandle){0};
    for(uint32_t i = 0; i < 2; i++)
    {
        pl__renderer_return_bindless_texture_index(ptView->ptParentScene, ptView->atCloudHistory[i]);
        pl__renderer_return_bindless_texture_index(ptView->ptParentScene, ptView->atCloudDistance[i]);
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->atCloudHistory[i]))
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atCloudHistory[i]);
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->atCloudDistance[i]))
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atCloudDistance[i]);
        ptView->atCloudHistory[i] = (plTextureHandle){0};
        ptView->atCloudDistance[i] = (plTextureHandle){0};
    }
    ptView->bCloudHistoryValid = false;
    ptView->uCloudHistoryIndex = 0;
}

static void
pl__cloud_release_shadows(plScene* ptScene)
{
    ptScene->tSceneData.tCloudShadowCenter.w = -1.0f;
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        if(gptGfx->is_bind_group_valid(gptData->ptDevice, ptScene->atCloudShadowBG[i]))
            gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptScene->atCloudShadowBG[i]);
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptScene->atCloudShadowTexture[i]))
        {
            pl__renderer_return_bindless_texture_index(ptScene, ptScene->atCloudShadowTexture[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->atCloudShadowTexture[i]);
        }
        ptScene->atCloudShadowBG[i] = (plBindGroupHandle){0};
        ptScene->atCloudShadowTexture[i] = (plTextureHandle){0};
        ptScene->auCloudShadowIndex[i] = UINT32_MAX;
    }
}

static void
pl__cloud_ensure_shadows(plScene* ptScene)
{
    const plTextureDesc tDesc = {
        .tDimensions = {256, 256 * 32, 1}, .eFormat = PL_FORMAT_R32_FLOAT,
        .uLayers = 1, .uMips = 1, .eType = PL_TEXTURE_TYPE_2D,
        .eUsage = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName = "cloud deep shadow atlas"
    };
    const plBindGroupDesc tBGDesc = {
        .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "simple_texture_lut"),
        .ptPool = gptData->ptBindGroupPool, .pcDebugName = "cloud deep shadow output"
    };
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        if(gptGfx->is_texture_valid(gptData->ptDevice, ptScene->atCloudShadowTexture[i]))
            continue;
        gptStarter->create_texture(&tDesc, NULL, 0, &ptScene->atCloudShadowTexture[i]);
        ptScene->atCloudShadowBG[i] = gptGfx->create_bind_group(gptData->ptDevice, &tBGDesc);
        const plBindGroupUpdateData tBindings = {
            .atTextureBindings = {{.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptScene->atCloudShadowTexture[i]}}
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atCloudShadowBG[i], &tBindings);
        ptScene->auCloudShadowIndex[i] = pl__renderer_get_bindless_texture_index(ptScene, ptScene->atCloudShadowTexture[i]);
    }
}

static void
pl__cloud_ensure_low_resolution(plView* ptView)
{
    if(gptGfx->is_texture_valid(gptData->ptDevice, ptView->tCloudLowResTexture))
        return;
    plTextureDesc tDesc = gptGfx->get_texture(gptData->ptDevice, ptView->tFinalTexture)->tDesc;
    tDesc.tDimensions.x = ceilf(tDesc.tDimensions.x / 2.0f);
    tDesc.tDimensions.y = ceilf(tDesc.tDimensions.y / 2.0f);
    tDesc.eUsage = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED;
    tDesc.pcDebugName = "half resolution cloud radiance and transmittance";
    gptStarter->create_texture(&tDesc, NULL, 0, &ptView->tCloudLowResTexture);
    for(uint32_t i = 0; i < 2; i++)
    {
        tDesc.pcDebugName = "cloud temporal radiance and transmittance";
        gptStarter->create_texture(&tDesc, NULL, 0, &ptView->atCloudHistory[i]);
    }
    tDesc.eFormat = PL_FORMAT_R32G32_FLOAT;
    tDesc.pcDebugName = "cloud representative distance and source depth";
    for(uint32_t i = 0; i < 2; i++)
        gptStarter->create_texture(&tDesc, NULL, 0, &ptView->atCloudDistance[i]);
}

static void
pl__cloud_render_half_resolution(plView* ptView, plCommandBuffer* ptCmd)
{
    pl__cloud_ensure_low_resolution(ptView);
    plScene* ptScene = ptView->ptParentScene;
    const uint32_t uFrame = gptGfx->get_current_frame_index();
    const uint32_t uCurrent = ptView->uCloudHistoryIndex;
    const uint32_t uPrevious = 1 - uCurrent;
    const plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    const float fDelta = ptScene->tSceneData.g_time - ptView->fCloudHistoryTime;
    const bool bTemporal = ptEnvironment->fCloudTemporalBlend > 0.0f && fabsf(ptView->tViewData.tCameraProjection[0].d[15]) < 0.5f;
    const plVec3 tCamera = ptView->tViewData.tCameraPos.xyz;
    const plVec3 tForward = pl_mul_mat4_vec4(&ptView->tViewData.tCameraViewInv[0], (plVec4){0, 0, 1, 0}).xyz;
    const bool bHistory = bTemporal && ptView->bCloudHistoryValid && ptView->ulCloudHistoryFrame + 1 == gptIO->ulFrameCount && fDelta >= 0.0f && fDelta < 0.25f &&
        ptView->tCloudHistorySize.x == ptView->tTargetSize.x && ptView->tCloudHistorySize.y == ptView->tTargetSize.y &&
        memcmp(&ptView->tCloudHistoryEnvironment, ptEnvironment, sizeof(*ptEnvironment)) == 0 &&
        memcmp(&ptView->tCloudHistoryProjection, &ptView->tViewData.tCameraProjection[0], sizeof(plMat4)) == 0 &&
        pl_length_vec3(pl_sub_vec3(tCamera, ptView->tCloudHistoryCamera)) * ptScene->tSceneData.fAtmosphereConversion < 0.5f &&
        pl_dot_vec3(tForward, ptView->tCloudHistoryForward) > 0.9f;

    const float fHistoryWeight = bHistory ? pl_clampf(0.0f, ptEnvironment->fCloudTemporalBlend, 0.95f) : 0.0f;
    PL_RENDERER_STAT("renderer / cloud temporal resolves", bTemporal ? 1 : 0);
    PL_RENDERER_STAT("renderer / cloud history accepted views", bHistory ? 1 : 0);
    PL_RENDERER_STAT("renderer / cloud history rejected views", bTemporal && !bHistory ? 1 : 0);
    const plTextureHandle tResolvedCloud = bTemporal ? ptView->atCloudHistory[uCurrent] : ptView->tCloudLowResTexture;
    gptGfx->push_debug_group(ptCmd, "Half resolution clouds, temporal resolve and upsample", (plVec4){0.3f, 0.5f, 0.8f, 1});
    for(uint32_t uPass = 0; uPass < 3; uPass++)
    {
        if(uPass == 1 && !bTemporal)
            continue;
        plTextureHandle tOutput = uPass == 0 ? ptView->tCloudLowResTexture : uPass == 1 ? ptView->atCloudHistory[uCurrent] : ptView->tRawOutputTexture;
        plComputeShaderHandle tShader = uPass == 0 ? ptScene->tCloudLowResShader : uPass == 1 ? ptScene->tCloudTemporalShader : ptScene->tCloudCompositeShader;
        const plBindGroupDesc tDesc = {
            .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, uPass == 0 ? "cloud_lowres" : uPass == 1 ? "cloud_temporal" : "aerial_luts"),
            .ptPool = gptData->aptTempGroupPools[uFrame], .pcDebugName = "cloud composite"
        };
        plBindGroupHandle tGroup = gptGfx->create_bind_group(gptData->ptDevice, &tDesc);
        plBindGroupUpdateData tBindings = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = tOutput},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->tDepthTexture}
            }
        };
        if(uPass == 0)
            tBindings.atTextureBindings[2] = (plBindGroupUpdateTextureData){.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptView->atCloudDistance[uCurrent]};

        else if(uPass == 1)
        {
            tBindings.atTextureBindings[1] = (plBindGroupUpdateTextureData){.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->tCloudLowResTexture};
            tBindings.atTextureBindings[2] = (plBindGroupUpdateTextureData){.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->atCloudDistance[uCurrent]};
            tBindings.atTextureBindings[3] = (plBindGroupUpdateTextureData){.uSlot = 3, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->atCloudHistory[uPrevious]};
            tBindings.atTextureBindings[4] = (plBindGroupUpdateTextureData){.uSlot = 4, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->atCloudDistance[uPrevious]};
        }
        else
            tBindings.atTextureBindings[2] = (plBindGroupUpdateTextureData){.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = tResolvedCloud};
        
            gptGfx->update_bind_group(gptData->ptDevice, tGroup, &tBindings);
        plPassResources tResources = {
            .atTextures = {
                {.tHandle = tOutput, .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = uPass < 2 ? PL_PASS_RESOURCE_ACCESS_WRITE : PL_PASS_RESOURCE_ACCESS_READ_WRITE, .eUsage = PL_TEXTURE_USAGE_STORAGE},
                {.tHandle = ptView->tDepthTexture, .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED},
                {.tHandle = uPass == 0 ? ptScene->atCloudShadowTexture[uFrame] : tResolvedCloud, .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED}
            }
        };
        if(uPass == 0)
            tResources.atTextures[3] = (plPassTextureResource){.tHandle = ptView->atCloudDistance[uCurrent], .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_TEXTURE_USAGE_STORAGE};
        else if(uPass == 1)
        {
            // All passes use the graphics queue. Barriers also order history reads
            // against the preceding frame's writes, including frames in flight.
            for(uint32_t i = 1; i < 5; i++)
                tResources.atTextures[i] = (plPassTextureResource){.tHandle = tBindings.atTextureBindings[i].tTexture, .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED};
        }
        else
        {
            // Transition the source-depth metadata for the later water lookup,
            // including when temporal resolve is disabled.
            tResources.atTextures[3] = (plPassTextureResource){.tHandle = ptView->atCloudDistance[uCurrent], .eStages = PL_PIPELINE_STAGE_FRAGMENT, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED};
        }
        gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_COMPUTE, PL_BARRIER_SCOPE_ALL);
        gptGfx->begin_compute_pass(ptCmd, &tResources);
        plDynamicBinding tDynamic = pl__allocate_dynamic_data(gptData->ptDevice, uPass == 1 ? sizeof(plCloudTemporalData) : 16);
        if(uPass == 1)
        {
            *(plCloudTemporalData*)tDynamic.pcData = (plCloudTemporalData){
                .tPreviousViewProjection = ptView->tCloudHistoryViewProjection,
                .tPreviousCamera = {.xyz = ptView->tCloudHistoryCamera},
                .tParams = {fHistoryWeight, fDelta, 0, 0}
            };
        }
        else
        {
            memset(tDynamic.pcData, 0, 16);
            ((float*)tDynamic.pcData)[1] = fHistoryWeight > 0.0f ? fmodf((float)(gptIO->ulFrameCount % 1024) * 0.618033989f, 1.0f) : -1.0f;
        }
        plBindGroupHandle atGroups[] = {ptScene->atSceneBindGroups[uFrame], ptView->atViewBG[uFrame], tGroup};
        gptGfx->bind_compute_shader(ptCmd, tShader);
        gptGfx->bind_compute_bind_groups(ptCmd, tShader, 0, 3, atGroups, 1, &tDynamic);
        uint32_t uDivisor = uPass < 2 ? 2 : 1;
        uint32_t uWidth = ((uint32_t)ptView->tTargetSize.x + uDivisor - 1) / uDivisor;
        uint32_t uHeight = ((uint32_t)ptView->tTargetSize.y + uDivisor - 1) / uDivisor;
        if(uPass == 0)
        {
            PL_RENDERER_STAT("renderer / cloud integration pixels", (double)uWidth * uHeight);
        }
        const plDispatch tDispatch = {.uGroupCountX = (uWidth + 7) / 8, .uGroupCountY = (uHeight + 7) / 8, .uGroupCountZ = 1, .uThreadPerGroupX = 8, .uThreadPerGroupY = 8, .uThreadPerGroupZ = 1};
        gptGfx->dispatch(ptCmd, 1, &tDispatch);
        gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_COMPUTE, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
        gptGfx->end_compute_pass(ptCmd);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tGroup);
    }
    ptView->uCloudHistoryIndex = uPrevious;
    ptView->bCloudHistoryValid = bTemporal;
    ptView->ulCloudHistoryFrame = gptIO->ulFrameCount;
    ptView->fCloudHistoryTime = ptScene->tSceneData.g_time;
    ptView->tCloudHistorySize = ptView->tTargetSize;
    ptView->tCloudHistoryViewProjection = ptView->tViewData.tCameraViewProjection[0];
    ptView->tCloudHistoryProjection = ptView->tViewData.tCameraProjection[0];
    ptView->tCloudHistoryCamera = tCamera;
    ptView->tCloudHistoryForward = tForward;
    
    // Copy padding too: the next frame's byte comparison must not depend on
    // whether a C struct assignment happened to preserve those bytes.
    memcpy(&ptView->tCloudHistoryEnvironment, ptEnvironment, sizeof(*ptEnvironment));
    gptGfx->pop_debug_group(ptCmd);
}

static void
pl__cloud_shadow_prepare(plScene* ptScene, const plCamera* ptCamera)
{
    plGpuSceneData* ptData = &ptScene->tSceneData;
    ptData->tCloudShadowCenter.w = -1.0f;
    const plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    const bool bClouds = ptData->tCloudLayer.x > 0.0f && ptData->tCloudLayer.w > 0.0f;
    // Include views that are not rendered this frame when retiring unused targets.
    if(!bClouds || ptEnvironment->uCloudResolutionDivisor < 2)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbptViews); i++)
            pl__cloud_release_low_resolution(ptScene->sbptViews[i]);
    }
    if(!bClouds || pl_length_vec3(ptData->tDirection) < 1e-6f)
    {
        pl__cloud_release_shadows(ptScene);
        return;
    }
    pl__cloud_ensure_shadows(ptScene);

    const float fScale = pl_maxf(ptData->fAtmosphereConversion, 1e-7f);
    plVec3 tCenter = ptCamera ? pl_mul_vec3_scalarf(ptCamera->tPositionF, fScale) : (plVec3){0};
    const float fRadius = ptData->planetRadius + ptData->tCloudLayer.y + 0.5f * ptData->tCloudLayer.z;
    tCenter.y = sqrtf(pl_maxf(0.0f, fRadius * fRadius - tCenter.x * tCenter.x - tCenter.z * tCenter.z)) - ptData->planetRadius;
    const float fRange = 40.0f;
    const float fCurveMargin = (2.0f * fRange * (fabsf(tCenter.x) + fabsf(tCenter.z)) + 2.0f * fRange * fRange) / pl_maxf(fRadius, 1.0f);
    const plVec3 tHalfSize = {fRange, 0.5f * ptData->tCloudLayer.z + fCurveMargin + 0.1f, fRange};
    const plVec3 tForward = pl_norm_vec3(ptData->tDirection); // from sun toward scene
    const plVec3 tReference = fabsf(tForward.y) > 0.99f ? (plVec3){1, 0, 0} : (plVec3){0, 1, 0};
    const plVec3 tRight = pl_norm_vec3(pl_cross_vec3(tReference, tForward));
    const plVec3 tUp = pl_cross_vec3(tForward, tRight);
    const plVec3 atAxes[] = {tRight, tUp, tForward};
    // Snap in light space, including deep-opacity slices. A one-cell move now
    // reuses the same world sampling lattice instead of sliding it through noise.
    const plCloudShadowGrid tGrid = pl_cloud_shadow_grid(tCenter, tHalfSize, atAxes, 256, 32);
    ptData->tCloudShadowRight = tGrid.atAxes[0];
    ptData->tCloudShadowUp = tGrid.atAxes[1];
    ptData->tCloudShadowForward = tGrid.atAxes[2];
    ptData->tCloudShadowCenter = (plVec4){.xyz = tGrid.tCenter, .w = (float)ptScene->auCloudShadowIndex[gptGfx->get_current_frame_index()]};
}

static void
pl__cloud_shadow_render(plScene* ptScene)
{
    if(ptScene->tSceneData.tCloudShadowCenter.w < 0.0f) return;
    const uint32_t uFrame = gptGfx->get_current_frame_index();
    plCommandBuffer* ptCmd = gptGfx->request_command_buffer(gptStarter->get_current_command_pool(), "cloud deep shadows");
    gptGfx->begin_command_recording(ptCmd);
    const plPassResources tResources = {
        .atTextures = {
            {.tHandle = ptScene->atCloudShadowTexture[uFrame], .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_TEXTURE_USAGE_STORAGE}
        }
    };
    gptGfx->begin_compute_pass(ptCmd, &tResources);
    gptGfx->push_debug_group(ptCmd, "Cloud deep shadow cache", (plVec4){0.4f, 0.5f, 0.8f, 1});
    plBindGroupHandle atGroups[] = {ptScene->atSceneBindGroups[uFrame], ptScene->atCloudShadowBG[uFrame]};
    gptGfx->bind_compute_shader(ptCmd, ptScene->tCloudShadowShader);
    gptGfx->bind_compute_bind_groups(ptCmd, ptScene->tCloudShadowShader, 0, 2, atGroups, 0, NULL);
    const plDispatch tDispatch = {
        .uGroupCountX     = 32,
        .uGroupCountY     = 32,
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptCmd, 1, &tDispatch);
    gptGfx->pop_debug_group(ptCmd);
    gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_COMPUTE, PL_PIPELINE_STAGE_COMPUTE | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL);
    gptGfx->end_compute_pass(ptCmd);
    gptGfx->end_command_recording(ptCmd);
    const plSubmitInfo tSubmit = {
        .uWaitSemaphoreCount     = 1, .atWaitSempahores = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1, .atSignalSempahores = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCmd, &tSubmit);
    gptGfx->return_command_buffer(ptCmd);
}


static void
pl__cloud_probe_composite(plScene* ptScene, plEnvironmentProbeData* ptProbe, plEnvironmentProbeDataPack* ptPack, uint32_t uFace, plCommandBuffer* ptCmd)
{
    if(ptScene->tSceneData.tCloudLayer.x <= 0.0f || ptScene->tSceneData.tCloudLayer.w <= 0.0f) return;
    const uint32_t uFrame = gptGfx->get_current_frame_index();
    const plBindGroupDesc tDesc = {
        .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"),
        .ptPool = gptData->aptTempGroupPools[uFrame], .pcDebugName = "probe clouds"
    };
    plBindGroupHandle tGroup = gptGfx->create_bind_group(gptData->ptDevice, &tDesc);
    const plBindGroupUpdateData tBindings = {
        .atTextureBindings = {
            {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptProbe->atRawOutputTextureViews[uFace]},
            {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptPack->atDepthTextureViews[uFace]}
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, tGroup, &tBindings);
    const plPassResources tResources = {
        .atTextures = {
            {.tHandle = ptProbe->atRawOutputTextureViews[uFace], .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE, .eUsage = PL_TEXTURE_USAGE_STORAGE},
            {.tHandle = ptPack->atDepthTextureViews[uFace], .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED},
            {.tHandle = ptScene->atCloudShadowTexture[uFrame], .eStages = PL_PIPELINE_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED}
        }
    };
    gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_COMPUTE, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_compute_pass(ptCmd, &tResources);
    plDynamicBinding tDynamic = pl__allocate_dynamic_data(gptData->ptDevice, sizeof(uint32_t));
    *(uint32_t*)tDynamic.pcData = uFace;
    plBindGroupHandle atGroups[] = {ptScene->atSceneBindGroups[uFrame], ptProbe->tViewBG, tGroup};
    gptGfx->bind_compute_shader(ptCmd, ptScene->tCloudShader);
    gptGfx->bind_compute_bind_groups(ptCmd, ptScene->tCloudShader, 0, 3, atGroups, 1, &tDynamic);
    const plDispatch tDispatch = {
        .uGroupCountX     = ((uint32_t)ptProbe->tTargetSize.x + 7) / 8,
        .uGroupCountY     = ((uint32_t)ptProbe->tTargetSize.y + 7) / 8,
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptCmd, 1, &tDispatch);
    gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_COMPUTE, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->end_compute_pass(ptCmd);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tGroup);
}

static void
pl__render_view_cloud_pass(plView* ptView, plCommandBuffer* ptPostCmdBuffer)
{
    plRenderEnvironment* ptCloudEnvironment = gptAsset->get_data(ptView->ptParentScene->tEnvironment);
    if(ptView->ptParentScene->tSceneData.tCloudLayer.x <= 0.0f ||
       ptView->ptParentScene->tSceneData.tCloudLayer.w <= 0.0f ||
       ptView->tTargetSize.x <= 0.0f || ptView->tTargetSize.y <= 0.0f)
    {
        pl__cloud_release_low_resolution(ptView);
        return;
    }
    if(ptCloudEnvironment->uCloudResolutionDivisor >= 2)
    {
        PL_RENDERER_STAT("renderer / cloud low resolution views", 1);
        pl__cloud_render_half_resolution(ptView, ptPostCmdBuffer);
        return;
    }
    PL_RENDERER_STAT("renderer / cloud full resolution views", 1);
    PL_RENDERER_STAT("renderer / cloud integration pixels", (double)ptView->tTargetSize.x * ptView->tTargetSize.y);
    pl__cloud_release_low_resolution(ptView);
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;

    const plPassResources tResources = {
        .atTextures = {
            {
                .tHandle = ptView->tRawOutputTexture,
                .eStages = PL_PIPELINE_STAGE_COMPUTE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                .eUsage = PL_TEXTURE_USAGE_STORAGE
            },
            {
                .tHandle = ptView->tDepthTexture,
                .eStages = PL_PIPELINE_STAGE_COMPUTE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = PL_TEXTURE_USAGE_SAMPLED
            },
            {
                .tHandle = ptScene->atCloudShadowTexture[gptGfx->get_current_frame_index()],
                .eStages = PL_PIPELINE_STAGE_COMPUTE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = PL_TEXTURE_USAGE_SAMPLED
            }
        }
    };

    gptGfx->consumer_barrier(ptPostCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_compute_pass(ptPostCmdBuffer, &tResources);
    gptGfx->push_debug_group(ptPostCmdBuffer, "Volumetric Clouds", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});

    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(uint32_t));
    uint32_t* ptDynamicData = (uint32_t*)tDynamicBinding.pcData;
    *ptDynamicData = 0; // camera index

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    // The aerial bind group targets the post-outline texture. Clouds now write
    // opaque HDR before transparency, so bind the raw output explicitly.
    const plBindGroupDesc tBGDesc = {
        .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"),
        .ptPool = gptData->aptTempGroupPools[uFrameIdx], .pcDebugName = "opaque HDR clouds"
    };
    plBindGroupHandle tCloudBG = gptGfx->create_bind_group(ptDevice, &tBGDesc);
    const plBindGroupUpdateData tBindings = {
        .atTextureBindings = {
            {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptView->tRawOutputTexture},
            {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->tDepthTexture}
        }
    };
    gptGfx->update_bind_group(ptDevice, tCloudBG, &tBindings);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tCloudBG);
    gptGfx->bind_compute_shader(ptPostCmdBuffer, ptScene->tCloudShader);
    plBindGroupHandle atPostBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atViewBG[uFrameIdx], tCloudBG };
    gptGfx->bind_compute_bind_groups(ptPostCmdBuffer, ptScene->tCloudShader, 0, PL_ARRAYSIZE(atPostBGs), atPostBGs, 1, &tDynamicBinding);

    plDispatch tCloudDispatch = {
        .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
        .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptPostCmdBuffer, 1, &tCloudDispatch);

    gptGfx->producer_barrier(ptPostCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->pop_debug_group(ptPostCmdBuffer);
    gptGfx->end_compute_pass(ptPostCmdBuffer);
}

static void
pl__render_view_aerial_pass(plView* ptView)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;

    plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "tonemap");
    gptGfx->begin_command_recording(ptPostCmdBuffer);

    const plPassResources tResources = {
        .atTextures = {
            {
                .tHandle = ptView->tFinalTexture,
                .eStages = PL_SHADER_STAGE_COMPUTE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                .eUsage = PL_TEXTURE_USAGE_STORAGE
            }
        }
    };

    gptGfx->consumer_barrier(ptPostCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_compute_pass(ptPostCmdBuffer, &tResources);
    gptGfx->push_debug_group(ptPostCmdBuffer, "Aerial Compute", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
    
    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(uint32_t));
    uint32_t* ptDynamicData = (uint32_t*)tDynamicBinding.pcData;
    *ptDynamicData = 0; // camera index

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    gptGfx->bind_compute_shader(ptPostCmdBuffer, ptScene->tSkyAerialShader);
    plBindGroupHandle atPostBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atViewBG[uFrameIdx], ptView->atSkyAerialBG2[uFrameIdx] };
    gptGfx->bind_compute_bind_groups(ptPostCmdBuffer, ptScene->tSkyAerialShader, 0, PL_ARRAYSIZE(atPostBGs), atPostBGs, 1, &tDynamicBinding);

    plDispatch tTonemapDispatch = {
        .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
        .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptPostCmdBuffer, 1, &tTonemapDispatch);

    gptGfx->producer_barrier(ptPostCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->pop_debug_group(ptPostCmdBuffer);
    gptGfx->end_compute_pass(ptPostCmdBuffer);

    gptGfx->end_command_recording(ptPostCmdBuffer);

    const plSubmitInfo tPostSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
    gptGfx->return_command_buffer(ptPostCmdBuffer);
}

static void
pl__render_view_tonemap_pass(plView* ptView)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;

    plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "tonemap");
    gptGfx->begin_command_recording(ptPostCmdBuffer);

    const plPassResources tResources = {
        .atTextures = {
            {
                .tHandle = ptView->tFinalTexture,
                .eStages = PL_SHADER_STAGE_COMPUTE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                .eUsage = PL_TEXTURE_USAGE_STORAGE
            }
        }
    };

    gptGfx->consumer_barrier(ptPostCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_compute_pass(ptPostCmdBuffer, &tResources);
    gptGfx->push_debug_group(ptPostCmdBuffer, "Tonemap Compute", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
    
    plDynamicBinding tTonemapDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynTonemap));
    plGpuDynTonemap* ptTonemapData = (plGpuDynTonemap*)tTonemapDynamicBinding.pcData;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
    ptTonemapData->iMode = ptSettings->tTonemap.tMode;
    ptTonemapData->fExposure = ptSettings->tTonemap.fExposure;
    ptTonemapData->fBrightness = ptSettings->tTonemap.fBrightness;
    ptTonemapData->fContrast = ptSettings->tTonemap.fContrast;
    ptTonemapData->fSaturation = ptSettings->tTonemap.fSaturation;

    plComputeShaderHandle tTonemapShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "tonemap", NULL);
    gptGfx->bind_compute_shader(ptPostCmdBuffer, tTonemapShader);
    gptGfx->bind_compute_bind_groups(ptPostCmdBuffer, tTonemapShader, 0, 1, &ptView->tTonemapBG, 1, &tTonemapDynamicBinding);

    plDispatch tTonemapDispatch = {
        .uGroupCountX     = (uint32_t)(ceilf(ptView->tTargetSize.x / 8.0f)),
        .uGroupCountY     = (uint32_t)(ceilf(ptView->tTargetSize.y / 8.0f)),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptPostCmdBuffer, 1, &tTonemapDispatch);

    gptGfx->producer_barrier(ptPostCmdBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->pop_debug_group(ptPostCmdBuffer);
    gptGfx->end_compute_pass(ptPostCmdBuffer);

    gptGfx->end_command_recording(ptPostCmdBuffer);

    const plSubmitInfo tPostSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
    gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
    gptGfx->return_command_buffer(ptPostCmdBuffer);
}

static void
pl__render_view_fxaa_pass(plView* ptView)
{
    if(ptView->tTargetSize.x <= 0 || ptView->tTargetSize.y <= 0)
        return;
    PL_RENDERER_STAT("renderer / FXAA views", 1);
    plDevice* ptDevice = gptData->ptDevice;
    plScene* ptScene = ptView->ptParentScene;
    if(!gptGfx->is_texture_valid(ptDevice, ptView->tFXAAInputTexture))
    {
        const plTextureDesc tDesc = {
            .tDimensions = gptGfx->get_texture(ptDevice, ptView->tFinalTexture)->tDesc.tDimensions,
            .eFormat     = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers     = 1,
            .uMips       = 1,
            .eType       = PL_TEXTURE_TYPE_2D,
            .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
            .pcDebugName = "FXAA input"
        };
        gptStarter->create_texture(&tDesc, NULL, 0, &ptView->tFXAAInputTexture);
    }

    const plBindGroupDesc tBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample"),
        .pcDebugName = "FXAA"
    };
    plBindGroupHandle tBG = gptGfx->create_bind_group(ptDevice, &tBGDesc);
    const plBindGroupUpdateData tBGData = {
        .atTextureBindings = {
            {.tTexture = ptView->tFinalTexture, .uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE},
            {.tTexture = ptView->tFXAAInputTexture, .uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        },
        .atSamplerBindings = {{.uSlot = 2, .tSampler = gptData->tSamplerLinearClamp}}
    };
    gptGfx->update_bind_group(ptDevice, tBG, &tBGData);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tBG);

    plCommandBuffer* ptCmd = gptGfx->request_command_buffer(gptStarter->get_current_command_pool(), "FXAA");
    gptGfx->begin_command_recording(ptCmd);
    gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_compute_pass(ptCmd, NULL);
    const plImageCopy tCopy = {
        .uSourceExtentX         = (uint32_t)ptView->tTargetSize.x,
        .uSourceExtentY         = (uint32_t)ptView->tTargetSize.y,
        .uSourceExtentZ         = 1,
        .uSourceLayerCount      = 1,
        .uDestinationLayerCount = 1
    };
    // Never filter in place: neighboring invocations need the original image.
    gptGfx->copy_texture(ptCmd, ptView->tFinalTexture, ptView->tFXAAInputTexture, 1, &tCopy);
    gptGfx->end_compute_pass(ptCmd);
    gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);

    const plPassResources tResources = {
        .atTextures = {
            {.tHandle = ptView->tFinalTexture, .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_TEXTURE_USAGE_STORAGE},
            {.tHandle = ptView->tFXAAInputTexture, .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ, .eUsage = PL_TEXTURE_USAGE_SAMPLED}
        }
    };
    gptGfx->begin_compute_pass(ptCmd, &tResources);
    gptGfx->push_debug_group(ptCmd, "FXAA Compute", (plVec4){0.0f, 0.32f, 0.10f, 1.0f});
    plDynamicBinding tDynamic = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynFXAA));
    plGpuDynFXAA* ptData = (plGpuDynFXAA*)tDynamic.pcData;
    *ptData = (plGpuDynFXAA){.iWidth = (int)ptView->tTargetSize.x, .iHeight = (int)ptView->tTargetSize.y};
    plComputeShaderHandle tShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "fxaa", NULL);
    gptGfx->bind_compute_shader(ptCmd, tShader);
    gptGfx->bind_compute_bind_groups(ptCmd, tShader, 0, 1, &tBG, 1, &tDynamic);
    const plDispatch tDispatch = {
        .uGroupCountX = ((uint32_t)ptData->iWidth + 7) / 8,
        .uGroupCountY = ((uint32_t)ptData->iHeight + 7) / 8,
        .uGroupCountZ = 1,
        .uThreadPerGroupX = 8, .uThreadPerGroupY = 8, .uThreadPerGroupZ = 1
    };
    gptGfx->dispatch(ptCmd, 1, &tDispatch);
    gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->pop_debug_group(ptCmd);
    gptGfx->end_compute_pass(ptCmd);
    gptGfx->end_command_recording(ptCmd);

    const plSubmitInfo tSubmit = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    ptScene->uLastSemValueForShadow = gptStarter->get_current_timeline_value();
    gptGfx->submit_command_buffer(ptCmd, &tSubmit);
    gptGfx->return_command_buffer(ptCmd);
}

static void
pl__renderer_probe_update_all(plScene* ptScene)
{

    plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    const plEcsTypeKey tTransformComponentType =gptTransform->get_ecs_type_key_transform();

    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plDrawStream* ptStream = &gptData->tDrawStream;

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        // The sky probe also participates in sheen filtering, but has no ECS component.
        plEnvironmentProbeData* ptProbe = uProbeIndex == 0 ? &ptScene->tDefaultProbeData : &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = uProbeIndex == 0 ? NULL : gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);
        plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptProbe->uDataPackIndex];

        if((ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED) && !gptGfx->is_texture_valid(ptDevice, ptProbe->tSheenEnvTexture))
        {
            PL_LOG_INFO_API(gptLog, gptData->uLogChannel, "creating required sheen env texture");
            const plTextureDesc tTextureDesc = {
                .tDimensions = {ptProbe->tTargetSize.x, ptProbe->tTargetSize.y, 1},
                .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
                .uLayers     = 6,
                .uMips       = (uint32_t)floorf(log2f(ptProbe->tTargetSize.x)) - 3, // guarantee final dispatch during filtering is 16 threads
                .eType       = PL_TEXTURE_TYPE_CUBE,
                .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .pcDebugName = "tSheenEnvTexture"
            };
            gptStarter->create_texture(&tTextureDesc, NULL, 0, &ptProbe->tSheenEnvTexture);
            ptProbe->uSheenEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, ptProbe->tSheenEnvTexture);
            if(ptProbeComp)
                ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
            else
                ptScene->tFlags |= PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY;
        }
        else if(!(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED) && gptGfx->is_texture_valid(ptDevice, ptProbe->tSheenEnvTexture))
        {
            PL_LOG_INFO_API(gptLog, gptData->uLogChannel, "freeing unneeded sheen env texture");
            gptGfx->queue_texture_for_deletion(ptDevice, ptProbe->tSheenEnvTexture);
            ptProbe->tSheenEnvTexture = (plTextureHandle){0};
            ptProbe->uSheenEnvSampler = 0;
        }

        if(uProbeIndex == 0)
        {
            // Scene teardown owns the sky probe's resources through this array entry.
            ptScene->sbtProbeData[0].tSheenEnvTexture = ptProbe->tSheenEnvTexture;
            ptScene->sbtProbeData[0].uSheenEnvSampler = ptProbe->uSheenEnvSampler;
            continue;
        }

        if(ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME || ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY || ptScene->tFlags & PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY)
        {
            if(ptProbe->uDirtyFaces == 0 && (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY))
                ptProbe->uDirtyFaces = 6;
        }
    }

    pl_sb_reset(ptScene->sbtGPUProbeData);

    pl__renderer_update_sky_probe(ptScene);

    for(uint32_t i = 1; i < pl_sb_size(ptScene->sbtProbeData); i++)
    {
        plEnvironmentProbeData* ptProbeData = &ptScene->sbtProbeData[i];
        plEnvironmentProbeComponent* ptProbe = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbeData->tEntity);
        plTransformComponent* ptProbeTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptProbeData->tEntity);
        plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptProbeData->uDataPackIndex];
        plGpuProbe tProbeData = {
            .tPosition              = ptProbeTransform->tTranslation,
            .fRangeSqr              = ptProbe->fRange * ptProbe->fRange,
            .uGGXEnvSampler         = ptProbeData->uGGXEnvSampler,
            .uLambertianEnvSampler  = ptProbeData->uLambertianEnvSampler,
            .uCharlieEnvSampler     = ptProbeData->uSheenEnvSampler,
            .tMin.xyz               = (plVec3){ptProbeTransform->tTranslation.x - ptProbe->fRange, ptProbeTransform->tTranslation.y - ptProbe->fRange, ptProbeTransform->tTranslation.z - ptProbe->fRange},
            .tMax.xyz               = (plVec3){ptProbeTransform->tTranslation.x + ptProbe->fRange, ptProbeTransform->tTranslation.y + ptProbe->fRange, ptProbeTransform->tTranslation.z + ptProbe->fRange},
            .iMips                  = ptProbeData->iMips,
            .iParallaxCorrection    = (int)(ptProbe->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX)
        };
        pl_sb_push(ptScene->sbtGPUProbeData, tProbeData);
    }

    if(pl_sb_size(ptScene->sbtGPUProbeData) > 0)
    {

        plBuffer* ptProbeDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->tGPUProbeDataBuffers);
        memcpy(ptProbeDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtGPUProbeData, sizeof(plGpuProbe) * pl_sb_size(ptScene->sbtGPUProbeData));
    }

    pl__renderer_update_object_probes(ptScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~common data~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plVec3 atPitchYawRoll[6] = {
        {    0.0f,  PL_PI_2,  0.0f },
        {    0.0f, -PL_PI_2,  0.0f },
        {  PL_PI_2,   PL_PI,  0.0f },
        { -PL_PI_2,   PL_PI,  0.0f },
        {    PL_PI,    0.0f, PL_PI },
        {     0.0f,    0.0f,  0.0f },
    };

    const plBindGroupUpdateSamplerData tSkyboxBG1SamplerData = {
        .tSampler = gptData->tSamplerLinearRepeat,
        .uSlot    = 1
    };


    for(uint32_t uProbeIndex = 1; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);

        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY) || (ptScene->tFlags & PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY)))
        {
            continue;
        }

        plTransformComponent* ptProbeTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptProbe->tEntity);

        ptProbe->tViewData.tViewportSize = (plVec4){.x = ptProbe->tTargetSize.x, .y = ptProbe->tTargetSize.y, .z = 1.0f, .w = 1.0f};
        ptProbe->tViewData.tCameraPos.xyz = ptProbeTransform->tTranslation;
        ptProbe->tViewData.fCameraRange  = ptProbeComp->fRange - 0.26f;
        ptProbe->tViewData.fCameraNearZ  = 0.26f;
        ptProbe->tViewData.fAspectRatio  = 1.0f;

        const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);

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

        for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
        {
            gptCamera->init(&atEnvironmentCamera[uFaceIndex]);
            atEnvironmentCamera[uFaceIndex].eProjectionType = PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            atEnvironmentCamera[uFaceIndex].eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
            atEnvironmentCamera[uFaceIndex].tPosition   = (plVec3d){(double)ptProbeTransform->tTranslation.x, (double)ptProbeTransform->tTranslation.y, (double)ptProbeTransform->tTranslation.z};
            atEnvironmentCamera[uFaceIndex].fNearZ       = 0.26f;
            atEnvironmentCamera[uFaceIndex].fFarZ        = ptProbeComp->fRange;
            atEnvironmentCamera[uFaceIndex].fYFov        = PL_PI_2;
            atEnvironmentCamera[uFaceIndex].fAspectRatio = 1.0f;
            gptCamera->set_euler(&atEnvironmentCamera[uFaceIndex], atPitchYawRoll[uFaceIndex].x, atPitchYawRoll[uFaceIndex].y, atPitchYawRoll[uFaceIndex].z);
            gptCamera->update(&atEnvironmentCamera[uFaceIndex]);

            ptProbe->tViewData.tCameraProjection[uFaceIndex]     = atEnvironmentCamera[uFaceIndex].tProjMat;
            ptProbe->tViewData.tCameraView[uFaceIndex]           = atEnvironmentCamera[uFaceIndex].tViewMat;
            ptProbe->tViewData.tInvViewMatNoTranslation[uFaceIndex] = atEnvironmentCamera[uFaceIndex].tInvViewMatNoTranslation;
            ptProbe->tViewData.tCameraProjectionInv[uFaceIndex]  = pl_mat4_invert(&atEnvironmentCamera[uFaceIndex].tProjMat);
            ptProbe->tViewData.tCameraViewInv[uFaceIndex]        = pl_mat4_invert(&atEnvironmentCamera[uFaceIndex].tViewMat);
            ptProbe->tViewData.tCameraViewProjection[uFaceIndex] = pl_mul_mat4(&atEnvironmentCamera[uFaceIndex].tProjMat, &atEnvironmentCamera[uFaceIndex].tViewMat);
        }

        // copy global buffer data for probe rendering
        plBuffer* ptProbeGlobalBuffer = gptGfx->get_buffer(ptDevice, ptProbe->tViewBuffer);
        memcpy(ptProbeGlobalBuffer->tMemoryAllocation.pHostMapped, &ptProbe->tViewData, sizeof(plGpuViewData));

        for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
        {


            plCommandBuffer* ptCSMCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "probe csm");
            gptGfx->begin_command_recording(ptCSMCommandBuffer);

            plRenderInfo tShadowRenderInfo = {
                .tRenderArea = {
                    .tMin = {0},
                    .tMax = {.x = (float)ptScene->uSunShadowAtlasResolution, .y = (float)ptScene->uSunShadowAtlasResolution}
                },
                .tDepthAttachment = {
                    .tTexture        = ptScene->tSunShadowTexture,
                    .eLoadOp         = PL_LOAD_OP_CLEAR,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                    .fClearZ         = 0.0f
                }
            };

            gptGfx->begin_render_pass(ptCSMCommandBuffer, &tShadowRenderInfo, NULL);
            gptGfx->push_debug_group(ptCSMCommandBuffer, "Probe CSM", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

            pl__renderer_generate_sun_shadow_map(ptCSMCommandBuffer, ptScene, &ptProbe->tViewData, ptProbe->tViewBuffer, ptProbe->tSunCameraBuffers, &atEnvironmentCamera[uFaceIndex], ptProbe->tSunCameraBG, true);

            plCSMInfo tCSMInfo = {
                .tBindGroup = ptProbe->tDShadowBG,
                .tDShadowCameraBuffer = ptProbe->tDShadowCameraBuffers,
                .tDLightShadowDataBuffer = ptProbe->tDLightShadowDataBuffer,
                .sbtDLightShadowData = ptProbe->sbtDLightShadowData
            };
            pl__renderer_generate_direction_view_map(ptCSMCommandBuffer, ptScene, &atEnvironmentCamera[uFaceIndex], tCSMInfo);
            gptGfx->pop_debug_group(ptCSMCommandBuffer);
            gptGfx->end_render_pass(ptCSMCommandBuffer);
            gptGfx->end_command_recording(ptCSMCommandBuffer);

            if(ptEnvironment->eMode & PL_RENDERER_SKY_MODE_REALISTIC)
            {
                plCommandBuffer* ptSunCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "sun transmission");
                gptGfx->begin_command_recording(ptSunCmdBuffer);

                plPassResources tPassResources0 = {
                    .atTextures = {
                        {
                            .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                            .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                            .eStages = PL_PIPELINE_STAGE_COMPUTE,
                            .tHandle = ptScene->atSunTransmissionLut[uFrameIdx]
                        },
                        {
                            .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                            .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                            .eStages = PL_PIPELINE_STAGE_COMPUTE,
                            .tHandle = ptScene->atSunMultiscatterLut[uFrameIdx]
                        },
                        {
                            .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                            .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                            .eStages = PL_PIPELINE_STAGE_COMPUTE,
                            .tHandle = ptScene->tSkyProbeLut
                        }
                    }
                };

                gptGfx->begin_compute_pass(ptSunCmdBuffer, &tPassResources0);
                gptGfx->push_debug_group(ptSunCmdBuffer, "Sky LUT", (plVec4){0.33f, 0.42f, 0.70f, 1.0f});

                plDispatch tDispatch3 = {
                    .uGroupCountX = (uint32_t)ceilf(ptSettings->tSky.tSkyLutResolution.x / 8.0f),
                    .uGroupCountY = (uint32_t)ceilf(ptSettings->tSky.tSkyLutResolution.y / 8.0f),
                    .uGroupCountZ = 1,
                    .uThreadPerGroupX = 8,
                    .uThreadPerGroupY = 8,
                    .uThreadPerGroupZ = 1
                };

                // vertical blur
                gptGfx->bind_compute_shader(ptSunCmdBuffer, ptScene->tSkyViewLutShader);
                plBindGroupHandle atSkyLutBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptProbe->tViewBG, ptScene->tSkyProbeLutBG2 };
                gptGfx->bind_compute_bind_groups(ptSunCmdBuffer, ptScene->tSkyViewLutShader, 0, PL_ARRAYSIZE(atSkyLutBGs), atSkyLutBGs, 0, NULL);

                gptGfx->dispatch(ptSunCmdBuffer, 1, &tDispatch3);

                gptGfx->pop_debug_group(ptSunCmdBuffer);
                gptGfx->end_compute_pass(ptSunCmdBuffer);

                gptGfx->end_command_recording(ptSunCmdBuffer);

                const plSubmitInfo tSunTransmissionSubmitInfo = {
                    .uWaitSemaphoreCount     = 1,
                    .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
                    .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                    .uSignalSemaphoreCount   = 1,
                    .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
                    .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
                };
                gptGfx->submit_command_buffer(ptSunCmdBuffer, &tSunTransmissionSubmitInfo);
                gptGfx->return_command_buffer(ptSunCmdBuffer);
            }

            const plSubmitInfo tSubmitCSMInfo2 = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCSMCommandBuffer, &tSubmitCSMInfo2);
            gptGfx->return_command_buffer(ptCSMCommandBuffer);

            plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "probe");
            gptGfx->begin_command_recording(ptCmdBuffer);

            uint32_t uFace = ptProbe->uCurrentFace + uFaceIndex;
            PL_RENDERER_STAT("renderer / reflection probe faces rendered", 1);
            uFace = uFace % 6;

            //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

            plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptProbe->uDataPackIndex];

            plRenderInfo tRenderInfo = {
                .tRenderArea = {
                    .tMin = {0},
                    .tMax = ptProbe->tTargetSize
                },
                .atColorAttachments = {
                    {
                        .tTexture       = ptProbe->atRawOutputTextureViews[uFace],
                        .eLoadOp        = PL_LOAD_OP_CLEAR,
                        .eStoreOp       = PL_STORE_OP_STORE,
                        .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                        .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
                    },
                    {
                        .tTexture       = ptPack->atAlbedoTextureViews[uFace],
                        .eLoadOp        = PL_LOAD_OP_CLEAR,
                        .eStoreOp       = PL_STORE_OP_STORE,
                        .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                        .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
                    },
                    {
                        .tTexture       = ptPack->atNormalTextureViews[uFace],
                        .eLoadOp        = PL_LOAD_OP_CLEAR,
                        .eStoreOp       = PL_STORE_OP_STORE,
                        .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                        .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
                    },
                    {
                        .tTexture       = ptPack->atAOMetalRoughnessTextureViews[uFace],
                        .eLoadOp        = PL_LOAD_OP_CLEAR,
                        .eStoreOp       = PL_STORE_OP_STORE,
                        .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                        .tClearColor    = {0.0f, 0.0f, 0.0f, 0.0f}
                    }
                },
                .tDepthAttachment = {
                    .tTexture        = ptPack->atDepthTextureViews[uFace],
                    .eLoadOp         = PL_LOAD_OP_CLEAR,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                    .fClearZ         = 0.0f
                },
                .tStencilAttachment = {
                    .tTexture        = ptPack->atDepthTextureViews[uFace],
                    .eLoadOp         = PL_LOAD_OP_CLEAR,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                    .uClearStencil   = 0
                }
            };

            // begin main renderpass (directly to swapchain)
            
            gptGfx->begin_render_pass(ptCmdBuffer, &tRenderInfo, NULL);
            gptGfx->set_depth_bias(ptCmdBuffer, 0.0f, 0.0f, 0.0f);

            plCullData tCullData = {
                .ptScene      = ptScene,
                .ptCullCamera = &atEnvironmentCamera[uFace]
            };
            pl__camera_build_perspective_frustum(&atEnvironmentCamera[uFace], &tCullData.tFrustum);
            
            plJobDesc tJobDesc = {
                .task  = pl__renderer_cull_job,
                .pData = &tCullData
            };

            plAtomicCounter* ptCullCounter = NULL;
            gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
            gptJob->wait_for_counter(ptCullCounter);

            for(uint32_t i = 0; i < PL_RENDER_BUCKET_COUNT; i++)
            {
                pl_sb_reset(ptScene->asbuRenderBuckets[i]);
            }

            for(uint32_t i = 0; i < uDrawableCount; i++)
            {
                uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
                plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
                if(!ptDrawable->bCulled)
                {
                    plDrawableResources*         ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
                    plObjectComponent*           ptObject            = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);
                    plMesh*                      ptMainMesh          = gptAsset->get_data(ptObject->tMesh);
                    plSubmesh*                   ptMesh              = &ptMainMesh->atSubmeshes[ptDrawableResources->uSubmeshIndex];
                    plMaterial*                  ptMaterial          = gptAsset->get_data(ptObject->atMaterialOverrides ? ptObject->atMaterialOverrides[ptDrawableResources->uSubmeshIndex - ptObject->uFirstSubmesh] : ptMesh->tMaterial);
                    

                    if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_PATH_FOLIAGE)
                    {
                        pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_FOLIAGE], uDrawableIndex);
                    }

                    // deferred objects
                    else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_PATH_DEFERRED)
                    {
                        if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                        {
                            pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_DEFERRED_DOUBLE_SIDED], uDrawableIndex);
                        }
                        else
                        {
                            pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_DEFERRED], uDrawableIndex);
                        }
                    }

                    // forward objects
                    else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_PATH_FORWARD)
                    {
                        if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                        {
                            if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                            {
                                pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_FORWARD_TRANSPARENT_DOUBLE_SIDED], uDrawableIndex);
                            }
                            else
                            {
                                pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_FORWARD_TRANSPARENT], uDrawableIndex);
                            }
                        }
                        else
                        {
                            if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                            {
                                pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_FORWARD_DOUBLE_SIDED], uDrawableIndex);
                            }
                            else
                            {
                                pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_FORWARD], uDrawableIndex);
                            }
                        }
                    }

                    // Probes have no screen-space transmission snapshot. Render
                    // water with its ordinary tinted, reflective fallback.
                    else if(ptMaterial->eMaterialModel == PL_MATERIAL_MODEL_WATER)
                    {
                        pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_FORWARD_TRANSPARENT_DOUBLE_SIDED], uDrawableIndex);
                    }

                    // transmission objects
                    else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_PATH_TRANSMISSION)
                    {
                        pl_sb_push(ptScene->asbuRenderBuckets[PL_RENDER_BUCKET_TRANSMISSION], uDrawableIndex);
                    }
                }
            }

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            plGbufferFillPassInfo tGbufferFillPassInfo1 = {
                .uGlobalIndex = uFace,
                .tBG2 = ptProbe->tGBufferBG,
                .ptArea = &tArea,
                .tBucket = PL_RENDER_BUCKET_DEFERRED,
                .tShader = ptScene->bWireframe ? ptScene->tGBufferFillWireframeShader : ptScene->tGBufferFillShader
            };
            pl__render_view_gbuffer_fill_pass(ptScene, ptCmdBuffer, &tGbufferFillPassInfo1);

            plGbufferFillPassInfo tGbufferFillPassInfo0 = {
                .uGlobalIndex = uFace,
                .tBG2 = ptProbe->tGBufferBG,
                .ptArea = &tArea,
                .tBucket = PL_RENDER_BUCKET_DEFERRED_DOUBLE_SIDED,
                .tShader = ptScene->bWireframe ? ptScene->tGBufferFillNoCullWireframeShader : ptScene->tGBufferFillNoCullShader
            };
            pl__render_view_gbuffer_fill_pass(ptScene, ptCmdBuffer, &tGbufferFillPassInfo0);

            if(ptScene->ptTerrain)
            {
                gptGfx->push_debug_group(ptCmdBuffer, "Probe Terrain", (plVec4){0.33f, 0.42f, 0.20f, 1.0f});

                plTerrainComponent* ptTerrainComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tTerrainComponentType, ptScene->tTerrain);

                gptGfx->set_depth_bias(ptCmdBuffer, 0.0f, 0.0f, 0.0f);
                gptGfx->bind_shader(ptCmdBuffer, ptScene->tTerrainShader);
                gptGfx->bind_vertex_buffer(ptCmdBuffer, ptScene->ptTerrain->tVertexBuffer);
                plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], ptProbe->tGBufferBG};
                gptGfx->bind_graphics_bind_groups(
                    ptCmdBuffer,
                    ptScene->tTerrainShader,
                    0, 2,
                    atBindGroups,
                    0, NULL
                );

                for(uint32_t i = 0; i < pl_sb_size(ptScene->ptTerrain->sbtChunkFiles); i++)
                {
                    pl__render_chunk(ptScene, ptScene->ptTerrain, &atEnvironmentCamera[uFace], ptCmdBuffer, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, uFace, ptProbe->tTargetSize.x);
                }

                gptGfx->pop_debug_group(ptCmdBuffer);
            }
            

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            const plPassResources tResourceUpdates = {
                .atTextures = {
                    {
                        .tHandle = ptPack->atAlbedoTextureViews[uFace],
                        .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                        .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
                    },
                    {
                        .tHandle = ptPack->atNormalTextureViews[uFace],
                        .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                        .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
                    },
                    {
                        .tHandle = ptPack->atAOMetalRoughnessTextureViews[uFace],
                        .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                        .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
                    },
                    {
                        .tHandle = ptPack->atDepthTextureViews[uFace],
                        .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                        .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
                    }
                }
            };

            gptGfx->intra_pass_barrier(ptCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_BARRIER_SCOPE_ALL, &tResourceUpdates);

            plDeferredLightingPassInfo tDeferredLightingPassInfo = {
                .uGlobalIndex = uFace,
                .uProbe = uProbeIndex,
                .tBG2 = ptPack->atLightingBindGroup[uFace],
                .ptArea = &tArea,
                .bProbe = true
            };
            pl__render_view_deferred_lighting_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tDeferredLightingPassInfo);

            gptGfx->producer_barrier(ptCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX, PL_BARRIER_SCOPE_ALL);
            gptGfx->end_render_pass(ptCmdBuffer);

            plRenderInfo tForwardRenderInfo = {
           .tRenderArea = {
                    .tMin = {0},
                    .tMax = ptProbe->tTargetSize
                },
                .atColorAttachments = {
                    {
                        .tTexture       = ptProbe->atRawOutputTextureViews[uFace],
                        .eLoadOp        = PL_LOAD_OP_LOAD,
                        .eStoreOp       = PL_STORE_OP_STORE,
                        .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                        .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
                    }
                },
                .tDepthAttachment = {
                    .tTexture        = ptPack->atDepthTextureViews[uFace],
                    .eLoadOp         = PL_LOAD_OP_LOAD,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                    .fClearZ         = 0.0f
                },
                .tStencilAttachment = {
                    .tTexture        = ptPack->atDepthTextureViews[uFace],
                    .eLoadOp         = PL_LOAD_OP_LOAD,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                    .uClearStencil   = 0
                }
            };

            gptGfx->consumer_barrier(ptCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL);
            gptGfx->begin_render_pass(ptCmdBuffer, &tForwardRenderInfo, NULL);
            gptGfx->set_depth_bias(ptCmdBuffer, 0.0f, 0.0f, 0.0f);
            
            if(ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY)
            {
                if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX && ptScene->tSkyboxTexture.uIndex != 0)
                {
                    plMat4 tTransformMat = pl_mat4_translate_vec3(ptProbeTransform->tTranslation);
                    pl__render_view_skybox_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tTransformMat, &tArea, uFace);  
                    // pl__render_view_skybox_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tTransformMat, &tArea, 0);  
                }
                else if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
                {
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(uint32_t));
                    uint32_t* ptDynamicData = (uint32_t*)tDynamicBinding.pcData;
                    *ptDynamicData = uFace;
                    gptGfx->bind_shader(ptCmdBuffer, ptScene->tSkyShader);
                    plBindGroupHandle atSkyBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptProbe->tViewBG, ptScene->tSkyProbeBG2 };
                    gptGfx->bind_graphics_bind_groups(ptCmdBuffer, ptScene->tSkyShader, 0, PL_ARRAYSIZE(atSkyBGs), atSkyBGs, 1, &tDynamicBinding);
                    plDraw tDraw = {
                        .uInstanceCount = 1,
                        .uVertexCount = 3
                    };
                    gptGfx->draw(ptCmdBuffer, 1, &tDraw);
                }
            }

            plForwardPassInfo tForwardPassInfo1 = {
                .tBucket = PL_RENDER_BUCKET_FORWARD,
                .tShader = ptScene->tForwardShader,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo1);

            plForwardPassInfo tForwardPassInfo0 = {
                .tBucket = PL_RENDER_BUCKET_FORWARD_DOUBLE_SIDED,
                .tShader = ptScene->tForwardNoCullShader,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo0);

            plForwardPassInfo tForwardPassInfo2 = {
                .tBucket = PL_RENDER_BUCKET_FORWARD_TRANSPARENT,
                .tShader = ptScene->tForwardTransparent,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo2);

            plForwardPassInfo tForwardPassInfo3 = {
                .tBucket = PL_RENDER_BUCKET_FORWARD_TRANSPARENT_DOUBLE_SIDED,
                .tShader = ptScene->tForwardNoCullTransparent,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo3);

            plForwardPassInfo tForwardPassInfo4 = {
                .tBucket = PL_RENDER_BUCKET_TRANSMISSION,
                .tShader = ptScene->tForwardNoCullShader,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo4);

            plForwardPassInfo tForwardPassInfo7 = {
                .tBucket = PL_RENDER_BUCKET_FOLIAGE,
                .tShader = ptScene->tForwardNoCullShader,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo7);

            gptGfx->end_render_pass(ptCmdBuffer);
            if((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY) && ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
                pl__cloud_probe_composite(ptScene, ptProbe, ptPack, uFace, ptCmdBuffer);

            gptGfx->end_command_recording(ptCmdBuffer);

            const plSubmitInfo tProbeSubmitInfo = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {tSemHandle},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCmdBuffer, &tProbeSubmitInfo);
            gptGfx->wait_on_command_buffer(ptCmdBuffer);
            gptGfx->return_command_buffer(ptCmdBuffer);
        }
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submission~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


        pl__renderer_probe_create_environment_map(ptScene, ptProbe, ptProbeComp->uSamples);

        ptProbe->uCurrentFace = (ptProbe->uCurrentFace + ptProbeComp->uInterval) % 6;

        if(ptProbe->uDirtyFaces == 0)
            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
        else
        {
            uint32_t uMaxDirty = pl_min(ptProbe->uDirtyFaces, ptProbeComp->uInterval);
            ptProbe->uDirtyFaces -= uMaxDirty;
            if(ptProbe->uDirtyFaces == 0)
                ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
        }
    }

    ptScene->tFlags &= ~PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY;
}

static void
pl__renderer_scene_create_bindgroups(plScene* ptScene)
{
    plBindGroupLayoutHandle tGlobalSceneBindGroupLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene");
    plBindGroupLayoutHandle tSunTransmissionBindGroupLayout1 = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "simple_texture_lut");
    plBindGroupLayoutHandle tSunMultiscatterBindGroupLayout1 = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_multiscatter_lut");

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // create global bindgroup
        const plBindGroupDesc tGlobalBindGroupDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = tGlobalSceneBindGroupLayout,
            .pcDebugName = "global bind group"
        };
        ptScene->atSceneBindGroups[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBindGroupDesc);

        const plBindGroupDesc tTransmissionBG1Desc = {
            .tLayout     = tSunTransmissionBindGroupLayout1,
            .pcDebugName = "tSunTransmissionBG1",
            .ptPool      = gptData->ptBindGroupPool
        };
        ptScene->atSunTransmissionBG1[i] = gptGfx->create_bind_group(gptData->ptDevice, &tTransmissionBG1Desc);
        const plBindGroupDesc tMultiscatterBG1Desc = {
            .tLayout     = tSunMultiscatterBindGroupLayout1,
            .pcDebugName = "tSunMultiscatterBG1",
            .ptPool      = gptData->ptBindGroupPool
        };
        ptScene->atSunMultiscatterBG1[i] = gptGfx->create_bind_group(gptData->ptDevice, &tMultiscatterBG1Desc);

        const plBindGroupDesc tSkinBindGroup2Desc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "skinning_1"),
            .pcDebugName = "skin bind group 2"
        };
        ptScene->atSkinBindGroup1[i] = gptGfx->create_bind_group(gptData->ptDevice, &tSkinBindGroup2Desc);

        const plBindGroupDesc tGlobalBGDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = ptScene->tShadowGlobalBGLayout,
            .pcDebugName = "temporary global bind group 0"
        };
        ptScene->atShadowBG[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);
    }

    const plBindGroupDesc tSkyLutBG2Desc = {
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_lut_2"),
        .pcDebugName = "atSkyLutBG2",
        .ptPool      = gptData->ptBindGroupPool
    };
    ptScene->tSkyProbeLutBG2 = gptGfx->create_bind_group(gptData->ptDevice, &tSkyLutBG2Desc);

    const plBindGroupDesc tSkyBG2Desc = {
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_2"),
        .pcDebugName = "tSkyBG1",
        .ptPool      = gptData->ptBindGroupPool
    };
    ptScene->tSkyProbeBG2 = gptGfx->create_bind_group(gptData->ptDevice, &tSkyBG2Desc);

    const plBindGroupDesc tSkinBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "skinning_0"),
        .pcDebugName = "skin bind group"
    };
    ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(gptData->ptDevice, &tSkinBindGroupDesc);
}

static void
pl__renderer_scene_update_sky_luts_bindgroups(plScene* ptScene)
{
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateData tBGMultiscatterData1 = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptScene->atSunMultiscatterLut[i]},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atSunMultiscatterBG1[i], &tBGMultiscatterData1);

        const plBindGroupUpdateData tBGSunTransmissionLut = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptScene->atSunTransmissionLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atSunTransmissionBG1[i], &tBGSunTransmissionLut);
    }

    const plBindGroupUpdateData tBGData3 = {
        .atTextureBindings = {
            {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->tSkyProbeLut},
            {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[0]}
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkyProbeBG2, &tBGData3);

    const plBindGroupUpdateData tBGSkyLut = {
        .atTextureBindings = {
            {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptScene->tSkyProbeLut},
            {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[0]},
            {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunMultiscatterLut[0]}
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkyProbeLutBG2, &tBGSkyLut);
}

static void
pl__renderer_scene_update_bindgroups(plScene* ptScene)
{

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateData tSkinBGData = {
            .atBufferBindings  = {
                { .uSlot = 0, .tBuffer = ptScene->atDynamicSkinBuffer[i], .szBufferRange = ptScene->tInit.szSkinBufferSize }
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atSkinBindGroup1[i], &tSkinBGData);

        // partially update global bindgroup
        const plBindGroupUpdateData tGlobalBindGroupData = {
            .atBufferBindings = {
                { .uSlot = 0, .tBuffer = ptScene->atSceneBuffer[i], .szBufferRange = sizeof(plGpuSceneData) },
                { .uSlot = 1, .tBuffer = ptScene->tStorageBuffer, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tStorageBuffer)->tDesc.szByteSize },
                { .uSlot = 2, .tBuffer = ptScene->atTransformBuffer[i], .szBufferRange = sizeof(plGpuTransform) * ptScene->uTransformCapacity },
                { .uSlot = 3, .tBuffer = ptScene->tMaterialDataBuffer, .szBufferRange = ptScene->tInit.szMaterialBufferSize },
                { .uSlot = PL_PROBE_SELECTION_TABLE_SLOT, .tBuffer = ptScene->atProbeSelectionBuffers[i], .szBufferRange = sizeof(plGpuProbeSelection) * (PL_MAX_PROBE_SELECTION_INDEX + 1) },
            },
            .atSamplerBindings = {
                { .uSlot = 4, .tSampler = gptData->tSamplerLinearClamp },
                { .uSlot = 5, .tSampler = gptData->tSamplerNearestClamp },
                { .uSlot = 6, .tSampler = gptData->tSamplerLinearRepeat },
                { .uSlot = 7, .tSampler = gptData->tSamplerNearestRepeat }
            },
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atSceneBindGroups[i], &tGlobalBindGroupData);

        const plBindGroupUpdateData tShadowBGData = {
            .atBufferBindings = {
                {
                    .tBuffer       = ptScene->atShadowCameraBuffers[i],
                    .uSlot         = 0,
                    .szBufferRange = 4096
                },
                {
                    .tBuffer       = ptScene->tInstanceBuffer,
                    .uSlot         = 1,
                    .szBufferRange = sizeof(plShadowInstanceBufferData) * ptScene->uShadowInstanceCapacity
                }   
            }
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atShadowBG[i], &tShadowBGData);
    }

    const plBindGroupUpdateData tBGData0 = {
        .atBufferBindings = {
            { .uSlot = 0, .tBuffer = ptScene->tStorageBuffer, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tStorageBuffer)->tDesc.szByteSize},
            { .uSlot = 1, .tBuffer = ptScene->tVertexBuffer,  .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tVertexBuffer)->tDesc.szByteSize}
        },
        .atSamplerBindings = {
             { .uSlot = 2, .tSampler = gptData->tSamplerLinearRepeat}
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkinBindGroup0, &tBGData0);

    pl__renderer_scene_update_sky_luts_bindgroups(ptScene);
}

static void
pl__renderer_scene_create_sky_luts_textures(plScene* ptScene)
{
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
    plDevice* ptDevice = gptData->ptDevice;
    if(gptGfx->is_texture_valid(ptDevice, ptScene->atSunTransmissionLut[0]))
    {
        gptGfx->queue_texture_for_deletion(ptDevice, ptScene->tSkyProbeLut);
        for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        {
            gptGfx->queue_texture_for_deletion(ptDevice, ptScene->atSunTransmissionLut[i]);
            gptGfx->queue_texture_for_deletion(ptDevice, ptScene->atSunMultiscatterLut[i]);
        }
    }

    const plTextureDesc tSunTransmissionLutDesc = {
        .tDimensions = { .xy = ptSettings->tSky.tTransmissionLutResolution, 1},
        .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tSunTransmissionLut"
    };

    const plTextureDesc tSunMultiscatterLutDesc = {
        .tDimensions = { .xy = ptSettings->tSky.tMultiscatterLutResolution, 1},
        .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tSunMultiscatterLut"
    };

    const plTextureDesc tSkyLutDesc = {
        .tDimensions = { .xy = ptSettings->tSky.tSkyLutResolution, 1},
        .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tSkyLut"
    };
    
    gptStarter->create_texture(&tSkyLutDesc, NULL, 0, &ptScene->tSkyProbeLut);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->create_texture(&tSunTransmissionLutDesc, NULL, 0, &ptScene->atSunTransmissionLut[i]);
        gptStarter->create_texture(&tSunMultiscatterLutDesc, NULL, 0, &ptScene->atSunMultiscatterLut[i]);
    }
}

static void
pl__renderer_scene_create_bindgroup_layouts(plScene* ptScene)
{
    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "cube_filter_set_0",
            .atTextureBindings = {
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
            .atSamplerBindings= {
                {.uSlot = 0, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_0", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "cube_filter_set_1",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 3, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 4, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 5, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_1", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "panorama_to_cubemap",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 3, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 4, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 5, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 6, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "panorama_to_cubemap", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "shadow",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "shadow", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "view",
            .atTextureBindings = {
                {.uSlot = 9, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_FRAGMENT}
            },
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_UNIFORM, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_UNIFORM, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_BUFFER_BINDING_TYPE_UNIFORM, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 3, .eType = PL_BUFFER_BINDING_TYPE_UNIFORM, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 4, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 5, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 6, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 7, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 8, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_COMPUTE},
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "view", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "scene",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 2, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 3, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = PL_PROBE_SELECTION_TABLE_SLOT, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
            },
            .atSamplerBindings= {
                {.uSlot = 4, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 5, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 6, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 7, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX}
            },
            .atTextureBindings = {
                {.uSlot = PL_MAX_BINDLESS_TEXTURE_SLOT, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE},
                {.uSlot = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "scene", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "sky_2",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "sky_2", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "skinning_0",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
            },
            .atSamplerBindings= {
                {.uSlot = 2, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "skinning_0", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "skinning_1",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "skinning_1", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "sky_lut_2",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "sky_lut_2", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "deferred_lighting_2",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT, .eStages = PL_SHADER_STAGE_FRAGMENT},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT, .eStages = PL_SHADER_STAGE_FRAGMENT},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT, .eStages = PL_SHADER_STAGE_FRAGMENT},
                {.uSlot = 3, .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT, .eStages = PL_SHADER_STAGE_FRAGMENT}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName       = "ssgi",
            .atSamplerBindings = {{.uSlot = 6, .eStages = PL_SHADER_STAGE_COMPUTE}}
        };
        for(uint32_t i = 0; i < 6; i++)
        {
            tDesc.atTextureBindings[i] = (plTextureBinding){.uSlot = i, .eType = i == 0 ? PL_TEXTURE_BINDING_TYPE_STORAGE : PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE};
            }
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "ssgi", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "bloom downsample",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
            .atSamplerBindings= {
                {.uSlot = 2, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample", tHandle);
    }

    {
        const plBindGroupLayoutDesc ocean = {
            .pcDebugName = "ocean FFT",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
            .atTextureBindings = {
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle oceanLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &ocean);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "ocean_fft", oceanLayout);
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "ambient occlusion",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 3, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
            .atSamplerBindings = {{.uSlot = 5, .eStages = PL_SHADER_STAGE_COMPUTE}}
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "ao", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "bloom upsample",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
            .atSamplerBindings= {
                {.uSlot = 3, .eStages = PL_SHADER_STAGE_COMPUTE}
            },
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "bloom_upsample", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "tonemap",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "tonemap", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "jfa 0",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "jfa_0", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "jfa 1",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_FRAGMENT},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_FRAGMENT}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "jfa_1", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "aerial_luts",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "cloud_lowres",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "cloud_lowres", tHandle);
        tDesc.pcDebugName = "cloud_temporal";
        tDesc.atTextureBindings[2].eType = PL_TEXTURE_BINDING_TYPE_SAMPLED;
        tDesc.atTextureBindings[3] = tDesc.atTextureBindings[1];
        tDesc.atTextureBindings[3].uSlot = 3;
        tDesc.atTextureBindings[4] = tDesc.atTextureBindings[1];
        tDesc.atTextureBindings[4].uSlot = 4;
        tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "cloud_temporal", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "gbuffer_fill_1",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX}
            },
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "skybox_2",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX}
            },
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "skybox_2", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "sky_multiscatter_lut",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "sky_multiscatter_lut", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "simple_buffer_lut",
            .atBufferBindings = {
                {.uSlot = 0, .eType = PL_BUFFER_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE }
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "simple_buffer_lut", tHandle);
    }

    {
        plBindGroupLayoutDesc tDesc = {
            .pcDebugName = "simple_texture_lut",
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .eStages = PL_SHADER_STAGE_COMPUTE}
            }
        };
        plBindGroupLayoutHandle tHandle = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDesc);
        gptShaderLibrary->register_bind_group_layout(ptScene->ptShaderLibrary, "simple_texture_lut", tHandle);
    }
}

static void
pl__renderer_scene_create_compute_shaders(plScene* ptScene)
{
    plDevice* ptDevice = gptData->ptDevice;

    plShaderOptions tOriginalOptions = *gptShader->get_options();

    plShaderOptions tNewDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->set_options(&tNewDefaultShaderOptions);

    {
        plComputeShaderDesc desc = {
            .pcDebugName        = "ocean_fft",
            .tShader            = gptShader->load_glsl("pl_ocean_fft.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "ocean_fft"))->tDesc}
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "ocean_fft", &desc);
    }
    {
        plComputeShaderDesc tClusterDesc = {
            .pcDebugName = "cluster_lights",
            .tShader = gptShader->load_glsl("pl_cluster_lights.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cluster_lights", &tClusterDesc);
    }

    {
        int aiPlaceHolder[3] = {0};
        plComputeShaderDesc tDesc = {
            .pcDebugName = "panorama_to_cubemap",
            .tShader = gptShader->load_glsl("pl_panorama_to_cubemap.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "panorama_to_cubemap"))->tDesc,
            },
            .atConstants = {
                { .eType = PL_DATA_TYPE_INT },
                { .eType = PL_DATA_TYPE_INT },
                { .eType = PL_DATA_TYPE_INT }
            },
            .pTempConstantData = aiPlaceHolder
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "panorama_to_cubemap", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "jumpfloodalgo",
            .tShader = gptShader->load_glsl("pl_jumpfloodalgo.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "jfa_0"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "jumpfloodalgo", &tDesc);
    }

    {
        const char* apcNames[] = {"ssgi_trace", "ssgi_apply"};
        const char* apcFiles[] = {"pl_ssgi_trace.comp", "pl_ssgi_apply.comp"};
        for(uint32_t i = 0; i < 2; i++)
        {
            plComputeShaderDesc tDesc = {
                .pcDebugName = apcNames[i],
                .tShader = gptShader->load_glsl(apcFiles[i], "main", NULL, NULL),
                .atBindGroupLayouts = {
                    gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "ssgi"))->tDesc
                }
            };
            gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, apcNames[i], &tDesc);
        }
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "bloom_apply",
            .tShader = gptShader->load_glsl("pl_bloom_apply.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "bloom_apply", &tDesc);
    }

    {
        const char* apcNames[] = {"ao_depth", "ao_evaluate", "ao_apply"};
        const char* apcFiles[] = {"pl_ao_depth.comp", "pl_ao_evaluate.comp", "pl_ao_apply.comp"};
        for(uint32_t i = 0; i < 3; i++)
        {
            plComputeShaderDesc tDesc = {
                .pcDebugName = apcNames[i],
                .tShader = gptShader->load_glsl(apcFiles[i], "main", NULL, NULL),
                .atBindGroupLayouts = {
                    gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "ao"))->tDesc
                }
            };
            gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, apcNames[i], &tDesc);
        }
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "bloom_downsample",
            .tShader = gptShader->load_glsl("pl_bloom_downsample.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "bloom_downsample", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "bloom_upsample",
            .tShader = gptShader->load_glsl("pl_bloom_upsample.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_upsample"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "bloom_upsample", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "brdf_lut",
            .tShader = gptShader->load_glsl("pl_brdf_lut.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "simple_buffer_lut"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "brdf_lut", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cloud_shadow",
            .tShader = gptShader->load_glsl("pl_cloud_shadow.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "simple_texture_lut"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cloud_shadow", &tDesc);
    }
    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "sky_transmission_lut",
            .tShader = gptShader->load_glsl("pl_sky_transmission_lut.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "simple_texture_lut"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "sky_transmission_lut", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "sky_multiscatter_lut",
            .tShader = gptShader->load_glsl("pl_sky_multiscatter_lut.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_multiscatter_lut"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "sky_multiscatter_lut", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "sky_lut",
            .tShader = gptShader->load_glsl("pl_sky_lut.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_lut_2"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "sky_lut", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "sky_aerial_lut",
            .tShader = gptShader->load_glsl("pl_sky_aerial_lut.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "sky_aerial_lut", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cloud_lowres",
            .tShader = gptShader->load_glsl("pl_cloud_lowres.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cloud_lowres"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cloud_lowres", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cloud_temporal",
            .tShader = gptShader->load_glsl("pl_cloud_temporal.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cloud_temporal"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cloud_temporal", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cloud_composite",
            .tShader = gptShader->load_glsl("pl_cloud_composite.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cloud_composite", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "clouds",
            .tShader = gptShader->load_glsl("pl_clouds.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "clouds", &tDesc);
    }
    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "sky_aerial",
            .tShader = gptShader->load_glsl("pl_sky_aerial.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"))->tDesc,
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "sky_aerial", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cube_filter_specular",
            .tShader = gptShader->load_glsl("pl_cube_filter_specular.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_0"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_1"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cube_filter_specular", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cube_filter_diffuse",
            .tShader = gptShader->load_glsl("pl_cube_filter_diffuse.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_0"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_1"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cube_filter_diffuse", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "cube_filter_sheen",
            .tShader = gptShader->load_glsl("pl_cube_filter_sheen.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_0"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "cube_filter_set_1"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "cube_filter_sheen", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "skinning",
            .tShader = gptShader->load_glsl("pl_skinning.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "skinning_0"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "skinning_1"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "skinning", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "tonemap",
            .tShader = gptShader->load_glsl("pl_tonemap.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "tonemap"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "tonemap", &tDesc);
    }

    {
        plComputeShaderDesc tDesc = {
            .pcDebugName = "fxaa",
            .tShader = gptShader->load_glsl("pl_fxaa.comp", "main", NULL, NULL),
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "bloom_downsample"))->tDesc
            }
        };
        gptShaderLibrary->register_compute_shader(ptScene->ptShaderLibrary, "fxaa", &tDesc);
    }

    gptShader->set_options(&tOriginalOptions);
}

static void
pl__renderer_scene_create_shaders(plScene* ptScene)
{
    plDevice* ptDevice = gptData->ptDevice;

    plShaderOptions tOriginalOptions = *gptShader->get_options();

    plShaderOptions tNewDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->set_options(&tNewDefaultShaderOptions);

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "skybox",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_skybox.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "skybox_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "skybox", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "jumpfloodalgo2",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_jumpfloodalgo.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "jfa_1"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tPostProcessRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "jumpfloodalgo2", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "sky",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_sky.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "sky", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "gbuffer_fill",
            .tVertexShader   = gptShader->load_glsl("pl_gbuffer_fill.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_gbuffer_fill.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_NONE},
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "gbuffer_fill", &tDesc);
        int iFoliage = 0;
        tDesc.pcDebugName = "ao_forward";
        tDesc.tVertexShader = gptShader->load_glsl("pl_forward.vert", "main", NULL, NULL);
        tDesc.tFragmentShader = gptShader->load_glsl("pl_ao_forward.frag", "main", NULL, NULL);
        tDesc.atBindGroupLayouts[1] = gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc;
        tDesc.tGraphicsState.bDepthWriteEnabled = true;
        tDesc.tGraphicsState.eDepthMode = PL_COMPARE_MODE_GREATER_OR_EQUAL;
        tDesc.atFragmentConstants[0].eType = PL_DATA_TYPE_INT;
        tDesc.pFragmentTempConstantData = &iFoliage;
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "ao_forward", &tDesc);
    }

    {
        int iDataHolder = 0;
        plShaderDesc tDesc = {
            .pcDebugName     = "gbuffer_fill_debug",
            .tVertexShader   = gptShader->load_glsl("pl_gbuffer_fill.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_gbuffer_fill_debug.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atFragmentConstants = {
                { .eType = PL_DATA_TYPE_INT }
            },
            .atBlendStates = {
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_NONE},
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                {.bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"))->tDesc
            },
            .pFragmentTempConstantData = &iDataHolder,
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "gbuffer_fill_debug", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "foliage",
            .tVertexShader   = gptShader->load_glsl("pl_forward.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_forward_foliage.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL, // AO prepass may already own this depth.
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
                }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "foliage", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "forward",
            .tVertexShader   = gptShader->load_glsl("pl_forward.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_forward.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eColorOp        = PL_BLEND_OP_ADD,
                    // Source-over alpha: keep an opaque scene opaque beneath transparency.
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eAlphaOp        = PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
                }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "forward", &tDesc);
    }

    {
        int iPlaceHolder = 0;
        plShaderDesc tDesc = {
            .pcDebugName     = "forward_debug",
            .tVertexShader   = gptShader->load_glsl("pl_forward.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_forward_debug.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atFragmentConstants = {
                { .eType = PL_DATA_TYPE_INT }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eColorOp        = PL_BLEND_OP_ADD,
                    // Source-over alpha: keep an opaque scene opaque beneath transparency.
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eAlphaOp        = PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
                }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc
            },
            .pFragmentTempConstantData = &iPlaceHolder,
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "forward_debug", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "grid",
            .tVertexShader   = gptShader->load_glsl("pl_grid.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_grid.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_GREATER,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eColorOp        = PL_BLEND_OP_ADD,
                    // Source-over alpha: keep an opaque scene opaque beneath transparency.
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eAlphaOp        = PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
                }
            },
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "grid", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "transmission",
            .tVertexShader   = gptShader->load_glsl("pl_forward.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_forward.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tTransparentRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "transmission", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName   = "uvmap",
            .tVertexShader = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_uvmap.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = true,
                .eStencilMode        = PL_COMPARE_MODE_LESS,
                .uStencilRef         = 128,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL }
            },
            .tRenderAttachmentInfo = gptData->tUVRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "uvmap", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "picking",
            .tVertexShader   = gptShader->load_glsl("pl_picking.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_picking.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_CULL_BACK,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tPickRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "picking", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName   = "shadow",
            .tVertexShader = gptShader->load_glsl("pl_shadow.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = true,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "shadow"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDepthRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "shadow", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "alphashadow",
            .tVertexShader   = gptShader->load_glsl("pl_shadow.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_shadow.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = true,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eColorOp =        PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .eAlphaOp =        PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
                }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "shadow"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDepthRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "alphashadow", &tDesc);
    }

    {
        plShaderDesc tDesc = {
            .pcDebugName     = "deferred_lighting",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_probe.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_ONE,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .eColorOp        = PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eAlphaOp        = PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_R | PL_COLOR_WRITE_MASK_G | PL_COLOR_WRITE_MASK_B
                },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting", &tDesc);
    }

   {
        plShaderDesc tDesc = {
            .pcDebugName     = "deferred_lighting_sun",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_sun.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {
                    .bBlendEnabled = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_ONE,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .eColorOp =        PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eAlphaOp =        PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_R | PL_COLOR_WRITE_MASK_G | PL_COLOR_WRITE_MASK_B
                },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting_sun", &tDesc);
    }

   {
        plShaderDesc tDesc = {
            .pcDebugName     = "deferred_lighting_directional",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_directional.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atBlendStates = {
                {
                    .bBlendEnabled = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_ONE,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .eColorOp =        PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eAlphaOp =        PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_R | PL_COLOR_WRITE_MASK_G | PL_COLOR_WRITE_MASK_B
                },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting_directional", &tDesc);
        tDesc.pcDebugName = "deferred_lighting_clustered";
        tDesc.tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_clustered.frag", "main", NULL, NULL);
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting_clustered", &tDesc);
    }

   {
        plShaderDesc tDesc = {
            .pcDebugName     = "deferred_lighting_spot",
            .tVertexShader   = gptShader->load_glsl("pl_deferred_lighting_spot.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_spot.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_LESS,
                .eCullMode           = PL_CULL_MODE_CULL_FRONT,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_ONE,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .eColorOp        = PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eAlphaOp        = PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_R | PL_COLOR_WRITE_MASK_G | PL_COLOR_WRITE_MASK_B
                }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting_spot", &tDesc);
    }

   {
        plShaderDesc tDesc = {
            .pcDebugName     = "deferred_lighting_point",
            .tVertexShader   = gptShader->load_glsl("pl_deferred_lighting_point.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_point.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_LESS,
                .eCullMode           = PL_CULL_MODE_CULL_FRONT,
                .bWireframe          = false,
                .bDepthClampEnabled  = true,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .eColorOp =        PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eAlphaOp =        PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_R | PL_COLOR_WRITE_MASK_G | PL_COLOR_WRITE_MASK_B
                },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting_point", &tDesc);
    }

   {
        int aiPlaceHolder[2] = {0};
        plShaderDesc tDesc = {
            .pcDebugName     = "deferred_lighting_debug",
            .tVertexShader   = gptShader->load_glsl("pl_full_screen.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_deferred_lighting_debug.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = false,
                .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atFragmentConstants = {
                { .eType = PL_DATA_TYPE_INT },
                { .eType = PL_DATA_TYPE_INT }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .eColorOp        = PL_BLEND_OP_ADD,
                    .eSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .eDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .eAlphaOp        = PL_BLEND_OP_ADD,
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
                },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 },
                { .bBlendEnabled = false, .uColorWriteMask = 0 }
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"))->tDesc
            },
            .pFragmentTempConstantData = aiPlaceHolder,
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "deferred_lighting_debug", &tDesc);
    }

   {
        plShaderDesc tDesc = {
            .pcDebugName     = "terrain",
            .tVertexShader   = gptShader->load_glsl("pl_terrain.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_terrain.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER,
                .eCullMode           = PL_CULL_MODE_CULL_BACK,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 },
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT2 },
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT2 }
                    }
                }
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_NONE },
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "terrain", &tDesc);
    }

   {
        int iDataHolder = 0;
        plShaderDesc tDesc = {
            .pcDebugName     = "terrain_debug",
            .tVertexShader   = gptShader->load_glsl("pl_terrain.vert", "main", NULL, NULL),
            .tFragmentShader = gptShader->load_glsl("pl_terrain_debug.frag", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = false,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 },
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT2 },
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT2 }
                    }
                }
            },
            .atFragmentConstants = {
                { .eType = PL_DATA_TYPE_INT }
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_NONE },
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL},
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .pFragmentTempConstantData = &iDataHolder,
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDeferredLightingRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "terrain_debug", &tDesc);
    }

   {
        plShaderDesc tDesc = {
            .pcDebugName   = "terrain_shadow",
            .tVertexShader = gptShader->load_glsl("pl_terrain_shadow.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .bDepthWriteEnabled  = true,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = false,
                .bDepthClampEnabled  = true,
                .bStencilTestEnabled = false,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .atAttributes = {
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT3 },
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT2 },
                        {.eFormat = PL_VERTEX_FORMAT_FLOAT2 }
                    }
                }
            },
            .atBlendStates = {
                { .bBlendEnabled = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}
            },
            .atBindGroupLayouts = {
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "scene"))->tDesc,
                gptGfx->get_bind_group_layout(ptDevice, gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "shadow"))->tDesc
            },
            .tRenderAttachmentInfo = gptData->tDepthRenderPassLayout
        };
        gptShaderLibrary->register_shader(ptScene->ptShaderLibrary, "terrain_shadow", &tDesc);
    }

    gptShader->set_options(&tOriginalOptions);
}

static void
pl__renderer_scene_create_textures(plScene* ptScene)
{
    ptScene->tSceneData.tCloudShadowCenter.w = -1.0f;

    // shadow atlas
    const plTextureDesc tShadowDepthTextureDesc = {
        .tDimensions   = {(float)ptScene->uShadowAtlasResolution, (float)ptScene->uShadowAtlasResolution, 1},
        .eFormat       = PL_FORMAT_D16_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "shadow map"
    };
    gptStarter->create_texture(&tShadowDepthTextureDesc, NULL, 0, &ptScene->tShadowTexture);
    
    // sun shadow atlas
    const plTextureDesc tSunShadowDepthTextureDesc = {
        .tDimensions   = {(float)ptScene->uSunShadowAtlasResolution, (float)ptScene->uSunShadowAtlasResolution, 1},
        .eFormat       = PL_FORMAT_D16_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "shadow map"
    };
    gptStarter->create_texture(&tSunShadowDepthTextureDesc, NULL, 0, &ptScene->tSunShadowTexture);
    
    // brdf lut
    const plTextureDesc tLutTextureDesc = {
        .tDimensions = {(float)1024, (float)1024, 1},
        .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tBrdfLutTexture"
    };
    gptStarter->create_texture(&tLutTextureDesc, NULL, 0, &ptScene->tBrdfLutTexture);

    pl__renderer_scene_create_sky_luts_textures(ptScene);
}

static void
pl__renderer_scene_create_buffers(plScene* ptScene)
{
    const plBufferDesc tIndexBufferDesc = {
        .eUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize  = ptScene->tInit.szIndexBufferSize,
        .pcDebugName = "index buffer"
    };

    const plBufferDesc tVertexBufferDesc = {
        .eUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize  = ptScene->tInit.szVertexBufferSize,
        .pcDebugName = "vertex buffer"
    };

    const plBufferDesc tStorageBufferDesc = {
        .eUsage      = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize  = ptScene->tInit.szDataBufferSize,
        .pcDebugName = "storage buffer"
    };

    const plBufferDesc tMaterialDataBufferDesc = {
        .eUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize = ptScene->tInit.szMaterialBufferSize,
        .pcDebugName = "material buffer"
    };

    gptStarter->create_buffer(&tIndexBufferDesc, NULL, &ptScene->tIndexBuffer);
    gptStarter->create_buffer(&tVertexBufferDesc, NULL, &ptScene->tVertexBuffer);
    gptStarter->create_buffer(&tStorageBufferDesc, NULL, &ptScene->tStorageBuffer);
    gptStarter->create_buffer(&tMaterialDataBufferDesc, NULL, &ptScene->tMaterialDataBuffer);

    gptStarter->get_staging_buffer(4096, &ptScene->tGPUProbeDataBuffers, "probe buffer");
    gptStarter->get_staging_buffer(sizeof(plShadowInstanceBufferData) * ptScene->uShadowInstanceCapacity, &ptScene->tInstanceBuffer, "instance");
    
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->get_staging_buffer(sizeof(plGpuSceneData), &ptScene->atSceneBuffer[i], "scene buffer");
        gptStarter->get_staging_buffer(PL_MAX_LIGHTS * sizeof(plGpuPointLightShadow), &ptScene->atPointLightShadowDataBuffer[i], "point shadow buffer");
        gptStarter->get_staging_buffer(PL_MAX_LIGHTS * sizeof(plGpuSpotLightShadow), &ptScene->atSpotLightShadowDataBuffer[i], "spot shadow buffer");
        gptStarter->get_staging_buffer(4096, &ptScene->atShadowCameraBuffers[i], "shadow camera buffer");
        gptStarter->get_staging_buffer(ptScene->tInit.szSkinBufferSize, &ptScene->atDynamicSkinBuffer[i], "joint buffer");

        gptStarter->get_staging_buffer(sizeof(plGpuPointLight) * PL_MAX_LIGHTS, &ptScene->atPointLightBuffer[i], "point light");
        gptStarter->get_staging_buffer(sizeof(plGpuSpotLight) * PL_MAX_LIGHTS, &ptScene->atSpotLightBuffer[i], "spot light");
        gptStarter->get_staging_buffer(sizeof(plGpuDirectionLight) * PL_MAX_LIGHTS, &ptScene->atDirectionLightBuffer[i], "direction light");
        gptStarter->get_staging_buffer(sizeof(plGpuProbeSelection) * (PL_MAX_PROBE_SELECTION_INDEX + 1), &ptScene->atProbeSelectionBuffers[i], "probe selections");
        gptStarter->get_staging_buffer(sizeof(plGpuTransform) * ptScene->uTransformCapacity, &ptScene->atTransformBuffer[i], "transform");
        
    }

    // pre-create working buffers for environment filtering, later we should defer this
    for(uint32_t i = 0; i < 7; i++)
    {
        const size_t uMaxFaceSize = ((size_t)1024 * (size_t)1024) * 4 * sizeof(float);

        const plBufferDesc tInputBufferDesc = {
            .eUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER,
            .szByteSize = uMaxFaceSize,
            .pcDebugName = "filter buffers"
        };
        gptStarter->create_buffer(&tInputBufferDesc, NULL, &ptScene->atFilterWorkingBuffers[i]);
    }
}

static void
pl__renderer_scene_create_brdf_lut(plScene* ptScene)
{
    const plBindGroupDesc tBrdfBGSet1Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "simple_buffer_lut"),
        .pcDebugName = "brdf_lut_set_1"
    };
    plBindGroupHandle tBrdfBGSet1 = gptGfx->create_bind_group(gptData->ptDevice, &tBrdfBGSet1Desc);
    
    const size_t uFaceSize = ((size_t)1024 * (size_t)1024) * 4 * sizeof(float);

    const plBindGroupUpdateData tBrdfBGSet1Data = {
        .atBufferBindings = {
            { .uSlot = 0, .tBuffer = ptScene->atFilterWorkingBuffers[0], .szBufferRange = uFaceSize}
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, tBrdfBGSet1, &tBrdfBGSet1Data);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tBrdfBGSet1);

    plComputeShaderHandle tBrdfLutShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "brdf_lut", NULL);
    
    const plDispatch tDispach0 = {
        .uGroupCountX     = (uint32_t)1024 / 16,
        .uGroupCountY     = (uint32_t)1024 / 16,
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 16,
        .uThreadPerGroupY = 16,
        .uThreadPerGroupZ = 1
    };

    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 2");
    gptGfx->begin_command_recording(ptCommandBuffer);

    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);

    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(gptData->ptDevice, sizeof(plGpuDynFilterSpec));
    plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
    ptDynamicData->iResolution = 1024;
    ptDynamicData->fRoughness = 0.0f;
    ptDynamicData->iSampleCount = (int)1024;
    ptDynamicData->iWidth = 0;
    ptDynamicData->fLodBias = 0.0f;
    ptDynamicData->iCurrentMipLevel = 0;

    const plPassResources tPassResources = {
        .atBuffers = {
            {
                .tHandle = ptScene->atFilterWorkingBuffers[0],
                .eStages = PL_SHADER_STAGE_COMPUTE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                .eUsage = PL_BUFFER_USAGE_STORAGE
            }
        }
    };

    gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
    gptGfx->bind_compute_bind_groups(ptCommandBuffer, tBrdfLutShader, 0, 1, &tBrdfBGSet1, 1, &tDynamicBinding);
    gptGfx->bind_compute_shader(ptCommandBuffer, tBrdfLutShader);
    gptGfx->dispatch(ptCommandBuffer, 1, &tDispach0);
    gptGfx->end_compute_pass(ptCommandBuffer);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tSubmitInfo0 = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {tSemHandle},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {tSemHandle},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 3");
    gptGfx->begin_command_recording(ptCommandBuffer);
    gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

    const plBufferImageCopy tBufferImageCopy0 = {
        .uImageWidth = 1024,
        .uImageHeight = 1024,
        .uImageDepth = 1,
        .uLayerCount = 1,
        .szBufferOffset = 0,
        .uBaseArrayLayer = 0,
    };
    gptGfx->copy_buffer_to_texture(ptCommandBuffer, ptScene->atFilterWorkingBuffers[0], ptScene->tBrdfLutTexture, 1, &tBufferImageCopy0);
    gptGfx->end_compute_pass(ptCommandBuffer);
    gptGfx->end_command_recording(ptCommandBuffer);
    const plSubmitInfo tSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {tSemHandle},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {tSemHandle},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
}

static void
pl__renderer_view_create_sky_luts_textures(plView* ptView)
{
    plDevice* ptDevice = gptData->ptDevice;
    plRenderSettings* ptSettings = gptAsset->get_data(ptView->ptParentScene->tSettings);

    if(gptGfx->is_texture_valid(ptDevice, ptView->atSkyLut[0]))
    {
        for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        {
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->atSkyLut[i]);
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->atSkyAerialLut[i]);
        }
    }

    const plTextureDesc tSkyLutDesc = {
        .tDimensions = { .xy = ptSettings->tSky.tSkyLutResolution, 1},
        .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tSkyLut"
    };

    const plTextureDesc tAerialLutDesc = {
        .tDimensions = ptSettings->tSky.tAerialLutResolution,
        .eFormat     = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_3D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tAerialLut"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->create_texture(&tSkyLutDesc, NULL, 0, &ptView->atSkyLut[i]);
        gptStarter->create_texture(&tAerialLutDesc, NULL, 0, &ptView->atSkyAerialLut[i]);
    }
}

static void
pl__renderer_view_create_textures(plView* ptView)
{

    plDevice* ptDevice = gptData->ptDevice;
    if(gptGfx->is_texture_valid(ptDevice, ptView->tSSGITexture))
    {
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tSSGITexture);
        ptView->tSSGITexture = (plTextureHandle){0};
    }
    plVec3 tNewDimensions = {ptView->tTargetSize.x + 300.0f, ptView->tTargetSize.y + 300.0f, 1};

    if(gptGfx->is_texture_valid(ptDevice, ptView->tRawOutputTexture))
    {
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tRawOutputTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOMetalRoughnessTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOResult);
        pl__renderer_return_bindless_texture_index(ptView->ptParentScene, ptView->tContactShadowDepthTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tContactShadowDepthTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture0);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture1);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture);
        pl__cloud_release_low_resolution(ptView);
        if(gptGfx->is_texture_valid(ptDevice, ptView->tFXAAInputTexture))
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFXAAInputTexture);
        ptView->tFXAAInputTexture = (plTextureHandle){0};
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickTexture);

        if(gptGfx->is_texture_valid(ptDevice, ptView->tTransmissionTexture))
        {
            pl__renderer_return_bindless_texture_index(ptView->ptParentScene, ptView->tTransmissionTexture);
            pl__renderer_return_bindless_texture_index(ptView->ptParentScene, ptView->tTransmissionDepthTexture);
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->tTransmissionTexture);
            const plTextureDesc tRawOutput2TextureDesc = {
                .tDimensions   = tNewDimensions,
                .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
                .uLayers       = 1,
                .uMips         = 0,
                .eType         = PL_TEXTURE_TYPE_2D,
                .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
                .pcDebugName = "tRawOutputTexture2"
            };
            gptStarter->create_texture(&tRawOutput2TextureDesc, NULL, 0, &ptView->tTransmissionTexture);
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->tTransmissionDepthTexture);
            const plTextureDesc tDepthCopyDesc = {
                .tDimensions = tNewDimensions,
                .eFormat     = PL_FORMAT_D32_FLOAT_S8_UINT,
                .uLayers     = 1,
                .uMips       = 1,
                .eType       = PL_TEXTURE_TYPE_2D,
                .eUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
                .pcDebugName = "transmission depth snapshot"
            };
            gptStarter->create_texture(&tDepthCopyDesc, NULL, 0, &ptView->tTransmissionDepthTexture);
            ptView->tViewData.iTransmissionFrameBufferIndex = pl__renderer_get_bindless_texture_index(ptView->ptParentScene, ptView->tTransmissionTexture);
            ptView->tViewData.iTransmissionDepthIndex = pl__renderer_get_bindless_texture_index(ptView->ptParentScene, ptView->tTransmissionDepthTexture);
        }
    }

    const plTextureDesc tRawOutputTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "offscreen final"
    };

    const plTextureDesc tPickTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "pick original"
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "albedo texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    const plTextureDesc tMaskTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R32G32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "mask texture"
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "emissive texture"
    };

    const plTextureDesc tAOMetalRoughnessTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "ORM / probe selection"
    };

    gptStarter->create_texture(&tRawOutputTextureDesc, NULL, 0, &ptView->tRawOutputTexture);
    gptStarter->create_texture(&tAlbedoTextureDesc, NULL, 0, &ptView->tAlbedoTexture);
    gptStarter->create_texture(&tNormalTextureDesc, NULL, 0, &ptView->tNormalTexture);
    gptStarter->create_texture(&tAOMetalRoughnessTextureDesc, NULL, 0, &ptView->tAOMetalRoughnessTexture);
    const plTextureDesc tAOResultDesc = {
        .tDimensions = tNewDimensions,
        .eFormat     = PL_FORMAT_R32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName = "screen AO"
    };
    gptStarter->create_texture(&tAOResultDesc, NULL, 0, &ptView->tAOResult);
    gptStarter->create_texture(&tDepthTextureDesc, NULL, 0, &ptView->tDepthTexture);
    plTextureDesc tContactDepthDesc = tDepthTextureDesc;
    tContactDepthDesc.pcDebugName = "contact shadow depth";
    gptStarter->create_texture(&tContactDepthDesc, NULL, 0, &ptView->tContactShadowDepthTexture);
    ptView->tViewData.iContactShadowDepthIndex = (int)pl__renderer_get_bindless_texture_index(ptView->ptParentScene, ptView->tContactShadowDepthTexture);
    gptStarter->create_texture(&tMaskTextureDesc, NULL, 0, &ptView->atUVMaskTexture0);
    gptStarter->create_texture(&tMaskTextureDesc, NULL, 0, &ptView->atUVMaskTexture1);
    gptStarter->create_texture(&tRawOutputTextureDesc, NULL, 0, &ptView->tFinalTexture);
    gptStarter->create_texture(&tPickTextureDesc, NULL, 0, &ptView->tPickTexture);

    pl__renderer_view_create_sky_luts_textures(ptView);
}

static void
pl__renderer_view_create_buffers(plView* ptView)
{
    const plBufferDesc tClusterDesc = {
        .eUsage = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = PL_CLUSTER_BUFFER_SIZE,
        .pcDebugName = "cluster light lists"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->get_staging_buffer(4096, &ptView->atSunCameraBuffers[i], "sun shadow camera buffer");
        gptStarter->get_staging_buffer(4096, &ptView->atDShadowCameraBuffers[i], "directional shadow camera buffer");
        gptStarter->get_staging_buffer(PL_MAX_LIGHTS * sizeof(plGpuDirectionLightShadow), &ptView->atDLightShadowDataBuffer[i], "directional shadow buffer");
        gptStarter->get_staging_buffer(sizeof(plGpuViewData), &ptView->atViewBuffers[i], "view buffer");
        gptStarter->get_readback_buffer(sizeof(uint32_t) * 2, &ptView->atPickBuffer[i], "pick buffer");
        gptStarter->create_buffer(&tClusterDesc, NULL, &ptView->atClusterBuffers[i]);
    }
}

static void
pl__renderer_view_create_bindgroups(plView* ptView)
{
    plScene* ptScene = ptView->ptParentScene;

    ptView->tFinalTextureHandle = gptDraw->create_bind_group_for_texture(ptView->tFinalTexture);

    const plBindGroupDesc tJFABindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "jfa_0"),
        .pcDebugName = "temp jfa bind group"
    };

    ptView->atJFABindGroups[0] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);
    ptView->atJFABindGroups[1] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);

    const plBindGroupDesc tOutlineBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "jfa_1"),
        .pcDebugName = "temp bind group 0"
    };
    ptView->atOutlineBG[0] = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);
    ptView->atOutlineBG[1] = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "deferred_lighting_2"),
        .pcDebugName = "lighting bind group"
    };
    ptView->tLightingBindGroup = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);

    const plBindGroupDesc tTonemapBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "tonemap"),
        .pcDebugName = "tonemap bind group c0"
    };
    ptView->tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = ptScene->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };

    const plBindGroupDesc tPickBindGroupDesc = {
        .ptPool = gptData->ptBindGroupPool,
        .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"),
        .pcDebugName = "pick bind group"
    };

    const plBindGroupDesc tDeferredBG1Desc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"),
        .pcDebugName = "view specific bindgroup"
    };

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = ptScene->tViewBGLayout,
        .pcDebugName = "light bind group 2"
    };

    const plBindGroupDesc tSkyLutBG2Desc = {
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_lut_2"),
        .pcDebugName = "atSkyLutBG2",
        .ptPool      = gptData->ptBindGroupPool
    };

    const plBindGroupDesc tSkyAerialLutBG2Desc = {
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"),
        .pcDebugName = "atSkyAerialLutBG2",
        .ptPool      = gptData->ptBindGroupPool
    };

    const plBindGroupDesc tSkyBG2Desc = {
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "sky_2"),
        .pcDebugName = "tSkyBG1",
        .ptPool      = gptData->ptBindGroupPool
    };

    const plBindGroupDesc tSkyAerialBG2Desc = {
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "aerial_luts"),
        .pcDebugName = "tSkyAerialBG2",
        .ptPool      = gptData->ptBindGroupPool
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        ptView->atDShadowBG[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);
        ptView->atSunCSMBG[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);
        ptView->atPickBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tPickBindGroupDesc);
        ptView->atDeferredBG1[i] = gptGfx->create_bind_group(gptData->ptDevice, &tDeferredBG1Desc);
        ptView->atViewBG[i] = gptGfx->create_bind_group(gptData->ptDevice, &tViewBGDesc);
        ptView->atSkyLutBG2[i] = gptGfx->create_bind_group(gptData->ptDevice, &tSkyLutBG2Desc);
        ptView->atSkyAerialLutBG2[i] = gptGfx->create_bind_group(gptData->ptDevice, &tSkyAerialLutBG2Desc);
        ptView->atSkyBG2[i] = gptGfx->create_bind_group(gptData->ptDevice, &tSkyBG2Desc);
        ptView->atSkyAerialBG2[i] = gptGfx->create_bind_group(gptData->ptDevice, &tSkyAerialBG2Desc);
    }
}

static void
pl__renderer_view_update_sky_luts_bindgroups(plView* ptView)
{
    plScene* ptScene = ptView->ptParentScene;

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateData tBGSkyLut = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptView->atSkyLut[i]},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[i]},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunMultiscatterLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atSkyLutBG2[i], &tBGSkyLut);

        const plBindGroupUpdateData tBGData4 = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptView->atSkyAerialLut[i]},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[i]},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunMultiscatterLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atSkyAerialLutBG2[i], &tBGData4);

        const plBindGroupUpdateData tBGData3 = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->atSkyLut[i]},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atSkyBG2[i], &tBGData3);

        const plBindGroupUpdateData tBGData6 = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptView->tFinalTexture},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->tDepthTexture},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->atSkyAerialLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atSkyAerialBG2[i], &tBGData6);
    }
}

static void
pl__renderer_view_update_bindgroups(plView* ptView)
{
    plScene* ptScene = ptView->ptParentScene;

    const plBindGroupUpdateData tJFABGData0 = {
        .atTextureBindings = {
            {
                .tTexture = ptView->atUVMaskTexture0,
                .uSlot    = 0,
                .eType    = PL_TEXTURE_BINDING_TYPE_STORAGE
            },
            {
                .tTexture = ptView->atUVMaskTexture1,
                .uSlot    = 1,
                .eType    = PL_TEXTURE_BINDING_TYPE_STORAGE
            }
        }
    };
    const plBindGroupUpdateData tJFABGData1 = {
        .atTextureBindings = {
            {
                .tTexture = ptView->atUVMaskTexture1,
                .uSlot    = 0,
                .eType    = PL_TEXTURE_BINDING_TYPE_STORAGE
            },
            {
                .tTexture = ptView->atUVMaskTexture0,
                .uSlot    = 1,
                .eType    = PL_TEXTURE_BINDING_TYPE_STORAGE
            }
        }
    };

    gptGfx->update_bind_group(gptData->ptDevice, ptView->atJFABindGroups[0], &tJFABGData0);
    gptGfx->update_bind_group(gptData->ptDevice, ptView->atJFABindGroups[1], &tJFABGData1);

    const plBindGroupUpdateData tBGData = {
        .atTextureBindings = {
            {
                .tTexture = ptView->tAlbedoTexture,
                .uSlot    = 0,
                .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tNormalTexture,
                .uSlot    = 1,
                .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tAOMetalRoughnessTexture,
                .uSlot    = 2,
                .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture,
                .uSlot    = 3, 
                .eType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup, &tBGData);

    const plBindGroupUpdateData tTonemapBGData = {
        .atTextureBindings = {
            {.tTexture = ptView->tFinalTexture, .uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE}
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptView->tTonemapBG, &tTonemapBGData);

    const plBindGroupUpdateData tOutlineBGData0 = {
        .atTextureBindings = {
            {
                .tTexture = ptView->tRawOutputTexture,
                .uSlot    = 0,
                .eType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = ptView->atUVMaskTexture0,
                .uSlot    = 1,
                .eType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptView->atOutlineBG[0], &tOutlineBGData0);

    const plBindGroupUpdateData tOutlineBGData1 = {
        .atTextureBindings = {
            {
                .tTexture = ptView->tRawOutputTexture,
                .uSlot    = 0,
                .eType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = ptView->atUVMaskTexture1,
                .uSlot    = 1,
                .eType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptView->atOutlineBG[1], &tOutlineBGData1);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateData tDShadowBGData = {
            .atBufferBindings = {
                {
                    .tBuffer       = ptView->atDShadowCameraBuffers[i],
                    .uSlot         = 0,
                    .szBufferRange = 4096
                },
                {
                    .tBuffer       = ptScene->tInstanceBuffer,
                    .uSlot         = 1,
                    .szBufferRange = sizeof(plShadowInstanceBufferData) * ptScene->uShadowInstanceCapacity
                } 
            }
        };

        const plBindGroupUpdateData tDShadowBGData2 = {
            .atBufferBindings = {
                {
                    .tBuffer       = ptView->atSunCameraBuffers[i],
                    .uSlot         = 0,
                    .szBufferRange = 4096
                },
                {
                    .tBuffer       = ptScene->tInstanceBuffer,
                    .uSlot         = 1,
                    .szBufferRange = sizeof(plShadowInstanceBufferData) * ptScene->uShadowInstanceCapacity
                } 
            }
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptView->atDShadowBG[i], &tDShadowBGData);
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atSunCSMBG[i], &tDShadowBGData2);

        const plBindGroupUpdateData tPickBGData = {
            .atBufferBindings = {
                { .uSlot = 0, .tBuffer = ptView->atPickBuffer[i], .szBufferRange = sizeof(uint32_t) * 2}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atPickBindGroup[i], &tPickBGData);

        const plBindGroupUpdateData tView2BGData = {
            .atBufferBindings = {
                {
                    .tBuffer       = ptView->atViewBuffers[i],
                    .uSlot         = 0,
                    .szBufferRange = sizeof(plGpuViewData)
                }
            }
        };
        
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atDeferredBG1[i], &tView2BGData);

        const plBindGroupUpdateData tViewBGData = {
            .atTextureBindings = {
                {.uSlot = 9, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptView->tAOResult}
            },
            .atBufferBindings  = {
                { .uSlot = 0, .tBuffer = ptView->atViewBuffers[i], .szBufferRange = sizeof(plGpuViewData) },
                { .uSlot = 1, .tBuffer = ptScene->atPointLightBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightBuffer[i])->tDesc.szByteSize},
                { .uSlot = 2, .tBuffer = ptScene->atSpotLightBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightBuffer[i])->tDesc.szByteSize},
                { .uSlot = 3, .tBuffer = ptScene->atDirectionLightBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atDirectionLightBuffer[i])->tDesc.szByteSize},
                { .uSlot = 4, .tBuffer = ptView->atDLightShadowDataBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptView->atDLightShadowDataBuffer[i])->tDesc.szByteSize},
                { .uSlot = 5, .tBuffer = ptScene->atPointLightShadowDataBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightShadowDataBuffer[i])->tDesc.szByteSize},
                { .uSlot = 6, .tBuffer = ptScene->atSpotLightShadowDataBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightShadowDataBuffer[i])->tDesc.szByteSize},
                { .uSlot = 7, .tBuffer = ptScene->tGPUProbeDataBuffers, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tGPUProbeDataBuffers)->tDesc.szByteSize},
                { .uSlot = 8, .tBuffer = ptView->atClusterBuffers[i], .szBufferRange = PL_CLUSTER_BUFFER_SIZE },
            }
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptView->atViewBG[i], &tViewBGData);

        if(ptView->ptParentScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED)
        {
            const plBindGroupUpdateData tGlobalBindGroupData = {
                .atTextureBindings = {
                    {
                        .tTexture = ptView->tTransmissionTexture,
                        .uSlot    = PL_MAX_BINDLESS_TEXTURE_SLOT,
                        .uIndex   = (uint32_t)ptView->tViewData.iTransmissionFrameBufferIndex,
                        .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED
                    },
                    {
                        .tTexture = ptView->tTransmissionDepthTexture,
                        .uSlot = PL_MAX_BINDLESS_TEXTURE_SLOT,
                        .uIndex = (uint32_t)ptView->tViewData.iTransmissionDepthIndex,
                        .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED
                    }
                }
            };
            gptGfx->update_bind_group(gptData->ptDevice, ptView->ptParentScene->atSceneBindGroups[i], &tGlobalBindGroupData);
        }
    }

    pl__renderer_view_update_sky_luts_bindgroups(ptView);
}

static void
pl__renderer_probe_create_textures(plScene* ptScene, plEnvironmentProbeData* ptProbeData)
{
    // plEnvironmentProbeComponent* ptProbe = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbeData->tEntity);

    // uint32_t uResol

    const plTextureDesc tRawOutputTextureCubeDesc = {
        .tDimensions   = {ptProbeData->tTargetSize.x, ptProbeData->tTargetSize.y, 1},
        .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 6,
        .uMips         = 0,
        .eType         = PL_TEXTURE_TYPE_CUBE,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "offscreen final cube"
    };
    gptStarter->create_texture(&tRawOutputTextureCubeDesc, NULL, 0, &ptProbeData->tRawOutputTexture);

    const plTextureDesc tSpecularTextureDesc = {
        .tDimensions = {ptProbeData->tTargetSize.x, ptProbeData->tTargetSize.y, 1},
        .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_CUBE,
        .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "probe lambertian env"
    };
    gptStarter->create_texture(&tSpecularTextureDesc, NULL, 0, &ptProbeData->tLambertianEnvTexture);
    ptProbeData->uLambertianEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, ptProbeData->tLambertianEnvTexture);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {ptProbeData->tTargetSize.x, ptProbeData->tTargetSize.y, 1},
        .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = (uint32_t)floorf(log2f(ptProbeData->tTargetSize.x)) - 3, // guarantee final dispatch during filtering is 16 threads
        .eType       = PL_TEXTURE_TYPE_CUBE,
        .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "probe tGGXEnvTexture"
    };
    gptStarter->create_texture(&tTextureDesc, NULL, 0, &ptProbeData->tGGXEnvTexture);
    ptProbeData->uGGXEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, ptProbeData->tGGXEnvTexture);
    
    plTextureViewDesc tRawOutputTextureViewDesc = {
        .eFormat     = tRawOutputTextureCubeDesc.eFormat,
        .uBaseMip    = 0,
        .uMips       = 1,
        .uBaseLayer  = 0,
        .uLayerCount = 1,
        .tTexture    = ptProbeData->tRawOutputTexture,
        .eType       = PL_TEXTURE_TYPE_2D
    };

    for(uint32_t uFace = 0; uFace < 6; uFace++)
    {
        tRawOutputTextureViewDesc.uBaseLayer = uFace;
        ptProbeData->atRawOutputTextureViews[uFace] = gptGfx->create_texture_view(gptData->ptDevice, &tRawOutputTextureViewDesc);
    }
}

static void
pl__renderer_probe_create_buffers(plScene* ptScene, plEnvironmentProbeData* ptProbeData)
{
    gptStarter->get_staging_buffer(4096, &ptProbeData->tDShadowCameraBuffers, "probe directional shadow camera buffer");
    gptStarter->get_staging_buffer(4096, &ptProbeData->tSunCameraBuffers, "probe sun shadow camera buffer");
    gptStarter->get_staging_buffer(PL_MAX_LIGHTS * sizeof(plGpuDirectionLightShadow), &ptProbeData->tDLightShadowDataBuffer, "directional shadow buffer");
    gptStarter->get_staging_buffer(sizeof(plGpuViewData), &ptProbeData->tViewBuffer, "view buffer");
}

static void
pl__renderer_probe_create_bindgroups(plScene* ptScene, plEnvironmentProbeData* ptProbeData)
{
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = ptScene->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };
    ptProbeData->tDShadowBG = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);
    ptProbeData->tSunCameraBG = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);

    const plBindGroupDesc tGBufferFillBG1Desc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "gbuffer_fill_1"),
        .pcDebugName = "gbuffer fill bg1"
    };
    ptProbeData->tGBufferBG = gptGfx->create_bind_group(gptData->ptDevice, &tGBufferFillBG1Desc);

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = ptScene->tViewBGLayout,
        .pcDebugName = "probe scene bg"
    };
    ptProbeData->tViewBG = gptGfx->create_bind_group(gptData->ptDevice, &tViewBGDesc);
}

static void
pl__renderer_probe_update_bindgroups(plScene* ptScene, plEnvironmentProbeData* ptProbeData)
{
    const plBindGroupUpdateData tDShadowBGData = {
        .atBufferBindings = {
            {
                .tBuffer       = ptProbeData->tDShadowCameraBuffers,
                .uSlot         = 0,
                .szBufferRange = 4096
            },
            {
                .tBuffer       = ptScene->tInstanceBuffer,
                .uSlot         = 1,
                .szBufferRange = sizeof(plShadowInstanceBufferData) * ptScene->uShadowInstanceCapacity
            }
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptProbeData->tDShadowBG, &tDShadowBGData);

    const plBindGroupUpdateData tDShadowBGData2 = {
        .atBufferBindings = {
            {
                .tBuffer       = ptProbeData->tSunCameraBuffers,
                .uSlot         = 0,
                .szBufferRange = 4096
            },
            {
                .tBuffer       = ptScene->tInstanceBuffer,
                .uSlot         = 1,
                .szBufferRange = sizeof(plShadowInstanceBufferData) * ptScene->uShadowInstanceCapacity
            }
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptProbeData->tSunCameraBG, &tDShadowBGData2);

    const plBindGroupUpdateData tGBufferFillBG1Data = {
        .atBufferBindings = {
            {
                .tBuffer       = ptProbeData->tViewBuffer,
                .uSlot         = 0,
                .szBufferRange = sizeof(plGpuViewData)
            }
        }
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptProbeData->tGBufferBG, &tGBufferFillBG1Data);

    const plBindGroupUpdateData tViewBGData = {
        .atTextureBindings = {
            {.uSlot = 9, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = gptData->tDummyTexture}
        },
        .atBufferBindings  = {
            { .uSlot = 0, .tBuffer = ptProbeData->tViewBuffer, .szBufferRange = sizeof(plGpuViewData) },
            { .uSlot = 1, .tBuffer = ptScene->atPointLightBuffer[0], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightBuffer[0])->tDesc.szByteSize},
            { .uSlot = 2, .tBuffer = ptScene->atSpotLightBuffer[0], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightBuffer[0])->tDesc.szByteSize},
            { .uSlot = 3, .tBuffer = ptScene->atDirectionLightBuffer[0], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atDirectionLightBuffer[0])->tDesc.szByteSize},
            { .uSlot = 4, .tBuffer = ptProbeData->tDLightShadowDataBuffer, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptProbeData->tDLightShadowDataBuffer)->tDesc.szByteSize},
            { .uSlot = 5, .tBuffer = ptScene->atPointLightShadowDataBuffer[0], .szBufferRange =  gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightShadowDataBuffer[0])->tDesc.szByteSize},
            { .uSlot = 6, .tBuffer = ptScene->atSpotLightShadowDataBuffer[0], .szBufferRange =  gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightShadowDataBuffer[0])->tDesc.szByteSize},
            { .uSlot = 7, .tBuffer = ptScene->tGPUProbeDataBuffers, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tGPUProbeDataBuffers)->tDesc.szByteSize},
            { .uSlot = 8, .tBuffer = ptScene->tGPUProbeDataBuffers, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tGPUProbeDataBuffers)->tDesc.szByteSize }, // Probes use the full-light fallback; bind a valid unused storage buffer.
        }
    };
    
    gptGfx->update_bind_group(gptData->ptDevice, ptProbeData->tViewBG, &tViewBGData);
}

void
pl__renderer_scene_load_skybox_from_panorama(plScene* ptScene, const char* pcPath, int iResolution)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    const int iSamples = 512;
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "load image");
    float* pfPanoramaData = gptImage->load_hdr_from_file(pcPath, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    PL_ASSERT(pfPanoramaData);

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    {
        int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
        plComputeShaderVariantDesc tPanoramaVariant = {
            .pConstants = aiSkyboxSpecializationData
        };
        plComputeShaderHandle tPanoramaShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "panorama_to_cubemap", &tPanoramaVariant);
        pl_temp_allocator_reset(&gptData->tTempAllocator);

        plBufferHandle atComputeBuffers[7] = {0};
        const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
        const plBufferDesc tInputBufferDesc = {
            .eUsage     = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uPanoramaSize,
            .pcDebugName = "panorama input buffer"
        };
        gptStarter->get_staging_buffer(uPanoramaSize, &atComputeBuffers[0], "panorama input");
        plBuffer* ptComputeBuffer = gptGfx->get_buffer(ptDevice, atComputeBuffers[0]);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, uPanoramaSize);
        
        gptImage->free(pfPanoramaData);

        const plBufferDesc tOutputBufferDesc = {
            .eUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER,
            .szByteSize = uFaceSize,
            .pcDebugName = "panorama output buffer"
        };
        
        for(uint32_t i = 0; i < 6; i++)
            gptStarter->create_buffer(&tOutputBufferDesc, NULL, &atComputeBuffers[i + 1]);

        const plBindGroupDesc tComputeBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "panorama_to_cubemap"),
            .pcDebugName = "compute bind group"
        };
        plBindGroupHandle tComputeBindGroup = gptGfx->create_bind_group(ptDevice, &tComputeBindGroupDesc);

        const plBindGroupUpdateData tBGData = {
            .atBufferBindings = {
                { .uSlot = 0, .tBuffer = atComputeBuffers[0], .szBufferRange = uPanoramaSize},
                { .uSlot = 1, .tBuffer = atComputeBuffers[1], .szBufferRange = uFaceSize},
                { .uSlot = 2, .tBuffer = atComputeBuffers[2], .szBufferRange = uFaceSize},
                { .uSlot = 3, .tBuffer = atComputeBuffers[3], .szBufferRange = uFaceSize},
                { .uSlot = 4, .tBuffer = atComputeBuffers[4], .szBufferRange = uFaceSize},
                { .uSlot = 5, .tBuffer = atComputeBuffers[5], .szBufferRange = uFaceSize},
                { .uSlot = 6, .tBuffer = atComputeBuffers[6], .szBufferRange = uFaceSize} 
            }
        };
        gptGfx->update_bind_group(ptDevice, tComputeBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tComputeBindGroup);

        // calculate cubemap data
        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 2,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };
        
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "load skybox 0");
        gptGfx->begin_command_recording(ptCommandBuffer);

        const plPassResources tPassResources = {
            .atBuffers = {
                { .tHandle = atComputeBuffers[0], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ,  .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = atComputeBuffers[1], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = atComputeBuffers[2], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = atComputeBuffers[3], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = atComputeBuffers[4], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = atComputeBuffers[5], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE },
                { .tHandle = atComputeBuffers[6], .eStages = PL_SHADER_STAGE_COMPUTE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE, .eUsage = PL_BUFFER_USAGE_STORAGE }
            }
        };

        gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->bind_compute_bind_groups(ptCommandBuffer, tPanoramaShader, 0, 1, &tComputeBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptCommandBuffer, tPanoramaShader);
        gptGfx->dispatch(ptCommandBuffer, 1, &tDispach);
        gptGfx->end_compute_pass(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);

        const plTextureDesc tSkyboxTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .eType       = PL_TEXTURE_TYPE_CUBE,
            .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
            .pcDebugName = "skybox"
        };
        gptStarter->create_texture(&tSkyboxTextureDesc, NULL, 0, &ptScene->tSkyboxTexture);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "load skybox 1");
        gptGfx->begin_command_recording(ptCommandBuffer);
        gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

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
            gptGfx->copy_buffer_to_texture(ptCommandBuffer, atComputeBuffers[i + 1], ptScene->tSkyboxTexture, 1, &tBufferImageCopy);
        }

        gptGfx->generate_mipmaps(ptCommandBuffer, ptScene->tSkyboxTexture);

        gptGfx->end_compute_pass(ptCommandBuffer);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        
        for(uint32_t i = 0; i < 7; i++)
            gptGfx->destroy_buffer(ptDevice, atComputeBuffers[i]);

    
        if(!gptGfx->is_bind_group_valid(ptDevice, ptScene->tSkyboxBindGroup))
        {
            const plBindGroupDesc tSkyboxBindGroupDesc = {
                .ptPool = gptData->ptBindGroupPool,
                .tLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "skybox_2"),
                .pcDebugName = "skybox bind group"
            };
            ptScene->tSkyboxBindGroup = gptGfx->create_bind_group(ptDevice, &tSkyboxBindGroupDesc);
        }

        {
            const plBindGroupUpdateData tBGData1 = {
                .atTextureBindings = {
                    {.tTexture = ptScene->tSkyboxTexture, .uSlot = 0, .uIndex = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            gptGfx->update_bind_group(ptDevice, ptScene->tSkyboxBindGroup, &tBGData1);
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static bool
pl__renderer_initialize(void)
{

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    if(gptData->bFullyInitialized)
    {
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
        return false;
    }

    gptData->bFullyInitialized = true;

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "create resources");

    // create dummy textures
    const plTextureDesc tDummyTextureDesc = {
        .tDimensions   = {2, 2, 1},
        .eFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "dummy"
    };

    const float afDummyTextureData[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    gptStarter->create_texture(&tDummyTextureDesc, afDummyTextureData, sizeof(afDummyTextureData), &gptData->tDummyTexture);

    const plTextureDesc tSkyboxTextureDesc = {
        .tDimensions = {1, 1, 1},
        .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_CUBE,
        .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "dummy cube"
    };
    gptStarter->create_texture(&tSkyboxTextureDesc, NULL, 0, &gptData->tDummyTextureCube);

    // create samplers
    const plSamplerDesc tSamplerLinearClampDesc = {
        .eMagFilter      = PL_FILTER_LINEAR,
        .eMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .eVAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .eUAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .eMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "linear clamp"
    };
    gptData->tSamplerLinearClamp = gptGfx->create_sampler(gptData->ptDevice, &tSamplerLinearClampDesc);

    const plSamplerDesc tSamplerNearestClampDesc = {
        .eMagFilter      = PL_FILTER_NEAREST,
        .eMinFilter      = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .eVAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .eUAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .eMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "nearest clamp"
    };
    gptData->tSamplerNearestClamp = gptGfx->create_sampler(gptData->ptDevice, &tSamplerNearestClampDesc);

    const plSamplerDesc tSamplerLinearRepeatDesc = {
        .eMagFilter      = PL_FILTER_LINEAR,
        .eMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .eVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .eUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .eMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "linear repeat"
    };
    gptData->tSamplerLinearRepeat = gptGfx->create_sampler(gptData->ptDevice, &tSamplerLinearRepeatDesc);

    const plSamplerDesc tSamplerNearestRepeatDesc = {
        .eMagFilter      = PL_FILTER_NEAREST,
        .eMinFilter      = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .eVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .eUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .eMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "nearest repeat"
    };
    gptData->tSamplerNearestRepeat = gptGfx->create_sampler(gptData->ptDevice, &tSamplerNearestRepeatDesc);

    gptData->tRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R16G16B16A16_FLOAT,
            PL_FORMAT_R8G8B8A8_UNORM,
            PL_FORMAT_R16G16_FLOAT,
            PL_FORMAT_R16G16B16A16_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    gptData->tDeferredLightingRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R16G16B16A16_FLOAT,
            PL_FORMAT_R8G8B8A8_UNORM,
            PL_FORMAT_R16G16_FLOAT,
            PL_FORMAT_R16G16B16A16_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    gptData->tTransparentRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R16G16B16A16_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    gptData->tDepthRenderPassLayout = (plRenderAttachmentInfo){
        .eDepthFormat = PL_FORMAT_D16_UNORM
    };

    gptData->tPickRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R8G8B8A8_UNORM
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    gptData->tPostProcessRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R16G16B16A16_FLOAT
        }
    };

    gptData->tFinalRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R16G16B16A16_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    gptData->tUVRenderPassLayout = (plRenderAttachmentInfo){
        .aeColorFormats = {
            PL_FORMAT_R32G32_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);

    pl_temp_allocator_reset(&gptData->tTempAllocator);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
};

static void
pl__renderer_remove_instance(plScene* ptScene, plEntity tBaseEntity, plEntity tInstanceEntity)
{
    const plDrawableKey tKey = {
        .uEntity  = tBaseEntity.uData,
        .uSubmesh = PL_DRAWABLE_OBJECT_KEY
    };

    uint64_t uFirstDrawHash = pl_hm_hash(&tKey, sizeof(plDrawableKey), 0);
    if(pl_hm_has_key(&ptScene->tDrawableHashmap, uFirstDrawHash))
    {
        uint32_t uFirstSubmeshIndex = (uint32_t)pl_hm_lookup(&ptScene->tDrawableHashmap, uFirstDrawHash);

        const plDrawableKey tFirstKey = {
            .uEntity  = tBaseEntity.uData,
            .uSubmesh = uFirstSubmeshIndex
        };

        uint64_t uMainDrawHash = pl_hm_hash(&tFirstKey, sizeof(plDrawableKey), 0);
        uint64_t uMainDrawableIndex = pl_hm_lookup(&ptScene->tDrawableHashmap, uMainDrawHash);
        plDrawableResources* ptMainDrawableResources = &ptScene->sbtDrawableResources[uMainDrawableIndex];

        uint32_t uSubmeshCount = ptMainDrawableResources->uSubmeshCount;
        uint32_t uFirstSubmesh = ptMainDrawableResources->uSubmeshIndex;
    
        for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < uSubmeshCount; uSubmeshIndex++)
        {
            const plDrawableKey tKey2 = {
                .uEntity  = tBaseEntity.uData,
                .uSubmesh = uFirstSubmesh + uSubmeshIndex
            };
            uint64_t uDrawHash = pl_hm_hash(&tKey2, sizeof(plDrawableKey), 0);
            if(pl_hm_has_key(&ptScene->tDrawableHashmap, uDrawHash))
            {
                uint64_t uDrawableIndex = pl_hm_lookup(&ptScene->tDrawableHashmap, uDrawHash);
                plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
                plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
                ptDrawable->uInstanceCount--;
                for(uint32_t i = 0; i < pl_sb_size(ptDrawableResources->sbtEntities); i++)
                {
                    if(ptDrawableResources->sbtEntities[i].uData == tInstanceEntity.uData)
                    {
                        pl_sb_del_swap(ptDrawableResources->sbtEntities, i);
                        break;
                    }
                }

                gptFreeList->return_node(&ptScene->tTransformFreelist, ptDrawableResources->ptTransformNode);
                gptFreeList->return_node(&ptScene->tInstanceFreelist, ptDrawableResources->ptInstanceNode);
                ptDrawableResources->ptTransformNode = gptFreeList->get_node(&ptScene->tTransformFreelist, ptDrawable->uInstanceCount);
                ptDrawableResources->ptInstanceNode = gptFreeList->get_node(&ptScene->tInstanceFreelist, ptDrawable->uInstanceCount * 6);
            }
        }
    }

    pl_hm_remove(&ptScene->tInstanceHashmap, tInstanceEntity.uData);
}

static bool
pl__renderer_add_drawable_objects_to_scene(plScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects)
{

    if(uObjectCount == 0)
        return true;

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    ptScene->tInternalFlags |= PL_SCENE_INTERNAL_FLAG_OBJECT_COUNT_DIRTY;

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, atObjects[i]);
        plMesh* ptMainMesh = gptAsset->get_data(ptObject->tMesh);

        bool bAssetModified = gptMesh->generate_indices(ptMainMesh);
        bAssetModified = gptMesh->generate_normals(ptMainMesh) || bAssetModified; // also generates normals
        bAssetModified = gptMesh->generate_tangents(ptMainMesh) || bAssetModified; // also generates normals
        bAssetModified = gptMesh->generate_uvs(ptMainMesh) || bAssetModified;
        bAssetModified = gptMesh->generate_colors(ptMainMesh) || bAssetModified;
        if(bAssetModified)
        {
            // TODO: decide if we want to save for them
            gptAsset->save(ptObject->tMesh, PL_ASSET_ENCODING_AUTO);
        }

        // check if can be instanced
        bool bAddInstance = false;
        plEntity tFirstEntity = {0};
        if(ptObject->atMaterialOverrides == NULL && !gptEcs->has_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), atObjects[i])) // ignore material overrides for now
        {
            const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbuActiveDrawables[uDrawableIndex]];
                plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[ptScene->sbuActiveDrawables[uDrawableIndex]];
                plObjectComponent* ptExistingObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);

                if(ptExistingObject->tMesh.uData != ptObject->tMesh.uData)
                    continue;

                if(ptExistingObject->uFirstSubmesh != ptObject->uFirstSubmesh)
                    continue;

                if(ptExistingObject->uSubmeshCount != ptObject->uSubmeshCount)
                    continue;

                if(ptExistingObject->tFlags != ptObject->tFlags)
                    continue;

                if(ptDrawable->uInstanceCount >= PL_MAX_INSTANCES_PER_DRAWABLE)
                    continue;

                tFirstEntity = ptDrawableResources->tEntity;
                bAddInstance = true;
                break;
            }
        }

        if(bAddInstance)
        {

            pl_hm_insert(&ptScene->tInstanceHashmap, atObjects[i].uData, tFirstEntity.uData);

            const plDrawableKey tKey = {
                .uEntity  = tFirstEntity.uData,
                .uSubmesh = PL_DRAWABLE_OBJECT_KEY
            };

            uint64_t uFirstDrawHash = pl_hm_hash(&tKey, sizeof(plDrawableKey), 0);
            if(pl_hm_has_key(&ptScene->tDrawableHashmap, uFirstDrawHash))
            {
                uint32_t uFirstSubmeshIndex = (uint32_t)pl_hm_lookup(&ptScene->tDrawableHashmap, uFirstDrawHash);

                const plDrawableKey tFirstKey = {
                    .uEntity  = tFirstEntity.uData,
                    .uSubmesh = uFirstSubmeshIndex
                };

                uint64_t uMainDrawHash = pl_hm_hash(&tFirstKey, sizeof(plDrawableKey), 0);
                uint64_t uMainDrawableIndex = pl_hm_lookup(&ptScene->tDrawableHashmap, uMainDrawHash);
                plDrawableResources* ptMainDrawableResources = &ptScene->sbtDrawableResources[uMainDrawableIndex];

                uint32_t uSubmeshCount = ptMainDrawableResources->uSubmeshCount;
                uint32_t uFirstSubmesh = ptMainDrawableResources->uSubmeshIndex;
                for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < uSubmeshCount; uSubmeshIndex++)
                {
                    const plDrawableKey tKey2 = {
                        .uEntity  = tFirstEntity.uData,
                        .uSubmesh = uFirstSubmesh + uSubmeshIndex
                    };
                    uint64_t uDrawHash = pl_hm_hash(&tKey2, sizeof(plDrawableKey), 0);

                    if(pl_hm_has_key(&ptScene->tDrawableHashmap, uDrawHash))
                    {

                        uint64_t uDrawableIndex = pl_hm_lookup(&ptScene->tDrawableHashmap, uDrawHash);
                        plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
                        plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
                        pl_sb_push(ptDrawableResources->sbtEntities, atObjects[i]);
                        ptDrawable->uInstanceCount++;
                        gptFreeList->return_node(&ptScene->tTransformFreelist, ptDrawableResources->ptTransformNode);
                        gptFreeList->return_node(&ptScene->tInstanceFreelist, ptDrawableResources->ptInstanceNode);
                        ptDrawableResources->ptTransformNode = gptFreeList->get_node(&ptScene->tTransformFreelist, ptDrawable->uInstanceCount);
                        ptDrawableResources->ptInstanceNode = gptFreeList->get_node(&ptScene->tInstanceFreelist, ptDrawable->uInstanceCount * 6);
                        ptDrawable->uTransformIndex = (uint32_t)ptDrawableResources->ptTransformNode->uOffset;
                        ptDrawable->uInstanceIndex = (uint32_t)ptDrawableResources->ptInstanceNode->uOffset;
                        ptScene->tInternalFlags |= PL_SCENE_INTERNAL_FLAG_INSTANCE_BUFFER_DIRTY;
                    }
                }
            }
        }
        else
        {
            const plDrawableKey tObjectKey = {
                .uEntity  = atObjects[i].uData,
                .uSubmesh = PL_DRAWABLE_OBJECT_KEY
            };

            pl_hm_insert(&ptScene->tDrawableHashmap, pl_hm_hash(&tObjectKey, sizeof(tObjectKey), 0), ptObject->uFirstSubmesh);

            for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptObject->uSubmeshCount; uSubmeshIndex++)
            {

                const plDrawableKey tKey = {
                    .uEntity  = atObjects[i].uData,
                    .uSubmesh = ptObject->uFirstSubmesh + uSubmeshIndex
                };

                uint64_t uDrawHash = pl_hm_hash(&tKey, sizeof(plDrawableKey), 0);
                if(pl_hm_has_key(&ptScene->tDrawableHashmap, uDrawHash))
                    continue;

                plSubmesh* ptMesh = &ptMainMesh->atSubmeshes[ptObject->uFirstSubmesh + uSubmeshIndex];

                uint32_t uDrawableIndex = pl__renderer_get_drawable_slot(ptScene);
                plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
                plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];
                ptDrawableResources->uSubmeshIndex = uSubmeshIndex + ptObject->uFirstSubmesh;
                if(uSubmeshIndex == 0)
                    ptDrawableResources->uSubmeshCount = ptObject->uSubmeshCount;
                else
                    ptDrawableResources->uSubmeshCount = 0;

                ptDrawable->uInstanceCount = 1;
                ptDrawableResources->tEntity = atObjects[i];
                ptDrawableResources->uSkinIndex = UINT32_MAX;
                ptDrawableResources->ptTransformNode = gptFreeList->get_node(&ptScene->tTransformFreelist, ptDrawable->uInstanceCount);
                ptDrawableResources->ptInstanceNode = gptFreeList->get_node(&ptScene->tInstanceFreelist, ptDrawable->uInstanceCount * 6);
                ptDrawable->uTransformIndex = (uint32_t)ptDrawableResources->ptTransformNode->uOffset;
                ptDrawable->uInstanceIndex = (uint32_t)ptDrawableResources->ptInstanceNode->uOffset;
                ptScene->tInternalFlags |= PL_SCENE_INTERNAL_FLAG_INSTANCE_BUFFER_DIRTY;

                pl_hm_insert(&ptScene->tDrawableHashmap, uDrawHash, uDrawableIndex);

                bool bResult = pl__renderer_add_drawable_data_to_global_buffer(ptScene, uDrawableIndex, ptObject->uFirstSubmesh + uSubmeshIndex);
                if(!bResult)
                {
                    PL_ASSERT(false && "Renderer Buffers Full");
                    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
                    return false;
                }

                plAssetHandle tMaterial = ptObject->atMaterialOverrides ? ptObject->atMaterialOverrides[uSubmeshIndex] : ptMesh->tMaterial;
                ptDrawable->uMaterialIndex = pl__renderer_get_or_create_material_slot(ptScene, tMaterial);
                pl_renderer_update_scene_asset(ptScene, tMaterial);
            }
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

static void
pl__renderer_add_probes_to_scene(plScene* ptScene, uint32_t uCount, const plEntity* atProbes)
{
    ptScene->uLastProbeAddFrame = gptIO->ulFrameCount;
    for(uint32_t i = 0; i < uCount; i++)
    {
        plEnvironmentProbeComponent* ptProbe = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, atProbes[i]);

        plEnvironmentProbeData tProbeData = {
            .tEntity = atProbes[i],
            .tTargetSize = {(float)ptProbe->uResolution, (float)ptProbe->uResolution}
        };

        pl__renderer_probe_create_textures(ptScene, &tProbeData);
        pl__renderer_probe_create_buffers(ptScene, &tProbeData);
        pl__renderer_probe_create_bindgroups(ptScene, &tProbeData);
        pl__renderer_probe_update_bindgroups(ptScene, &tProbeData);
        tProbeData.uDataPackIndex = pl__renderer_probe_data_pack_index(ptScene, ptProbe->uResolution);
        tProbeData.iMips = (int)(uint32_t)floorf(log2f((float)ptProbe->uResolution)) - 3; // guarantee final dispatch during filtering is 16 threads
        pl_sb_push(ptScene->sbtProbeData, tProbeData);
    }
}

static void
pl__renderer_add_lights_to_scene(plScene* ptScene, uint32_t uCount, const plEntity* atLights)
{
    for(uint32_t i = 0; i < uCount; i++)
    {
        plRendererLight tRendererLight = {
            .tEntity = atLights[i]
        };

        plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, atLights[i]);

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {  
            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            {
                tRendererLight.ptShadowBufferOffset = gptFreeList->get_node(&ptScene->tShadowCameraFreeList, sizeof(plMat4) * 6);
            }
            pl_sb_push(ptScene->sbtPointLights, tRendererLight);
        }
        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {  
            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            {
                tRendererLight.ptShadowBufferOffset = gptFreeList->get_node(&ptScene->tShadowCameraFreeList, sizeof(plMat4));
            }
            pl_sb_push(ptScene->sbtSpotLights, tRendererLight);
        }
        else
        {
            pl_sb_push(ptScene->sbtDirectionLights, tRendererLight);
        }
    }
    
}

static void
pl__renderer_update_instance_buffer(plScene* ptScene)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "create shaders");

    plDevice* ptDevice  = gptData->ptDevice;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
    
    // update transform & instance buffers since we are now using indices
    plBuffer* ptInstanceBuffer = gptGfx->get_buffer(ptDevice, ptScene->tInstanceBuffer);

    const uint32_t uObjectCount = pl_sb_size(ptScene->sbuActiveDrawables);

    if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT)
    {
        for(uint32_t i = 0; i < uObjectCount; i++)
        {

            plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbuActiveDrawables[i]];
            plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[ptScene->sbuActiveDrawables[i]];

            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);

            // if using instancing, set index into instance buffer and
            // this includes setting the viewport instance data for multiviewport
            // shadow technique
            // ptDrawable->uInstanceIndex = uInstanceOffset;

            for(int32_t iViewport = 0; iViewport < 6; iViewport++)
            {
                for(uint32_t uInstance = 0; uInstance < ptDrawable->uInstanceCount; uInstance++)
                {
                    uint32_t uTransformIndex = ptScene->sbtDrawables[ptScene->sbuActiveDrawables[i]].uTransformIndex + uInstance;

                    plShadowInstanceBufferData tShadowInstanceData = {
                        .uTransformIndex = uTransformIndex,
                        .iViewportIndex  = iViewport
                    };
                    
                    memcpy(&ptInstanceBuffer->tMemoryAllocation.pHostMapped[(ptDrawable->uInstanceIndex + uInstance + ptDrawable->uInstanceCount * iViewport)  * sizeof(tShadowInstanceData)], &tShadowInstanceData, sizeof(tShadowInstanceData));
                }
            }
        }
    }
    else
    {
        for(uint32_t i = 0; i < uObjectCount; i++)
        {

            plDrawable* ptDrawable = &ptScene->sbtDrawables[ptScene->sbuActiveDrawables[i]];
            plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[ptScene->sbuActiveDrawables[i]];

            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);

            // if using instancing, set index into instance buffer and
            // this includes setting the viewport instance data for multiviewport
            // shadow technique
            for(int32_t iViewport = 0; iViewport < 6; iViewport++)
            {
                for(uint32_t uInstance = 0; uInstance < ptDrawable->uInstanceCount; uInstance++)
                {
                    uint32_t uTransformIndex = ptScene->sbtDrawables[ptScene->sbuActiveDrawables[i]].uTransformIndex + uInstance;

                    plShadowInstanceBufferData tShadowInstanceData = {
                        .uTransformIndex = uTransformIndex,
                        .iViewportIndex  = 0
                    };
                    
                    memcpy(&ptInstanceBuffer->tMemoryAllocation.pHostMapped[(ptDrawable->uInstanceIndex + uInstance + ptDrawable->uInstanceCount * iViewport) * sizeof(tShadowInstanceData)], &tShadowInstanceData, sizeof(tShadowInstanceData));
                }
            }
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_load_shaders(plScene* ptScene)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "create shaders");

    ptScene->tViewBGLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "view");
    ptScene->tShadowGlobalBGLayout = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "shadow");

    if(!(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_ACTIVE))
    {
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
        return;
    }

    plDevice* ptDevice = gptData->ptDevice;

    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "reload shaders for scene %s", ptScene->pcName);

    plShaderOptions tOriginalOptions = *gptShader->get_options();

    plShaderOptions tNewDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->set_options(&tNewDefaultShaderOptions);


    int aiLightingConstantData[] = {ptScene->tShaderDebugMode};

    plShaderVariantDesc tDeferredVariant = {
        .ptAttachmentInfo = &gptData->tDeferredLightingRenderPassLayout,
        .pFragmentConstants = aiLightingConstantData
    };
    
    if(ptScene->tShaderDebugMode)
        ptScene->tDirectionalLightingShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting_debug", &tDeferredVariant);
    else
    {
        ptScene->tSunShader                 = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting_sun", &tDeferredVariant);
        ptScene->tDirectionalLightingShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting_directional", &tDeferredVariant);
    }

    ptScene->tClusteredLightingShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting_clustered", &tDeferredVariant);
    ptScene->tSpotLightingShader      = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting_spot", &tDeferredVariant);
    ptScene->tPointLightingShader     = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting_point", &tDeferredVariant);
    ptScene->tProbeLightingShader     = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "deferred_lighting", &tDeferredVariant);

    plShaderVariantDesc tTerrainVariant = {
        .ptAttachmentInfo = &gptData->tRenderPassLayout,
        .pFragmentConstants = aiLightingConstantData,
    };
    if(ptScene->tShaderDebugMode)
        ptScene->tTerrainShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "terrain_debug", &tTerrainVariant);
    else
        ptScene->tTerrainShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "terrain", &tTerrainVariant);

    plShaderVariantDesc tShadowVariant = {
        .ptAttachmentInfo = &gptData->tDepthRenderPassLayout
    };
    ptScene->tTerrainShadowShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "terrain_shadow", &tShadowVariant);
    ptScene->tShadowShader        = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "shadow", &tShadowVariant);

    plShaderVariantDesc tTransparentVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout
    };
    ptScene->tSkyShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "sky", &tTransparentVariant);

    // compute
    ptScene->tSkinningShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "skinning", NULL);
    ptScene->tSunTransmissionLutShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "sky_transmission_lut", NULL);
    ptScene->tSunMultiscatterShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "sky_multiscatter_lut", NULL);
    ptScene->tSkyViewLutShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "sky_lut", NULL);
    ptScene->tSkyAerialLutShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "sky_aerial_lut", NULL);
    ptScene->tSkyAerialShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "sky_aerial", NULL);
    ptScene->tCloudShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "clouds", NULL);
    ptScene->tCloudLowResShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cloud_lowres", NULL);
    ptScene->tCloudCompositeShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cloud_composite", NULL);
    ptScene->tCloudTemporalShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cloud_temporal", NULL);
    ptScene->tCloudShadowShader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "cloud_shadow", NULL);


    plGraphicsState tAlphaShadowState = {
        .bDepthWriteEnabled  = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
        .eCullMode           = PL_CULL_MODE_NONE,
        .bWireframe          = 0,
        .bDepthClampEnabled  = 1,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP
    };

    plShaderVariantDesc tAlphaShadowVariant = {
        .ptAttachmentInfo = &gptData->tDepthRenderPassLayout,
        .ptGraphicsState = &tAlphaShadowState
    };
    ptScene->tShadowAlphaShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "alphashadow", &tAlphaShadowVariant);

    plGraphicsState tGBufferFileVariantTemp = {
        .bDepthWriteEnabled  = true,
        .eDepthMode          = PL_COMPARE_MODE_GREATER,
        .eCullMode           = PL_CULL_MODE_CULL_BACK,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP,
        .bWireframe          = false
    };

    plShaderVariantDesc tGBufferVariant = {
        .ptAttachmentInfo = &gptData->tDeferredLightingRenderPassLayout,
        .pVertexConstants = aiLightingConstantData,
        .pFragmentConstants = aiLightingConstantData,
        .ptGraphicsState = &tGBufferFileVariantTemp
    };

    if(ptScene->tShaderDebugMode)
    {
        ptScene->tGBufferFillShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill_debug", &tGBufferVariant);
        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tGBufferFillNoCullShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill_debug", &tGBufferVariant);

        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tGBufferFileVariantTemp.bWireframe = true;
        ptScene->tGBufferFillWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill_debug", &tGBufferVariant);
        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tGBufferFillNoCullWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill_debug", &tGBufferVariant);
    }
    else
    {
        tGBufferVariant.pFragmentConstants = NULL;
        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        // tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tGBufferFillShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill", &tGBufferVariant);
        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tGBufferFillNoCullShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill", &tGBufferVariant);

        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tGBufferFileVariantTemp.bWireframe = true;
        ptScene->tGBufferFillWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill", &tGBufferVariant);
        tGBufferFileVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tGBufferFillNoCullWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "gbuffer_fill", &tGBufferVariant);
    }

    plGraphicsState tForwardVariantTemp = {
        .bDepthWriteEnabled  = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
        .eCullMode           = PL_CULL_MODE_CULL_BACK,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP,
        .bWireframe          = false
    };

    plShaderVariantDesc tForwardVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout,
        .pVertexConstants = aiLightingConstantData,
        .pFragmentConstants = aiLightingConstantData,
        .ptGraphicsState = &tForwardVariantTemp
    };

    if(ptScene->tShaderDebugMode)
    {
        ptScene->tForwardShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);
        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tForwardNoCullShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tForwardVariantTemp.bWireframe = true;
        ptScene->tForwardWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);
        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tForwardNoCullWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tForwardVariantTemp.bWireframe = true;
        tForwardVariantTemp.bDepthWriteEnabled = false;
        ptScene->tForwardWireframeTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tForwardVariantTemp.bWireframe = false;
        ptScene->tForwardTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        tForwardVariantTemp.bWireframe = true;
        tForwardVariantTemp.bDepthWriteEnabled = false;
        ptScene->tForwardNoCullWireframeTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        tForwardVariantTemp.bWireframe = false;
        ptScene->tForwardNoCullTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tForwardVariant);
    }
    else
    {
        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        ptScene->tForwardShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);
        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tForwardNoCullShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tForwardVariantTemp.bWireframe = true;
        ptScene->tForwardWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);
        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        ptScene->tForwardNoCullWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);

        tForwardVariantTemp.bDepthWriteEnabled = false;
        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tForwardVariantTemp.bWireframe = true;
        tForwardVariantTemp.bDepthWriteEnabled = false;
        ptScene->tForwardWireframeTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_CULL_BACK;
        tForwardVariantTemp.bWireframe = false;
        ptScene->tForwardTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        tForwardVariantTemp.bWireframe = true;
        tForwardVariantTemp.bDepthWriteEnabled = false;
        ptScene->tForwardNoCullWireframeTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);

        tForwardVariantTemp.eCullMode = PL_CULL_MODE_NONE;
        tForwardVariantTemp.bWireframe = false;
        ptScene->tForwardNoCullTransparent = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tForwardVariant);
    }

    plShaderVariantDesc tFoliageVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout,
        .pVertexConstants = aiLightingConstantData,
        .pFragmentConstants = aiLightingConstantData,
        // .ptGraphicsState = &tForwardVariantTemp
    };

    ptScene->tFoliageShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "foliage", &tFoliageVariant);

    plGraphicsState tTransmissionVariantTemp = {
        .bDepthWriteEnabled  = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
        .eCullMode           = PL_CULL_MODE_NONE,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP,
        .bWireframe          = false
    };

    plShaderVariantDesc tTransmissionVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout,
        .pVertexConstants = aiLightingConstantData,
        .pFragmentConstants = aiLightingConstantData,
        .ptGraphicsState = &tTransmissionVariantTemp
    };

    if(ptScene->tShaderDebugMode)
    {
        ptScene->tTransmissionShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tTransmissionVariant);
        tTransmissionVariantTemp.bWireframe = true;
        ptScene->tTransmissionWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward_debug", &tTransmissionVariant);
    }
    else
    {
        ptScene->tTransmissionShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "transmission", &tTransmissionVariant);
        tTransmissionVariantTemp.bWireframe = true;
        ptScene->tTransmissionWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "transmission", &tTransmissionVariant);
    }

    plGraphicsState tOutlineVariantTemp = {
        .bDepthWriteEnabled  = 1,
        .bStencilTestEnabled = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
        .eCullMode           = PL_CULL_MODE_NONE,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_REPLACE,
        .eStencilOpDepthFail = PL_STENCIL_OP_REPLACE,
        .eStencilOpPass      = PL_STENCIL_OP_REPLACE,
        .bWireframe          = false
    };

    plShaderVariantDesc tOutlineVariant = {
        .ptAttachmentInfo = &gptData->tTransparentRenderPassLayout,
        .ptGraphicsState = &tOutlineVariantTemp
    };

    ptScene->tOutlineShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tOutlineVariant);
    tOutlineVariantTemp.eCullMode = PL_CULL_MODE_NONE;
    ptScene->tOutlineNoCullShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "forward", &tOutlineVariant);

    plGraphicsState tTerrainVariantTemp = {
        .bDepthWriteEnabled  = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER,
        .eCullMode           = PL_CULL_MODE_NONE,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP,
        .bWireframe          = 1
    };

    plShaderVariantDesc tTerrainWireframeVariant = {
        .ptAttachmentInfo = &gptData->tRenderPassLayout,
        .ptGraphicsState = &tTerrainVariantTemp,
    };
    ptScene->tTerrainWireframeShader = gptShaderLibrary->get_shader(ptScene->ptShaderLibrary, "terrain", &tTerrainWireframeVariant);
    
    gptShader->set_options(&tOriginalOptions);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_render_debug_view(plView* ptView, const plRenderViewDesc* ptViewDesc)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plScene* ptScene   = ptView->ptParentScene;
    const plCamera* ptCamera = ptViewDesc->ptCamera;

    if(ptView->bFreezeCameraNextFrame || !ptView->bFrozenCamera)
    {
        ptView->tCullCamera = *ptCamera;
        ptView->bFreezeCameraNextFrame = false;
    }

    const plCamera* ptCullCamera = ptView->bFrustumCulling ? &ptView->tCullCamera : NULL;

    // debug lights
    if(ptView->bDrawLightsDebug)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "draw lights");

        plLightComponent* ptLights = NULL;
        const uint32_t uLightCount = gptEcs->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);

        for(uint32_t i = 0; i < uLightCount; i++)
        {
            if(ptLights[i].tFlags & PL_LIGHT_FLAG_VISUALIZER)
            {
                const plVec4 tColor = {.rgb = ptLights[i].tColor, .a = 1.0f};
                if(ptLights[i].tType == PL_LIGHT_TYPE_POINT)
                {
                    plSphere tSphere = {
                        .fRadius = ptLights[i].fRadius,
                        .tCenter = ptLights[i].tPosition
                    };
                    gptDraw->add_3d_sphere(ptView->pt3DDrawList, tSphere, 6, 6, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.005f});
                    tSphere.fRadius = ptLights[i].fRange;
                    plSphere tSphere2 = {
                        .fRadius = ptLights[i].fRange,
                        .tCenter = ptLights[i].tPosition
                    };
                    gptDraw->add_3d_sphere(ptView->pt3DDrawList, tSphere2, 0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
                }
                else if(ptLights[i].tType == PL_LIGHT_TYPE_SPOT)
                {
                    plCone tCone0 = {
                        .fRadius = tanf(ptLights[i].fOuterConeAngle * 0.5f) * ptLights[i].fRange,
                        .tTipPos = ptLights[i].tPosition,
                        .tBasePos = pl_add_vec3(ptLights[i].tPosition, pl_mul_vec3_scalarf(ptLights[i].tDirection, ptLights[i].fRange))
                    };
                    gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});

                    if(ptLights[i].fInnerConeAngle > 0.0f)
                    {
                        plCone tCone1 = {
                            .fRadius = tanf(ptLights[i].fInnerConeAngle) * ptLights[i].fRange,
                            .tTipPos = ptLights[i].tPosition,
                            .tBasePos = pl_add_vec3(ptLights[i].tPosition, pl_mul_vec3_scalarf(ptLights[i].tDirection, ptLights[i].fRange))
                        };
                        gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone1, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
                    }
                }

                else if(ptLights[i].tType == PL_LIGHT_TYPE_DIRECTIONAL)
                {
                    plVec3 tDirection = pl_norm_vec3(ptLights[i].tDirection);
                    plCone tCone0 = {
                        .fRadius = 0.125f,
                        .tBasePos = (plVec3){0.0f, 3.0f, 0.0f},
                        .tTipPos = pl_add_vec3((plVec3){0.0f, 3.0f, 0.0f}, pl_mul_vec3_scalarf(tDirection, 0.25f))
                    };
                    gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
                    plCylinder tCylinder = {
                        .fRadius = 0.0625f,
                        .tBasePos = tCone0.tBasePos,
                        .tTipPos = pl_add_vec3((plVec3){0.0f, 3.0f, 0.0f}, pl_mul_vec3_scalarf(tDirection, -0.25f))
                    };
                    gptDraw->add_3d_cylinder(ptView->pt3DDrawList, tCylinder, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
                }
            }
        }
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    // debug bounding boxes
    if(ptView->bDrawBoundingBoxesDebug)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "draw bounding boxes");
        const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
            // plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
            plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];

            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);

            uint32_t uInstances = pl_sb_size(ptDrawableResources->sbtEntities);
            for(uint32_t j = 0; j < uInstances; j++)
            {
                plEntity tInstancedEntity = ptDrawableResources->sbtEntities[j];
                plObjectComponent* ptInstancedObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tInstancedEntity);
                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptInstancedObject->tAABB.tMin, ptInstancedObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.2f, 0.0f), .fThickness = 0.02f});
            }
            gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.2f, 0.8f, 0.0f), .fThickness = 0.02f});
        }
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    if(ptView->bDrawObjectNames)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "draw names");
        plEcsTypeKey tTagType = gptEcs->get_ecs_type_key_tag();
        const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
            // plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
            plDrawableResources* ptDrawableResources = &ptScene->sbtDrawableResources[uDrawableIndex];

            uint32_t uInstances = pl_sb_size(ptDrawableResources->sbtEntities);
            for(uint32_t j = 0; j < uInstances; j++)
            {
                plEntity tInstancedEntity = ptDrawableResources->sbtEntities[j];
                plTagComponent* ptTag = gptEcs->get_component(ptScene->ptComponentLibrary, tTagType, tInstancedEntity);
                plObjectComponent* ptInstancedObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tInstancedEntity);

                if(ptTag)
                    gptDraw->add_3d_text(ptView->pt3DDrawList, pl_aabb_center(&ptInstancedObject->tAABB), (plDrawTextOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.2f, 0.0f), .ptFont = gptUI->get_default_font()}, ptTag->pcName);
            }

            plTagComponent* ptTag = gptEcs->get_component(ptScene->ptComponentLibrary, tTagType, ptDrawableResources->tEntity);
            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptDrawableResources->tEntity);
            if(ptTag)
                gptDraw->add_3d_text(ptView->pt3DDrawList, pl_aabb_center(&ptObject->tAABB), (plDrawTextOptions){.uColor = PL_COLOR_32_RGB(0.2f, 0.8f, 0.0f), .ptFont = gptUI->get_default_font()}, ptTag->pcName);
        }
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    // debug bvh
    if(ptView->bDrawBvhDebug)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "draw BVH");

        plObjectComponent* ptComponents = NULL;
        gptEcs->get_components(ptScene->ptComponentLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

        plBVHNode* ptNode = NULL;
        uint32_t uLeafIndex = UINT32_MAX;
        while (gptBvh->traverse(&ptScene->tBvh, &ptNode, &uLeafIndex))
        {
            if(uLeafIndex != UINT32_MAX)
            {
                plObjectComponent* ptObject = &ptComponents[uLeafIndex];
                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.fThickness = 0.02f, .uColor = PL_COLOR_32_DARK_BLUE});
            }
            else
            {
                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptNode->tAABB.tMin, ptNode->tAABB.tMax, (plDrawLineOptions){.fThickness = 0.02f, .uColor = PL_COLOR_32_WHITE});
            }
        }
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    }

    if(ptView->bShowOrigin)
    {
        const plMat4 tTransform = pl_identity_mat4();
        gptDraw->add_3d_transform(ptView->pt3DDrawList, &tTransform, 10.0f, (plDrawLineOptions){.fThickness = 0.02f});
    }

    plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);

    if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_SHOW_VISUALIZER)
    {
        const plVec4 tColor = {.rgb = ptEnvironment->tSunColor, .a = 1.0f};
        plVec3 tDirection = pl_norm_vec3(ptEnvironment->tSunDirection);
        plCone tCone0 = {
            .fRadius = 0.125f,
            .tBasePos = pl_add_vec3((plVec3){0.0f, 3.0f, 0.0f}, pl_mul_vec3_scalarf(tDirection, -0.25f)),
            .tTipPos = (plVec3){0.0f, 3.0f, 0.0f}
        };
        gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
        plCylinder tCylinder = {
            .fRadius = 0.0625f,
            .tBasePos = tCone0.tBasePos,
            .tTipPos = pl_add_vec3(tCone0.tBasePos, pl_mul_vec3_scalarf(tDirection, -1.0f))
        };
        gptDraw->add_3d_cylinder(ptView->pt3DDrawList, tCylinder, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
    }

    if(ptView->bFrozenCamera)
    {
        const plDrawFrustumDesc tFrustumDesc = {
            .fAspectRatio = ptCullCamera->fAspectRatio,
            .fFarZ        = ptCullCamera->fFarZ,
            .fNearZ       = ptCullCamera->fNearZ,
            .fYFov        = ptCullCamera->fYFov
        };
        gptDraw->add_3d_frustum(ptView->pt3DSelectionDrawList, &ptCullCamera->tInvViewMat, tFrustumDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__renderer_update_sky_probe(plScene* ptScene)
{

    plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    const plEcsTypeKey tTransformComponentType =gptTransform->get_ecs_type_key_transform();

    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plDrawStream* ptStream = &gptData->tDrawStream;

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptScene->tDefaultProbeData.uDataPackIndex];
    plGpuProbe tProbeData = {
        .tPosition              = {0},
        .fRangeSqr              = FLT_MAX,
        .uGGXEnvSampler         = ptScene->tDefaultProbeData.uGGXEnvSampler,
        .uLambertianEnvSampler  = ptScene->tDefaultProbeData.uLambertianEnvSampler,
        .uCharlieEnvSampler     = ptScene->tDefaultProbeData.uSheenEnvSampler,
        .tMin.xyz               = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX},
        .tMax.xyz               = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX},
        .iMips                  = ptScene->tDefaultProbeData.iMips,
        .iParallaxCorrection    = 0
    };
    pl_sb_push(ptScene->sbtGPUProbeData, tProbeData);

    if(ptScene->tFlags & PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY)
    {

    }
    else
        return;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~common data~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plVec3 atPitchYawRoll[6] = {
        {    0.0f,  PL_PI_2,  0.0f },
        {    0.0f, -PL_PI_2,  0.0f },
        {  PL_PI_2,   PL_PI,  0.0f },
        { -PL_PI_2,   PL_PI,  0.0f },
        {    PL_PI,    0.0f, PL_PI },
        {     0.0f,    0.0f,  0.0f },
    };

    const plBindGroupUpdateSamplerData tSkyboxBG1SamplerData = {
        .tSampler = gptData->tSamplerLinearRepeat,
        .uSlot    = 1
    };

    // for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        ptScene->tDefaultProbeData.tViewData.tViewportSize = (plVec4){.x = ptScene->tDefaultProbeData.tTargetSize.x, .y = ptScene->tDefaultProbeData.tTargetSize.y, .z = 1.0f, .w = 1.0f};
        ptScene->tDefaultProbeData.tViewData.tCameraPos.xyz = (plVec3){0};
        ptScene->tDefaultProbeData.tViewData.fCameraRange  = 100000.0f - 0.26f;
        ptScene->tDefaultProbeData.tViewData.fCameraNearZ  = 0.26f;
        ptScene->tDefaultProbeData.tViewData.fAspectRatio  = 1.0f;

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~probe face pre-calc~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        plDrawArea tArea = {
            .ptDrawStream = ptStream,
            .atScissors = 
            {
                    {
                        .uWidth  = (uint32_t)ptScene->tDefaultProbeData.tTargetSize.x,
                        .uHeight = (uint32_t)ptScene->tDefaultProbeData.tTargetSize.x,
                    }
            },
            .atViewports =
            {
                    {
                        .fWidth  = (float)ptScene->tDefaultProbeData.tTargetSize.x,
                        .fHeight = (float)ptScene->tDefaultProbeData.tTargetSize.x,
                        .fMaxDepth = 1.0f
                    }
            }
        };
        

        plCamera atEnvironmentCamera[6] = {0};

        for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
        {
            gptCamera->init(&atEnvironmentCamera[uFaceIndex]);
            atEnvironmentCamera[uFaceIndex].eProjectionType = PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            atEnvironmentCamera[uFaceIndex].eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
            atEnvironmentCamera[uFaceIndex].tPosition   = (plVec3d){0};
            atEnvironmentCamera[uFaceIndex].fNearZ       = 0.26f;
            atEnvironmentCamera[uFaceIndex].fFarZ        = 100000.0f;
            atEnvironmentCamera[uFaceIndex].fYFov        = PL_PI_2;
            atEnvironmentCamera[uFaceIndex].fAspectRatio = 1.0f;
            gptCamera->set_euler(&atEnvironmentCamera[uFaceIndex], atPitchYawRoll[uFaceIndex].x, atPitchYawRoll[uFaceIndex].y, atPitchYawRoll[uFaceIndex].z);
            gptCamera->update(&atEnvironmentCamera[uFaceIndex]);

            ptScene->tDefaultProbeData.tViewData.tCameraProjection[uFaceIndex]     = atEnvironmentCamera[uFaceIndex].tProjMat;
            ptScene->tDefaultProbeData.tViewData.tCameraView[uFaceIndex]           = atEnvironmentCamera[uFaceIndex].tViewMat;
            ptScene->tDefaultProbeData.tViewData.tInvViewMatNoTranslation[uFaceIndex] = atEnvironmentCamera[uFaceIndex].tInvViewMatNoTranslation;
            ptScene->tDefaultProbeData.tViewData.tCameraProjectionInv[uFaceIndex]  = pl_mat4_invert(&atEnvironmentCamera[uFaceIndex].tProjMat);
            ptScene->tDefaultProbeData.tViewData.tCameraViewInv[uFaceIndex]        = pl_mat4_invert(&atEnvironmentCamera[uFaceIndex].tViewMat);
            ptScene->tDefaultProbeData.tViewData.tCameraViewProjection[uFaceIndex] = pl_mul_mat4(&atEnvironmentCamera[uFaceIndex].tProjMat, &atEnvironmentCamera[uFaceIndex].tViewMat);
        }

        // copy global buffer data for probe rendering
        plBuffer* ptProbeGlobalBuffer = gptGfx->get_buffer(ptDevice, ptScene->tDefaultProbeData.tViewBuffer);
        memcpy(ptProbeGlobalBuffer->tMemoryAllocation.pHostMapped, &ptScene->tDefaultProbeData.tViewData, sizeof(plGpuViewData));

        for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
        {

            if(ptEnvironment->eMode & PL_RENDERER_SKY_MODE_REALISTIC)
            {
                plCommandBuffer* ptSunCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "sun transmission");
                gptGfx->begin_command_recording(ptSunCmdBuffer);

                plPassResources tPassResources0 = {
                    .atTextures = {
                        {
                            .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                            .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                            .eStages = PL_PIPELINE_STAGE_COMPUTE,
                            .tHandle = ptScene->atSunTransmissionLut[uFrameIdx]
                        },
                        {
                            .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                            .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                            .eStages = PL_PIPELINE_STAGE_COMPUTE,
                            .tHandle = ptScene->atSunMultiscatterLut[uFrameIdx]
                        },
                        {
                            .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                            .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                            .eStages = PL_PIPELINE_STAGE_COMPUTE,
                            .tHandle = ptScene->tSkyProbeLut
                        }
                    }
                };

                gptGfx->begin_compute_pass(ptSunCmdBuffer, &tPassResources0);
                gptGfx->push_debug_group(ptSunCmdBuffer, "Sky LUT", (plVec4){0.33f, 0.42f, 0.70f, 1.0f});

                plDispatch tDispatch3 = {
                    .uGroupCountX = (uint32_t)ceilf(ptSettings->tSky.tSkyLutResolution.x / 8.0f),
                    .uGroupCountY = (uint32_t)ceilf(ptSettings->tSky.tSkyLutResolution.y / 8.0f),
                    .uGroupCountZ = 1,
                    .uThreadPerGroupX = 8,
                    .uThreadPerGroupY = 8,
                    .uThreadPerGroupZ = 1
                };

                // vertical blur
                gptGfx->bind_compute_shader(ptSunCmdBuffer, ptScene->tSkyViewLutShader);
                plBindGroupHandle atSkyLutBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptScene->tDefaultProbeData.tViewBG, ptScene->tSkyProbeLutBG2 };
                gptGfx->bind_compute_bind_groups(ptSunCmdBuffer, ptScene->tSkyViewLutShader, 0, PL_ARRAYSIZE(atSkyLutBGs), atSkyLutBGs, 0, NULL);

                gptGfx->dispatch(ptSunCmdBuffer, 1, &tDispatch3);

                gptGfx->pop_debug_group(ptSunCmdBuffer);
                gptGfx->end_compute_pass(ptSunCmdBuffer);

                gptGfx->end_command_recording(ptSunCmdBuffer);

                const plSubmitInfo tSunTransmissionSubmitInfo = {
                    .uWaitSemaphoreCount     = 1,
                    .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
                    .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                    .uSignalSemaphoreCount   = 1,
                    .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
                    .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
                };
                gptGfx->submit_command_buffer(ptSunCmdBuffer, &tSunTransmissionSubmitInfo);
                gptGfx->return_command_buffer(ptSunCmdBuffer);
            }

            plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "probe");
            gptGfx->begin_command_recording(ptCmdBuffer);

            uint32_t uFace = ptScene->tDefaultProbeData.uCurrentFace + uFaceIndex;
            PL_RENDERER_STAT("renderer / sky probe faces rendered", 1);
            uFace = uFace % 6;

            plRenderInfo tForwardRenderInfo = {
           .tRenderArea = {
                    .tMin = {0},
                    .tMax = ptScene->tDefaultProbeData.tTargetSize
                },
                .atColorAttachments = {
                    {
                        .tTexture       = ptScene->tDefaultProbeData.atRawOutputTextureViews[uFace],
                        .eLoadOp        = PL_LOAD_OP_CLEAR,
                        .eStoreOp       = PL_STORE_OP_STORE,
                        .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                        .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
                    }
                },
                .tDepthAttachment = {
                    .tTexture        = ptPack->atDepthTextureViews[uFace],
                    .eLoadOp         = PL_LOAD_OP_CLEAR,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                    .fClearZ         = 0.0f
                },
                .tStencilAttachment = {
                    .tTexture        = ptPack->atDepthTextureViews[uFace],
                    .eLoadOp         = PL_LOAD_OP_CLEAR,
                    .eStoreOp        = PL_STORE_OP_STORE,
                    .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                    .uClearStencil   = 0
                }
            };

            gptGfx->consumer_barrier(ptCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL);
            gptGfx->begin_render_pass(ptCmdBuffer, &tForwardRenderInfo, NULL);
            gptGfx->set_depth_bias(ptCmdBuffer, 0.0f, 0.0f, 0.0f);
            
            if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX && ptScene->tSkyboxTexture.uIndex != 0)
            {
                plMat4 tTransformMat = pl_identity_mat4();
                pl__render_view_skybox_pass(ptScene, ptCmdBuffer, ptScene->tDefaultProbeData.tViewBG, &tTransformMat, &tArea, uFace);  
            }
            else if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
            {
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(uint32_t));
                uint32_t* ptDynamicData = (uint32_t*)tDynamicBinding.pcData;
                *ptDynamicData = uFace;
                gptGfx->bind_shader(ptCmdBuffer, ptScene->tSkyShader);
                plBindGroupHandle atSkyBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptScene->tDefaultProbeData.tViewBG, ptScene->tSkyProbeBG2 };
                gptGfx->bind_graphics_bind_groups(ptCmdBuffer, ptScene->tSkyShader, 0, PL_ARRAYSIZE(atSkyBGs), atSkyBGs, 1, &tDynamicBinding);
                plDraw tDraw = {
                    .uInstanceCount = 1,
                    .uVertexCount = 3
                };
                gptGfx->draw(ptCmdBuffer, 1, &tDraw);
            }

            gptGfx->end_render_pass(ptCmdBuffer);
            if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
                pl__cloud_probe_composite(ptScene, &ptScene->tDefaultProbeData, ptPack, uFace, ptCmdBuffer);

            gptGfx->end_command_recording(ptCmdBuffer);

            const plSubmitInfo tProbeSubmitInfo = {
                .uWaitSemaphoreCount     = 1,
                .atWaitSempahores        = {tSemHandle},
                .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCmdBuffer, &tProbeSubmitInfo);
            gptGfx->wait_on_command_buffer(ptCmdBuffer);
            gptGfx->return_command_buffer(ptCmdBuffer);
        }
        
        pl__renderer_probe_create_environment_map(ptScene, &ptScene->tDefaultProbeData, 512);
    }
}

static void
pl_probe_selection_table_reset(plProbeSelectionTable* ptTable, bool bSkyAvailable)
{
    memset(ptTable->atEntries, 0, sizeof(ptTable->atEntries));
    ptTable->tLookup = (plHashMapStatic64){
        .auKeys        = ptTable->auKeys,
        .auValueBucket = ptTable->auValues,
        .uBucketCount  = PL_PROBE_SELECTION_HASH_SIZE
    };
    pl_hms_clear(&ptTable->tLookup);
    ptTable->uCount = 3;
    ptTable->uOverflowRequests = 0;
    ptTable->uInvalidRequests = 0;
    ptTable->atEntries[2].iCount = bSkyAvailable ? 1 : 0;
}

static uint32_t
pl_probe_selection_table_intern(plProbeSelectionTable* ptTable, plGpuProbeSelection tSelection)
{
    // Check before packing (including unused fields): truncation would alias
    // distinct selections. Invalid keys use the same safe fallback as overflow.
    if((uint32_t)tSelection.iProbe0 > UINT16_MAX ||
       (uint32_t)tSelection.iProbe1 > UINT16_MAX ||
       (uint32_t)tSelection.iProbe2 > UINT16_MAX ||
       (uint32_t)tSelection.iCount > 3u)
    {
        ptTable->uInvalidRequests++;
        return 2;
    }

    if(tSelection.iCount == 0)
        return 1;

    if(memcmp(&tSelection, &ptTable->atEntries[2], sizeof(tSelection)) == 0)
        return 2;

    // Exact key, not a hash: ordering and count are preserved. The upper 14
    // bits remain zero, so this cannot equal PL_DS_HASH_INVALID (UINT64_MAX).
    const uint64_t uKey = (uint64_t)tSelection.iProbe0 |
        ((uint64_t)tSelection.iProbe1 << 16) |
        ((uint64_t)tSelection.iProbe2 << 32) |
        ((uint64_t)tSelection.iCount << 48);
    const uint64_t uExisting = pl_hms_get(&ptTable->tLookup, uKey);
    if(uExisting != PL_DS_HASH_INVALID)
        return (uint32_t)uExisting;
    
    // Never evict an entry: earlier draws may already reference it.
    if(ptTable->uCount > PL_MAX_PROBE_SELECTION_INDEX)
    {
        ptTable->uOverflowRequests++;
        return 2;
    }

    uint32_t uIndex = ptTable->uCount++;
    if(ptTable->uCount - 3 > ptTable->uPeakEntries)
        ptTable->uPeakEntries = ptTable->uCount - 3;
    ptTable->atEntries[uIndex] = tSelection;
    pl_hms_set(&ptTable->tLookup, uKey, uIndex);
    return uIndex;
}

// Select at the object's world-space bounds center, with stable ties by probe
// index. The first probe is also the object's specular probe.
static plGpuProbeSelection
pl__renderer_select_probes(const plGpuProbe* ptProbes, uint32_t uProbeCount, plVec3 tPosition)
{
    int aiIndices[3] = {0};
    float afDistances[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    int iCount = 0;
    for(uint32_t i = 1; i < uProbeCount; i++)
    {
        plVec3 tDelta = pl_sub_vec3(ptProbes[i].tPosition, tPosition);
        float fDistanceSqr = pl_dot_vec3(tDelta, tDelta);
        if(fDistanceSqr > ptProbes[i].fRangeSqr)
            continue;

        for(int j = 0; j < 3; j++)
        {
            if(fDistanceSqr >= afDistances[j])
                continue;
            for(int k = 2; k > j; k--)
            {
                afDistances[k] = afDistances[k - 1];
                aiIndices[k] = aiIndices[k - 1];
            }
            afDistances[j] = fDistanceSqr;
            aiIndices[j] = (int)i;
            if(iCount < 3)
                iCount++;
            break;
        }
    }

    // Probe zero is the global sky fallback, when one is available.
    if(iCount == 0 && uProbeCount > 0)
        iCount = 1;
    return (plGpuProbeSelection){aiIndices[0], aiIndices[1], aiIndices[2], iCount};
}

static uint32_t
pl__renderer_intern_probe_selection(plScene* ptScene, plGpuProbeSelection tSelection)
{
    uint32_t uOldCount = ptScene->tProbeSelections.uCount;
    uint32_t uIndex = pl_probe_selection_table_intern(&ptScene->tProbeSelections, tSelection);
    if(ptScene->tProbeSelections.uCount != uOldCount)
    {
        PL_RENDERER_STAT("renderer / probe selections created", 1);
        plBuffer* ptBuffer = gptGfx->get_buffer(gptData->ptDevice, ptScene->atProbeSelectionBuffers[gptGfx->get_current_frame_index()]);
        ((plGpuProbeSelection*)ptBuffer->tMemoryAllocation.pHostMapped)[uIndex] = tSelection;
    }
    else if(uIndex == 2 && memcmp(&tSelection, &ptScene->tProbeSelections.atEntries[2], sizeof(tSelection)) != 0)
    {
        PL_RENDERER_STAT("renderer / probe selection fallback requests", 1);
        if(!ptScene->bProbeSelectionOverflowReported)
        {
            ptScene->bProbeSelectionOverflowReported = true;
            PL_LOG_WARN_API(gptLog, gptData->uLogChannel, "Probe selection table full or packed-key bounds exceeded; using sky fallback (warning once per scene)");
        }
        return uIndex | PL_PROBE_SELECTION_FALLBACK_FLAG;
    }
    return uIndex;
}

static void
pl__renderer_update_object_probes(plScene* ptScene)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plBuffer* ptBuffer = gptGfx->get_buffer(gptData->ptDevice, ptScene->atTransformBuffer[uFrameIdx]);
    plGpuTransform* ptTransforms = (plGpuTransform*)ptBuffer->tMemoryAllocation.pHostMapped;
    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtGPUProbeData);
    pl_probe_selection_table_reset(&ptScene->tProbeSelections, uProbeCount > 0);
    plBuffer* ptSelections = gptGfx->get_buffer(gptData->ptDevice, ptScene->atProbeSelectionBuffers[uFrameIdx]);
    memcpy(ptSelections->tMemoryAllocation.pHostMapped, ptScene->tProbeSelections.atEntries, 3 * sizeof(plGpuProbeSelection));
    
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbuActiveDrawables);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {
        uint32_t uDrawableIndex = ptScene->sbuActiveDrawables[i];
        plDrawable* ptDrawable = &ptScene->sbtDrawables[uDrawableIndex];
        plDrawableResources* ptResources = &ptScene->sbtDrawableResources[uDrawableIndex];
        const uint32_t uEntityCount = pl_sb_size(ptResources->sbtEntities);
        for(uint32_t j = 0; j <= uEntityCount; j++)
        {
            plEntity tEntity = j == 0 ? ptResources->tEntity : ptResources->sbtEntities[j - 1];
            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
            plVec3 tCenter = pl_mul_vec3_scalarf(pl_add_vec3(ptObject->tAABB.tMin, ptObject->tAABB.tMax), 0.5f);
            ptTransforms[ptDrawable->uTransformIndex + j].uProbeSelection = pl__renderer_intern_probe_selection(ptScene, pl__renderer_select_probes(ptScene->sbtGPUProbeData, uProbeCount, tCenter));
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] water simulation
//-----------------------------------------------------------------------------

// Per-scene, per-material spectral oceans. All work runs on the renderer's
// ordered graphics queue; compute barriers protect reuse across frames/views.


// Deterministic Phillips spectrum, in world units (gravity = 9.81 units/s^2).

static inline uint32_t
pl_ocean_hash(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

// Output: h0.real, h0.imaginary, angular frequency, unused. Unshifted FFT order.
// The L1 spectral bound normalizes the maximum possible vertical displacement
// to fft_height_limit, independently of resolution. Actual waves are lower.
static inline void
pl_ocean_build_spectrum(plMaterialWater water, plVec4* output)
{
    const int n = water.iFftResolution;
    const double tau = 6.283185307179586;
    const double dk = tau / water.fFftPatchSize;
    const double heading = water.fFftWindHeading * tau / 360.0;
    const double windX = cos(heading), windZ = sin(heading);
    const double length = (double)water.fFftWindSpeed * water.fFftWindSpeed / 9.81;
    double bound = 0;
    for(int z = 0; z < n; z++)
    {
        for(int x = 0; x < n; x++)
        {
            plVec4* h = &output[z * n + x];
            const plVec4 zero = {0};
            *h = zero;
            // Remove self-conjugate Nyquist axes for real horizontal displacement.
            if((x == 0 && z == 0) || x == n / 2 || z == n / 2 || length == 0)
                continue;
            const double kx = (x < n / 2 ? x : x - n) * dk;
            const double kz = (z < n / 2 ? z : z - n) * dk;
            const double k2 = kx * kx + kz * kz;
            const double directional = (kx * windX + kz * windZ) / sqrt(k2);
            double power = exp(-1.0 / (k2 * length * length)) * directional * directional / (k2 * k2);
            if(directional < 0)
                power *= water.fFftWindDependency;
            power *= exp(-k2 * water.fFftDamping * water.fFftDamping);
            const uint32_t seed = (uint32_t)water.iFftSeed ^ ((uint32_t)(z * n + x) * 0x9e3779b9u);
            const double u = ((pl_ocean_hash(seed) >> 8) + 1.0) / 16777217.0;
            const double v = (pl_ocean_hash(seed ^ 0x68bc21ebu) >> 8) / 16777216.0;
            const double radius = sqrt(-log(u) * power);
            h->x = (float)(radius * cos(tau * v));
            h->y = (float)(radius * sin(tau * v));
            h->z = (float)sqrt(9.81 * sqrt(k2));
            bound += 2.0 * hypot(h->x, h->y);
        }
    }
    const double scale = bound > 0 ? (double)water.fFftHeight * n * n / bound : 0;
    for(int i = 0; i < n * n; i++)
    {
        output[i].x = (float)(output[i].x * scale);
        output[i].y = (float)(output[i].y * scale);
    }
}

static void
pl__ocean_release(plScene* ptScene, plOcean* ptOcean)
{
    if(gptGfx->is_texture_valid(gptData->ptDevice, ptOcean->tDisplacement))
    {
        pl__renderer_return_bindless_texture_index(ptScene, ptOcean->tDisplacement);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptOcean->tDisplacement);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptOcean->tSpectrum);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptOcean->tScratch);
    }
    ptOcean->tDisplacement = (plTextureHandle){0};
}

static void
pl__ocean_cleanup(plScene* ptScene)
{
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtOceans); i++)
        pl__ocean_release(ptScene, &ptScene->sbtOceans[i]);
    pl_sb_free(ptScene->sbtOceans);
}

// Called during the existing material upload flush, before rendering can see
// updated parameters. Descriptor replacement therefore never races old frames.
// ptMaterial is the sanitized copy used by the entire GPU material upload.
static void
pl__ocean_update_material(plScene* ptScene, plAssetHandle tMaterial, const plMaterial* ptMaterial, plGpuWater* ptGPU)
{
    const plMaterialWater water = ptMaterial->tWater;
    ptGPU->tWaves = (plVec4){(float)water.iWaveMode, water.fGerstnerSteepness, water.fFftHeight, water.fFftChoppiness};
    ptGPU->tOcean = (plVec4){-1, water.fFftPatchSize, (float)water.iFftResolution, 0};
    plOcean* ptOcean = NULL;
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtOceans); i++)
    {
        if(ptScene->sbtOceans[i].tMaterial.uData == tMaterial.uData)
            ptOcean = &ptScene->sbtOceans[i];
    }
    if(ptMaterial->eMaterialModel != PL_MATERIAL_MODEL_WATER || water.iWaveMode != PL_WATER_WAVE_MODE_FFT)
    {
        if(ptOcean) pl__ocean_release(ptScene, ptOcean);
        return;
    }
    if(!ptOcean)
    {
        plOcean ocean = {.tMaterial = tMaterial};
        pl_sb_push(ptScene->sbtOceans, ocean);
        ptOcean = &pl_sb_back(ptScene->sbtOceans);
    }
    pl__ocean_release(ptScene, ptOcean);
    ptOcean->tSettings = water;
    const uint32_t n = (uint32_t)water.iFftResolution;
    plVec4* h0 = PL_ALLOC(sizeof(plVec4) * n * n);
    pl_ocean_build_spectrum(water, h0);
    double energy = 0.0;
    for(uint32_t i = 0; i < n * n; i++)
        energy += (double)h0[i].x * h0[i].x + (double)h0[i].y * h0[i].y;
    ptGPU->tOcean.w = (float)(3.0 * sqrt(2.0 * energy) / (n * n)); // crest reference: 3 RMS
    plBufferDesc buffer = {
        .eUsage      = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize  = sizeof(plVec4) * n * n,
        .pcDebugName = "ocean spectrum"
    };
    gptStarter->create_buffer(&buffer, h0, &ptOcean->tSpectrum);
    PL_FREE(h0);
    buffer.szByteSize = sizeof(plVec2) * n * n * 3;
    buffer.pcDebugName = "ocean FFT scratch";
    gptStarter->create_buffer(&buffer, NULL, &ptOcean->tScratch);
    const plTextureDesc texture = {
        .tDimensions = {(float)n, (float)n, 1}, .uLayers = 1, .uMips = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .eUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName = "ocean displacement"
    };
    gptStarter->create_texture(&texture, NULL, 0, &ptOcean->tDisplacement);
    ptOcean->uTextureIndex = pl__renderer_get_bindless_texture_index(ptScene, ptOcean->tDisplacement);
    ptGPU->tOcean.x = (float)ptOcean->uTextureIndex;
}

static void
pl__ocean_simulate(plScene* ptScene, plCommandBuffer* ptCmd)
{
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtOceans); i++)
    {
        plOcean* ocean = &ptScene->sbtOceans[i];
        if(!gptGfx->is_texture_valid(gptData->ptDevice, ocean->tDisplacement)) continue;
        if(!gptAsset->is_valid(ocean->tMaterial)) continue;
        const uint32_t n = (uint32_t)ocean->tSettings.iFftResolution;
        PL_RENDERER_STAT("renderer / ocean simulations", 1);
        PL_RENDERER_STAT("renderer / ocean FFT texels", (double)n * n);
        const plBindGroupDesc desc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderLibrary->get_bind_group_layout(ptScene->ptShaderLibrary, "ocean_fft"),
            .pcDebugName = "ocean FFT"
        };
        const plBindGroupHandle bg = gptGfx->create_bind_group(gptData->ptDevice, &desc);
        const plBindGroupUpdateData data = {
            .atBufferBindings = {
                {.uSlot = 0, .tBuffer = ocean->tSpectrum, .szBufferRange = sizeof(plVec4) * n * n},
                {.uSlot = 1, .tBuffer = ocean->tScratch, .szBufferRange = sizeof(plVec2) * n * n * 3}
            },
            .atTextureBindings = {
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ocean->tDisplacement}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, bg, &data);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, bg);
        const plComputeShaderHandle shader = gptShaderLibrary->get_compute_shader(ptScene->ptShaderLibrary, "ocean_fft", NULL);
        gptGfx->push_debug_group(ptCmd, "Ocean FFT", (plVec4){0.0f, 0.4f, 0.6f, 1.0f});
        for(uint32_t pass = 0; pass < 2; pass++)
        {
            const plPassResources resources = {
                .atBuffers = {
                    {.tHandle = ocean->tSpectrum, .eStages = PL_SHADER_STAGE_COMPUTE, .eUsage = PL_BUFFER_USAGE_STORAGE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ},
                    {.tHandle = ocean->tScratch, .eStages = PL_SHADER_STAGE_COMPUTE, .eUsage = PL_BUFFER_USAGE_STORAGE, .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE}
                },
                .atTextures = {
                    {.tHandle = ocean->tDisplacement, .eStages = PL_SHADER_STAGE_COMPUTE, .eUsage = PL_TEXTURE_USAGE_STORAGE, .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE}
                }
            };
            gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_COMPUTE, PL_BARRIER_SCOPE_ALL);
            gptGfx->begin_compute_pass(ptCmd, &resources);
            struct { uint32_t n, pass; float time, chop; } params = {n, pass, (float)gptIO->dTime * ocean->tSettings.fFftSpeed, ocean->tSettings.fFftChoppiness};
            plDynamicBinding dynamic = pl__allocate_dynamic_data(gptData->ptDevice, sizeof(params));
            memcpy(dynamic.pcData, &params, sizeof(params));
            gptGfx->bind_compute_shader(ptCmd, shader);
            gptGfx->bind_compute_bind_groups(ptCmd, shader, 0, 1, &bg, 1, &dynamic);
            const plDispatch dispatch = {
                .uGroupCountX     = n,
                .uGroupCountY     = 1,
                .uGroupCountZ     = 1,
                .uThreadPerGroupX = 256,
                .uThreadPerGroupY = 1,
                .uThreadPerGroupZ = 1
            };
            gptGfx->dispatch(ptCmd, 1, &dispatch);
            gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_COMPUTE, PL_PIPELINE_STAGE_COMPUTE | PL_PIPELINE_STAGE_VERTEX | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL);
            gptGfx->end_compute_pass(ptCmd);
        }
        gptGfx->pop_debug_group(ptCmd);
    }
}
