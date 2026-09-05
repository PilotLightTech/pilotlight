/*
   pl_animation_ext.c
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
// [SECTION] unity build
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
#include "pl_json_ext.h"

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

    #ifndef PL_JSON_ALLOC
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_JSON_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plProfileI*   gptProfile   = NULL;
    static const plLogI*       gptLog       = NULL;
    static const plEcsI*       gptEcs       = NULL;
    static const plAssetI*     gptAsset     = NULL;
    static const plVfsI*       gptVfs       = NULL;
    static const plTransformI* gptTransform = NULL;
    static const plJsonI*     gptJson = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAnimationDataHeader
{
    uint32_t uKeyFrameCount;
    size_t   szDataSize;
} plAnimationDataHeader;

typedef struct _plAnimationFileHeader
{
    uint32_t uFlags;
    float    fStart;
    float    fEnd;
    uint32_t uChannelCount;
    uint32_t uDataCount;
    uint64_t uFileSize;
} plAnimationFileHeader;

typedef struct _plAnimationContext
{
    plEcsTypeKey tAnimationComponentType;
    plEcsTypeKey tHumanoidComponentType;
    plEcsTypeKey tTransformComponentType;
    plEcsTypeKey tHierarchyComponentType;

    plAssetTypeKey tAssetTypeKey;
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
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptAnimationCtx->tAnimationComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        PL_FREE(ptComponents[i].atTargets);
        PL_FREE(ptComponents[i].atTargetIds);
        ptComponents[i].atTargets = NULL;
        ptComponents[i].uTargetCount = 0;
    }
}

plEntity
pl_animation_create(plComponentLibrary* ptLibrary, const char* pcName, uint32_t uChannelCount, plAnimationComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed animation";
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);

    plAnimationComponent* ptCompOut = gptEcs->add_component(ptLibrary, gptAnimationCtx->tAnimationComponentType, tNewEntity);

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
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptAnimationCtx->tAnimationComponentType, (void**)&ptComponents, NULL);

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plAnimationComponent* ptAnimationComponent = &ptComponents[i];

        if(!(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_PLAYING))
            continue;

        plAnimation* ptAnimation = gptAsset->get_data(ptAnimationComponent->tAnimation);

        ptAnimationComponent->fTimer += fDeltaTime * ptAnimationComponent->fSpeed;

        if(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_LOOPED)
        {
            const float fDuration = ptAnimation->fEnd - ptAnimation->fStart;
            ptAnimationComponent->fTimer = ptAnimation->fStart + fmodf(ptAnimationComponent->fTimer - ptAnimation->fStart, fDuration);
        }

        if(ptAnimationComponent->fTimer > ptAnimation->fEnd)
        {
            ptAnimationComponent->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
            ptAnimationComponent->fTimer = ptAnimation->fStart;
            continue;
        }

        for(uint32_t j = 0; j < ptAnimation->uChannelCount; j++)
        {
            
            const plAnimationChannel* ptChannel = &ptAnimation->atChannels[j];
            const plAnimationData* ptData = &ptAnimation->atData[ptChannel->uDataIndex];
            plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary, gptAnimationCtx->tTransformComponentType, ptAnimationComponent->atTargets[ptChannel->uTargetIndex]);
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

                    if(ptChannel->tMode == PL_ANIMATION_MODE_LINEAR)
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

                    else if(ptChannel->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        const plVec3 tTranslation = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                        ptTransform->tTranslation = pl_lerp_vec3(ptTransform->tTranslation, tTranslation, ptAnimationComponent->fBlendAmount);
                    }

                    else if(ptChannel->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
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

                    plVec3 tScale = {0};
                    if(ptChannel->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec3 tPrev = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                        const plVec3 tNext = *(plVec3*)&afKeyFrameData[iNextKey * 3];
                        tScale = (plVec3){
                            .x = tPrev.x * (1.0f - fTn) + tNext.x * fTn,
                            .y = tPrev.y * (1.0f - fTn) + tNext.y * fTn,
                            .z = tPrev.z * (1.0f - fTn) + tNext.z * fTn,
                        };
                        ptTransform->tScale = pl_lerp_vec3(ptTransform->tScale, tScale, ptAnimationComponent->fBlendAmount);
                    }

                    else if(ptChannel->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        tScale = *(plVec3*)&afKeyFrameData[iPrevKey * 3];
                        ptTransform->tScale = pl_lerp_vec3(ptTransform->tScale, tScale, ptAnimationComponent->fBlendAmount);
                    }

                    else if(ptChannel->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
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
                            tScale.d[k] = ((2 * fTCub - 3 * fTSq + 1) * v0) + ((fTCub - 2 * fTSq + fTn) * b) + ((-2 * fTCub + 3 * fTSq) * v1) + ((fTCub - fTSq) * a);
                        }
                        ptTransform->tScale = pl_lerp_vec3(ptTransform->tScale, tScale, ptAnimationComponent->fBlendAmount);
                    }
                    break;
                }

                case PL_ANIMATION_PATH_ROTATION:
                {

                    if(ptChannel->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec4 tQ0 = *(plVec4*)&afKeyFrameData[iPrevKey * 4];
                        const plVec4 tQ1 = *(plVec4*)&afKeyFrameData[iNextKey * 4];
                        const plVec4 tRotation = pl_quat_slerp(tQ0, tQ1, fTn);
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tRotation, ptAnimationComponent->fBlendAmount);
                    }
                    else if(ptChannel->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        const plVec4 tRotation = *(plVec4*)&afKeyFrameData[iPrevKey * 4];
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tRotation, ptAnimationComponent->fBlendAmount);
                    }
                    else if(ptChannel->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
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
                        ptTransform->tRotation = pl_norm_quat(ptTransform->tRotation);
                    }
                    break;
                }
            }
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static bool
pl__animation_serialize(const char* pcName, const void* pAnimation, plAssetEncoding eEncoding)
{

    const plAnimation* ptAnimation = pAnimation;
    if(eEncoding == PL_ASSET_ENCODING_BINARY)
    {
        plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ_WRITE);

        plAssetFileHeader tAssetFileHeader = {
            .uMagic = PL_ASSET_MAGIC
        };

        plAnimationFileHeader tHeader = {
            .uFlags        = 0,
            .fStart        = ptAnimation->fStart,
            .fEnd          = ptAnimation->fEnd,
            .uChannelCount = ptAnimation->uChannelCount,
            .uDataCount    = ptAnimation->uDataCount
        };

        size_t szRawDataSize = ptAnimation->uChannelCount * sizeof(plAnimationChannel);
        szRawDataSize += ptAnimation->uDataCount * sizeof(plAnimationData);

        gptVfs->write_file_stream(tFileHandle, 1, sizeof(plAssetFileHeader), &tAssetFileHeader);
        gptVfs->write_file_stream(tFileHandle, 1, sizeof(tHeader), &tHeader);
        gptVfs->write_file_stream(tFileHandle, 1, szRawDataSize, ptAnimation->puRawData);

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
    else
    {
        plJsonObject* ptRoot = gptJson->new_root_object("root");
        gptJson->add_string_member(ptRoot, "format", "planimation");
        gptJson->add_uint32_member(ptRoot, "version", 1);

        gptJson->add_float_member(ptRoot, "start", ptAnimation->fStart);
        gptJson->add_float_member(ptRoot, "end", ptAnimation->fEnd);

        plJsonObject* ptChannels = gptJson->add_member_array(ptRoot, "channels", ptAnimation->uChannelCount);
        for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
        {
            plJsonObject* ptChannel = gptJson->member_by_index(ptChannels, i);

            gptJson->add_uint32_member(ptChannel, "data", ptAnimation->atChannels[i].uDataIndex);
            gptJson->add_uint32_member(ptChannel, "target", ptAnimation->atChannels[i].uTargetIndex);
            if     (ptAnimation->atChannels[i].tPath == PL_ANIMATION_PATH_ROTATION)    gptJson->add_string_member(ptChannel, "path", "rotation");
            else if(ptAnimation->atChannels[i].tPath == PL_ANIMATION_PATH_TRANSLATION) gptJson->add_string_member(ptChannel, "path", "translation");
            else if(ptAnimation->atChannels[i].tPath == PL_ANIMATION_PATH_SCALE)       gptJson->add_string_member(ptChannel, "path", "scale");
            else if(ptAnimation->atChannels[i].tPath == PL_ANIMATION_PATH_WEIGHTS)     gptJson->add_string_member(ptChannel, "path", "weights");

            if     (ptAnimation->atChannels[i].tMode == PL_ANIMATION_MODE_LINEAR)       gptJson->add_string_member(ptChannel, "mode", "linear");
            else if(ptAnimation->atChannels[i].tMode == PL_ANIMATION_MODE_CUBIC_SPLINE) gptJson->add_string_member(ptChannel, "mode", "cubic");
            else if(ptAnimation->atChannels[i].tMode == PL_ANIMATION_MODE_STEP)         gptJson->add_string_member(ptChannel, "mode", "step");
        }

        plJsonObject* ptDatas = gptJson->add_member_array(ptRoot, "data", ptAnimation->uDataCount);
        for(uint32_t i = 0; i < ptAnimation->uDataCount; i++)
        {
            plJsonObject* ptData = gptJson->member_by_index(ptDatas, i);

            gptJson->add_float_array(ptData, "times", ptAnimation->atData[i].afKeyFrameTimes, ptAnimation->atData[i].uKeyFrameCount);
            gptJson->add_float_array(ptData, "data", (float*)ptAnimation->atData[i].pKeyFrameData, (uint32_t)(ptAnimation->atData[i].szDataSize / sizeof(float)));
        }

        uint32_t uBufferSize = 0;
        gptJson->write(ptRoot, NULL, &uBufferSize);
        char* pcBuffer = PL_ALLOC(uBufferSize);
        memset(pcBuffer, 0, uBufferSize);
        gptJson->write(ptRoot, pcBuffer, &uBufferSize);
        
        gptVfs->register_file(pcName, false);
        plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
        gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
        gptVfs->close_file(tFileHandle);

        PL_FREE(pcBuffer);
        gptJson->unload(&ptRoot);
    }
    return true;
}

static void
pl__animation_cleanup(void* pAnimation)
{
    plAnimation* ptAnimation = pAnimation;

    for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
    {
        if(ptAnimation->atData[i].afKeyFrameTimes)
        {
            PL_FREE(ptAnimation->atData[i].afKeyFrameTimes);
        }

        // if(ptAnimation->atData[i].pKeyFrameData)
        // {
        //     PL_FREE(ptAnimation->atData[i].pKeyFrameData);
        // }
        ptAnimation->atData[i].afKeyFrameTimes = NULL;
        ptAnimation->atData[i].pKeyFrameData = NULL;
        
    }

    if(ptAnimation->puRawData)
    {
        PL_FREE(ptAnimation->puRawData);
        ptAnimation->puRawData = NULL;
        ptAnimation->atData = NULL;
        ptAnimation->atChannels = NULL;
    }
    ptAnimation->uChannelCount = 0;
    ptAnimation->fEnd = 0.0f;
    ptAnimation->fStart = 0.0f;
}

static bool
pl__animation_deserialize(const char* pcName, void* pAnimation)
{
    plAnimation* ptAnimation = pAnimation;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plAssetFileHeader tAssetHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(plAssetFileHeader), 1, &tAssetHeader);

    if(tAssetHeader.uMagic == PL_ASSET_MAGIC)
    {

        plAnimationFileHeader tHeader = {0};
        gptVfs->read_file_stream(tFileHandle, sizeof(tHeader), 1, &tHeader);


        size_t szAllocationSize = tHeader.uChannelCount * sizeof(plAnimationChannel);
        szAllocationSize += tHeader.uDataCount * sizeof(plAnimationData);

        ptAnimation->fStart = tHeader.fStart;
        ptAnimation->fEnd = tHeader.fEnd;
        ptAnimation->uChannelCount = tHeader.uChannelCount;
        ptAnimation->puRawData = PL_ALLOC(szAllocationSize);
        memset(ptAnimation->puRawData, 0, szAllocationSize);
        ptAnimation->atChannels = (plAnimationChannel*)ptAnimation->puRawData;
        ptAnimation->atData = (plAnimationData*)&ptAnimation->atChannels[tHeader.uChannelCount];
        ptAnimation->uDataCount = tHeader.uDataCount;
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
    }
    else // json
    {
        char acTempBuffer[256] = {0};

        gptVfs->set_file_stream_position(tFileHandle, 0);
        size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);
        
        gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);

        plJsonObject* ptRoot = NULL;
        gptJson->load((const char*)puFileBuffer, &ptRoot);

        gptJson->array_member(ptRoot, "channels", &ptAnimation->uChannelCount);
        gptJson->array_member(ptRoot, "data", &ptAnimation->uDataCount);

        size_t szAllocationSize = ptAnimation->uChannelCount * sizeof(plAnimationChannel);
        szAllocationSize += ptAnimation->uDataCount * sizeof(plAnimationData);
        
        ptAnimation->fStart = gptJson->float_member(ptRoot, "start", 0.0f);
        ptAnimation->fEnd = gptJson->float_member(ptRoot, "end", 1.0f);
        ptAnimation->puRawData = PL_ALLOC(szAllocationSize);
        memset(ptAnimation->puRawData, 0, szAllocationSize);
        ptAnimation->atChannels = (plAnimationChannel*)ptAnimation->puRawData;
        ptAnimation->atData = (plAnimationData*)&ptAnimation->atChannels[ptAnimation->uChannelCount];

        plJsonObject* ptJsonChannels = gptJson->array_member(ptRoot, "channels", NULL);
        for(uint32_t i = 0; i < ptAnimation->uChannelCount; i++)
        {
            plJsonObject* ptJsonChannel = gptJson->member_by_index(ptJsonChannels, i);
            ptAnimation->atChannels[i].uTargetIndex = gptJson->uint32_member(ptJsonChannel, "target", UINT32_MAX);

            gptJson->string_member(ptJsonChannel, "path", acTempBuffer, 256);

            if(acTempBuffer[0] == 't')      ptAnimation->atChannels[i].tPath = PL_ANIMATION_PATH_TRANSLATION;
            else if(acTempBuffer[0] == 'r') ptAnimation->atChannels[i].tPath = PL_ANIMATION_PATH_ROTATION;
            else if(acTempBuffer[0] == 's') ptAnimation->atChannels[i].tPath = PL_ANIMATION_PATH_SCALE;
            else if(acTempBuffer[0] == 'w') ptAnimation->atChannels[i].tPath = PL_ANIMATION_PATH_WEIGHTS;

            ptAnimation->atChannels[i].uDataIndex = gptJson->uint32_member(ptJsonChannel, "data", UINT32_MAX);

            gptJson->string_member(ptJsonChannel, "mode", acTempBuffer, 256);

            if(acTempBuffer[0] == 'l')      ptAnimation->atChannels[i].tMode = PL_ANIMATION_MODE_LINEAR;
            else if(acTempBuffer[0] == 's') ptAnimation->atChannels[i].tMode = PL_ANIMATION_MODE_STEP;
            else if(acTempBuffer[0] == 'c') ptAnimation->atChannels[i].tMode = PL_ANIMATION_MODE_CUBIC_SPLINE;
        }

        plJsonObject* ptJsonDatas = gptJson->array_member(ptRoot, "data", NULL);
        for(uint32_t i = 0; i < ptAnimation->uDataCount; i++)
        {
            plJsonObject* ptJsonData = gptJson->member_by_index(ptJsonDatas, i);
            gptJson->float_array_member(ptJsonData, "times", NULL, &ptAnimation->atData[i].uKeyFrameCount);

            

            uint32_t uFloatCount = 0;
            gptJson->float_array_member(ptJsonData, "data", NULL, &uFloatCount);

            ptAnimation->atData[i].szDataSize = uFloatCount * sizeof(float);

            ptAnimation->atData[i].afKeyFrameTimes = PL_ALLOC(sizeof(float) * ptAnimation->atData[i].uKeyFrameCount + ptAnimation->atData[i].szDataSize);
            ptAnimation->atData[i].pKeyFrameData = (void*)&ptAnimation->atData[i].afKeyFrameTimes[ptAnimation->atData[i].uKeyFrameCount];

            gptJson->float_array_member(ptJsonData, "times", ptAnimation->atData[i].afKeyFrameTimes, &ptAnimation->atData[i].uKeyFrameCount);
            gptJson->float_array_member(ptJsonData, "data", (float*)ptAnimation->atData[i].pKeyFrameData, NULL);
        }

        PL_FREE(puFileBuffer);
    }

    gptVfs->close_file(tFileHandle);
    return true;
}

static void
pl__ecs_animation_serialize(void* pComponent, plJsonObject* ptJson)
{
    plAnimationComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "animation", gptAsset->get_path(ptComponent->tAnimation));
    gptJson->add_float_member(ptJson, "speed", ptComponent->fSpeed);
    gptJson->add_float_member(ptJson, "blend_amount", ptComponent->fBlendAmount);
    gptJson->add_bool_member(ptJson, "playing", ptComponent->tFlags & PL_ANIMATION_FLAG_PLAYING);
    gptJson->add_bool_member(ptJson, "looped", ptComponent->tFlags & PL_ANIMATION_FLAG_LOOPED);
    gptJson->add_uint64_array(ptJson, "targets", ptComponent->atTargetIds, ptComponent->uTargetCount);
}

static void
pl__ecs_animation_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plAnimationComponent* ptComponent = pComponent;
    if(gptJson->bool_member(ptJson, "playing", false)) ptComponent->tFlags |= PL_ANIMATION_FLAG_PLAYING;
    if(gptJson->bool_member(ptJson, "looped", false))  ptComponent->tFlags |= PL_ANIMATION_FLAG_LOOPED;

    ptComponent->fSpeed       = gptJson->float_member(ptJson, "speed", 1.0f);
    ptComponent->fBlendAmount = gptJson->float_member(ptJson, "blend_amount", 1.0f);

    char acTempBuffer0[1024] = {0};
    gptJson->string_member(ptJson, "animation", acTempBuffer0, 1024);
    ptComponent->tAnimation = gptAsset->load(acTempBuffer0);
    gptJson->uint64_array_member(ptJson, "targets", NULL, &ptComponent->uTargetCount);

    // animation target scene references
    ptComponent->atTargetIds = PL_ALLOC(ptComponent->uTargetCount * sizeof(plEntityId));
    gptJson->uint64_array_member(ptJson, "targets", ptComponent->atTargetIds, &ptComponent->uTargetCount);

}

void
pl_animation_register_ecs_components(void)
{

    gptAnimationCtx->tTransformComponentType = gptTransform->get_ecs_type_key_transform();
    gptAnimationCtx->tHierarchyComponentType = gptTransform->get_ecs_type_key_hierarchy();

    const plComponentDesc tAnimationDesc = {
        .pcDisplayName  = "Animation",
        .pcName  = "animation",
        .szSize  = sizeof(plAnimationComponent),
        .cleanup = pl__ecs_animation_cleanup,
        .reset   = pl__ecs_animation_cleanup,
        .serialize = pl__ecs_animation_serialize,
        .deserialize = pl__ecs_animation_deserialize,
    };

    static const plAnimationComponent tAnimationComponentDefault = {
        .fSpeed       = 1.0f,
        .fBlendAmount = 1.0f
    };
    gptAnimationCtx->tAnimationComponentType = gptEcs->register_type(tAnimationDesc, &tAnimationComponentDefault);

    const plComponentDesc tHumanoidDesc = {
        .pcDisplayName = "Humanoid",
        .pcName = "humanoid",
        .szSize = sizeof(plHumanoidComponent)
    };

    static plHumanoidComponent tHumanoidComponentDefault = {0};
    for(uint32_t i = 0; i < PL_HUMANOID_BONE_COUNT; i++)
    {
        tHumanoidComponentDefault.atBones[i].uData = UINT64_MAX;
    }
    gptAnimationCtx->tHumanoidComponentType = gptEcs->register_type(tHumanoidDesc, &tHumanoidComponentDefault);

}

void
pl_animation_register_asset_type(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName           = "Animation",
        .pcFileExtension  = "planimation",
        .eDefaultEncoding = PL_ASSET_ENCODING_BINARY,
        .szSize           = sizeof(plAnimation),
        .serialize        = pl__animation_serialize,
        .deserialize      = pl__animation_deserialize,
        .cleanup          = pl__animation_cleanup,
    };
    gptAnimationCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_animation_get_asset_type_key(void)
{
    return gptAnimationCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_animation_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAnimationI tApi = {
        .register_asset_types = pl_animation_register_asset_type,
        .get_asset_type_key = pl_animation_get_asset_type_key,
        .register_ecs_components         = pl_animation_register_ecs_components,
        .create                      = pl_animation_create,
        .run_animation_update_system = pl_animation_run_animation_update_system,
        .get_ecs_type_key_animation  = pl_animation_get_ecs_type_key_animation,
        .get_ecs_type_key_humanoid   = pl_animation_get_ecs_type_key_humanoid,
    };
    pl_set_api(ptApiRegistry, plAnimationI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptEcs       = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptProfile   = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog       = pl_get_api_latest(ptApiRegistry, plLogI);
    gptAsset     = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptVfs       = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptTransform = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptJson = pl_get_api_latest(ptApiRegistry, plJsonI);
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