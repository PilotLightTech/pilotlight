/*
   pl_renderer_internal.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] job system tasks
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
    plDrawable tDrawable = ptCullData->atDrawables[tInvoData.uGlobalIndex];
    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
    ptCullData->atDrawables[tInvoData.uGlobalIndex].bCulled = true;

    if(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE)
    {
        if(ptCullData->atDrawables[tInvoData.uGlobalIndex].uInstanceCount == 1) // ignore instanced
        {
            if(gptGjk->pen(pl_gjk_support_aabb, &ptObject->tAABB, pl_gjk_support_frustum, &ptCullData->tFrustum, NULL))
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

static void
pl__renderer_cull_point_light_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
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
            if(gptGjk->pen(pl_gjk_support_aabb, &ptObject->tAABB, pl_gjk_support_sphere, &ptCullData->tSphere, NULL))
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

static void
pl__renderer_cull_spot_light_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
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
            if(gptGjk->pen(pl_gjk_support_aabb, &ptObject->tAABB, pl_gjk_support_cone, &ptCullData->tCone, NULL))
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
        const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
        for(uint32_t i = 0; i < uSkinCount; i++)
        {
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynSkinData));
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
            gptGfx->bind_compute_bind_groups(ptCommandBuffer, ptScene->sbtSkinData[i].tShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
            gptGfx->bind_compute_shader(ptCommandBuffer, ptScene->sbtSkinData[i].tShader);
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

    const uint32_t uProbeCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, (void**)&ptProbes, NULL);
    uint32_t uLightCount = pl_sb_size(ptScene->sbtDirectionLights);

    const plPackRect tSunPackRect = {
        .iWidth  = (int)ptScene->tLightingOptions.uSunResolution * (int)ptScene->tLightingOptions.uSunCascadeCount,
        .iHeight = (int)ptScene->tLightingOptions.uSunResolution,
        .iId     = 0
    };
    pl_sb_push(ptScene->sbtShadowViewRects, tSunPackRect);

    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        const plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[uLightIndex].tEntity);

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
            memcpy(&pcBufferStart[ptScene->sbtSpotLights[ptData->uLightIndex].uShadowBufferOffset], &tCamViewProjs, sizeof(plMat4));
        }    
    }

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

    // uint32_t uCameraBufferIndex = 0;
    for(uint32_t uLightIndex = 0; uLightIndex < uLightCount; uLightIndex++)
    {
        
        const plPackRect* ptRect = &ptScene->sbtShadowRects[uLightIndex];

        const plShadowPackData* ptData = &ptScene->sbtShadowRectData[ptRect->iId];


        plLightComponent* ptLight = NULL;
        if(ptData->tType == PL_LIGHT_TYPE_POINT)            ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[ptData->uLightIndex].tEntity);
        else if(ptData->tType == PL_LIGHT_TYPE_SPOT)        ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[ptData->uLightIndex].tEntity);

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
                    .fRadius = tanf(ptLight->fOuterConeAngle * 0.5f) * ptLight->fRange,
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
            .atDrawables  = ptScene->sbtDrawables,
            .tSphere      = {
                .fRadius = ptLight->fRange,
                .tCenter = ptLight->tPosition
            },
            .tCone = {
                .tTipPos = ptLight->tPosition,
                .fRadius = tanf(ptLight->fOuterConeAngle * 0.5f) * ptLight->fRange,
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
        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
            {
                plVisibleDrawable tVisibleDrawable = {
                    .uDrawableIndex = uDrawableIndex
                };
                if(tDrawable.tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                {
                    pl_sb_push(ptScene->sbtVisibleDrawables1, tVisibleDrawable);
                }
                else if(!(tDrawable.tFlags & PL_DRAWABLE_FLAG_PROBE))
                {
                    pl_sb_push(ptScene->sbtVisibleDrawables0, tVisibleDrawable);
                }
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables0);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables1);

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {

            const uint32_t uCameraBufferIndex = ptScene->sbtPointLights[ptData->uLightIndex].uShadowBufferOffset / sizeof(plMat4);

            if(ptScene->tShadowOptions.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT)
            {

                gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                gptGfx->set_depth_bias(ptCommandBuffer, ptScene->tShadowOptions.fConstantDepthBias, 0.0f, ptScene->tShadowOptions.fSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];

                    if(tDrawable.uInstanceCount != 0)
                    {

                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

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
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];

                    if(tDrawable.uInstanceCount != 0)
                    {

                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCameraBufferIndex;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader        = ptScene->sbtShadowShaders[ptScene->sbtVisibleDrawables1[i].uDrawableIndex],
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

                gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
            }
            else
            {
                for(uint32_t uFaceIndex = 0; uFaceIndex < 6; uFaceIndex++)
                {

                    gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
                    gptGfx->set_depth_bias(ptCommandBuffer, ptScene->tShadowOptions.fConstantDepthBias, 0.0f, ptScene->tShadowOptions.fSlopeDepthBias);
                    *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                    for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                    {
                        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];

                        if(tDrawable.uInstanceCount != 0)
                        {
                            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                            
                            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

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
                        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                        if(tDrawable.uInstanceCount != 0)
                        {
                            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                            
                            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                            plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                            ptDynamicData->iIndex = (int)uCameraBufferIndex + uFaceIndex;

                            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                            {
                                .tShader        = ptScene->sbtShadowShaders[ptScene->sbtVisibleDrawables1[i].uDrawableIndex],
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

                    gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
                }
            }
        }

        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {

            const uint32_t uCameraBufferIndex = ptScene->sbtSpotLights[ptData->uLightIndex].uShadowBufferOffset / sizeof(plMat4);

            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptCommandBuffer, ptScene->tShadowOptions.fConstantDepthBias, 0.0f, ptScene->tShadowOptions.fSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];
                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

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
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = (int)uCameraBufferIndex;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = ptScene->sbtShadowShaders[ptScene->sbtVisibleDrawables1[i].uDrawableIndex],
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

    int aiConstantData[] = {0, 0};
    plShaderHandle tShadowShader = gptShaderVariant->get_shader("shadow", NULL, aiConstantData, aiConstantData, &gptData->tDepthRenderPassLayout);

    const float g = 1.0f / tanf(ptSceneCamera->fYFov / 2.0f);
    const float s = ptSceneCamera->fAspectRatio;

    float fShadowFarZ = pl_min(ptSceneCamera->fFarZ, ptScene->tShadowOptions.fMaxShadowRange);

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

        plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptRect->iId].tEntity);

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
            gptGfx->set_depth_bias(ptCommandBuffer, ptScene->ptTerrain->tRuntimeOptions.fTerrainShadowConstantDepthBias, 0.0f, ptScene->ptTerrain->tRuntimeOptions.fTerrainShadowSlopeDepthBias);
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
                pl__render_chunk_shadow(ptScene, ptScene->ptTerrain, &tShadowCamera, ptCommandBuffer, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, &ptShadowData->viewProjMat, 0);
        }

        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, tInfo.tDLightShadowDataBuffer);
        memcpy(&ptDShadowDataBuffer->tMemoryAllocation.pHostMapped[iShadowIndex * sizeof(plGpuDirectionLightShadow)], ptShadowData, sizeof(plGpuDirectionLightShadow));
    }

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    for(uint32_t uRectIndex = 1; uRectIndex < uAtlasRectCount; uRectIndex++)
    {
        const plPackRect* ptRect = &ptScene->sbtShadowViewRects[uRectIndex];

        const plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[ptRect->iId].tEntity);


        int iShadowIndex = ptScene->sbtDirectionLightData[ptRect->iId].iShadowIndex;

        plAtomicCounter* ptCullCounter = NULL;
        pl_sb_reset(ptScene->sbtVisibleDrawables0);
        pl_sb_reset(ptScene->sbtVisibleDrawables1);
        plCullData tCullData = {
            .ptScene      = ptScene,
            .atDrawables  = ptScene->sbtDrawables,
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

        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
            {
                plVisibleDrawable tVisibleDrawable = {
                    .uDrawableIndex = uDrawableIndex
                };
                if(tDrawable.tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                {
                    pl_sb_push(ptScene->sbtVisibleDrawables1, tVisibleDrawable);
                }
                else if(!(tDrawable.tFlags & PL_DRAWABLE_FLAG_PROBE))
                {
                    pl_sb_push(ptScene->sbtVisibleDrawables0, tVisibleDrawable);
                }
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables0);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables1);

        gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
        gptGfx->set_depth_bias(ptCommandBuffer, ptScene->tShadowOptions.fConstantDepthBias, 0.0f, ptScene->tShadowOptions.fSlopeDepthBias);
        *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
        for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];

            if(tDrawable.uInstanceCount != 0)
            {
                plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)iShadowIndex;

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

        *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
        for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];

            if(tDrawable.uInstanceCount != 0)
            {
                plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                ptDynamicData->iIndex = (int)iShadowIndex;

                pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                {
                    .tShader        = ptScene->sbtShadowShaders[ptScene->sbtVisibleDrawables1[i].uDrawableIndex],
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

    float fShadowLambda = 0.6f;
    uint32_t uShadowResolution = (uint32_t)ptScene->sbtShadowViewRects[0].iHeight;
    int iX = ptScene->sbtShadowViewRects[0].iX;
    int iY = ptScene->sbtShadowViewRects[0].iY;
    const uint32_t uCascadeCount = bProbe ? 1 : ptScene->tSceneData.iCascadeCount;

    // for convience
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    int aiConstantData[] = {0, 0};
    plShaderHandle tShadowShader = gptShaderVariant->get_shader("shadow", NULL, aiConstantData, aiConstantData, &gptData->tDepthRenderPassLayout);

    const float g = 1.0f / tanf(ptSceneCamera->fYFov / 2.0f);
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

    float fShadowFarZ = pl_min(ptSceneCamera->fFarZ, ptScene->tShadowOptions.fMaxShadowRange);

    // const uint32_t uAtlasRectCount = pl_sb_size(ptScene->sbtShadowRects);
    // for(uint32_t uRectIndex = 0; uRectIndex < uAtlasRectCount; uRectIndex++)
    {



        plMat4 atCamViewProjs[PL_MAX_SHADOW_CASCADES] = {0};
        float fLastSplitDist = ptSceneCamera->fNearZ;
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

                float logSplit =
                    ptSceneCamera->fNearZ * powf(fShadowFarZ / ptSceneCamera->fNearZ, p);

                float linSplit =
                    ptSceneCamera->fNearZ + (fShadowFarZ - ptSceneCamera->fNearZ) * p;

                float di = fShadowLambda * logSplit + (1.0f - fShadowLambda) * linSplit;

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
            const float fUnitPerTexel = fDim / (float)uShadowResolution;
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
            tShadowCamera.fAspectRatio   = 1.0f;

            gptCamera->update(&tShadowCamera);

            // direct stable view matrix construction (no look_at)
            tShadowCamera.tViewMat = tStableLightView;
            tShadowCamera.tViewMat.col[3].x = -pl_dot_vec3(tLightRight,   tSnappedCenterWS);
            tShadowCamera.tViewMat.col[3].y = -pl_dot_vec3(tLightUp,      tSnappedCenterWS);
            tShadowCamera.tViewMat.col[3].z = -pl_dot_vec3(tLightForward, tSnappedCenterWS);

            

            // if update() overwrites tViewMat in your camera system, move the
            // tViewMat assignment to after update() and then rebuild viewProj here.

            atCamViewProjs[uCascade] = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
            tShadowCamera.tViewProjMat = atCamViewProjs[uCascade];
            ptViewData->viewProjMat[uCascade] = atCamViewProjs[uCascade];
            fLastSplitDist = fSplitDist;

            char* pcBufferStart = gptGfx->get_buffer(ptDevice, tSunCameraBuffer)->tMemoryAllocation.pHostMapped;
            memcpy(&pcBufferStart[uCascade * sizeof(plMat4)], &atCamViewProjs[uCascade], sizeof(plMat4));

            if(ptScene->ptTerrain)
            {
   
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
                gptGfx->set_depth_bias(ptCommandBuffer, ptScene->ptTerrain->tRuntimeOptions.fTerrainShadowConstantDepthBias, 0.0f, ptScene->ptTerrain->tRuntimeOptions.fTerrainShadowSlopeDepthBias);
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
                    pl__render_chunk_shadow(ptScene, ptScene->ptTerrain, &tShadowCamera, ptCommandBuffer, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, &ptViewData->viewProjMat[uCascade], 0);
            }
        }
    }

    plBuffer* ptViewBuffer = gptGfx->get_buffer(ptDevice, tViewBuffer);
    memcpy(ptViewBuffer->tMemoryAllocation.pHostMapped, ptViewData, sizeof(plGpuViewData));

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    {

        plAtomicCounter* ptCullCounter = NULL;
        pl_sb_reset(ptScene->sbtVisibleDrawables0);
        pl_sb_reset(ptScene->sbtVisibleDrawables1);
        plCullData tCullData = {
            .ptScene      = ptScene,
            .atDrawables  = ptScene->sbtDrawables,
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

        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
            {
                plVisibleDrawable tVisibleDrawable = {
                    .uDrawableIndex = uDrawableIndex
                };
                if(tDrawable.tFlags & PL_DRAWABLE_FLAG_HAS_ALPHA)
                {
                    pl_sb_push(ptScene->sbtVisibleDrawables1, tVisibleDrawable);
                }
                else if(!(tDrawable.tFlags & PL_DRAWABLE_FLAG_PROBE))
                {
                    pl_sb_push(ptScene->sbtVisibleDrawables0, tVisibleDrawable);
                }
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables0);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtVisibleDrawables1);


        if(ptScene->tShadowOptions.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT)
        {
            gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);
            gptGfx->set_depth_bias(ptCommandBuffer, ptScene->tShadowOptions.fConstantDepthBias, 0.0f, ptScene->tShadowOptions.fSlopeDepthBias);
            *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];

                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = 0;

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
                            tBindGroup
                        },
                        .auDynamicBufferOffsets = {
                            tDynamicBinding.uByteOffset
                        },
                        .uInstanceOffset = tDrawable.uInstanceIndex,
                        .uInstanceCount = uCascadeCount * tDrawable.uInstanceCount
                    });
                }
            }

            *gptData->pdDrawCalls += (double)uVisibleTransparentDrawCount;
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];
                if(tDrawable.uInstanceCount != 0)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                    plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                    
                    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                    plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                    ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                    ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                    ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                    ptDynamicData->iIndex = 0;

                    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                    {
                        .tShader        = ptScene->sbtShadowShaders[ptScene->sbtVisibleDrawables1[i].uDrawableIndex],
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
                            tBindGroup
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
                        .fWidth  = uCascadeCount > 1 ? (float)uShadowResolution : 0.0f,
                        .fHeight = (float)uShadowResolution,
                        .fMaxDepth = 1.0f
                    },
                    {
                        .fX = (float)(iX + 3 * uShadowResolution),
                        .fY = (float)iY,
                        .fWidth  = uCascadeCount > 1 ? (float)uShadowResolution : 0.0f,
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
                gptGfx->set_depth_bias(ptCommandBuffer, ptScene->tShadowOptions.fConstantDepthBias, 0.0f, ptScene->tShadowOptions.fSlopeDepthBias);
                *gptData->pdDrawCalls += (double)uVisibleOpaqueDrawCount;
                for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
                {
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables0[i].uDrawableIndex];

                    if(tDrawable.uInstanceCount != 0)
                    {
                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCascade;

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
                                tBindGroup
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
                    const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables1[i].uDrawableIndex];

                    if(tDrawable.uInstanceCount != 0)
                    {
                        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
                        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
                        
                        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynShadow));

                        plGpuDynShadow* ptDynamicData = (plGpuDynShadow*)tDynamicBinding.pcData;
                        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
                        ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
                        ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
                        ptDynamicData->iIndex = (int)uCascade;

                        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
                        {
                            .tShader        = ptScene->sbtShadowShaders[ptScene->sbtVisibleDrawables1[i].uDrawableIndex],
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
                                tBindGroup
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
        .tLayout     = gptShaderVariant->get_bind_group_layout("deferred lighting 1"),
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

static uint64_t
pl__renderer_add_material_to_scene(plScene* ptScene, plEntity tMaterial)
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

        ptScene->uMaterialDirtyValue = 1;
        gptStage->stage_buffer_upload(ptScene->tMaterialDataBuffer,
            ptScene->sbtMaterialNodes[uMaterialIndex]->uOffset,
            &tGPUMaterial,
            sizeof(plGpuMaterial));

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_SHEEN)
            ptScene->tFlags |= PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED;

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_TRANSMISSION || ptMaterial->tFlags & PL_MATERIAL_FLAG_VOLUME || ptMaterial->tFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
            ptScene->tFlags |= PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED;
        pl_hm_insert(&ptScene->tMaterialHashmap, tMaterial.uData, uMaterialIndex);

        gptStage->flush();

        if(ptScene->uMaterialDirtyValue > 0)
        {
            const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();

            const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
            for(uint32_t i = 0; i < uDrawableCount; i++)
            {
                plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);
                plMeshComponent* ptMesh = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);

                if(pl_hm_has_key(&ptScene->tMaterialHashmap, ptMesh->tMaterial.uData))
                {
                    uint32_t uMaterialIndex2 = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, ptMesh->tMaterial.uData);
                    ptScene->sbtDrawables[i].uMaterialIndex = (uint32_t)ptScene->sbtMaterialNodes[uMaterialIndex2]->uOffset / sizeof(plGpuMaterial);
                }
            }
            ptScene->uMaterialDirtyValue = 0;
        }

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
        if(ptSkinComponent->_atTextureData)
        {
            PL_FREE(ptSkinComponent->_atTextureData);
            ptSkinComponent->_atTextureData = NULL;
        }
        ptSkinComponent->_atTextureData = PL_ALLOC(ptSkinComponent->uJointCount * 8 * sizeof(plMat4));
        memset(ptSkinComponent->_atTextureData, 0, ptSkinComponent->uJointCount * 8 * sizeof(plMat4));
        tSkinData.ptFreeListNode = gptFreeList->get_node(&ptScene->tSkinBufferFreeList, ptSkinComponent->uJointCount * 8 * sizeof(plMat4));

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
pl__renderer_probe_create_environment_map(plScene* ptScene, plEnvironmentProbeData* ptProbe)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();
    plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptProbe->uDataPackIndex];

    
    plComputeShaderHandle tCubeFilterSpecularShader = gptShaderVariant->get_compute_shader("cube_filter_specular", NULL);
    plComputeShaderHandle tCubeFilterDiffuseShader = gptShaderVariant->get_compute_shader("cube_filter_diffuse", NULL);
    plComputeShaderHandle tCubeFilterSheenShader = gptShaderVariant->get_compute_shader("cube_filter_sheen", NULL);

    plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptProbe->tRawOutputTexture);
    const int iResolution = (int)(ptTexture->tDesc.tDimensions.x);
    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);

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
            memcpy(&ptReadbackBuffer->tMemoryAllocation.pHostMapped[uStageOffset], tChain.atLevels[i].pData, tChain.atLevels[i].szFaceStride);
            

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
        .tLayout     = gptShaderVariant->get_bind_group_layout("cube_filter_set_0"),
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
        .tLayout     = gptShaderVariant->get_bind_group_layout("cube_filter_set_1"),
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
        ptDynamicData->iSampleCount = (int)ptProbeComp->uSamples;
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
            ptDynamicData->iSampleCount = i == 0 ? 1 : (int)ptProbeComp->uSamples;
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
    if(ptScene->tFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED)
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
            ptDynamicData->iSampleCount = i == 0 ? 1 : (int)ptProbeComp->uSamples;
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
    uint32_t*         sbuVisibleDeferredEntities;
    uint32_t          uGlobalIndex;
    plBindGroupHandle tBG2;
    plDrawArea*       ptArea;
    plShaderHandle*   sbtShaders;
} plGbufferFillPassInfo;

static void
pl__render_view_gbuffer_fill_pass(plScene* ptScene, plCommandBuffer* ptCommandBuffer, plGbufferFillPassInfo* ptInfo)
{
    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    gptGfx->push_debug_group(ptCommandBuffer, "G-Buffer Fill", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});
    gptGfx->set_depth_bias(ptCommandBuffer, 0.0f, 0.0f, 0.0f);

    const uint32_t uVisibleDeferredDrawCount = pl_sb_size(ptInfo->sbuVisibleDeferredEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleDeferredDrawCount);

    for(uint32_t i = 0; i < uVisibleDeferredDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptInfo->sbuVisibleDeferredEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynData));
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

    gptGfx->push_debug_group(ptCommandBuffer, "Lights", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    const uint32_t uPointLightCount = pl_sb_size(ptScene->sbtPointLightData);
    const uint32_t uSpotLightCount = pl_sb_size(ptScene->sbtSpotLightData);
    const uint32_t uDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLightData);

    if(ptScene->tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS)
    {
        gptGfx->reset_draw_stream(ptStream, uPointLightCount + uSpotLightCount);
        for(uint32_t uLightIndex = 0; uLightIndex < uPointLightCount; uLightIndex++)
        {
            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
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

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
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
        gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);

        gptGfx->reset_draw_stream(ptStream, uDirectionLightCount);
        for(uint32_t uLightIndex = 0; uLightIndex < uDirectionLightCount; uLightIndex++)
        {

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
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
        gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);

        // sun
        gptGfx->reset_draw_stream(ptStream, 1);
        {

            plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
            plGpuDynDeferredLighting* ptLightingDynamicData = (plGpuDynDeferredLighting*)tLightingDynamicData.pcData;
            ptLightingDynamicData->uGlobalIndex = ptInfo->uGlobalIndex;
            ptLightingDynamicData->iProbe = (int)ptInfo->bProbe;
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

    if(uProbeCount > 0 && !ptInfo->bProbe && ptScene->tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED)
    {
        gptGfx->push_debug_group(ptCommandBuffer, "Probes", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});
        gptGfx->reset_draw_stream(ptStream, 1);
        plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynDeferredLighting));
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

    gptGfx->reset_draw_stream(ptStream, 1);
    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
    {
        .tShader        = gptShaderVariant->get_shader("skybox", NULL, NULL, NULL, &gptData->tTransparentRenderPassLayout),
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

typedef struct _plForwardPassInfo
{
    uint32_t* sbuVisibleEntities;
    uint32_t uGlobalIndex;
    plShaderHandle* sbtShaders;
    plDrawArea* ptArea;
    bool bProbe;
} plForwardPassInfo;

static void
pl__render_view_forward_pass(plScene* ptScene, plCommandBuffer* ptCommandBuffer, plBindGroupHandle tViewBG, plForwardPassInfo* ptInfo)
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
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynForwardData));

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
    gptGfx->draw_stream(ptCommandBuffer, 1, ptInfo->ptArea);
}


static plCommandBuffer*
pl__render_view_full_screen_blit(plView* ptView)
{
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();

    plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "main scene blit");
    gptGfx->begin_command_recording(ptSceneCmdBuffer);

    gptGfx->begin_compute_pass(ptSceneCmdBuffer, NULL);

    plImageCopy tFrameCopy = {

        .uSourceExtentX = (uint32_t)ptView->tTargetSize.x,
        .uSourceExtentY = (uint32_t)ptView->tTargetSize.y,
        .uSourceExtentZ = 1,
        .uSourceLayerCount = 1,
        .uDestinationLayerCount = 1,
    };
    gptGfx->copy_texture(ptSceneCmdBuffer, ptView->tRawOutputTexture, ptView->tTransmissionTexture, 1, &tFrameCopy);
    gptGfx->generate_mipmaps(ptSceneCmdBuffer, ptView->tTransmissionTexture);
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


    gptGfx->push_debug_group(ptCommandBuffer, "Transmission Pass", (plVec4){0.33f, 0.20f, 0.10f, 1.0f});

    const uint32_t uVisibleTransmissionDrawCount = pl_sb_size(ptScene->sbuVisibleTransmissionEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleTransmissionDrawCount);
    for(uint32_t i = 0; i < uVisibleTransmissionDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbuVisibleTransmissionEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynForwardData));

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
                .tShader        = ptScene->sbtRegularShaders[ptScene->sbuVisibleTransmissionEntities[i]],
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
    gptGfx->draw_stream(ptCommandBuffer, 1, &tArea);
    gptGfx->pop_debug_group(ptCommandBuffer);
}

static void
pl__render_view_grid_pass(plView* ptView, plCommandBuffer* ptCommandBuffer, const plCamera* ptCamera)
{
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

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

    plShaderHandle tGridShader = gptShaderVariant->get_shader("grid", NULL, NULL, NULL, &gptData->tTransparentRenderPassLayout);
    gptGfx->bind_shader(ptCommandBuffer, tGridShader);

    plDynamicBinding tGridDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynGrid));
    plGpuDynGrid* ptGridDynamicData = (plGpuDynGrid*)tGridDynamicBinding.pcData;
    const float fGridFactor = pl_squaref(ptCamera->fFarZ) - pl_squaref(ptCamera->tPositionF.y);
    ptGridDynamicData->fGridSize = fGridFactor > 0.0f ? sqrtf(fGridFactor) : 100.0f;
    ptGridDynamicData->fGridCellSize = ptView->tEditorOptions.fGridCellSize;
    ptGridDynamicData->fGridMinPixelsBetweenCells = ptView->tEditorOptions.fGridMinPixelsBetweenCells;
    ptGridDynamicData->tGridColorThin = ptView->tEditorOptions.tGridColorThin;
    ptGridDynamicData->tGridColorThick = ptView->tEditorOptions.tGridColorThick;
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

    // bounding boxes
    const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlinedEntities);
    if(uOutlineDrawableCount > 0 && ptView->tEditorOptions.bShowSelectedBoundingBox)
    {
        const plVec4 tOutlineColor = (plVec4){0.0f, (float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 1.0f};
        for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtOutlinedEntities[i]);
            gptDraw->add_3d_aabb(ptView->pt3DSelectionDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tOutlineColor), .fThickness = 0.01f});
            
        }
    }

    if(ptScene->tDebugOptions.bShowOrigin)
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
            .fYFov        = ptCullCamera->fYFov
        };
        gptDraw->add_3d_frustum(ptView->pt3DSelectionDrawList, &ptCullCamera->tInvViewMat, tFrustumDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
    }

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
        const plDrawable tDrawable = ptScene->sbtDrawables[ptScene->sbtVisibleDrawables[i]];

        uint32_t uId = tDrawable.tEntity.uIndex;
        
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
        
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynPick));
        plGpuDynPick* ptDynamicData = (plGpuDynPick*)tDynamicBinding.pcData;
        
        ptDynamicData->uID = uId;
        ptDynamicData->tModel = ptTransform->tWorld;
        ptDynamicData->tMousePos.xy = tMousePos;
        
        gptGfx->bind_graphics_bind_groups(ptSceneCmdBuffer, tPickShader, 0, 0, NULL, 1, &tDynamicBinding);

        if(tDrawable.uIndexCount > 0)
        {
            plDrawIndex tDraw = {
                .tIndexBuffer   = ptScene->tIndexBuffer,
                .uIndexCount    = tDrawable.uIndexCount,
                .uIndexStart    = tDrawable.uIndexOffset,
                .uInstanceCount = 1
            };
            gptGfx->draw_indexed(ptSceneCmdBuffer, 1, &tDraw);
        }
        else
        {
            plDraw tDraw = {
                .uVertexStart   = tDrawable.uVertexOffset,
                .uInstanceCount = 1,
                .uVertexCount   = tDrawable.uVertexCount
            };
            gptGfx->draw(ptSceneCmdBuffer, 1, &tDraw);
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
    plShaderHandle tUVShader = gptShaderVariant->get_shader("uvmap", NULL, NULL, NULL, &gptData->tUVRenderPassLayout);
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
    uint32_t uHalfWidth = ptView->tEditorOptions.uOutlineWidth / 2;
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
    ptDynamicData->fTargetWidth = (float)ptView->tEditorOptions.uOutlineWidth * tOutlineColor.r + 1.0f;
    ptDynamicData->tOutlineColor = tOutlineColor;
    plVec2 tUVScale = {0};
    pl_renderer_get_view_color_bind_group(ptView, &tUVScale);
    ptDynamicData->fXScale = tUVScale.x;
    ptDynamicData->fYScale = tUVScale.y;

    plShaderHandle tTonemapShader = gptShaderVariant->get_shader("jumpfloodalgo2", NULL, NULL, NULL, &gptData->tPostProcessRenderPassLayout);
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

static void
pl__render_view_bloom_pass(plView* ptView)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "bloom");

    // for convience
    // const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plScene*       ptScene   = ptView->ptParentScene;

    for(uint32_t i = 0; i < ptView->tBloomOptions.uChainLength - 1; i++)
    {

        const int sourceMip = i;
        const int targetMip = i + 1;

        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("bloom_downsample", 0),
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

        plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("bloom_downsample", NULL);
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

    for(uint32_t i = 0; i < ptView->tBloomOptions.uChainLength - 1; i++)
    {

        const int targetMip = ptView->tBloomOptions.uChainLength - 2 - i;
        const int sourceMip = targetMip + 1;

        const plBindGroupDesc tTonemapBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("bloom_upsample", 0),
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
        ptTonemapData->bloomStrength = ptView->tBloomOptions.fStrength;
        ptTonemapData->blurRadius = ptView->tBloomOptions.fRadius;
        ptTonemapData->isLowestMip = i == 0 ? 1 : 0;

        plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("bloom_upsample", NULL);
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
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("bloom_apply", 0),
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
        ptTonemapData->bloomStrength = ptView->tBloomOptions.fStrength;
        ptTonemapData->blurRadius = ptView->tBloomOptions.fRadius;

        plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("bloom_apply", NULL);
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
    ptTonemapData->iMode = ptView->tTonemapOptions.tMode;
    ptTonemapData->fExposure = ptView->tTonemapOptions.fExposure;
    ptTonemapData->fBrightness = ptView->tTonemapOptions.fBrightness;
    ptTonemapData->fContrast = ptView->tTonemapOptions.fContrast;
    ptTonemapData->fSaturation = ptView->tTonemapOptions.fSaturation;

    plComputeShaderHandle tTonemapShader = gptShaderVariant->get_compute_shader("tonemap", NULL);
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
pl__renderer_probe_update_all(plScene* ptScene)
{

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plDrawStream* ptStream = &gptData->tDrawStream;

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);
        plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptProbe->uDataPackIndex];

        if((ptScene->tFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED) && !gptGfx->is_texture_valid(ptDevice, ptProbe->tSheenEnvTexture))
        {
            PL_LOG_INFO_API(gptLog, gptData->uLogChannel, "creating required sheen env texture");
            const plTextureDesc tTextureDesc = {
                .tDimensions = {(float)ptProbeComp->uResolution, (float)ptProbeComp->uResolution, 1},
                .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
                .uLayers     = 6,
                .uMips       = (uint32_t)floorf(log2f((float)ptProbeComp->uResolution)) - 3, // guarantee final dispatch during filtering is 16 threads
                .eType       = PL_TEXTURE_TYPE_CUBE,
                .eUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .pcDebugName = "tSheenEnvTexture"
            };
            gptStarter->create_texture(&tTextureDesc, NULL, 0, &ptProbe->tSheenEnvTexture);
            ptProbe->uSheenEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, ptProbe->tSheenEnvTexture);
            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
        }
        else if(!(ptScene->tFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED) && gptGfx->is_texture_valid(ptDevice, ptProbe->tSheenEnvTexture))
        {
            PL_LOG_INFO_API(gptLog, gptData->uLogChannel, "freeing unneeded sheen env texture");
            gptGfx->queue_texture_for_deletion(ptDevice, ptProbe->tSheenEnvTexture);
        }

        
        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY)))
        {
            continue;
        }

        if(ptProbe->uDirtyFaces == 0 && (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY))
            ptProbe->uDirtyFaces = 6;

    }

    pl_sb_reset(ptScene->sbtGPUProbeData);
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtProbeData); i++)
    {
        plEnvironmentProbeComponent* ptProbe = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtProbeData[i].tEntity);
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtProbeData[i].tEntity);
        plTransformComponent* ptProbeTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptScene->sbtProbeData[i].tEntity);
        plEnvironmentProbeDataPack* ptPack = &ptScene->sbtProbeDataPacks[ptScene->sbtProbeData[i].uDataPackIndex];
        plGpuProbe tProbeData = {
            .tPosition              = ptProbeTransform->tTranslation,
            .fRangeSqr              = ptProbe->fRange * ptProbe->fRange,
            .uGGXEnvSampler         = ptScene->sbtProbeData[i].uGGXEnvSampler,
            .uLambertianEnvSampler  = ptScene->sbtProbeData[i].uLambertianEnvSampler,
            .uCharlieEnvSampler     = ptScene->sbtProbeData[i].uSheenEnvSampler,
            .tMin.xyz               = ptObject->tAABB.tMin,
            .tMax.xyz               = ptObject->tAABB.tMax,
            .iMips                  = ptScene->sbtProbeData[i].iMips,
            .iParallaxCorrection    = (int)(ptProbe->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX)
        };
        pl_sb_push(ptScene->sbtGPUProbeData, tProbeData);
    }

    if(pl_sb_size(ptScene->sbtGPUProbeData) > 0)
    {

        plBuffer* ptProbeDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->tGPUProbeDataBuffers);
        memcpy(ptProbeDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtGPUProbeData, sizeof(plGpuProbe) * pl_sb_size(ptScene->sbtGPUProbeData));
    }

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


    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);

        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY)))
        {
            continue;
        }

        plTransformComponent* ptProbeTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptProbe->tEntity);

        ptProbe->tViewData.tViewportSize = (plVec4){.x = ptProbe->tTargetSize.x, .y = ptProbe->tTargetSize.y, .z = 1.0f, .w = 1.0f};
        ptProbe->tViewData.tCameraPos.xyz = ptProbeTransform->tTranslation;
        ptProbe->tViewData.fCameraRange  = ptProbeComp->fRange - 0.26f;
        ptProbe->tViewData.fCameraNearZ  = 0.26f;
        ptProbe->tViewData.fAspectRatio  = 1.0f;

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

            // begin main renderpass (directly to swapchain)
            
            gptGfx->begin_render_pass(ptCmdBuffer, &tRenderInfo, NULL);
            gptGfx->set_depth_bias(ptCmdBuffer, 0.0f, 0.0f, 0.0f);

            plCullData tCullData = {
                .ptScene      = ptScene,
                .ptCullCamera = &atEnvironmentCamera[uFace],
                .atDrawables  = ptScene->sbtDrawables
            };
            pl__camera_build_perspective_frustum(&atEnvironmentCamera[uFace], &tCullData.tFrustum);
            
            plJobDesc tJobDesc = {
                .task  = pl__renderer_cull_job,
                .pData = &tCullData
            };

            plAtomicCounter* ptCullCounter = NULL;
            gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
            gptJob->wait_for_counter(ptCullCounter);
            pl_sb_reset(ptScene->sbuVisibleDeferredEntities);
            pl_sb_reset(ptScene->sbuVisibleForwardEntities);
            pl_sb_reset(ptScene->sbuVisibleTransmissionEntities);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                {
                    if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                        pl_sb_push(ptScene->sbuVisibleDeferredEntities, uDrawableIndex);
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                        pl_sb_push(ptScene->sbuVisibleForwardEntities, uDrawableIndex);
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
                        pl_sb_push(ptScene->sbuVisibleTransmissionEntities, uDrawableIndex);
                }
            }

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

            plGbufferFillPassInfo tGbufferFillPassInfo = {
                .sbuVisibleDeferredEntities = ptScene->sbuVisibleDeferredEntities,
                .uGlobalIndex = uFace,
                .tBG2 = ptProbe->tGBufferBG,
                .ptArea = &tArea,
                .sbtShaders = ptScene->sbtProbeShaders
            };
            pl__render_view_gbuffer_fill_pass(ptScene, ptCmdBuffer, &tGbufferFillPassInfo);
            

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
            
            if(ptScene->tSkyboxTexture.uIndex != 0 && ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY)
            {
                plMat4 tTransformMat = pl_mat4_translate_vec3(ptProbeTransform->tTranslation);
                pl__render_view_skybox_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tTransformMat, &tArea, uFace);  
                // pl__render_view_skybox_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tTransformMat, &tArea, 0);  
            }

            plForwardPassInfo tForwardPassInfo = {
                .sbuVisibleEntities = ptScene->sbuVisibleForwardEntities,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true,
                .sbtShaders = ptScene->sbtProbeShaders
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo);

            plForwardPassInfo tForwardPassInfo2 = {
                .sbuVisibleEntities = ptScene->sbuVisibleTransmissionEntities,
                .ptArea = &tArea,
                .uGlobalIndex = uFace,
                .bProbe = true,
                .sbtShaders = ptScene->sbtProbeShaders
            };
            pl__render_view_forward_pass(ptScene, ptCmdBuffer, ptProbe->tViewBG, &tForwardPassInfo2);

            gptGfx->end_render_pass(ptCmdBuffer);

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


        pl__renderer_probe_create_environment_map(ptScene, ptProbe);

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
}

static void
pl__renderer_scene_create_bindgroups(plScene* ptScene)
{
    plBindGroupLayoutHandle tGlobalSceneBindGroupLayout = gptShaderVariant->get_bind_group_layout("scene");
    plBindGroupLayoutHandle tSunTransmissionBindGroupLayout1 = gptShaderVariant->get_compute_bind_group_layout("sky_transmission_lut", 1);
    plBindGroupLayoutHandle tSunMultiscatterBindGroupLayout1 = gptShaderVariant->get_compute_bind_group_layout("sky_multiscatter_lut", 1);

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
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("skinning", 1),
            .pcDebugName = "skin bind group 2"
        };
        ptScene->atSkinBindGroup1[i] = gptGfx->create_bind_group(gptData->ptDevice, &tSkinBindGroup2Desc);

        const plBindGroupDesc tGlobalBGDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = gptData->tShadowGlobalBGLayout,
            .pcDebugName = "temporary global bind group 0"
        };
        ptScene->atShadowBG[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);
    }

    const plBindGroupDesc tSkinBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("skinning", 0),
        .pcDebugName = "skin bind group"
    };
    ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(gptData->ptDevice, &tSkinBindGroupDesc);
}

static void
pl__renderer_scene_update_bindgroups(plScene* ptScene)
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
                { .uSlot = 2, .tBuffer = ptScene->atTransformBuffer[i], .szBufferRange = sizeof(plMat4) * 10000 },
                { .uSlot = 3, .tBuffer = ptScene->tMaterialDataBuffer, .szBufferRange = ptScene->tInit.szMaterialBufferSize },
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
                    .tBuffer       = ptScene->atInstanceBuffer[i],
                    .uSlot         = 1,
                    .szBufferRange = sizeof(plShadowInstanceBufferData) * 10000
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
}

static void
pl__renderer_scene_create_textures(plScene* ptScene)
{
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

    const plTextureDesc tSunTransmissionLutDesc = {
        .tDimensions = { .xy = ptScene->tSunTransmissionLutResolution, 1},
        .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tSunTransmissionLut"
    };

    const plTextureDesc tSunMultiscatterLutDesc = {
        .tDimensions = { .xy = ptScene->tSunMultiscatterLutResolution, 1},
        .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "tSunMultiscatterLut"
    };
    
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->create_texture(&tSunTransmissionLutDesc, NULL, 0, &ptScene->atSunTransmissionLut[i]);
        gptStarter->create_texture(&tSunMultiscatterLutDesc, NULL, 0, &ptScene->atSunMultiscatterLut[i]);
    }
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
        gptStarter->get_staging_buffer(sizeof(plMat4) * 10000, &ptScene->atTransformBuffer[i], "transform");
        gptStarter->get_staging_buffer(sizeof(plShadowInstanceBufferData) * 10000, &ptScene->atInstanceBuffer[i], "instance");
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
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("brdf_lut", 0),
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

    plComputeShaderHandle tBrdfLutShader = gptShaderVariant->get_compute_shader("brdf_lut", NULL);
    
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
pl__renderer_view_create_textures(plView* ptView)
{

    plDevice* ptDevice = gptData->ptDevice;
    plVec3 tNewDimensions = {ptView->tTargetSize.x + 300.0f, ptView->tTargetSize.y + 300.0f, 1};

    if(gptGfx->is_texture_valid(ptDevice, ptView->tRawOutputTexture))
    {
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tRawOutputTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOMetalRoughnessTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture0);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture1);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickTexture);

        if(gptGfx->is_texture_valid(ptDevice, ptView->tTransmissionTexture))
        {
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
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = tNewDimensions,
        .eFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
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

    gptStarter->create_texture(&tRawOutputTextureDesc, NULL, 0, &ptView->tRawOutputTexture);
    gptStarter->create_texture(&tAlbedoTextureDesc, NULL, 0, &ptView->tAlbedoTexture);
    gptStarter->create_texture(&tNormalTextureDesc, NULL, 0, &ptView->tNormalTexture);
    gptStarter->create_texture(&tEmmissiveTexDesc, NULL, 0, &ptView->tAOMetalRoughnessTexture);
    gptStarter->create_texture(&tDepthTextureDesc, NULL, 0, &ptView->tDepthTexture);
    gptStarter->create_texture(&tMaskTextureDesc, NULL, 0, &ptView->atUVMaskTexture0);
    gptStarter->create_texture(&tMaskTextureDesc, NULL, 0, &ptView->atUVMaskTexture1);
    gptStarter->create_texture(&tRawOutputTextureDesc, NULL, 0, &ptView->tFinalTexture);
    gptStarter->create_texture(&tPickTextureDesc, NULL, 0, &ptView->tPickTexture);

    if(!gptGfx->is_texture_valid(ptDevice, ptView->atSkyLut[0]))
    {
        const plTextureDesc tSkyLutDesc = {
            .tDimensions = { .xy = ptView->ptParentScene->tSkyLutResolution, 1},
            .eFormat     = PL_FORMAT_R11G11B10_FLOAT,
            .uLayers     = 1,
            .uMips       = 1,
            .eType       = PL_TEXTURE_TYPE_2D,
            .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
            .pcDebugName = "tSkyLut"
        };
        for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        {
            gptStarter->create_texture(&tSkyLutDesc, NULL, 0, &ptView->atSkyLut[i]);
        }
    }
}

static void
pl__renderer_view_create_buffers(plView* ptView)
{
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->get_staging_buffer(4096, &ptView->atSunCameraBuffers[i], "directional shadow camera buffer");
        gptStarter->get_staging_buffer(4096, &ptView->atDShadowCameraBuffers[i], "directional shadow camera buffer");
        gptStarter->get_staging_buffer(PL_MAX_LIGHTS * sizeof(plGpuDirectionLightShadow), &ptView->atDLightShadowDataBuffer[i], "directional shadow buffer");
        gptStarter->get_staging_buffer(sizeof(plGpuViewData), &ptView->atViewBuffers[i], "view buffer");

        const plBufferDesc tPickBufferDesc = {
            .eUsage     = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = sizeof(uint32_t) * 2,
            .pcDebugName = "Picking buffer"
        };
        gptStarter->get_readback_buffer(sizeof(uint32_t) * 2, &ptView->atPickBuffer[i], "pick buffer");
    }
}

static void
pl__renderer_view_create_bindgroups(plView* ptView)
{
    plScene* ptScene = ptView->ptParentScene;

    ptView->tFinalTextureHandle = gptDraw->create_bind_group_for_texture(ptView->tFinalTexture);

    const plBindGroupDesc tJFABindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("jumpfloodalgo", 0),
        .pcDebugName = "temp jfa bind group"
    };

    ptView->atJFABindGroups[0] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);
    ptView->atJFABindGroups[1] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);

    const plBindGroupDesc tOutlineBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("jumpfloodalgo2", 1),
        .pcDebugName = "temp bind group 0"
    };
    ptView->atOutlineBG[0] = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);
    ptView->atOutlineBG[1] = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_bind_group_layout("deferred lighting 1"),
        .pcDebugName = "lighting bind group"
    };
    ptView->tLightingBindGroup = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);

    const plBindGroupDesc tTonemapBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("tonemap", 0),
        .pcDebugName = "tonemap bind group c0"
    };
    ptView->tTonemapBG = gptGfx->create_bind_group(gptData->ptDevice, &tTonemapBGDesc);

    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };

    const plBindGroupDesc tPickBindGroupDesc = {
        .ptPool = gptData->ptBindGroupPool,
        .tLayout = gptShaderVariant->get_graphics_bind_group_layout("picking", 1),
        .pcDebugName = "pick bind group"
    };

    const plBindGroupDesc tDeferredBG1Desc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("gbuffer_fill", 1),
        .pcDebugName = "view specific bindgroup"
    };

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tViewBGLayout,
        .pcDebugName = "light bind group 2"
    };

    plBindGroupLayoutHandle tSkyLutBindGroupLayout2 = gptShaderVariant->get_compute_bind_group_layout("sky_lut", 2);

    const plBindGroupDesc tSkyLutBG2Desc = {
        .tLayout     = tSkyLutBindGroupLayout2,
        .pcDebugName = "atSkyLutBG2",
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
                    .tBuffer       = ptScene->atInstanceBuffer[i],
                    .uSlot         = 1,
                    .szBufferRange = sizeof(plShadowInstanceBufferData) * 10000
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
                    .tBuffer       = ptScene->atInstanceBuffer[i],
                    .uSlot         = 1,
                    .szBufferRange = sizeof(plShadowInstanceBufferData) * 10000
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
            .atBufferBindings  = {
                { .uSlot = 0, .tBuffer = ptView->atViewBuffers[i], .szBufferRange = sizeof(plGpuViewData) },
                { .uSlot = 1, .tBuffer = ptScene->atPointLightBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightBuffer[i])->tDesc.szByteSize},
                { .uSlot = 2, .tBuffer = ptScene->atSpotLightBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightBuffer[i])->tDesc.szByteSize},
                { .uSlot = 3, .tBuffer = ptScene->atDirectionLightBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atDirectionLightBuffer[i])->tDesc.szByteSize},
                { .uSlot = 4, .tBuffer = ptView->atDLightShadowDataBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptView->atDLightShadowDataBuffer[i])->tDesc.szByteSize},
                { .uSlot = 5, .tBuffer = ptScene->atPointLightShadowDataBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightShadowDataBuffer[i])->tDesc.szByteSize},
                { .uSlot = 6, .tBuffer = ptScene->atSpotLightShadowDataBuffer[i], .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightShadowDataBuffer[i])->tDesc.szByteSize},
                { .uSlot = 7, .tBuffer = ptScene->tGPUProbeDataBuffers, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tGPUProbeDataBuffers)->tDesc.szByteSize} 
            }
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptView->atViewBG[i], &tViewBGData);

        if(ptView->ptParentScene->tFlags & PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED)
        {
            const plBindGroupUpdateData tGlobalBindGroupData = {
                .atTextureBindings = {
                    {
                        .tTexture = ptView->tTransmissionTexture,
                        .uSlot    = PL_MAX_BINDLESS_TEXTURE_SLOT,
                        .uIndex   = (uint32_t)ptView->tViewData.iTransmissionFrameBufferIndex,
                        .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED
                    }  
                }
            };
            gptGfx->update_bind_group(gptData->ptDevice, ptView->ptParentScene->atSceneBindGroups[i], &tGlobalBindGroupData);
        }

        const plBindGroupUpdateData tBGSkyLut = {
            .atTextureBindings = {
                {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptView->atSkyLut[i]},
                {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunTransmissionLut[i]},
                {.uSlot = 2, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tTexture = ptScene->atSunMultiscatterLut[i]}
            }
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atSkyLutBG2[i], &tBGSkyLut);
    }
}

static void
pl__renderer_probe_create_textures(plScene* ptScene, plEnvironmentProbeData* ptProbeData)
{
    plEnvironmentProbeComponent* ptProbe = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbeData->tEntity);

    const plTextureDesc tRawOutputTextureCubeDesc = {
        .tDimensions   = {ptProbeData->tTargetSize.x, ptProbeData->tTargetSize.y, 1},
        .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 6,
        .uMips         = 0,
        .eType         = PL_TEXTURE_TYPE_CUBE,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen final cube"
    };
    gptStarter->create_texture(&tRawOutputTextureCubeDesc, NULL, 0, &ptProbeData->tRawOutputTexture);

    const plTextureDesc tSpecularTextureDesc = {
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
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
        .tDimensions = {(float)ptProbe->uResolution, (float)ptProbe->uResolution, 1},
        .eFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = (uint32_t)floorf(log2f((float)ptProbe->uResolution)) - 3, // guarantee final dispatch during filtering is 16 threads
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
    gptStarter->get_staging_buffer(4096, &ptProbeData->tDShadowCameraBuffers, "directional shadow camera buffer");
    gptStarter->get_staging_buffer(4096, &ptProbeData->tSunCameraBuffers, "directional shadow camera buffer");
    gptStarter->get_staging_buffer(PL_MAX_LIGHTS * sizeof(plGpuDirectionLightShadow), &ptProbeData->tDLightShadowDataBuffer, "directional shadow buffer");
    gptStarter->get_staging_buffer(sizeof(plGpuViewData), &ptProbeData->tViewBuffer, "view buffer");
}

static void
pl__renderer_probe_create_bindgroups(plScene* ptScene, plEnvironmentProbeData* ptProbeData)
{
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tShadowGlobalBGLayout,
        .pcDebugName = "temporary global bind group 0"
    };
    ptProbeData->tDShadowBG = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);
    ptProbeData->tSunCameraBG = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBGDesc);

    const plBindGroupDesc tGBufferFillBG1Desc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("gbuffer_fill", 1),
        .pcDebugName = "gbuffer fill bg1"
    };
    ptProbeData->tGBufferBG = gptGfx->create_bind_group(gptData->ptDevice, &tGBufferFillBG1Desc);

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tViewBGLayout,
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
                .tBuffer       = ptScene->atInstanceBuffer[0],
                .uSlot         = 1,
                .szBufferRange = sizeof(plShadowInstanceBufferData) * 10000
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
                .tBuffer       = ptScene->atInstanceBuffer[0],
                .uSlot         = 1,
                .szBufferRange = sizeof(plShadowInstanceBufferData) * 10000
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
        .atBufferBindings  = {
            { .uSlot = 0, .tBuffer = ptProbeData->tViewBuffer,            .szBufferRange = sizeof(plGpuViewData) },
            { .uSlot = 1, .tBuffer = ptScene->atPointLightBuffer[0],            .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightBuffer[0])->tDesc.szByteSize},
            { .uSlot = 2, .tBuffer = ptScene->atSpotLightBuffer[0],            .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightBuffer[0])->tDesc.szByteSize},
            { .uSlot = 3, .tBuffer = ptScene->atDirectionLightBuffer[0],            .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->atDirectionLightBuffer[0])->tDesc.szByteSize},
            { .uSlot = 4, .tBuffer = ptProbeData->tDLightShadowDataBuffer, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptProbeData->tDLightShadowDataBuffer)->tDesc.szByteSize},
            { .uSlot = 5, .tBuffer = ptScene->atPointLightShadowDataBuffer[0],  .szBufferRange =  gptGfx->get_buffer(gptData->ptDevice, ptScene->atPointLightShadowDataBuffer[0])->tDesc.szByteSize},
            { .uSlot = 6, .tBuffer = ptScene->atSpotLightShadowDataBuffer[0],  .szBufferRange =  gptGfx->get_buffer(gptData->ptDevice, ptScene->atSpotLightShadowDataBuffer[0])->tDesc.szByteSize},
            { .uSlot = 7, .tBuffer = ptScene->tGPUProbeDataBuffers,    .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tGPUProbeDataBuffers)->tDesc.szByteSize}
        }
    };
    
    gptGfx->update_bind_group(gptData->ptDevice, ptProbeData->tViewBG, &tViewBGData);
}