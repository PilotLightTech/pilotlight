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
#include "pl_asset_ext.h"
#include "pl_vfs_ext.h"
#include "pl_transform_ext.h"

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

    static const plProfileI*           gptProfile           = NULL;
    static const plLogI*               gptLog               = NULL;
    static const plEcsI*               gptECS               = NULL;
    static const plAssetI*             gptAsset             = NULL;
    static const plVfsI*               gptVfs               = NULL;
    static const plTransformI*         gptTransform         = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAnimationContext
{
    plEcsTypeKey tAnimationComponentType;
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
pl_animation_get_ecs_type_key_animation(void)
{
    return gptAnimationCtx->tAnimationComponentType;
}

plEcsTypeKey
pl_animation_get_ecs_type_key_humanoid(void)
{
    return gptAnimationCtx->tHumanoidComponentType;
}

static void
pl__ecs_animation_cleanup(plComponentLibrary* ptLibrary)
{
    plAnimationComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tAnimationComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        // for(uint32_t j = 0; j < ptComponents[i].uChannelCount; j++)
        // {
        //     PL_FREE(ptComponents[i].atData[j].afKeyFrameTimes);
        // }
        // PL_FREE(ptComponents[i].puRawData);
        // ptComponents[i].atChannels = NULL;
        // ptComponents[i].atSamplers = NULL;
        // ptComponents[i].atData = NULL;
        // ptComponents[i].atTargets = NULL;
        // ptComponents[i].puRawData = NULL;
        // ptComponents[i].uChannelCount = 0;

        // for(uint32_t j = 0; j < ptComponents[i].uChannelCount; j++)
        // {
        //     PL_FREE(ptComponents[i].atData[j].afKeyFrameTimes);
        // }

        PL_FREE(ptComponents[i].atTargets);
        ptComponents[i].atTargets = NULL;
        ptComponents[i].uTargetCount = 0;
    }
}

plEntity
pl_animation_create(plComponentLibrary* ptLibrary, const char* pcName, uint32_t uChannelCount, plAnimationComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed animation";
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plAnimationComponent* ptCompOut = gptECS->add_component(ptLibrary, gptAnimationCtx->tAnimationComponentType, tNewEntity);

    // size_t szAllocationSize = (sizeof(plAnimationChannel) + sizeof(plAnimationSampler) + sizeof(plEntity) + sizeof(plAnimationData)) * uChannelCount;
    // ptCompOut->uChannelCount = uChannelCount;
    // ptCompOut->puRawData = PL_ALLOC(szAllocationSize);
    // memset(ptCompOut->puRawData, 0, szAllocationSize);
    // ptCompOut->atChannels = (plAnimationChannel*)ptCompOut->puRawData;
    // ptCompOut->atSamplers = (plAnimationSampler*)&ptCompOut->atChannels[uChannelCount];
    // ptCompOut->atTargets = (plEntity*)&ptCompOut->atSamplers[uChannelCount];
    // ptCompOut->atData = (plAnimationData*)&ptCompOut->atTargets[uChannelCount];

    size_t szAllocationSize = uChannelCount * sizeof(plEntity);
    ptCompOut->atTargets = PL_ALLOC(szAllocationSize);
    memset(ptCompOut->atTargets, 0, szAllocationSize);

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;
}

void
pl_animation_run_animation_update_system(plComponentLibrary* ptLibrary, float fDeltaTime)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plAnimationComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptAnimationCtx->tAnimationComponentType, (void**)&ptComponents, NULL);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plAnimationComponent* ptAnimationComponent = &ptComponents[i];

        if(!(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_PLAYING))
            continue;

        plAnimation* ptAnimation = gptAsset->get_animation(ptAnimationComponent->tAnimation);

        ptAnimationComponent->fTimer += fDeltaTime * ptAnimationComponent->fSpeed;

        if(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_LOOPED)
        {
            ptAnimationComponent->fTimer = fmodf(ptAnimationComponent->fTimer, ptAnimation->fEnd);
        }

        if(ptAnimationComponent->fTimer > ptAnimation->fEnd)
        {
            ptAnimationComponent->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
            ptAnimationComponent->fTimer = 0.0f;
            continue;
        }

        for(uint32_t j = 0; j < ptAnimation->uChannelCount; j++)
        {
            
            const plAnimationChannel* ptChannel = &ptAnimation->atChannels[j];
            const plAnimationSampler* ptSampler = &ptAnimation->atSamplers[ptChannel->uSamplerIndex];
            const plAnimationData* ptData = &ptAnimation->atData[ptSampler->uDataIndex];
            plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, gptAnimationCtx->tTransformComponentType, ptAnimationComponent->atTargets[ptChannel->uTargetIndex]);
            ptTransform->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;

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

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

typedef struct _plAnimationDataHeader
{
    uint32_t uKeyFrameCount;
    size_t   szDataSize;
} plAnimationDataHeader;

typedef struct _plAnimationFileHeader
{
    uint32_t uMagic;
    uint32_t uVersion;
    uint32_t uFlags;
    float    fStart;
    float    fEnd;
    uint32_t uChannelCount;
    uint64_t uFileSize;
} plAnimationFileHeader;

