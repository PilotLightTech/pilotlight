/*
   pl_skeleton_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <float.h> // FLT_MAX
#include "pl.h"
#include "pl_skeleton_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_log_ext.h"
#include "pl_vfs_ext.h"
#include "pl_resource_ext.h"
#include "pl_asset_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_ecs_ext.h"
#include "pl_profile_ext.h"
#include "pl_transform_ext.h"
#include "pl_json_ext.h"

// libraries
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_string.h"
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plLogI*          gptLog       = NULL;
    static const plVfsI*          gptVfs       = NULL;
    static const plResourceI*     gptResource  = NULL;
    static const plAssetI*        gptAsset     = NULL;
    static const plMemoryI*       gptMemory    = NULL;
    static const plStringInternI* gptString    = NULL;
    static const plEcsI*          gptEcs       = NULL;
    static const plProfileI*      gptProfile   = NULL;
    static const plTransformI*    gptTransform = NULL;
    static const plJsonI*         gptJson      = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)


    #ifndef PL_JSON_ALLOC
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_JSON_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSkeletonContext
{
    plAssetTypeKey tAssetTypeKeySkeleton;
    plAssetTypeKey tAssetTypeKeySkin;
    plEcsTypeKey tSkinComponentType;
} plSkeletonContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plSkeletonContext* gptSkeletonCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

static void
pl__renderer_skin_cleanup(plComponentLibrary* ptLibrary)
{
    plSkinComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptSkeletonCtx->tSkinComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        PL_FREE(ptComponents[i]._atTextureData);
        PL_FREE(ptComponents[i]._atJoints);
        // PL_FREE(ptComponents[i].atInverseBindMatrices); // same allocation as "atJoints"
        ptComponents[i]._atTextureData = NULL;
        ptComponents[i]._atJoints = NULL;
        ptComponents[i]._uJointCount = 0;
    }
}

static void
pl__ecs_skin_serialize(void* pComponent, plJsonObject* ptJson)
{
    plSkinComponent* ptComponent = pComponent;

    if(gptAsset->is_valid(ptComponent->tSkin))
    {
        gptJson->add_string_member(ptJson, "skin", gptAsset->get_path(ptComponent->tSkin));
    }
}

static void
pl__ecs_skin_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plSkinComponent* ptComponent = pComponent;

    char acTempBuffer0[1024] = {0};
    gptJson->string_member(ptJson, "skin", acTempBuffer0, 1024);
    ptComponent->tSkin = gptAsset->load(acTempBuffer0);
}

void
pl_skeleton_ecs_register_system(void)
{
    const plComponentDesc tSkinDesc = {
        .pcDisplayName  = "Skin",
        .pcName  = "skin",
        .szSize  = sizeof(plSkinComponent),
        .cleanup = pl__renderer_skin_cleanup,
        .reset   = pl__renderer_skin_cleanup,
        .serialize = pl__ecs_skin_serialize,
        .deserialize = pl__ecs_skin_deserialize,
    };

    gptSkeletonCtx->tSkinComponentType = gptEcs->register_type(tSkinDesc, NULL);
}

static bool
pl__skeleton_serialize(const char* pcName, const void* pSkeleton, plAssetEncoding eEncoding)
{
    const plSkeleton* ptSkeleton = pSkeleton;
    plJsonObject* ptRoot = gptJson->new_root_object("root");
    gptJson->add_string_member(ptRoot, "format", "plskeleton");
    gptJson->add_uint32_member(ptRoot, "version", 1);

    plJsonObject* ptJoints = gptJson->add_member_array(ptRoot, "joints", ptSkeleton->uJointCount);

    for(uint32_t i = 0; i < ptSkeleton->uJointCount; i++)
    {
        plJsonObject* ptJoint = gptJson->member_by_index(ptJoints, i);

        gptJson->add_string_member(ptJoint, "name", ptSkeleton->atJoints[i].pcName);
        gptJson->add_int_member(ptJoint, "parent", ptSkeleton->atJoints[i].uParent == UINT32_MAX ? -1 : (int)ptSkeleton->atJoints[i].uParent);
        gptJson->add_float_array(ptJoint, "translation", ptSkeleton->atJoints[i].tTranslation.d, 3);
        gptJson->add_float_array(ptJoint, "rotation", ptSkeleton->atJoints[i].tRotation.d, 4);
        gptJson->add_float_array(ptJoint, "scale", ptSkeleton->atJoints[i].tScale.d, 3);
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
    return true;
}

static bool
pl__skeleton_deserialize(const char* pcName, void* pSkeleton)
{
    plSkeleton* ptSkeleton = pSkeleton;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    plJsonObject* ptRoot = NULL;
    gptJson->load((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = gptJson->uint32_member(ptRoot, "version", 0);

    plJsonObject* ptJoints = gptJson->array_member(ptRoot, "joints", &ptSkeleton->uJointCount);
    ptSkeleton->atJoints = PL_ALLOC(ptSkeleton->uJointCount * sizeof(plSkeletonJoint));
    memset(ptSkeleton->atJoints, 0, ptSkeleton->uJointCount * sizeof(plSkeletonJoint));

    char acTempBuffer[256] = {0};

    for(uint32_t i = 0; i < ptSkeleton->uJointCount; i++)
    {
        plJsonObject* ptJoint = gptJson->member_by_index(ptJoints, i);

        gptJson->string_member(ptJoint, "name", acTempBuffer, 256);
        ptSkeleton->atJoints[i].pcName = gptString->intern(acTempBuffer);
        int iParent = gptJson->int_member(ptJoint, "parent", -1);
        ptSkeleton->atJoints[i].uParent = iParent == -1 ? UINT32_MAX : iParent;
        gptJson->float_array_member(ptJoint, "translation", ptSkeleton->atJoints[i].tTranslation.d, NULL);
        gptJson->float_array_member(ptJoint, "rotation", ptSkeleton->atJoints[i].tRotation.d, NULL);
        gptJson->float_array_member(ptJoint, "scale", ptSkeleton->atJoints[i].tScale.d, NULL);
    }

    PL_FREE(puFileBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static void
pl__skeleton_destroy(void* pSkeleton)
{
    plSkeleton* ptSkeleton = pSkeleton;

    if(ptSkeleton->atJoints)
    {
        PL_FREE(ptSkeleton->atJoints);
        ptSkeleton->atJoints = NULL;
    }
}

static bool
pl__skeleton_serialize_skin(const char* pcName, const void* pSkin, plAssetEncoding eEncoding)
{
    const plSkin* ptSkin = pSkin;

    plJsonObject* ptRoot = gptJson->new_root_object("root");
    gptJson->add_string_member(ptRoot, "format", "plskin");
    gptJson->add_uint32_member(ptRoot, "version", 1);

    const char* pcSkeletonName = gptAsset->get_path(ptSkin->tSkeleton);
    gptJson->add_string_member(ptRoot, "skeleton", pcSkeletonName);

    gptJson->add_uint64_array(ptRoot, "joints", ptSkin->atJoints, ptSkin->uJointCount);

    plJsonObject* ptInvBindMatrices = gptJson->add_member_array(ptRoot, "inverse_bind_matrices", ptSkin->uJointCount);

    if(ptSkin->atInverseBindMatrices)
    {
        for(uint32_t i = 0; i < ptSkin->uJointCount; i++)
        {
            plJsonObject* ptInvBindMatrix = gptJson->member_by_index(ptInvBindMatrices, i);
            gptJson->add_float_array(ptInvBindMatrix, "matrix", ptSkin->atInverseBindMatrices[i].d, 16);
        }
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
    return true;
}

static bool
pl__skeleton_deserialize_skin(const char* pcName, void* pSkin)
{
    plSkin* ptSkin = pSkin;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    plJsonObject* ptRoot = NULL;
    gptJson->load((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = gptJson->uint32_member(ptRoot, "version", 0);

    // load skeleton asset
    char acTempBuffer[256] = {0};
    gptJson->string_member(ptRoot, "skeleton", acTempBuffer, 256);
    ptSkin->tSkeleton = gptAsset->load(acTempBuffer);

    // load joints
    gptJson->uint64_array_member(ptRoot, "joints", NULL, &ptSkin->uJointCount);
    ptSkin->atJoints = PL_ALLOC(ptSkin->uJointCount * sizeof(plEntityId));
    memset(ptSkin->atJoints, 0, ptSkin->uJointCount * sizeof(plEntityId));
    gptJson->uint64_array_member(ptRoot, "joints", ptSkin->atJoints, &ptSkin->uJointCount);

    // load inverse bind matrices
    uint32_t uMatrixCount = 0;
    plJsonObject* ptInvBindMatrices = gptJson->array_member(ptRoot, "inverse_bind_matrices", &uMatrixCount);
    if(ptInvBindMatrices)
    {
        ptSkin->atInverseBindMatrices = PL_ALLOC(ptSkin->uJointCount * sizeof(plMat4));
        memset(ptSkin->atInverseBindMatrices, 0, ptSkin->uJointCount * sizeof(plMat4));

        for(uint32_t i = 0; i < ptSkin->uJointCount; i++)
        {
            plJsonObject* ptMatrix = gptJson->member_by_index(ptInvBindMatrices, i);
            gptJson->float_array_member(ptMatrix, "matrix", ptSkin->atInverseBindMatrices[i].d, NULL);
        }
    }
    PL_FREE(puFileBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static void
pl__skeleton_destroy_skin(void* pSkin)
{
    plSkin* ptSkin = pSkin;

    if(ptSkin->atInverseBindMatrices)
    {
        PL_FREE(ptSkin->atInverseBindMatrices);
        ptSkin->atInverseBindMatrices = NULL;
    }

    if(ptSkin->atJoints)
    {
        PL_FREE(ptSkin->atJoints);
        ptSkin->atJoints = NULL;
    }
}

plEcsTypeKey
pl_skeleton_ecs_get_type_key_skin(void)
{
    return gptSkeletonCtx->tSkinComponentType;
}

bool
pl_skeleton_ecs_bind_skin(plComponentLibrary* ptLibrary, plEntity tEntity, const plEntity* atJoints, uint32_t uJointCount)
{
    plSkinComponent* ptComponent = gptEcs->get_component(ptLibrary, gptSkeletonCtx->tSkinComponentType, tEntity);

    if(!ptComponent)
        return false;

    plSkin* ptSkin = gptAsset->get_data(ptComponent->tSkin);

    if(!ptSkin || uJointCount != ptSkin->uJointCount)
        return false;

    PL_FREE(ptComponent->_atJoints);
    PL_FREE(ptComponent->_atTextureData);

    ptComponent->_atJoints = PL_ALLOC(sizeof(plEntity) * uJointCount);
    ptComponent->_atTextureData = PL_ALLOC(sizeof(plMat4) * uJointCount * 2);

    ptComponent->_uJointCount = uJointCount;

    memcpy(ptComponent->_atJoints, atJoints, sizeof(plEntity) * uJointCount);

    return true;
}

void
pl_skeleton_ecs_run_skin_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plSkinComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptSkeletonCtx->tSkinComponentType, (void**)&ptComponents, &ptEntities);

    const plEcsTypeKey tTransformComponentType = gptTransform->get_ecs_type_key_transform();

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plSkinComponent* ptSkinComponent = &ptComponents[i];
        plSkin* ptSkin = gptAsset->get_data(ptSkinComponent->tSkin);

        if(ptSkinComponent->_uJointCount == 0 ||
            ptSkinComponent->_atJoints == NULL ||
            ptSkinComponent->_atTextureData == NULL)
        {
            continue;
        }

        PL_ASSERT(ptSkinComponent->_uJointCount == ptSkin->uJointCount);

        // calculate AABB
        ptSkinComponent->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
        ptSkinComponent->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};

        plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary, tTransformComponentType, ptEntities[i]);
        if(ptTransform)
        {
            plMat4 tInverseWorldTransform = pl_mat4_invert(&ptTransform->tWorld);
            for(uint32_t j = 0; j < ptSkinComponent->_uJointCount; j++)
            {
                plEntity tJointEntity = ptSkinComponent->_atJoints[j];
                plTransformComponent* ptJointComponent = gptEcs->get_component(ptLibrary, tTransformComponentType, tJointEntity);

                const plMat4* ptIBM = &ptSkin->atInverseBindMatrices[j];

                plMat4 tJointMatrix = pl_mul_mat4_3(&tInverseWorldTransform, &ptJointComponent->tWorld, ptIBM);

                plMat4 tInvertJoint = pl_mat4_invert(&tJointMatrix);
                plMat4 tNormalMatrix = pl_mat4_transpose(&tInvertJoint);
                ptSkinComponent->_atTextureData[j*2] = tJointMatrix;
                ptSkinComponent->_atTextureData[j*2 + 1] = tNormalMatrix;

                plVec3 tBonePos = ptJointComponent->tWorld.col[3].xyz;

                const float fBoneRadius = 1.0f;
                plAABB tBoneAABB = {
                    .tMin = {tBonePos.x - fBoneRadius, tBonePos.y - fBoneRadius, tBonePos.z - fBoneRadius},
                    .tMax = {tBonePos.x + fBoneRadius, tBonePos.y + fBoneRadius, tBonePos.z + fBoneRadius},
                };

                if(tBoneAABB.tMin.x < ptSkinComponent->tAABB.tMin.x) ptSkinComponent->tAABB.tMin.x = tBoneAABB.tMin.x;
                if(tBoneAABB.tMin.y < ptSkinComponent->tAABB.tMin.y) ptSkinComponent->tAABB.tMin.y = tBoneAABB.tMin.y;
                if(tBoneAABB.tMin.z < ptSkinComponent->tAABB.tMin.z) ptSkinComponent->tAABB.tMin.z = tBoneAABB.tMin.z;
                if(tBoneAABB.tMax.x > ptSkinComponent->tAABB.tMax.x) ptSkinComponent->tAABB.tMax.x = tBoneAABB.tMax.x;
                if(tBoneAABB.tMax.y > ptSkinComponent->tAABB.tMax.y) ptSkinComponent->tAABB.tMax.y = tBoneAABB.tMax.y;
                if(tBoneAABB.tMax.z > ptSkinComponent->tAABB.tMax.z) ptSkinComponent->tAABB.tMax.z = tBoneAABB.tMax.z;
            }
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_skeleton_register_asset_types(void)
{
    static const plAssetTypeDesc tDesc0 = {
        .pcName          = "Skeleton",
        .pcFileExtension = "plskeleton",
        .szSize          = sizeof(plSkeleton),
        .serialize       = pl__skeleton_serialize,
        .deserialize     = pl__skeleton_deserialize,
        .cleanup         = pl__skeleton_destroy
    };
    gptSkeletonCtx->tAssetTypeKeySkeleton = gptAsset->register_type(tDesc0);

    static const plAssetTypeDesc tDesc1 = {
        .pcName          = "Skin",
        .pcFileExtension = "plskin",
        .szSize          = sizeof(plSkin),
        .serialize       = pl__skeleton_serialize_skin,
        .deserialize     = pl__skeleton_deserialize_skin,
        .cleanup         = pl__skeleton_destroy_skin
    };
    gptSkeletonCtx->tAssetTypeKeySkin = gptAsset->register_type(tDesc1);
}

plAssetTypeKey
pl_skeleton_get_asset_type_key_skeleton(void)
{
    return gptSkeletonCtx->tAssetTypeKeySkeleton;
}

plAssetTypeKey
pl_skeleton_get_asset_type_key_skin(void)
{
    return gptSkeletonCtx->tAssetTypeKeySkin;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_skeleton_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plSkeletonI tApi = {
        .register_asset_types   = pl_skeleton_register_asset_types,
        .get_asset_type_key_skeleton   = pl_skeleton_get_asset_type_key_skeleton,
        .get_asset_type_key_skin   = pl_skeleton_get_asset_type_key_skin,
        .bind_skin              = pl_skeleton_ecs_bind_skin,
        .get_ecs_type_key_skin  = pl_skeleton_ecs_get_type_key_skin,
        .run_skin_update_system = pl_skeleton_ecs_run_skin_update_system,
        .register_ecs_components    = pl_skeleton_ecs_register_system,
    };
    pl_set_api(ptApiRegistry, plSkeletonI, &tApi);

    gptLog       = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs       = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptResource  = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset     = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptString    = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptEcs       = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptProfile   = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptTransform = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptJson = pl_get_api_latest(ptApiRegistry, plJsonI);
    
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptSkeletonCtx = ptDataRegistry->get_data("plSkeletonContext");
    }
    else // first load
    {
        static plSkeletonContext tCtx = {0};
        gptSkeletonCtx = &tCtx;
        ptDataRegistry->set_data("plSkeletonContext", gptSkeletonCtx);
    }
}

void
pl_unload_skeleton_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plSkeletonI* ptApi = pl_get_api_latest(ptApiRegistry, plSkeletonI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif