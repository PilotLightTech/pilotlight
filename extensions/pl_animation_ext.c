/*
   pl_ecs_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_ecs_ext.h"
#include "pl_animation_ext.h"
#include "pl_math.h"

// extensions
#include "pl_profile_ext.h"
#include "pl_log_ext.h"

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

    static plApiRegistryI*             gptApiRegistry       = NULL;
    static const plExtensionRegistryI* gptExtensionRegistry = NULL;
    static const plProfileI*           gptProfile           = NULL;
    static const plLogI*               gptLog               = NULL;
    static const plEcsI*               gptECS               = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plComponentLibraryData
{
    plTransformComponent* sbtTransformsCopy; // used for inverse kinematics system
} plComponentLibraryData;

typedef struct _plAnimationContext
{
    plEcsTypeKey tAnimationComponentType;
    plEcsTypeKey tAnimationDataComponentType;
    plEcsTypeKey tInverseKinematicsComponentType;
    plEcsTypeKey tHumanoidComponentType;
    plEcsTypeKey tTransformComponentType;
    plEcsTypeKey tHierarchyComponentType;
} plAnimationContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plAnimationContext* gptAnimationCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

plEcsTypeKey
pl_ecs_core_get_ecs_type_key_animation(void)
{
    return gptAnimationCtx->tAnimationComponentType;
}

plEcsTypeKey
pl_ecs_core_get_ecs_type_key_animation_data(void)
{
    return gptAnimationCtx->tAnimationDataComponentType;
}

plEcsTypeKey
pl_ecs_core_get_ecs_type_key_inverse_kinematics(void)
{
    return gptAnimationCtx->tInverseKinematicsComponentType;
}

plEcsTypeKey
pl_ecs_core_get_ecs_type_key_humanoid(void)
{
    return gptAnimationCtx->tHumanoidComponentType;
}

static void
pl__ecs_ik_init(plComponentLibrary* ptLibrary)
{
    void* pData = PL_ALLOC(sizeof(plComponentLibraryData));
    memset(pData, 0, sizeof(plComponentLibraryData));
    gptECS->set_library_type_data(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType, pData);
}

static void
pl__ecs_animation_cleanup(plComponentLibrary* ptLibrary)
{
    plAnimationComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tAnimationComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        PL_FREE(ptComponents[i].atChannels);
        ptComponents[i].atChannels = NULL;
        ptComponents[i].atSamplers = NULL;
        ptComponents[i].uChannelCount = 0;
    }
}

static void
pl__ecs_animation_data_cleanup(plComponentLibrary* ptLibrary)
{
    plAnimationDataComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tAnimationDataComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        PL_FREE(ptComponents[i].afKeyFrameTimes);
        ptComponents[i].afKeyFrameTimes = NULL;
        ptComponents[i].pKeyFrameData = NULL;
    }
}

static void
pl__ecs_ik_cleanup(plComponentLibrary* ptLibrary)
{
    plComponentLibraryData* ptData = gptECS->get_library_type_data(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType);
    pl_sb_free(ptData->sbtTransformsCopy);
    PL_FREE(ptData);
    gptECS->set_library_type_data(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType, NULL);
}

static void
pl__ecs_ik_reset(plComponentLibrary* ptLibrary)
{
    plComponentLibraryData* ptData = gptECS->get_library_type_data(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType);
    pl_sb_reset(ptData->sbtTransformsCopy);
}

plEntity
pl_ecs_create_animation(plComponentLibrary* ptLibrary, const char* pcName, uint32_t uChannelCount, plAnimationComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed animation";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created animation: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plAnimationComponent* ptCompOut = gptECS->add_component(ptLibrary, gptAnimationCtx->tAnimationComponentType, tNewEntity);

    size_t szAllocationSize = (sizeof(plAnimationChannel) + sizeof(plAnimationSampler)) * uChannelCount;
    ptCompOut->uChannelCount = uChannelCount;
    ptCompOut->atChannels = PL_ALLOC(szAllocationSize);
    memset(ptCompOut->atChannels, 0, szAllocationSize);
    ptCompOut->atSamplers = (plAnimationSampler*)&ptCompOut->atChannels[uChannelCount];

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;
}

plEntity
pl_ecs_create_animation_data(plComponentLibrary* ptLibrary, const char* pcName, uint32_t uKeyFrameCount, size_t szDataSize, plAnimationDataComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed animation data";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created animation data: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plAnimationDataComponent* ptCompOut = gptECS->add_component(ptLibrary, gptAnimationCtx->tAnimationDataComponentType, tNewEntity);

    ptCompOut->uKeyFrameCount = uKeyFrameCount;
    ptCompOut->szDataSize = szDataSize;

    ptCompOut->afKeyFrameTimes = PL_ALLOC(sizeof(float) * uKeyFrameCount + szDataSize);
    memset(ptCompOut->afKeyFrameTimes, 0, sizeof(float) * uKeyFrameCount + szDataSize);
    ptCompOut->pKeyFrameData = (void*)&ptCompOut->afKeyFrameTimes[uKeyFrameCount];

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;
}

void
pl_run_animation_update_system(plComponentLibrary* ptLibrary, float fDeltaTime)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plAnimationComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tAnimationComponentType, (void**)&ptComponents, NULL);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plAnimationComponent* ptAnimationComponent = &ptComponents[i];

        if(!(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_PLAYING))
            continue;

        ptAnimationComponent->fTimer += fDeltaTime * ptAnimationComponent->fSpeed;

        if(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_LOOPED)
        {
            ptAnimationComponent->fTimer = fmodf(ptAnimationComponent->fTimer, ptAnimationComponent->fEnd);
        }

        if(ptAnimationComponent->fTimer > ptAnimationComponent->fEnd)
        {
            ptAnimationComponent->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
            ptAnimationComponent->fTimer = 0.0f;
            continue;
        }

        for(uint32_t j = 0; j < ptAnimationComponent->uChannelCount; j++)
        {
            
            const plAnimationChannel* ptChannel = &ptAnimationComponent->atChannels[j];
            const plAnimationSampler* ptSampler = &ptAnimationComponent->atSamplers[ptChannel->uSamplerIndex];
            const plAnimationDataComponent* ptData = gptECS->get_component(ptLibrary, gptAnimationCtx->tAnimationDataComponentType, ptSampler->tData);
            plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, gptAnimationCtx->tTransformComponentType, ptChannel->tTarget);
            ptTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;

            // wrap t around, so the animation loops.
            // make sure that t is never earlier than the first keyframe and never later then the last keyframe.
            const float fModTime = pl_clampf(ptData->afKeyFrameTimes[0], ptAnimationComponent->fTimer, ptData->afKeyFrameTimes[ptData->uKeyFrameCount - 1]);
            int iNextKey = 0;

            for(uint32_t k = 0; k < ptData->uKeyFrameCount; k++)
            {
                
                if(fModTime <= ptData->afKeyFrameTimes[k])
                {
                    iNextKey = pl_clampi(1, k, ptData->uKeyFrameCount - 1);
                    break;
                }
            }
            const int iPrevKey = pl_clampi(0, iNextKey - 1, ptData->uKeyFrameCount - 1);

            const float fKeyDelta = ptData->afKeyFrameTimes[iNextKey] - ptData->afKeyFrameTimes[iPrevKey];

            // normalize t: [t0, t1] -> [0, 1]
            const float fTn = (fModTime - ptData->afKeyFrameTimes[iPrevKey]) / fKeyDelta;

            const float fTSq = fTn * fTn;
            const float fTCub = fTSq * fTn;

            const int iA = 0;

            float* afKeyFrameData = (float*)ptData->pKeyFrameData;

            switch(ptChannel->tPath)
            {
                case PL_ANIMATION_PATH_TRANSLATION:
                {

                    if(ptSampler->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec3 tPrev = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                        const plVec3 tNext = *(plVec3*)&afKeyFrameData[iNextKey * 3];
                        const plVec3 tTranslation = (plVec3){
                            .x = tPrev.x * (1.0f - fTn) + tNext.x * fTn,
                            .y = tPrev.y * (1.0f - fTn) + tNext.y * fTn,
                            .z = tPrev.z * (1.0f - fTn) + tNext.z * fTn,
                        };
                        ptTransform->tTranslation = pl_lerp_vec3(ptTransform->tTranslation, tTranslation, ptAnimationComponent->fBlendAmount);
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        const plVec3 tTranslation = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                        ptTransform->tTranslation = pl_lerp_vec3(ptTransform->tTranslation, tTranslation, ptAnimationComponent->fBlendAmount);
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
                    {
                        const int iPrevIndex = iPrevKey * 3 * 3;
                        const int iNextIndex = iNextKey * 3 * 3;
                        const int iV = 1 * 3;
                        const int iB = 2 * 3;

                        plVec3 tTranslation = {0};
                        for(uint32_t k = 0; k < 3; k++)
                        {
                            const float iV0 = afKeyFrameData[iPrevIndex + k + iV];
                            const float a = fKeyDelta * afKeyFrameData[iNextIndex + k + iA];
                            const float b = fKeyDelta * afKeyFrameData[iPrevIndex + k + iB];
                            const float v1 = afKeyFrameData[iNextIndex + k + iV];
                            tTranslation.d[k] = ((2 * fTCub - 3 * fTSq + 1) * iV0) + ((fTCub - 2 * fTSq + fTn) * b) + ((-2 * fTCub + 3 * fTSq) * v1) + ((fTCub - fTSq) * a);
                        }
                        ptTransform->tTranslation = pl_lerp_vec3(ptTransform->tTranslation, tTranslation, ptAnimationComponent->fBlendAmount);
                    }
                    break;
                }

                case PL_ANIMATION_PATH_SCALE:
                {

                    if(ptSampler->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec3 tPrev = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                        const plVec3 tNext = *(plVec3*)&afKeyFrameData[iNextKey * 3];
                        ptTransform->tScale = (plVec3){
                            .x = tPrev.x * (1.0f - fTn) + tNext.x * fTn,
                            .y = tPrev.y * (1.0f - fTn) + tNext.y * fTn,
                            .z = tPrev.z * (1.0f - fTn) + tNext.z * fTn,
                        };
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        ptTransform->tScale = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
                    {
                        const int iPrevIndex = iPrevKey * 3 * 3;
                        const int iNextIndex = iNextKey * 3 * 3;
                        const int iV = 1 * 3;
                        const int iB = 2 * 3;

                        for(uint32_t k = 0; k < 3; k++)
                        {
                            float v0 = afKeyFrameData[iPrevIndex + k + iV];
                            float a = fKeyDelta * afKeyFrameData[iNextIndex + k + iA];
                            float b = fKeyDelta * afKeyFrameData[iPrevIndex + k + iB];
                            float v1 = afKeyFrameData[iNextIndex + k + iV];
                            ptTransform->tScale.d[k] = ((2 * fTCub - 3 * fTSq + 1) * v0) + ((fTCub - 2 * fTSq + fTn) * b) + ((-2 * fTCub + 3 * fTSq) * v1) + ((fTCub - fTSq) * a);
                        }
                    }
                    break;
                }

                case PL_ANIMATION_PATH_ROTATION:
                {

                    if(ptSampler->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec4 tQ0 = *(plVec4*)&afKeyFrameData[iPrevKey * 4];
                        const plVec4 tQ1 = *(plVec4*)&afKeyFrameData[iNextKey * 4];
                        const plVec4 tRotation = pl_quat_slerp(tQ0, tQ1, fTn);
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tRotation, ptAnimationComponent->fBlendAmount);
                    }
                    else if(ptSampler->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        const plVec4 tRotation = *(plVec4*)&afKeyFrameData[iPrevKey * 4];
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tRotation, ptAnimationComponent->fBlendAmount);
                    }
                    else if(ptSampler->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
                    {
                        const int iPrevIndex = iPrevKey * 4 * 3;
                        const int iNextIndex = iNextKey * 4 * 3;
                        const int iV = 1 * 4;
                        const int iB = 2 * 4;

                        plVec4 tResult = {0};
                        for(uint32_t k = 0; k < 4; k++)
                        {
                            const float iV0 = afKeyFrameData[iPrevIndex + k + iV];
                            const float a = fKeyDelta * afKeyFrameData[iNextIndex + k + iA];
                            const float b = fKeyDelta * afKeyFrameData[iPrevIndex + k + iB];
                            const float iV1 = afKeyFrameData[iNextIndex + k + iV];

                            tResult.d[k] = ((2 * fTCub - 3 * fTSq + 1) * iV0) + ((fTCub - 2 * fTSq + fTn) * b) + ((-2 * fTCub + 3 * fTSq) * iV1) + ((fTCub - fTSq) * a);
                        }
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tResult, ptAnimationComponent->fBlendAmount);
                    }
                    break;
                }
            }
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_run_inverse_kinematics_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plInverseKinematicsComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType, (void**)&ptComponents, &ptEntities);


    plTransformComponent* ptTransforms = NULL;
    const uint32_t uTransformCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tTransformComponentType, (void**)&ptTransforms, NULL);

    plComponentLibraryData* ptData = gptECS->get_library_type_data(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType);
    pl_sb_resize(ptData->sbtTransformsCopy, uTransformCount);
    memcpy(ptData->sbtTransformsCopy, ptTransforms, uTransformCount * sizeof(plTransformComponent));
    gptECS->set_library_type_data(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType, ptData);
    
    bool bRecomputeHierarchy = false;
    for(uint32_t i = 0; i < uComponentCount; i++)
    {

        const plEntity tIKEntity = ptEntities[i];
        const size_t uIKIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tInverseKinematicsComponentType, tIKEntity);

        const plInverseKinematicsComponent* ptInverseKinematicsComponent = &ptComponents[uIKIndex];
        
        if(!ptInverseKinematicsComponent->bEnabled)
            continue;

        const size_t uTransformIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tTransformComponentType, tIKEntity);
        const size_t uTargetIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tTransformComponentType, ptInverseKinematicsComponent->tTarget);

        plTransformComponent* ptTransform = &ptData->sbtTransformsCopy[uTransformIndex];
        plTransformComponent* ptTarget = &ptData->sbtTransformsCopy[uTargetIndex];
        plHierarchyComponent* ptHierComp = gptECS->get_component(ptLibrary, gptAnimationCtx->tHierarchyComponentType, tIKEntity);
        
        PL_ASSERT(uTransformIndex != UINT64_MAX);
        PL_ASSERT(uTargetIndex != UINT64_MAX);
        PL_ASSERT(ptHierComp);

        const plVec3 tTargetPos = ptTarget->tWorld.col[3].xyz;
        for(uint32_t j = 0; j < ptInverseKinematicsComponent->uIterationCount; j++)
        {
            plTransformComponent* aptStack[32] = {0};
            plEntity tParentEntity = ptHierComp->tParent;
            plTransformComponent* ptChildTransform = ptTransform;
            for(uint32_t uChain = 0; uChain < pl_min(ptInverseKinematicsComponent->uChainLength, 32); ++uChain)
            {
                bRecomputeHierarchy = true;

                // stack stores all traversed chain links so far
                aptStack[uChain] = ptChildTransform;

                // compute required parent rotation that moves ik transform closer to target transform
                const size_t uParentIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tTransformComponentType, tParentEntity);
                PL_ASSERT(uParentIndex != UINT64_MAX);
                plTransformComponent* ptParentTransform =  &ptData->sbtTransformsCopy[uParentIndex];
                const plVec3 tParentPos = ptParentTransform->tWorld.col[3].xyz;
                const plVec3 tDirParentToIk = pl_norm_vec3(pl_sub_vec3(ptTransform->tWorld.col[3].xyz, tParentPos));
                const plVec3 tDirParentToTarget = pl_norm_vec3(pl_sub_vec3(tTargetPos, tParentPos));

                // TODO: check if this transform is part of a humanoid and need some constraining

                plVec4 tQ = {0};

                // simple shortest rotation without constraint
                const plVec3 tAxis = pl_norm_vec3(pl_cross_vec3(tDirParentToIk, tDirParentToTarget));
                const float fAngle = acosf(pl_clampf(-1.0f, pl_dot_vec3(tDirParentToIk, tDirParentToTarget), 1.0f));
                tQ = pl_norm_vec4(pl_quat_rotation_vec3(fAngle, tAxis));

                // parent to world space
                pl_decompose_matrix(&ptParentTransform->tWorld, &ptParentTransform->tScale, &ptParentTransform->tRotation, &ptParentTransform->tTranslation);

                // rotate parent
                ptParentTransform->tRotation = pl_norm_vec4(pl_mul_quat(tQ, ptParentTransform->tRotation));
                ptParentTransform->tWorld = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);

                // parent back to local space (if parent has parent)
                plHierarchyComponent* ptHierParentComp = gptECS->get_component(ptLibrary, gptAnimationCtx->tHierarchyComponentType, tParentEntity);
                if(ptHierParentComp)
                {
                    plEntity tParentOfParentEntity = ptHierParentComp->tParent;
                    const size_t uGrandParentIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tTransformComponentType, tParentOfParentEntity);
                    PL_ASSERT(uGrandParentIndex != UINT64_MAX);
                    plTransformComponent* ptParentOfParentTransform = &ptData->sbtTransformsCopy[uGrandParentIndex];
                    const plMat4 tParentOfParentInverse = pl_mat4_invert(&ptParentOfParentTransform->tWorld);
                    plMat4 tW = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);
                    plMat4 tNewMatrix = pl_mul_mat4(&tParentOfParentInverse, &tW);
                    pl_decompose_matrix(&tNewMatrix, &ptParentTransform->tScale, &ptParentTransform->tRotation, &ptParentTransform->tTranslation);
                    // keep parent world matrix in world space!
                }

                // update chain from parent to children
                const plTransformComponent* ptRecurseParent = ptParentTransform;
                for(int recurse_chain = (int)uChain; recurse_chain >=0; --recurse_chain)
                {
                    plMat4 tW = pl_rotation_translation_scale(aptStack[recurse_chain]->tRotation, aptStack[recurse_chain]->tTranslation, aptStack[recurse_chain]->tScale);
                    aptStack[recurse_chain]->tWorld = pl_mul_mat4(&ptRecurseParent->tWorld, &tW);
                    ptRecurseParent = aptStack[recurse_chain];
                }

                if(ptHierParentComp == NULL)
                {
                    // chain root reached, exit
                    break;
                }

                // move up in the chain by one
                ptChildTransform = ptParentTransform;
                tParentEntity = ptHierParentComp->tParent;
                PL_ASSERT(uChain < 32);
            }
        }

    }

    if(bRecomputeHierarchy)
    {
        const plEntity* ptHierarchyEntities = NULL;
        const uint32_t uHierarchyCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tHierarchyComponentType, NULL, &ptHierarchyEntities);
        for(uint32_t i = 0; i < uHierarchyCount; i++)
        {
            const plEntity tChildEntity = ptHierarchyEntities[i];
            
            const size_t uChildIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tTransformComponentType, tChildEntity);
            PL_ASSERT(uChildIndex != UINT64_MAX);

            const plTransformComponent* ptTransformChild = &ptData->sbtTransformsCopy[uChildIndex];

            plMat4 tWorldMatrix = pl_rotation_translation_scale(ptTransformChild->tRotation, ptTransformChild->tTranslation, ptTransformChild->tScale);

            plHierarchyComponent* ptHierarchyComponent = gptECS->get_component(ptLibrary, gptAnimationCtx->tHierarchyComponentType, tChildEntity);
            
            plEntity tParentID = ptHierarchyComponent->tParent;
            while(tParentID.uIndex != UINT32_MAX)
            {
                const size_t uParentIndex = gptECS->get_index(ptLibrary, gptAnimationCtx->tTransformComponentType, tParentID);
                if(uParentIndex == UINT64_MAX)
                    break;
                plTransformComponent* ptTransformParent = &ptData->sbtTransformsCopy[uParentIndex];
                plMat4 tLocalMatrix = pl_rotation_translation_scale(ptTransformParent->tRotation, ptTransformParent->tTranslation, ptTransformParent->tScale);
                tWorldMatrix = pl_mul_mat4(&tLocalMatrix, &tWorldMatrix);

                const plHierarchyComponent* ptHierarchyRecursive = gptECS->get_component(ptLibrary, gptAnimationCtx->tHierarchyComponentType, tParentID);
                if(ptHierarchyRecursive)
                    tParentID = ptHierarchyRecursive->tParent;
                else
                    tParentID.uIndex = UINT32_MAX;
            }

            ptTransforms[uChildIndex].tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
            ptTransforms[uChildIndex].tWorld = tWorldMatrix;
        }

    }

    pl_sb_reset(ptData->sbtTransformsCopy);

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_core_register_system(void)
{

    gptAnimationCtx->tTransformComponentType = gptECS->get_ecs_type_key_transform();
    gptAnimationCtx->tHierarchyComponentType = gptECS->get_ecs_type_key_hierarchy();

    const plComponentDesc tAnimationDesc = {
        .pcName = "Animation",
        .szSize = sizeof(plAnimationComponent),
        .cleanup = pl__ecs_animation_cleanup,
        .reset = pl__ecs_animation_cleanup,
    };

    static const plAnimationComponent tAnimationComponentDefault = {
        .fSpeed       = 1.0f,
        .fBlendAmount = 1.0f
    };

    gptAnimationCtx->tAnimationComponentType = gptECS->register_type(tAnimationDesc, &tAnimationComponentDefault);

    const plComponentDesc tAnimationDataDesc = {
        .pcName = "Animation Data",
        .szSize = sizeof(plAnimationDataComponent),
        .cleanup = pl__ecs_animation_data_cleanup,
        .reset = pl__ecs_animation_data_cleanup,
    };
    gptAnimationCtx->tAnimationDataComponentType = gptECS->register_type(tAnimationDataDesc, NULL);

    const plComponentDesc tIKDesc = {
        .pcName = "Inverse Kinematics",
        .szSize = sizeof(plInverseKinematicsComponent),
        .init   = pl__ecs_ik_init,
        .cleanup = pl__ecs_ik_cleanup,
        .reset = pl__ecs_ik_reset
    };

    static const plInverseKinematicsComponent tIkComponentDefault = {
        .bEnabled = true,
        .tTarget = UINT32_MAX,
        .uIterationCount = 1
    };
    gptAnimationCtx->tInverseKinematicsComponentType = gptECS->register_type(tIKDesc, &tIkComponentDefault);

    const plComponentDesc tHumanoidDesc = {
        .pcName = "Humanoid",
        .szSize = sizeof(plHumanoidComponent)
    };

    static plHumanoidComponent tHumanoidComponentDefault = {0};
    for(uint32_t i = 0; i < PL_HUMANOID_BONE_COUNT; i++)
    {
        tHumanoidComponentDefault.atBones[i].uData = UINT64_MAX;
    }
    gptAnimationCtx->tHumanoidComponentType = gptECS->register_type(tHumanoidDesc, &tHumanoidComponentDefault);

}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_animation_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAnimationI tApi = {
        .register_ecs_system                  = pl_core_register_system,
        .create_animation                     = pl_ecs_create_animation,
        .create_animation_data                = pl_ecs_create_animation_data,
        .run_animation_update_system          = pl_run_animation_update_system,
        .run_inverse_kinematics_update_system = pl_run_inverse_kinematics_update_system,
        .get_ecs_type_key_animation           = pl_ecs_core_get_ecs_type_key_animation,
        .get_ecs_type_key_animation_data      = pl_ecs_core_get_ecs_type_key_animation_data,
        .get_ecs_type_key_inverse_kinematics  = pl_ecs_core_get_ecs_type_key_inverse_kinematics,
        .get_ecs_type_key_humanoid            = pl_ecs_core_get_ecs_type_key_humanoid,
    };
    pl_set_api(ptApiRegistry, plAnimationI, &tApi);

    gptApiRegistry = ptApiRegistry;
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptECS = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog = pl_get_api_latest(ptApiRegistry, plLogI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptAnimationCtx = ptDataRegistry->get_data("plAnimationContext");
    }
    else // first load
    {
        static plAnimationContext tCtx = {0};
        gptAnimationCtx = &tCtx;
        ptDataRegistry->set_data("plAnimationContext", gptAnimationCtx);
    }
}

static void
pl_unload_animation_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plAnimationI* ptApi = pl_get_api_latest(ptApiRegistry, plAnimationI);
    ptApiRegistry->remove_api(ptApi);
}