void
pl_animation_serialize(const char* pcName, const plAnimation* ptAnimation)
{
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ_WRITE);

    plAnimationFileHeader tHeader = {
        .uMagic = 0x494E4150, // "PANI"
        .uVersion = 1,
        .uFlags = 0,
        .fStart = ptAnimation->fStart,
        .fEnd = ptAnimation->fEnd,
        .uChannelCount = ptAnimation->uChannelCount
    };
    // tHeader.uFileSize = sizeof(tHeader) + ptAnimation->uChannelCount * (sizeof(plAnimationChannel) + sizeof(plAnimationSampler) + sizeof(plAnimationDataHeader));
    // for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
    // {
    //     tHeader.uFileSize += ptAnimation->atData[i].szDataSize;
    //     tHeader.uFileSize += ptAnimation->atData[i].uKeyFrameCount * sizeof(float);
    // }

    size_t uBlah = ptAnimation->uChannelCount * (sizeof(plAnimationChannel) + sizeof(plAnimationSampler) + sizeof(plAnimationData));
    gptVfs->write_file_stream(tFileHandle, 1, sizeof(tHeader), &tHeader);
    gptVfs->write_file_stream(tFileHandle, 1, uBlah, ptAnimation->puRawData);

    for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
    {
        plAnimationDataHeader tDataHeader = {
            .szDataSize = ptAnimation->atData[i].szDataSize,
            .uKeyFrameCount = ptAnimation->atData[i].uKeyFrameCount
        };
        gptVfs->write_file_stream(tFileHandle, 1, sizeof(plAnimationDataHeader), &tDataHeader);
        gptVfs->write_file_stream(tFileHandle, 1, ptAnimation->atData[i].uKeyFrameCount * sizeof(float), ptAnimation->atData[i].afKeyFrameTimes);
        gptVfs->write_file_stream(tFileHandle, 1, ptAnimation->atData[i].szDataSize, ptAnimation->atData[i].pKeyFrameData);
    }
    gptVfs->close_file(tFileHandle);
}

void
pl_animation_destroy(plAnimation* ptAnimation)
{
    for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
    {
 
        PL_FREE(ptAnimation->atData[i].afKeyFrameTimes);
        ptAnimation->atData[i].afKeyFrameTimes = NULL;
        ptAnimation->atData[i].pKeyFrameData = NULL;
        
    }

    if(ptAnimation->puRawData)
    {
        PL_FREE(ptAnimation->puRawData);
        ptAnimation->puRawData = NULL;
        ptAnimation->atData = NULL;
        ptAnimation->atSamplers = NULL;
        ptAnimation->atChannels = NULL;
    }
    ptAnimation->uChannelCount = 0;
    ptAnimation->fEnd = 0.0f;
    ptAnimation->fStart = 0.0f;
}

void
pl_animation_deserialize(const char* pcName, plAnimation* ptAnimation)
{
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plAnimationFileHeader tHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(tHeader), 1, &tHeader);


    size_t szAllocationSize = (sizeof(plAnimationChannel) + sizeof(plAnimationSampler) + sizeof(plAnimationData)) * tHeader.uChannelCount;
    ptAnimation->fStart = tHeader.fStart;
    ptAnimation->fEnd = tHeader.fEnd;
    ptAnimation->uChannelCount = tHeader.uChannelCount;
    ptAnimation->puRawData = PL_ALLOC(szAllocationSize);
    memset(ptAnimation->puRawData, 0, szAllocationSize);
    ptAnimation->atChannels = (plAnimationChannel*)ptAnimation->puRawData;
    ptAnimation->atSamplers = (plAnimationSampler*)&ptAnimation->atChannels[tHeader.uChannelCount];
    ptAnimation->atData = (plAnimationData*)&ptAnimation->atSamplers[tHeader.uChannelCount];
    gptVfs->read_file_stream(tFileHandle, szAllocationSize, 1, ptAnimation->puRawData);
    
    for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
    {
        plAnimationDataHeader tDataHeader = {0};
        gptVfs->read_file_stream(tFileHandle, sizeof(tDataHeader), 1, &tDataHeader);

        ptAnimation->atData[i].uKeyFrameCount = tDataHeader.uKeyFrameCount;
        ptAnimation->atData[i].szDataSize = tDataHeader.szDataSize;
        ptAnimation->atData[i].afKeyFrameTimes = PL_ALLOC(sizeof(float) * tDataHeader.uKeyFrameCount + ptAnimation->atData[i].szDataSize);
        ptAnimation->atData[i].pKeyFrameData = (void*)&ptAnimation->atData[i].afKeyFrameTimes[tDataHeader.uKeyFrameCount];

        gptVfs->read_file_stream(tFileHandle, sizeof(float) * tDataHeader.uKeyFrameCount, 1, ptAnimation->atData[i].afKeyFrameTimes);
        gptVfs->read_file_stream(tFileHandle, ptAnimation->atData[i].szDataSize, 1, ptAnimation->atData[i].pKeyFrameData);
    }

    gptVfs->close_file(tFileHandle);
}

void
pl_animation_register_ecs_system(void)
{

    gptAnimationCtx->tTransformComponentType = gptTransform->get_ecs_type_key_transform();
    gptAnimationCtx->tHierarchyComponentType = gptTransform->get_ecs_type_key_hierarchy();

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

void
pl_load_animation_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAnimationI tApi = {
        .destroy                     = pl_animation_destroy,
        .serialize                   = pl_animation_serialize,
        .deserialize                 = pl_animation_deserialize,
        .register_ecs_system         = pl_animation_register_ecs_system,
        .create                      = pl_animation_create,
        .run_animation_update_system = pl_animation_run_animation_update_system,
        .get_ecs_type_key_animation  = pl_animation_get_ecs_type_key_animation,
        .get_ecs_type_key_humanoid   = pl_animation_get_ecs_type_key_humanoid,
    };
    pl_set_api(ptApiRegistry, plAnimationI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptECS = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog = pl_get_api_latest(ptApiRegistry, plLogI);
    gptAsset = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptVfs = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptTransform = pl_get_api_latest(ptApiRegistry, plTransformI);
    #endif

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

void
pl_unload_animation_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plAnimationI* ptApi = pl_get_api_latest(ptApiRegistry, plAnimationI);
    ptApiRegistry->remove_api(ptApi);
}