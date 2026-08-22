/*
   pl_gltf_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal API
// [SECTION] implementation
// [SECTION] internal API implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include <stddef.h> // ptrdiff_t
#include "pl.h"
#include "pl_gltf_ext.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_animation_ext.h"
#include "pl_mesh_ext.h"
#include "pl_renderer_ext.h"
#include "pl_vfs_ext.h"
#include "pl_material_ext.h"
#include "pl_graphics_ext.h"
#include "pl_asset_ext.h"
#include "pl_image_ext.h"
#include "pl_transform_ext.h"
#include "pl_skeleton_ext.h"
#include "pl_string_intern_ext.h"

// shaders
#include "pl_shader_interop_renderer.h" // PL_MESH_FORMAT_FLAG_XXXX

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

    static const plEcsI*         gptECS         = NULL;
    static const plAnimationI*   gptAnimation   = NULL;
    static const plRendererI*    gptRenderer    = NULL;
    static const plRendererEcsI* gptRendererEcs = NULL;
    static const plMeshI*        gptMesh        = NULL;
    static const plVfsI*         gptVfs         = NULL;
    static const plMaterialI*    gptMaterial    = NULL;
    static const plAssetI*       gptAsset       = NULL;
    static const plImageI*       gptImage       = NULL;
    static const plTransformI*   gptTransform   = NULL;
    static const plSkeletonI*    gptSkeleton    = NULL;
    static const plStringInternI* gptString     = NULL;
#endif

#define CGLTF_MALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define CGLTF_FREE(x)   gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#include "pl_ds.h"

// misc
#include "cgltf.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plGltfLoadingData
{
    plComponentLibrary*   ptLibrary;
    plHashMap64           tNodeHashmap;
    plHashMap64           tSkinHashmap;
    plAssetHandle*        sbtMaterialEntities;
    plModelInstanceHandle tHandle;
} plGltfLoadingData;

typedef struct _plModelLoadedData
{
    plModelLoaderData tData;
    plHashMap64       tAnimationHashmap; // name to animation entity data
} plModelLoadedData;

typedef struct _plGltfContext
{
    plModelLoadedData* sbtModels;
    uint32_t*          sbtModelGenerations;
    uint32_t*          sbtModelFreeIndices;

    plGltfImportResult tNullResults;
    plHashMap64        tAssetHashmap;
} plGltfContext;

static plGltfContext* gptGltfCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// new import helpers
static plAssetHandle pl__import_gltf_texture         (const char* pcFileNameOnly, bool bSrgb, const cgltf_texture*, const char* pcDirectory, uint32_t uTextureIndex);
static plAssetHandle pl__import_gltf_material_texture(const char* pcFileNameOnly, bool bSrgb, plMaterialTexture*, const cgltf_texture_view*, const char* pcDirectory, uint32_t uTextureIndex);
static plAssetHandle pl__import_gltf_material   (cgltf_data*, const char* pcFileNameOnly, const char* pcDirectory, const cgltf_material*, uint32_t uMaterialIndex);
static uint32_t pl__import_gltf_material_textures_only(cgltf_data*, const char* pcFileNameOnly, const char* pcDirectory, const cgltf_material*, uint32_t uMaterialIndex, plAssetHandle* atAssetsOut);
static plAssetHandle pl__import_gltf_mesh(cgltf_data*, const char* pcFileNameOnly, const char* pcDirectory, const cgltf_mesh*, uint32_t uMeshIndex);
static plAssetHandle pl__import_gltf_animation(const char* pcFileNameOnly, const cgltf_animation*, uint32_t uAnimationIndex);

// internal gltf helpers
static void pl__refr_load_attributes(plSubmesh* ptMesh, const cgltf_primitive* ptPrimitive);
static void pl__refr_load_gltf_object(cgltf_data*, const char* pcPath, plModelInstanceHandle, plGltfLoadingData* ptSceneData, const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode);
static void pl__refr_load_gltf_animation(const char* pcPath, plGltfLoadingData* ptSceneData, plModelInstanceHandle, const cgltf_animation* ptAnimation, cgltf_data* ptGltfData);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

void
pl_gltf_free_data(plModelInstanceHandle tHandle)
{

    pl_sb_free(gptGltfCtx->sbtModels[tHandle.uIndex].tData.atObjects);
    pl_hm64_free(&gptGltfCtx->sbtModels[tHandle.uIndex].tAnimationHashmap);
    gptGltfCtx->sbtModels[tHandle.uIndex].tData.uObjectCount = 0;
    gptGltfCtx->sbtModels[tHandle.uIndex].tData.atObjects = NULL;
    gptGltfCtx->sbtModelGenerations[tHandle.uIndex]++;
    pl_sb_push(gptGltfCtx->sbtModelFreeIndices, tHandle.uIndex);
}

bool
pl_gltf_get_animation_by_name(plModelInstanceHandle tHandle, const char* pcName, plEntity* ptEntityOut)
{
    if(!pl_hm64_has_key_str(&gptGltfCtx->sbtModels[tHandle.uIndex].tAnimationHashmap, pcName))
        return false;

    (*ptEntityOut).uData = pl_hm64_lookup_str(&gptGltfCtx->sbtModels[tHandle.uIndex].tAnimationHashmap, pcName);
    return true;
}

static void
pl__load_mixamorig(const cgltf_node* ptJointNode, plHumanoidComponent* ptHumanoid, plEntity tTransformEntity)
{
    if (pl_str_equal(ptJointNode->name, "mixamorig:Hips"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_HIPS] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:Spine"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_SPINE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:Spine1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_CHEST] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:Spine2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_UPPER_CHEST] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:Neck"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_NECK] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:Head"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_HEAD] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftShoulder"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_SHOULDER] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightShoulder"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_SHOULDER] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftArm"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_UPPER_ARM] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightArm"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_UPPER_ARM] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftForeArm"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_LOWER_ARM] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightForeArm"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_LOWER_ARM] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHand"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_HAND] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHand"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_HAND] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandThumb1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_THUMB_METACARPAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandThumb1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_THUMB_METACARPAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandThumb2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_THUMB_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandThumb2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_THUMB_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandThumb3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_THUMB_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandThumb3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_THUMB_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandIndex1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_INDEX_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandIndex1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_INDEX_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandIndex2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_INDEX_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandIndex2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_INDEX_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandIndex3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_INDEX_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandIndex3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_INDEX_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandMiddle1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_MIDDLE_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandMiddle1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_MIDDLE_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandMiddle2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_MIDDLE_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandMiddle2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_MIDDLE_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandMiddle3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_MIDDLE_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandMiddle3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_MIDDLE_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandRing1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_RING_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandRing1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_RING_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandRing2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_RING_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandRing2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_RING_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandRing3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_RING_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandRing3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_RING_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandPinky1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_LITTLE_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandPinky1"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_LITTLE_PROXIMAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandPinky2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_LITTLE_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandPinky2"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_LITTLE_INTERMEDIATE] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftHandPinky3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_LITTLE_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightHandPinky3"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_LITTLE_DISTAL] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftUpLeg"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_UPPER_LEG] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightUpLeg"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_UPPER_LEG] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftLeg"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_LOWER_LEG] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightLeg"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_LOWER_LEG] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftFoot"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_FOOT] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightFoot"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_FOOT] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:LeftToeBase"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_LEFT_TOES] = tTransformEntity;
    else if (pl_str_equal(ptJointNode->name, "mixamorig:RightToeBase"))
        ptHumanoid->atBones[PL_HUMANOID_BONE_RIGHT_TOES] = tTransformEntity;
}

bool
pl_gltf_import(const char* pcPath, const plGltfImportOptions* ptOptions, plGltfImportResult* ptResults)
{
    if(!gptVfs->does_file_exist(pcPath))
        return false;

    static const plGltfImportOptions tDefaultOptions = {
        .eFlags = PL_GLTF_IMPORT_FLAGS_IMPORT_ALL_ASSETS
    };
    if(ptOptions == NULL)
        ptOptions = &tDefaultOptions;

    if(ptResults == NULL)
        ptResults = &gptGltfCtx->tNullResults;

    cgltf_options tGltfOptions = {0};
    cgltf_data* ptGltfData = NULL;

    // read in file
    size_t szFileSize = gptVfs->get_file_size_str(pcPath);
    uint8_t* pcBuffer = PL_ALLOC(szFileSize);
    memset(pcBuffer, 0, szFileSize);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, pcBuffer, &szFileSize);
    gptVfs->close_file(tFileHandle);

    char acDirectory[1024] = {0};
    pl_str_get_directory(pcPath, acDirectory, 1024);

    char acFileNameOnly[256] = {0};
    pl_str_get_file_name_only(pcPath, acFileNameOnly, 256);

    char acFilepath[1024] = {0};

    char acTempBuffer[256] = {0};

    cgltf_result tGltfResult = cgltf_parse(&tGltfOptions, pcBuffer, szFileSize, &ptGltfData);
    if(tGltfResult != cgltf_result_success)
        return false;

    tGltfResult = cgltf_load_buffers(&tGltfOptions, ptGltfData, gptVfs->get_real_path(tFileHandle));
    if(tGltfResult != cgltf_result_success)
        return false;

    size_t szTotalDataSize = 0;

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES)   szTotalDataSize += sizeof(plAssetHandle) * ptGltfData->textures_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS)  szTotalDataSize += sizeof(plAssetHandle) * ptGltfData->materials_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES)     szTotalDataSize += sizeof(plAssetHandle) * ptGltfData->meshes_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS) szTotalDataSize += sizeof(plAssetHandle) * ptGltfData->animations_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS)  szTotalDataSize += sizeof(plAssetHandle) * ptGltfData->skins_count;

    if(ptResults->_szDataSize < szTotalDataSize)
    {
        if(ptResults->_puData)
        {
            PL_FREE(ptResults->_puData);
        }
        ptResults->_puData = PL_ALLOC(szTotalDataSize);
        memset(ptResults->_puData, 0, szTotalDataSize);
        ptResults->_szDataSize = szTotalDataSize;
    }

    size_t szCurrentDataOffset = 0;

    plAssetHandle* sbtHandles = NULL;

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES)
    {
        
        if(ptGltfData->textures_count > 0)
        {
            // wire up pointers
            ptResults->atTextures = (plAssetHandle*)&ptResults->_puData[szCurrentDataOffset];
            szCurrentDataOffset += sizeof(plAssetHandle) * ptGltfData->textures_count;
        }
        ptResults->uTextureCount = (uint32_t)ptGltfData->textures_count;

        uint32_t uCurrentTexture = 0;

        // for(size_t szTextureIndex = 0; szTextureIndex < ptGltfData->textures_count; szTextureIndex++)
        // {
        //     const cgltf_texture* ptGltfTexture = &ptGltfData->textures[szTextureIndex];
        //     ptrdiff_t tTextureIndex = ptGltfTexture - ptGltfData->textures;
        //     uint32_t uTextureIndex = (uint32_t)tTextureIndex / (uint32_t)sizeof(cgltf_texture);
        //     ptResults->atTextures[szTextureIndex] = pl__import_gltf_texture(acFileNameOnly, false, ptGltfTexture, acDirectory, uTextureIndex);
        // }

        // have to go through materials since the textures alone don't allow us to
        // know whether they should be SRGB or not without the use case
        for(size_t szMaterialIndex = 0; szMaterialIndex < ptGltfData->materials_count; szMaterialIndex++)
        {
            const cgltf_material* ptGltfMaterial = &ptGltfData->materials[szMaterialIndex];

            ptrdiff_t tMaterialIndex = ptGltfMaterial - ptGltfData->materials;

            uint32_t uTextureCount = pl__import_gltf_material_textures_only(ptGltfData, acFileNameOnly, acDirectory, ptGltfMaterial, (uint32_t)tMaterialIndex, NULL);
            pl_sb_resize(sbtHandles, uTextureCount);
            pl__import_gltf_material_textures_only(ptGltfData, acFileNameOnly, acDirectory, ptGltfMaterial, (uint32_t)tMaterialIndex, sbtHandles);

            // only allow unique textures
            for(uint32_t i = 0; i < uTextureCount; i++)
            {
                if(!pl_hm_has_key(&gptGltfCtx->tAssetHashmap, sbtHandles[i].uData))
                {
                    pl_hm_insert(&gptGltfCtx->tAssetHashmap, sbtHandles[i].uData, uCurrentTexture);
                    ptResults->atTextures[uCurrentTexture] = sbtHandles[i];
                    uCurrentTexture++;
                }
            }

            pl_sb_reset(sbtHandles);
        }

    }
    pl_hm_free(&gptGltfCtx->tAssetHashmap);

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS)
    {
        if(ptGltfData->materials_count > 0)
        {
            // wire up pointers
            ptResults->atMaterials = (plAssetHandle*)&ptResults->_puData[szCurrentDataOffset];
            szCurrentDataOffset += sizeof(plAssetHandle) * ptGltfData->materials_count;
        }
        ptResults->uMaterialCount = (uint32_t)ptGltfData->materials_count;

        for(size_t szMaterialIndex = 0; szMaterialIndex < ptGltfData->materials_count; szMaterialIndex++)
        {
            const cgltf_material* ptGltfMaterial = &ptGltfData->materials[szMaterialIndex];
            ptrdiff_t tMaterialIndex = ptGltfMaterial - ptGltfData->materials;
            ptResults->atMaterials[szMaterialIndex] = pl__import_gltf_material(ptGltfData, acFileNameOnly, acDirectory, ptGltfMaterial, (uint32_t)tMaterialIndex);
        }
    }

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES)
    {
        if(ptGltfData->meshes_count > 0)
        {
            // wire up pointers
            ptResults->atMeshes = (plAssetHandle*)&ptResults->_puData[szCurrentDataOffset];
            szCurrentDataOffset += sizeof(plAssetHandle) * ptGltfData->meshes_count;
        }
        ptResults->uMeshCount = (uint32_t)ptGltfData->meshes_count;

        for(size_t szMeshIndex = 0; szMeshIndex < ptGltfData->meshes_count; szMeshIndex++)
        {
            const cgltf_mesh* ptGltfMesh = &ptGltfData->meshes[szMeshIndex];
            ptrdiff_t tMeshIndex = ptGltfMesh - ptGltfData->meshes;
            ptResults->atMeshes[szMeshIndex] = pl__import_gltf_mesh(ptGltfData, acFileNameOnly, acDirectory, ptGltfMesh, (uint32_t)tMeshIndex);
        }
    }

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS)
    {
        if(ptGltfData->animations_count > 0)
        {
            // wire up pointers
            ptResults->atAnimations = (plAssetHandle*)&ptResults->_puData[szCurrentDataOffset];
            szCurrentDataOffset += sizeof(plAssetHandle) * ptGltfData->animations_count;
        }
        ptResults->uAnimationCount = (uint32_t)ptGltfData->animations_count;

        for(size_t szAnimationIndex = 0; szAnimationIndex < ptGltfData->animations_count; szAnimationIndex++)
        {
            const cgltf_animation* ptGltfAnimation = &ptGltfData->animations[szAnimationIndex];
            ptrdiff_t tAnimationIndex = ptGltfAnimation - ptGltfData->animations;
            ptResults->atAnimations[szAnimationIndex] = pl__import_gltf_animation(acFileNameOnly, ptGltfAnimation, (uint32_t)tAnimationIndex);
        }
    }

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS)
    {
        if(ptGltfData->skins_count > 0)
        {
            // wire up pointers
            ptResults->atSkeletons = (plAssetHandle*)&ptResults->_puData[szCurrentDataOffset];
            szCurrentDataOffset += sizeof(plAssetHandle) * ptGltfData->skins_count;
        }
        ptResults->uSkeletonCount = (uint32_t)ptGltfData->skins_count;

        const cgltf_node** sbtTempNodes = NULL;

        for(size_t szSkinIndex = 0; szSkinIndex < ptGltfData->skins_count; szSkinIndex++)
        {
            const cgltf_skin* ptSkin = &ptGltfData->skins[szSkinIndex];

            plSkeleton tSkeleton = {0};
            tSkeleton.uJointCount = (uint32_t)ptSkin->joints_count;
            tSkeleton.atJoints     = PL_ALLOC(ptSkin->joints_count * sizeof(plSkeletonJoint));
            memset(tSkeleton.atJoints, 0, ptSkin->joints_count * sizeof(plSkeletonJoint));

            for(size_t szJointIndex = 0; szJointIndex < ptSkin->joints_count; szJointIndex++)
            {
                const cgltf_node* ptJointNode = ptSkin->joints[szJointIndex];
                tSkeleton.atJoints[szJointIndex].pcName = gptString->intern(NULL, ptJointNode->name);
                tSkeleton.atJoints[szJointIndex].uParent = UINT32_MAX;
                pl_sb_push(sbtTempNodes, ptJointNode);

                // transform defaults
                plMat4 tWorld = pl_identity_mat4();
                tSkeleton.atJoints[szJointIndex].tRotation    = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
                tSkeleton.atJoints[szJointIndex].tScale       = (plVec3){1.0f, 1.0f, 1.0f};
                tSkeleton.atJoints[szJointIndex].tTranslation = (plVec3){0.0f, 0.0f, 0.0f};

                if(ptJointNode->has_rotation)    memcpy(tSkeleton.atJoints[szJointIndex].tRotation.d, ptJointNode->rotation, sizeof(plVec4));
                if(ptJointNode->has_scale)       memcpy(tSkeleton.atJoints[szJointIndex].tScale.d, ptJointNode->scale, sizeof(plVec3));
                if(ptJointNode->has_translation) memcpy(tSkeleton.atJoints[szJointIndex].tTranslation.d, ptJointNode->translation, sizeof(plVec3));

                // must use provided matrix, otherwise calculate based on rot, scale, trans
                if(ptJointNode->has_matrix)
                {
                    memcpy(tWorld.d, ptJointNode->matrix, sizeof(plMat4));
                    pl_decompose_matrix(&tWorld, &tSkeleton.atJoints[szJointIndex].tScale, &tSkeleton.atJoints[szJointIndex].tRotation, &tSkeleton.atJoints[szJointIndex].tTranslation);
                }

                const cgltf_node* ptParent = ptJointNode->parent;
                while(ptParent)
                {
                    for(uint32_t i = 0; i < pl_sb_size(sbtTempNodes); i++)
                    {
                        if(sbtTempNodes[i] == ptParent)
                        {
                            tSkeleton.atJoints[szJointIndex].uParent = i;
                            ptParent = NULL;
                            break;
                        }
                    }
                    if(ptParent)
                        ptParent = ptParent->parent;
                }

                // // pass 1
                // for(i = 0; i < jointCount; i++)
                //     sbtTempNodes[i] = ptSkin->joints[i];

                // // pass 2
                // for(i = 0; i < jointCount; i++)
                // {
                //     const cgltf_node* parent = ptSkin->joints[i]->parent;

                //     while(parent)
                //     {
                //         for(j = 0; j < jointCount; j++)
                //         {
                //             if(ptSkin->joints[j] == parent)
                //             {
                //                 skeleton.joints[i].uParent = j;
                //                 goto found;
                //             }
                //         }

                //         parent = parent->parent;
                //     }

                // found:;
                // }
            }
            pl_sb_reset(sbtTempNodes);

            if(ptSkin->name)
            {
                pl_sprintf(acTempBuffer, "/assets/skeletons/%s.plskeleton", ptSkin->name);
            }
            else
            {
                ptrdiff_t tSkeletonIndex = ptSkin - ptGltfData->skins;
                pl_sprintf(acTempBuffer, "/assets/skeletons/%s_%u.plskeleton", acFileNameOnly, (uint32_t)tSkeletonIndex);
            }

            plSkeletonAssetDesc tAssetDesc = {
                .tDesc = {
                    .eType = PL_ASSET_TYPE_SKELETON,
                    .pcName = acTempBuffer
                },
                .ptSkeleton = &tSkeleton
            };
            ptResults->atSkeletons[szSkinIndex] = gptAsset->create_skeleton_asset(&tAssetDesc);
        }
        pl_sb_free(sbtTempNodes);
    }

    pl_sb_free(sbtHandles);
    sbtHandles = NULL;

    cgltf_free(ptGltfData);
    return true;
}

void
pl__prefill_nodes(cgltf_data* ptGltfData, const cgltf_node* ptNode, plComponentLibrary* ptLibrary, plGltfLoadingData* tLoadingData)
{
    for(size_t i = 0; i < ptNode->children_count; i++)
    {
        const cgltf_node* ptChildNode = ptNode->children[i];
        plEntity tRoot = gptTransform->create_transform(ptLibrary, ptChildNode->name, NULL);
        pl_hm_insert(&tLoadingData->tNodeHashmap, (uint64_t)ptChildNode, tRoot.uData);
        pl__prefill_nodes(ptGltfData, ptChildNode, ptLibrary, tLoadingData);
    }
}

plModelInstanceHandle
pl_gltf_load(plComponentLibrary* ptLibrary, const char* pcPath, const plMat4* ptTransform)
{

    pl_gltf_import(pcPath, NULL, NULL);

    plModelInstanceHandle tHandle = {0};
    if(pl_sb_size(gptGltfCtx->sbtModelFreeIndices) > 0)
    {
        tHandle.uIndex = pl_sb_pop(gptGltfCtx->sbtModelFreeIndices);
        
    }
    else
    {
        tHandle.uIndex = pl_sb_size(gptGltfCtx->sbtModels);
        pl_sb_add(gptGltfCtx->sbtModels);
        pl_sb_push(gptGltfCtx->sbtModelGenerations, 0);
    }
    tHandle.uGeneration = gptGltfCtx->sbtModelGenerations[tHandle.uIndex];

    plGltfLoadingData tLoadingData = {
        .ptLibrary = ptLibrary,
        .tHandle   = tHandle
    };
    cgltf_options tGltfOptions = {0};
    cgltf_data* ptGltfData = NULL;

    // read in file
    size_t szFileSize = gptVfs->get_file_size_str(pcPath);
    uint8_t* pcBuffer = PL_ALLOC(szFileSize);
    memset(pcBuffer, 0, szFileSize);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, pcBuffer, &szFileSize);
    gptVfs->close_file(tFileHandle);

    char acDirectory[1024] = {0};
    pl_str_get_directory(pcPath, acDirectory, 1024);

    cgltf_result tGltfResult = cgltf_parse(&tGltfOptions, pcBuffer, szFileSize, &ptGltfData);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    tGltfResult = cgltf_load_buffers(&tGltfOptions, ptGltfData, gptVfs->get_real_path(tFileHandle));
    PL_ASSERT(tGltfResult == cgltf_result_success);


    for(size_t i = 0; i < ptGltfData->scenes_count; i++)
    {
        const cgltf_scene* ptGScene = &ptGltfData->scenes[i];
        for(size_t j = 0; j < ptGScene->nodes_count; j++)
        {
            const cgltf_node* ptNode = ptGScene->nodes[j];
            plEntity tRoot = gptTransform->create_transform(ptLibrary, ptNode->name, NULL);
            pl_hm_insert(&tLoadingData.tNodeHashmap, (uint64_t)ptNode, tRoot.uData);
            pl__prefill_nodes(ptGltfData, ptNode, ptLibrary, &tLoadingData);
        }
    }

    char acFilepath[1024] = {0};

    char acTempBuffer[256] = {0};

    char acFileNameOnly[256] = {0};
    pl_str_get_file_name_only(pcPath, acFileNameOnly, 256);

    for(size_t szSkinIndex = 0; szSkinIndex < ptGltfData->skins_count; szSkinIndex++)
    {
        const cgltf_skin* ptSkin = &ptGltfData->skins[szSkinIndex];

        plSkinComponent* ptSkinComponent = NULL;
        plEntity tSkinEntity = gptRendererEcs->create_skin(ptLibrary, ptSkin->name, &ptSkinComponent);
        
        if(ptSkin->name)
        {
            pl_sprintf(acTempBuffer, "/assets/skeletons/%s.plskeleton", ptSkin->name);
        }
        else
        {
            ptrdiff_t tSkeletonIndex = ptSkin - ptGltfData->skins;
            pl_sprintf(acTempBuffer, "/assets/skeletons/%s_%u.plskeleton", acFileNameOnly, (uint32_t)tSkeletonIndex);
        }
        ptSkinComponent->tSkeleton = gptAsset->load(acTempBuffer);

        plSkeleton* ptSkeleton = gptAsset->get_skeleton(ptSkinComponent->tSkeleton);

        plTransformComponent* ptSkinTransform = gptECS->add_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tSkinEntity);

        ptSkinComponent->_atJoints = PL_ALLOC(ptSkeleton->uJointCount * (sizeof(plEntity) + sizeof(plMat4)));
        memset(ptSkinComponent->_atJoints, 0, ptSkeleton->uJointCount * (sizeof(plEntity) + sizeof(plMat4)));
        ptSkinComponent->atInverseBindMatrices = (plMat4*)&ptSkinComponent->_atJoints[ptSkeleton->uJointCount];
        ptSkinComponent->uJointCount = (uint32_t)ptSkeleton->uJointCount;

        for(uint32_t i = 0; i < ptSkeleton->uJointCount; i++)
        {
            plSkeletonJoint* ptJoint = &ptSkeleton->atJoints[i];
            const cgltf_node* ptJointNode = ptSkin->joints[i];
            uint64_t ulJoint = pl_hm_lookup(&tLoadingData.tNodeHashmap, (uint64_t)ptJointNode);
            PL_ASSERT(ulJoint != UINT64_MAX);
            plEntity tJoint = {.uData = ulJoint };
            ptSkinComponent->_atJoints[i] = tJoint;
            ptSkinComponent->atInverseBindMatrices[i] = pl_identity_mat4();
        }

        if(ptSkin->inverse_bind_matrices)
        {
            const cgltf_buffer_view* ptInverseBindMatrixView = ptSkin->inverse_bind_matrices->buffer_view;
            const char* pcBufferData = ptInverseBindMatrixView->buffer->data;
            cgltf_accessor_unpack_floats(ptSkin->inverse_bind_matrices, (cgltf_float*)ptSkinComponent->atInverseBindMatrices, ptSkin->joints_count * 16);
            // memcpy(ptSkinComponent->atInverseBindMatrices, &pcBufferData[ptInverseBindMatrixView->offset], sizeof(plMat4) * ptSkin->joints_count);
        }
        pl_hm_insert(&tLoadingData.tSkinHashmap, (uint64_t)ptSkin, tSkinEntity.uData);
    }

    for(size_t i = 0; i < ptGltfData->scenes_count; i++)
    {
        const cgltf_scene* ptGScene = &ptGltfData->scenes[i];
        for(size_t j = 0; j < ptGScene->nodes_count; j++)
        {
            const cgltf_node* ptNode = ptGScene->nodes[j];
            plEntity tRoot = {UINT32_MAX, UINT32_MAX};
            pl__refr_load_gltf_object(ptGltfData, pcPath, tHandle, &tLoadingData, acDirectory, tRoot, ptNode);
        }
    }

    for(size_t i = 0; i < ptGltfData->animations_count; i++)
    {
        const cgltf_animation* ptAnimation = &ptGltfData->animations[i];
        pl__refr_load_gltf_animation(pcPath, &tLoadingData, tHandle, ptAnimation, ptGltfData);
    }

    pl_hm_free(&tLoadingData.tNodeHashmap);
    pl_hm_free(&tLoadingData.tSkinHashmap);
    pl_sb_free(tLoadingData.sbtMaterialEntities);
    gptGltfCtx->sbtModels[tHandle.uIndex].tData.uObjectCount = pl_sb_size(gptGltfCtx->sbtModels[tHandle.uIndex].tData.atObjects);
    cgltf_free(ptGltfData);
    PL_FREE(pcBuffer);
    return tHandle;
}

const plModelLoaderData*
pl_gltf_get_objects(plModelInstanceHandle tHandle)
{
    return &gptGltfCtx->sbtModels[tHandle.uIndex].tData;
}

//-----------------------------------------------------------------------------
// [SECTION] internal API implementation
//-----------------------------------------------------------------------------

static plAssetHandle
pl__import_gltf_texture(const char* pcFileNameOnly, bool bSrgb, const cgltf_texture* ptGltfTexture, const char* pcDirectory, uint32_t uTextureIndex)
{
    char acTempBuffer[256] = {0};
    char acFilepath[1024] = {0};

    if(ptGltfTexture->name)
    {
        pl_sprintf(acTempBuffer, "/assets/textures/%s.pltexture", ptGltfTexture->name);
    }
    else
    {
        pl_sprintf(acTempBuffer, "/assets/textures/%s_%u.pltexture", pcFileNameOnly, uTextureIndex);
    }

    plTextureAssetDesc tAssetDesc = {
        .tDesc = {
            .eType        = PL_ASSET_TYPE_TEXTURE,
            .pcName       = acTempBuffer,
            .pcSourceFile = acFilepath
        },
        .bGenerateMips = true,
        .bCompress = true,
        .bSRGB = bSrgb
    };

    if(ptGltfTexture->image->buffer_view)
    {
        char* pucBufferData = ptGltfTexture->image->buffer_view->buffer->data;
        char* pucActualBuffer = &pucBufferData[ptGltfTexture->image->buffer_view->offset];

        int iOffset = 0;

        if(ptGltfTexture->name)
        {
            iOffset = pl_sprintf(acFilepath, "/cache/imports/%s.", ptGltfTexture->name);
        }
        else
        {
            iOffset = pl_sprintf(acFilepath, "/cache/imports/%s_%u.", pcFileNameOnly, uTextureIndex);
        }

        char* pcNext = acFilepath;
        pcNext += iOffset;
        
        pl_str_get_file_name_only(ptGltfTexture->image->mime_type, pcNext, 4);

        plImageInfo tImageInfo = {0};
        plImageWriteInfo tImageWrite = {0};
        if(gptImage->get_info((unsigned char*)pucActualBuffer, (int)ptGltfTexture->image->buffer_view->size, &tImageInfo))
        {
            tImageWrite.iWidth = tImageInfo.iWidth;
            tImageWrite.iHeight = tImageInfo.iHeight;
            tImageWrite.iComponents = tImageInfo.iChannels;

            char* pucBuffer = NULL;
            
            if(tImageInfo.bHDR)
            {
                tAssetDesc.eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
                tImageWrite.iByteStride = sizeof(float) * tImageInfo.iWidth;
                pucBuffer = (char*)gptImage->load_hdr((const unsigned char*)pucActualBuffer, (int)ptGltfTexture->image->buffer_view->size, &tImageInfo.iWidth, &tImageInfo.iHeight, &tImageInfo.iChannels, tImageInfo.iChannels);
            }
            else if(tImageInfo.b16Bit)
            {
                tAssetDesc.eFormat = PL_FORMAT_R16G16B16A16_UNORM;
                tImageWrite.iByteStride = sizeof(short) * tImageInfo.iWidth;
                pucBuffer = (char*)gptImage->load_16bit((const unsigned char*)pucActualBuffer, (int)ptGltfTexture->image->buffer_view->size, &tImageInfo.iWidth, &tImageInfo.iHeight, &tImageInfo.iChannels, tImageInfo.iChannels);
            }
            else
            {
                tAssetDesc.eFormat = PL_FORMAT_BC3_UNORM;
                tImageWrite.iByteStride = tImageInfo.iWidth;
                pucBuffer = (char*)gptImage->load((const unsigned char*)pucActualBuffer, (int)ptGltfTexture->image->buffer_view->size, &tImageInfo.iWidth, &tImageInfo.iHeight, &tImageInfo.iChannels, tImageInfo.iChannels);
            }
            gptImage->write(acFilepath, pucBuffer, &tImageWrite);
            gptImage->free(pucBuffer);
        }
    }
    else if(strncmp(ptGltfTexture->image->uri, "data:", 5) == 0)
    {
        PL_ASSERT(false && "currently don't support gltf with embedded data");
        // const char* comma = strchr(ptTexture->texture->image->uri, ',');

        // if (comma && comma - ptTexture->texture->image->uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
        // {
        //     cgltf_options tOptions = {0};
        //     ptMaterial->atTextureMaps[tSlot].acName[0] = (char)tSlot + 1;
        //     strcpy(&ptMaterial->atTextureMaps[tSlot].acName[1], ptGltfMaterial->name);
            
        //     void* outData = NULL;
        //     const char *base64 = comma + 1;
        //     const size_t szBufferLength = strlen(base64);
        //     size_t szSize = szBufferLength - szBufferLength / 4;
        //     if(szBufferLength >= 2)
        //     {
        //         szSize -= base64[szBufferLength - 2] == '=';
        //         szSize -= base64[szBufferLength - 1] == '=';
        //     }
        //     cgltf_result res = cgltf_load_buffer_base64(&tOptions, szSize, base64, &outData);
        //     PL_ASSERT(res == cgltf_result_success);
        //     ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptMaterial->atTextureMaps[tSlot].acName, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, outData, szSize);
        // }
    }
    else
    {
        strcpy(acFilepath, pcDirectory);
        pl_str_concatenate(acFilepath, ptGltfTexture->image->uri, acFilepath, 1024);

        plImageInfo tImageInfo = {0};
        if(gptImage->get_info_from_file(acFilepath, &tImageInfo))
        {
            if(tImageInfo.bHDR)
            {
                tAssetDesc.eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
            }
            else if(tImageInfo.b16Bit)
            {
                tAssetDesc.eFormat = PL_FORMAT_R16G16B16A16_UNORM;
            }
            else
            {
                tAssetDesc.eFormat = PL_FORMAT_BC3_UNORM;
            }
        }
    }
    return gptAsset->create_texture_asset(&tAssetDesc);
}

static plAssetHandle
pl__import_gltf_material_texture(const char* pcFileNameOnly, bool bSrgb, plMaterialTexture* ptMaterialTexture, const cgltf_texture_view* ptGltfTextureView, const char* pcDirectory, uint32_t uTextureIndex)
{
    plAssetHandle tAsset = pl__import_gltf_texture(pcFileNameOnly, bSrgb, ptGltfTextureView->texture, pcDirectory, uTextureIndex);
    if(ptMaterialTexture)
    {
        ptMaterialTexture->uUVSet = ptGltfTextureView->texcoord;
        ptMaterialTexture->tScale = (plVec2){1.0f, 1.0f};
        ptMaterialTexture->tOffset = (plVec2){0.0f, 0.0f};
        ptMaterialTexture->fRotation = 0.0f;

        if(ptGltfTextureView->has_transform)
        {
            ptMaterialTexture->fRotation = ptGltfTextureView->transform.rotation;
            ptMaterialTexture->tOffset.x = ptGltfTextureView->transform.offset[0];
            ptMaterialTexture->tOffset.y = ptGltfTextureView->transform.offset[1];
            ptMaterialTexture->tScale.x = ptGltfTextureView->transform.scale[0];
            ptMaterialTexture->tScale.y = ptGltfTextureView->transform.scale[1];
        }

        ptMaterialTexture->tTexture = tAsset;
    }
    return tAsset;
}

static uint32_t
pl__import_gltf_material_textures_only(cgltf_data* ptGltfData, const char* pcFileNameOnly, const char* pcDirectory, const cgltf_material* ptGltfMaterial, uint32_t uMaterialIndex, plAssetHandle* atAssetsOut)
{
    uint32_t uCurrentAssetCount = 0;

    if(atAssetsOut == NULL)
    {
        if(ptGltfMaterial->normal_texture.texture) uCurrentAssetCount++;
        if(ptGltfMaterial->emissive_texture.texture) uCurrentAssetCount++;
        if(ptGltfMaterial->occlusion_texture.texture) uCurrentAssetCount++;
        if(ptGltfMaterial->has_pbr_metallic_roughness)
        {
            if(ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture)         uCurrentAssetCount++;
            if(ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture) uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_clearcoat)
        {
            if(ptGltfMaterial->clearcoat.clearcoat_texture.texture)           uCurrentAssetCount++;
            if(ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture) uCurrentAssetCount++;
            if(ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture)    uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_sheen)
        {
            if(ptGltfMaterial->sheen.sheen_color_texture.texture)     uCurrentAssetCount++;
            if(ptGltfMaterial->sheen.sheen_roughness_texture.texture) uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_iridescence)
        {
            if(ptGltfMaterial->iridescence.iridescence_texture.texture)           uCurrentAssetCount++;
            if(ptGltfMaterial->iridescence.iridescence_thickness_texture.texture) uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_anisotropy)
        {
            if(ptGltfMaterial->anisotropy.anisotropy_texture.texture) uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_transmission)
        {
            if(ptGltfMaterial->transmission.transmission_texture.texture) uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_volume)
        {
            if(ptGltfMaterial->volume.thickness_texture.texture) uCurrentAssetCount++;
        }

        if(ptGltfMaterial->has_diffuse_transmission)
        {
            if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture.texture) uCurrentAssetCount++;
            if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture) uCurrentAssetCount++;
        }

        return uCurrentAssetCount;
    }
    
	if(ptGltfMaterial->normal_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->normal_texture.texture - ptGltfData->textures;
		atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->normal_texture, pcDirectory, (uint32_t)tTextureIndex);
    }

	if(ptGltfMaterial->emissive_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->emissive_texture.texture - ptGltfData->textures;
		atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, true, NULL, &ptGltfMaterial->emissive_texture, pcDirectory, (uint32_t)tTextureIndex);
    }

	if(ptGltfMaterial->occlusion_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->occlusion_texture.texture - ptGltfData->textures;
		atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->occlusion_texture, pcDirectory, (uint32_t)tTextureIndex);
    }

    if(ptGltfMaterial->has_pbr_metallic_roughness)
    {
        if(ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, true, NULL, &ptGltfMaterial->pbr_metallic_roughness.base_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_clearcoat)
    {
        if(ptGltfMaterial->clearcoat.clearcoat_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->clearcoat.clearcoat_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->clearcoat.clearcoat_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        
        if(ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->clearcoat.clearcoat_normal_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_sheen)
    {
        if(ptGltfMaterial->sheen.sheen_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->sheen.sheen_color_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->sheen.sheen_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->sheen.sheen_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->sheen.sheen_roughness_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->sheen.sheen_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_iridescence)
    {
        if(ptGltfMaterial->iridescence.iridescence_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->iridescence.iridescence_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->iridescence.iridescence_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        if(ptGltfMaterial->iridescence.iridescence_thickness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->iridescence.iridescence_thickness_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->iridescence.iridescence_thickness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_anisotropy)
    {
        if(ptGltfMaterial->anisotropy.anisotropy_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->anisotropy.anisotropy_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->anisotropy.anisotropy_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_transmission)
    {
        if(ptGltfMaterial->transmission.transmission_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->transmission.transmission_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->transmission.transmission_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_volume)
    {
        if(ptGltfMaterial->volume.thickness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->volume.thickness_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->volume.thickness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_diffuse_transmission)
    {
        if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture - ptGltfData->textures;
            atAssetsOut[uCurrentAssetCount++] = pl__import_gltf_material_texture(pcFileNameOnly, false, NULL, &ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }
    
    return uCurrentAssetCount;
}

static plAssetHandle
pl__import_gltf_animation(const char* pcFileNameOnly, const cgltf_animation* ptGltfAnimation, uint32_t uAnimationIndex)
{

    plAnimation tAnimation = {0};
    size_t szAllocationSize = (sizeof(plAnimationChannel) + sizeof(plAnimationSampler) + sizeof(plAnimationData)) * ptGltfAnimation->channels_count;
    tAnimation.uChannelCount = (uint32_t)ptGltfAnimation->channels_count;
    tAnimation.puRawData = PL_ALLOC(szAllocationSize);
    memset(tAnimation.puRawData, 0, szAllocationSize);
    tAnimation.atChannels = (plAnimationChannel*)tAnimation.puRawData;
    tAnimation.atSamplers = (plAnimationSampler*)&tAnimation.atChannels[ptGltfAnimation->channels_count];
    tAnimation.atData = (plAnimationData*)&tAnimation.atSamplers[ptGltfAnimation->channels_count];

    // load channels
    for(size_t i = 0; i < ptGltfAnimation->channels_count; i++)
    {
        const cgltf_animation_channel* ptChannel = &ptGltfAnimation->channels[i];
        plAnimationChannel tChannel = {.uSamplerIndex = (uint32_t)i};
        switch(ptChannel->target_path)
        {
            case cgltf_animation_path_type_translation:
                tChannel.tPath = PL_ANIMATION_PATH_TRANSLATION;
                break;
            case cgltf_animation_path_type_rotation:
                tChannel.tPath = PL_ANIMATION_PATH_ROTATION;
                break;
            case cgltf_animation_path_type_scale:
                tChannel.tPath = PL_ANIMATION_PATH_SCALE;
                break;
            case cgltf_animation_path_type_weights:
                tChannel.tPath = PL_ANIMATION_PATH_WEIGHTS;
                break;
            default:
                tChannel.tPath = PL_ANIMATION_PATH_UNKNOWN;

        }

        const cgltf_animation_sampler* ptSampler = ptChannel->sampler;
        plAnimationSampler tSampler = {0};

        switch(ptSampler->interpolation)
        {
            case cgltf_interpolation_type_linear:
                tSampler.tMode = PL_ANIMATION_MODE_LINEAR;
                break;
            case cgltf_interpolation_type_step:
                tSampler.tMode = PL_ANIMATION_MODE_STEP;
                break;
            case cgltf_interpolation_type_cubic_spline:
                tSampler.tMode = PL_ANIMATION_MODE_CUBIC_SPLINE;
                break;
            default:
                tSampler.tMode = PL_ANIMATION_MODE_UNKNOWN;
        }

        const uint32_t uKeyFrameCount = (uint32_t)ptSampler->input->count;
        uint32_t uKeyFrameDataComponents = 1;

        if(ptSampler->output->type == cgltf_type_vec3)
        {
            uKeyFrameDataComponents = 3;
        }
        else if(ptSampler->output->type == cgltf_type_vec4)
        {
            uKeyFrameDataComponents = 4;
        }

        tAnimation.atData[i].uKeyFrameCount = uKeyFrameCount;
        tAnimation.atData[i].szDataSize = sizeof(float) * uKeyFrameDataComponents * ptSampler->output->count;
        tAnimation.atData[i].afKeyFrameTimes = PL_ALLOC(sizeof(float) * uKeyFrameCount + tAnimation.atData[i].szDataSize);
        tAnimation.atData[i].pKeyFrameData = (void*)&tAnimation.atData[i].afKeyFrameTimes[uKeyFrameCount];
        tAnimation.fEnd = pl_maxf(tAnimation.fEnd, ptSampler->input->max[0]);
        
        const cgltf_buffer* ptInputBuffer = ptSampler->input->buffer_view->buffer;
        const cgltf_buffer* ptOutputBuffer = ptSampler->output->buffer_view->buffer;
        unsigned char* pucInputBufferStart = &((unsigned char*)ptInputBuffer->data)[ptSampler->input->buffer_view->offset + ptSampler->input->offset];
        unsigned char* pucOutputBufferStart = &((unsigned char*)ptOutputBuffer->data)[ptSampler->output->buffer_view->offset + ptSampler->output->offset];

        for(size_t j = 0; j < ptSampler->input->count; j++)
        {
            const float fValue = *(float*)&pucInputBufferStart[ptSampler->input->stride * j];
            tAnimation.atData[i].afKeyFrameTimes[j] = fValue;
        }

        if(ptSampler->output->type == cgltf_type_scalar)
        {
            float* afKeyFrameData = (float*)tAnimation.atData[i].pKeyFrameData;
            for(size_t j = 0; j < ptSampler->output->count; j++)
            {
                const float fValue0 = *(float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                afKeyFrameData[j] =  fValue0;
            }
        }
        else if(ptSampler->output->type == cgltf_type_vec3)
        {
            plVec3* atKeyFrameData = (plVec3*)tAnimation.atData[i].pKeyFrameData;
            for(size_t j = 0; j < ptSampler->output->count; j++)
            {
                float* fFloatData = (float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                atKeyFrameData[j].x = fFloatData[0];
                atKeyFrameData[j].y = fFloatData[1];
                atKeyFrameData[j].z = fFloatData[2];
            }
        }
        else if(ptSampler->output->type == cgltf_type_vec4)
        {
            plVec4* atKeyFrameData = (plVec4*)tAnimation.atData[i].pKeyFrameData;
            for(size_t j = 0; j < ptSampler->output->count; j++)
            {
                float* fFloatData = (float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                atKeyFrameData[j].x = fFloatData[0];
                atKeyFrameData[j].y = fFloatData[1];
                atKeyFrameData[j].z = fFloatData[2];
                atKeyFrameData[j].w = fFloatData[3];
            }
        }

        tChannel.uTargetIndex = (uint32_t)i;
        tSampler.uDataIndex = (uint32_t)i;
        tAnimation.atSamplers[i] = tSampler;
        tAnimation.atChannels[i] = tChannel;
    }

    char acFileNameOnly[128] = {0};

    char acBuffer[256] = {0};
    if(ptGltfAnimation->name)
    {
        pl_sprintf(acBuffer, "/assets/animations/%s.planim", ptGltfAnimation->name);
    }
    else
    {
        pl_sprintf(acBuffer, "/assets/animations/%s_%u.planim", acFileNameOnly, uAnimationIndex);
    }

    plAnimationAssetDesc tAssetDesc = {
        .tDesc = {
            .eType = PL_ASSET_TYPE_ANIMATION,
            .pcName = acBuffer
        },
        .ptAnimation = &tAnimation
    };
    return gptAsset->create_animation_asset(&tAssetDesc);
}

static plAssetHandle
pl__import_gltf_mesh(cgltf_data* ptGltfData, const char* pcFileNameOnly, const char* pcDirectory, const cgltf_mesh* ptGltfMesh, uint32_t uMeshIndex)
{
    plMesh tMesh = {0};

    plSubmeshAllocationDesc* sbtSubAllocs = NULL;
    pl_sb_resize(sbtSubAllocs, (uint32_t)ptGltfMesh->primitives_count);

    // get mesh data size needed
    for(size_t szPrimitiveIndex = 0; szPrimitiveIndex < ptGltfMesh->primitives_count; szPrimitiveIndex++)
    {
        
        const cgltf_primitive* ptPrimitive = &ptGltfMesh->primitives[szPrimitiveIndex];
        sbtSubAllocs[szPrimitiveIndex].szVertexCount = ptPrimitive->attributes[0].data->count;

        if(ptPrimitive->indices)
            sbtSubAllocs[szPrimitiveIndex].szIndexCount = (uint32_t)ptPrimitive->indices->count;

        for(size_t szAttributeIndex = 0; szAttributeIndex < ptPrimitive->attributes_count; szAttributeIndex++)
        {
            const cgltf_attribute* ptAttribute = &ptPrimitive->attributes[szAttributeIndex];

            switch(ptAttribute->type)
            {
                case cgltf_attribute_type_position:break;
                case cgltf_attribute_type_normal: 
                    sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL;
                    break;
                case cgltf_attribute_type_tangent:
                    sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT;
                    break;
                case cgltf_attribute_type_texcoord:
                    if(ptAttribute->index == 0 || ptAttribute->index == 1)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;
                    break;
                case cgltf_attribute_type_color:
                    if(ptAttribute->index == 0)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
                    else if(ptAttribute->index == 1)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1;
                    break;
                case cgltf_attribute_type_joints:
                    if(ptAttribute->index == 0)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0;
                    else if(ptAttribute->index == 1)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1;
                    break;
                case cgltf_attribute_type_weights:
                    if(ptAttribute->index == 0)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0;
                    else if(ptAttribute->index == 1)
                        sbtSubAllocs[szPrimitiveIndex].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1;
                    break;
                default:
                    break;
            }
        }
    }

    gptMesh->allocate(&tMesh, sbtSubAllocs, (uint32_t)ptGltfMesh->primitives_count);
    pl_sb_free(sbtSubAllocs);

    for(size_t szPrimitiveIndex = 0; szPrimitiveIndex < ptGltfMesh->primitives_count; szPrimitiveIndex++)
    {
        
        const cgltf_primitive* ptPrimitive = &ptGltfMesh->primitives[szPrimitiveIndex];

        // load attributes
        pl__refr_load_attributes(&tMesh.atSubmeshes[szPrimitiveIndex], ptPrimitive);

        for(uint32_t i = 0; i < tMesh.atSubmeshes[szPrimitiveIndex].szVertexCount; i++)
        {
            if(tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].x > tMesh.tAABB.tMax.x) tMesh.tAABB.tMax.x = tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].x;
            if(tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].y > tMesh.tAABB.tMax.y) tMesh.tAABB.tMax.y = tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].y;
            if(tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].z > tMesh.tAABB.tMax.z) tMesh.tAABB.tMax.z = tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].z;
            if(tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].x < tMesh.tAABB.tMin.x) tMesh.tAABB.tMin.x = tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].x;
            if(tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].y < tMesh.tAABB.tMin.y) tMesh.tAABB.tMin.y = tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].y;
            if(tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].z < tMesh.tAABB.tMin.z) tMesh.tAABB.tMin.z = tMesh.atSubmeshes[szPrimitiveIndex].ptVertexPositions[i].z;
        }

        // load material
        if(ptPrimitive->material)
        {
            ptrdiff_t tMaterialIndex = ptPrimitive->material - ptGltfData->materials;
            tMesh.atSubmeshes[szPrimitiveIndex].tMaterial = pl__import_gltf_material(ptGltfData, pcFileNameOnly, pcDirectory, ptPrimitive->material, (uint32_t)tMaterialIndex);
        }
    }

    char acBuffer[256] = {0};
    if(ptGltfMesh->name)
    {
        pl_sprintf(acBuffer, "/assets/meshes/%s.plmesh", ptGltfMesh->name);
    }
    else
    {
        pl_sprintf(acBuffer, "/assets/meshes/%s_%u.plmesh", pcFileNameOnly, uMeshIndex);
    }

    plMeshAssetDesc tMeshDesc = {
        .tDesc = {
            .pcName = acBuffer,
            .eType = PL_ASSET_TYPE_MESH
        },
        .ptMesh = &tMesh
    };
    return gptAsset->create_mesh_asset(&tMeshDesc);
}

static plAssetHandle
pl__import_gltf_material(cgltf_data* ptGltfData, const char* pcFileNameOnly, const char* pcDirectory, const cgltf_material* ptGltfMaterial, uint32_t uMaterialIndex)
{

    char acTempBuffer[256] = {0};
    char acFilepath[1024] = {0};

    if(ptGltfMaterial->name)
    {
        pl_sprintf(acTempBuffer, "/assets/materials/%s.plmaterial", ptGltfMaterial->name);
    }
    else
    {
        pl_sprintf(acTempBuffer, "/assets/materials/%s_%u.plmaterial", pcFileNameOnly, uMaterialIndex);
    }

    plMaterial tMaterial = {0};
    gptMaterial->init(&tMaterial);
    plMaterial* ptMaterial = &tMaterial;

    ptMaterial->eFlags |= ptGltfMaterial->double_sided ? PL_MATERIAL_FLAG_DOUBLE_SIDED : PL_MATERIAL_FLAG_NONE;
    ptMaterial->fAlphaCutoff = ptGltfMaterial->alpha_cutoff;
    
    if(ptGltfMaterial->has_ior)
    {
        ptMaterial->fIor = ptGltfMaterial->ior.ior;
    }

    // blend mode
    if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_opaque)
        ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_OPAQUE;
    else if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_blend)
        ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_BLEND;
    else
        ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_MASK;

	if(ptGltfMaterial->normal_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->normal_texture.texture - ptGltfData->textures;
		pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_NORMAL], &ptGltfMaterial->normal_texture, pcDirectory, (uint32_t)tTextureIndex);
        ptMaterial->fNormalMapStrength = ptGltfMaterial->normal_texture.scale;
    }

    if(ptGltfMaterial->has_emissive_strength)
        ptMaterial->fEmissiveStrength = ptGltfMaterial->emissive_strength.emissive_strength;
    ptMaterial->tEmissiveColor.r = ptGltfMaterial->emissive_factor[0];
    ptMaterial->tEmissiveColor.g = ptGltfMaterial->emissive_factor[1];
    ptMaterial->tEmissiveColor.b = ptGltfMaterial->emissive_factor[2];
    if(ptMaterial->tEmissiveColor.r != 0.0f || ptMaterial->tEmissiveColor.g != 0.0f || ptMaterial->tEmissiveColor.b != 0.0f)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_EMISSIVE;
    }
	if(ptGltfMaterial->emissive_texture.texture)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_EMISSIVE;
        ptrdiff_t tTextureIndex = ptGltfMaterial->emissive_texture.texture - ptGltfData->textures;
		pl__import_gltf_material_texture(pcFileNameOnly, true, &ptMaterial->atTextures[PL_TEXTURE_SLOT_EMISSIVE], &ptGltfMaterial->emissive_texture, pcDirectory, (uint32_t)tTextureIndex);
    }

	if(ptGltfMaterial->occlusion_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->occlusion_texture.texture - ptGltfData->textures;
		pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_OCCLUSION], &ptGltfMaterial->occlusion_texture, pcDirectory, (uint32_t)tTextureIndex);

        ptMaterial->fOcclusionStrength = ptGltfMaterial->occlusion_texture.scale;
    }

    if(ptGltfMaterial->has_pbr_metallic_roughness)
    {
        ptMaterial->eMaterialModel = PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS;
        ptMaterial->tBaseColor.x = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[0];
        ptMaterial->tBaseColor.y = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[1];
        ptMaterial->tBaseColor.z = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[2];
        ptMaterial->tBaseColor.w = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[3];

		ptMaterial->fMetalness = ptGltfMaterial->pbr_metallic_roughness.metallic_factor;
		ptMaterial->fRoughness = ptGltfMaterial->pbr_metallic_roughness.roughness_factor;

        if(ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, true, &ptMaterial->atTextures[PL_TEXTURE_SLOT_BASE_COLOR], &ptGltfMaterial->pbr_metallic_roughness.base_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_METAL_ROUGHNESS], &ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_clearcoat)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_CLEARCOAT;
        ptMaterial->tClearcoat.fFactor = ptGltfMaterial->clearcoat.clearcoat_factor;
        ptMaterial->tClearcoat.fRoughness = ptGltfMaterial->clearcoat.clearcoat_roughness_factor;
        if(ptGltfMaterial->clearcoat.clearcoat_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_CLEARCOAT], &ptGltfMaterial->clearcoat.clearcoat_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS], &ptGltfMaterial->clearcoat.clearcoat_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        
        if(ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_CLEARCOAT_NORMAL], &ptGltfMaterial->clearcoat.clearcoat_normal_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_sheen)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_SHEEN;
        ptMaterial->tSheen.fRoughness = ptGltfMaterial->sheen.sheen_roughness_factor;

        ptMaterial->tSheen.tColor.r = ptGltfMaterial->sheen.sheen_color_factor[0];
        ptMaterial->tSheen.tColor.g = ptGltfMaterial->sheen.sheen_color_factor[1];
        ptMaterial->tSheen.tColor.b = ptGltfMaterial->sheen.sheen_color_factor[2];

        if(ptGltfMaterial->sheen.sheen_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->sheen.sheen_color_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_SHEEN_COLOR], &ptGltfMaterial->sheen.sheen_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->sheen.sheen_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->sheen.sheen_roughness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_SHEEN_ROUGHNESS], &ptGltfMaterial->sheen.sheen_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_iridescence)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_IRIDESCENCE;
        ptMaterial->tIridescence.fFactor = ptGltfMaterial->iridescence.iridescence_factor;
        ptMaterial->tIridescence.fIor = ptGltfMaterial->iridescence.iridescence_ior;
        ptMaterial->tIridescence.fThicknessMin = ptGltfMaterial->iridescence.iridescence_thickness_min;
        ptMaterial->tIridescence.fThicknessMax = ptGltfMaterial->iridescence.iridescence_thickness_max;

        if(ptGltfMaterial->iridescence.iridescence_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->iridescence.iridescence_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_IRIDESCENCE], &ptGltfMaterial->iridescence.iridescence_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        if(ptGltfMaterial->iridescence.iridescence_thickness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->iridescence.iridescence_thickness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS], &ptGltfMaterial->iridescence.iridescence_thickness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_anisotropy)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_ANISOTROPY;
        ptMaterial->tAnisotropy.fRotation = ptGltfMaterial->anisotropy.anisotropy_rotation;
        ptMaterial->tAnisotropy.fStrength = ptGltfMaterial->anisotropy.anisotropy_strength;
        if(ptGltfMaterial->anisotropy.anisotropy_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->anisotropy.anisotropy_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_ANISOTROPY], &ptGltfMaterial->anisotropy.anisotropy_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_transmission)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_TRANSMISSION;
        ptMaterial->tTransmission.fFactor = ptGltfMaterial->transmission.transmission_factor;
        if(ptGltfMaterial->transmission.transmission_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->transmission.transmission_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_TRANSMISSION], &ptGltfMaterial->transmission.transmission_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_volume)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_VOLUME;
        ptMaterial->tVolume.fThickness = ptGltfMaterial->volume.thickness_factor;
        ptMaterial->tVolume.fAttenuationDistance = ptGltfMaterial->volume.attenuation_distance;
        ptMaterial->tVolume.tAttenuationColor.r = ptGltfMaterial->volume.attenuation_color[0];
        ptMaterial->tVolume.tAttenuationColor.g = ptGltfMaterial->volume.attenuation_color[1];
        ptMaterial->tVolume.tAttenuationColor.b = ptGltfMaterial->volume.attenuation_color[2];
        if(ptGltfMaterial->volume.thickness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->volume.thickness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_THICKNESS], &ptGltfMaterial->volume.thickness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_diffuse_transmission)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION;
        ptMaterial->tDiffuseTransmission.fFactor = ptGltfMaterial->diffuse_transmission.diffuse_transmission_factor;
        ptMaterial->tDiffuseTransmission.tColor.r = ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_factor[0];
        ptMaterial->tDiffuseTransmission.tColor.g = ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_factor[1];
        ptMaterial->tDiffuseTransmission.tColor.b = ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_factor[2];
        if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION], &ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION_COLOR], &ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    ptMaterial->tDispersion.fDispersion = 0;
    if(ptGltfMaterial->has_dispersion)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_DISPERSION;
        ptMaterial->tDispersion.fDispersion = ptGltfMaterial->dispersion.dispersion;
    }

    plMaterialAssetDesc tAssetDesc = {
        .ptMaterial = ptMaterial,
        .tDesc = {
            .eType = PL_ASSET_TYPE_MATERIAL,
            .pcName = acTempBuffer
        }
    };
    return gptAsset->create_material_asset(&tAssetDesc);
}

static void
pl__refr_load_attributes(plSubmesh* ptMesh, const cgltf_primitive* ptPrimitive)
{
    size_t szVertexCount = ptMesh->szVertexCount;
    for(size_t szAttributeIndex = 0; szAttributeIndex < ptPrimitive->attributes_count; szAttributeIndex++)
    {
        const cgltf_attribute* ptAttribute = &ptPrimitive->attributes[szAttributeIndex];
        const cgltf_buffer* ptBuffer = ptAttribute->data->buffer_view->buffer;
        const size_t szStride = ptAttribute->data->stride;
        PL_ASSERT(szStride > 0 && "attribute stride must node be zero");

        unsigned char* pucBufferStart = &((unsigned char*)ptBuffer->data)[ptAttribute->data->buffer_view->offset + ptAttribute->data->offset];

        switch(ptAttribute->type)
        {
            case cgltf_attribute_type_position:
            {
                ptMesh->tAABB.tMax = (plVec3){ptAttribute->data->max[0], ptAttribute->data->max[1], ptAttribute->data->max[2]};
                ptMesh->tAABB.tMin = (plVec3){ptAttribute->data->min[0], ptAttribute->data->min[1], ptAttribute->data->min[2]};

                if(szStride == sizeof(plVec3))
                {
                    memcpy(ptMesh->ptVertexPositions, pucBufferStart, sizeof(plVec3) * szVertexCount);
                }
                else
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec3* ptRawData = (plVec3*)&pucBufferStart[i * szStride];
                        ptMesh->ptVertexPositions[i] = *ptRawData;
                    }
                }
                break;
            }

            case cgltf_attribute_type_normal:
            {
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f && ptAttribute->data->type == cgltf_type_vec3)
                {
                    if(szStride == sizeof(plVec3))
                    {
                        memcpy(ptMesh->ptVertexNormals, pucBufferStart, sizeof(plVec3) * szVertexCount);
                    }
                    else
                    {
                        for(size_t i = 0; i < szVertexCount; i++)
                        {
                            plVec3* ptRawData = (plVec3*)&pucBufferStart[i * szStride];
                            ptMesh->ptVertexNormals[i] = *ptRawData;
                        }
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8 && ptAttribute->data->type == cgltf_type_vec3)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        int8_t* puRawData = (int8_t*)&pucBufferStart[i * szStride];
                        ptMesh->ptVertexNormals[i].x = (float)puRawData[0];
                        ptMesh->ptVertexNormals[i].y = (float)puRawData[1];
                        ptMesh->ptVertexNormals[i].z = (float)puRawData[2];
                    }
                }
                else
                {
                    PL_ASSERT(false);
                }
                break;
            }

            case cgltf_attribute_type_tangent:
            {
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f && ptAttribute->data->type == cgltf_type_vec4)
                {
                    if(szStride == sizeof(plVec4))
                    {
                        memcpy(ptMesh->ptVertexTangents, pucBufferStart, sizeof(plVec4) * szVertexCount);
                    }
                    else
                    {
                        for(size_t i = 0; i < szVertexCount; i++)
                        {
                            plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                            ptMesh->ptVertexTangents[i] = *ptRawData;
                        }
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8 && ptAttribute->data->type == cgltf_type_vec4)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        int8_t* puRawData = (int8_t*)&pucBufferStart[i * szStride];
                        ptMesh->ptVertexTangents[i].x = (float)puRawData[0];
                        ptMesh->ptVertexTangents[i].y = (float)puRawData[1];
                        ptMesh->ptVertexTangents[i].z = (float)puRawData[2];
                        ptMesh->ptVertexTangents[i].w = (float)puRawData[3];
                    }
                }
                else
                {
                    PL_ASSERT(false);
                }
                break;
            }

            case cgltf_attribute_type_texcoord:
            {
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    if(szStride == sizeof(plVec2))
                    {
                        memcpy((ptMesh->ptVertexTextureCoordinates[ptAttribute->index]), pucBufferStart, sizeof(plVec2) * szVertexCount);
                    }
                    else
                    {
                        for(size_t i = 0; i < szVertexCount; i++)
                        {
                            plVec2* ptRawData = (plVec2*)&pucBufferStart[i * szStride];
                            (ptMesh->ptVertexTextureCoordinates[ptAttribute->index])[i] = *ptRawData;
                        }
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexTextureCoordinates[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->ptVertexTextureCoordinates[ptAttribute->index])[i].y = (float)puRawData[1];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexTextureCoordinates[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->ptVertexTextureCoordinates[ptAttribute->index])[i].y = (float)puRawData[1];
                    }
                }
                break;
            }

            case cgltf_attribute_type_color:
            {
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    if(szStride == sizeof(plVec4))
                    {
                        memcpy((ptMesh->ptVertexColors[ptAttribute->index]), pucBufferStart, sizeof(plVec4) * szVertexCount);
                    }
                    else
                    {
                        for(size_t i = 0; i < szVertexCount; i++)
                        {
                            plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                            (ptMesh->ptVertexColors[ptAttribute->index])[i] = *ptRawData;
                        }
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    const float fConversion = 1.0f / (256.0f * 256.0f);
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].r = (float)puRawData[0] * fConversion;
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].g = (float)puRawData[1] * fConversion;
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].b = (float)puRawData[2] * fConversion;
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].a = (float)puRawData[3] * fConversion;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    const float fConversion = 1.0f / (256.0f * 256.0f);
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].r = (float)puRawData[0] * fConversion;
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].g = (float)puRawData[1] * fConversion;
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].b = (float)puRawData[2] * fConversion;
                        (ptMesh->ptVertexColors[ptAttribute->index])[i].a = (float)puRawData[3] * fConversion;
                    }
                }
                else
                {
                    PL_ASSERT(false);
                }

                break;
            }

            case cgltf_attribute_type_joints:
            {
                if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->ptVertexJoints[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                break;
            }

            case cgltf_attribute_type_weights:
            {
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    if(szStride == sizeof(plVec4))
                    {
                        memcpy((ptMesh->ptVertexWeights[ptAttribute->index]), pucBufferStart, sizeof(plVec4) * szVertexCount);
                    }
                    else
                    {
                        for(size_t i = 0; i < szVertexCount; i++)
                        {
                            plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                            (ptMesh->ptVertexWeights[ptAttribute->index])[i] = *ptRawData;
                        }
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->ptVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                break;
            }

            default:
            {
                PL_ASSERT(false && "unknown attribute");
            }
        }
    }

    // index buffer
    if(ptPrimitive->indices)
    {
        unsigned char* pucIdexBufferStart = &((unsigned char*)ptPrimitive->indices->buffer_view->buffer->data)[ptPrimitive->indices->buffer_view->offset + ptPrimitive->indices->offset];
        switch(ptPrimitive->indices->component_type)
        {
            case cgltf_component_type_r_32u:
            {
                
                if(ptPrimitive->indices->buffer_view->stride == 0 || ptPrimitive->indices->buffer_view->stride == sizeof(uint32_t))
                {
                    memcpy(ptMesh->puIndices, pucIdexBufferStart, ptPrimitive->indices->count * sizeof(uint32_t));
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->puIndices[i] = *(uint32_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }

            case cgltf_component_type_r_16u:
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->puIndices[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * sizeof(unsigned short)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->puIndices[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }
            case cgltf_component_type_r_8u:
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->puIndices[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * sizeof(uint8_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->puIndices[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }
            default:
            {
                PL_ASSERT(false);
            }
        }
    }


}

static void
pl__refr_load_gltf_animation(const char* pcPath, plGltfLoadingData* ptSceneData, plModelInstanceHandle tHandle, const cgltf_animation* ptAnimation, cgltf_data* ptGltfData)
{
    plComponentLibrary* ptLibrary = ptSceneData->ptLibrary;

    plAnimationComponent* ptAnimationComp = NULL;
    plEntity tAnimationEntity = gptAnimation->create(ptLibrary, ptAnimation->name, (uint32_t)ptAnimation->channels_count, &ptAnimationComp);
    const char* pcAnimationName = ptAnimation->name;
    if(pcAnimationName == NULL)
        pcAnimationName = "unnamed animation";
    pl_hm64_insert_str(&gptGltfCtx->sbtModels[ptSceneData->tHandle.uIndex].tAnimationHashmap, pcAnimationName, tAnimationEntity.uData);

    // load channels
    for(size_t i = 0; i < ptAnimation->channels_count; i++)
    {
        const cgltf_animation_channel* ptChannel = &ptAnimation->channels[i];
        ptAnimationComp = gptECS->get_component(ptLibrary, gptAnimation->get_ecs_type_key_animation(), tAnimationEntity);
        const uint64_t ulTargetEntity = pl_hm_lookup(&ptSceneData->tNodeHashmap, (uint64_t)ptChannel->target_node);
        ptAnimationComp->atTargets[i] = *(plEntity*)&ulTargetEntity;
    }

    char acFileNameOnly[128] = {0};
    pl_str_get_file_name_only(pcPath, acFileNameOnly, 128);

    char acBuffer[256] = {0};

    ptrdiff_t tAnimationIndex = ptAnimation - ptGltfData->animations;
    if(ptAnimation->name)
    {
        pl_sprintf(acBuffer, "/assets/animations/%s.planim", ptAnimation->name);
    }
    else
    {
        pl_sprintf(acBuffer, "/assets/animations/%s_%u.planim", acFileNameOnly, (uint32_t)tAnimationIndex);
    }
    ptAnimationComp->tAnimation = gptAsset->load(acBuffer);
}

static void
pl__refr_load_gltf_object(cgltf_data* ptGltfData, const char* pcPath, plModelInstanceHandle tModelHandle, plGltfLoadingData* ptSceneData, const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode)
{
    char acFileNameOnly[256] = {0};
    pl_str_get_file_name_only(pcPath, acFileNameOnly, 256);

    plComponentLibrary* ptLibrary = ptSceneData->ptLibrary;

    plEntity tSkinEntity = {UINT32_MAX, UINT32_MAX};
    plTransformComponent* ptTransform = NULL;

    if(ptNode->skin)
    {
        tSkinEntity.uData = pl_hm_lookup(&ptSceneData->tSkinHashmap, (uint64_t)ptNode->skin);
        PL_ASSERT(tSkinEntity.uData != UINT64_MAX && "skin not preregistered");
    }

    const uint64_t ulObjectIndex = pl_hm_lookup(&ptSceneData->tNodeHashmap, (uint64_t)ptNode);
    PL_ASSERT(ulObjectIndex != UINT64_MAX);
    plEntity tNewEntity = {.uData = ulObjectIndex};
    ptTransform = gptECS->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tNewEntity);

    // transform defaults
    ptTransform->tWorld       = pl_identity_mat4();
    ptTransform->tRotation    = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptTransform->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
    ptTransform->tTranslation = (plVec3){0.0f, 0.0f, 0.0f};

    if(ptNode->has_rotation)    memcpy(ptTransform->tRotation.d, ptNode->rotation, sizeof(plVec4));
    if(ptNode->has_scale)       memcpy(ptTransform->tScale.d, ptNode->scale, sizeof(plVec3));
    if(ptNode->has_translation) memcpy(ptTransform->tTranslation.d, ptNode->translation, sizeof(plVec3));

    // must use provided matrix, otherwise calculate based on rot, scale, trans
    if(ptNode->has_matrix)
    {
        memcpy(ptTransform->tWorld.d, ptNode->matrix, sizeof(plMat4));
        pl_decompose_matrix(&ptTransform->tWorld, &ptTransform->tScale, &ptTransform->tRotation, &ptTransform->tTranslation);
    }
    else
        ptTransform->tWorld = pl_rotation_translation_scale(ptTransform->tRotation, ptTransform->tTranslation, ptTransform->tScale);


    // attach to parent if parent is valid
    if(tParentEntity.uIndex != UINT32_MAX)
        gptTransform->attach_component(ptLibrary, tNewEntity, tParentEntity);

    // check if node has attached mesh
    const plEcsTypeKey tObjectComponentType = gptRendererEcs->get_ecs_type_key_object();
    if(ptNode->mesh)
    {

        // add mesh to our node
        plObjectComponent* ptObject = gptECS->add_component(ptLibrary, tObjectComponentType, tNewEntity);
        ptObject->tTransform = tNewEntity;
        ptObject->tSkinComponent = tSkinEntity;
        ptObject->uFirstSubmesh = 0;
        ptObject->uSubmeshCount = (uint32_t)ptNode->mesh->primitives_count;

        char acBuffer[512] = {0};
        const cgltf_mesh* ptGltfMesh = ptNode->mesh;
        ptrdiff_t tMeshIndex = ptGltfMesh - ptGltfData->meshes;
        if(ptNode->mesh->name)
        {
            pl_sprintf(acBuffer, "/assets/meshes/%s.plmesh", ptNode->mesh->name);
        }
        else
        {
            pl_sprintf(acBuffer, "/assets/meshes/%s_%u.plmesh", acFileNameOnly, (uint32_t)tMeshIndex);
        }
        ptObject->tMesh = gptAsset->load(acBuffer);
        pl_sb_push(gptGltfCtx->sbtModels[tModelHandle.uIndex].tData.atObjects, tNewEntity);
    }

    // recurse through children
    for(size_t i = 0; i < ptNode->children_count; i++)
    {
        pl__refr_load_gltf_object(ptGltfData, pcPath, tModelHandle, ptSceneData, pcDirectory, tNewEntity, ptNode->children[i]);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_gltf_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGltfI tApi = {
        .import                = pl_gltf_import,
        .load                  = pl_gltf_load,
        .get_objects           = pl_gltf_get_objects,
        .get_animation_by_name = pl_gltf_get_animation_by_name,
        .free_data             = pl_gltf_free_data
    };
    pl_set_api(ptApiRegistry, plGltfI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptECS         = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptAnimation   = pl_get_api_latest(ptApiRegistry, plAnimationI);
        gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptMesh        = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptMaterial    = pl_get_api_latest(ptApiRegistry, plMaterialI);
        gptRendererEcs = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
        gptAsset       = pl_get_api_latest(ptApiRegistry, plAssetI);
        gptImage       = pl_get_api_latest(ptApiRegistry, plImageI);
        gptTransform   = pl_get_api_latest(ptApiRegistry, plTransformI);
        gptSkeleton    = pl_get_api_latest(ptApiRegistry, plSkeletonI);
        gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptGltfCtx = ptDataRegistry->get_data("plGltfContext");
    }
    else  // first load
    {
        static plGltfContext tCtx = {0};
        gptGltfCtx = &tCtx;
        ptDataRegistry->set_data("plGltfContext", gptGltfCtx);
        pl_sb_push(gptGltfCtx->sbtModelGenerations, UINT32_MAX);
    }
}

void
pl_unload_gltf_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    if(gptGltfCtx)
    {
        pl_sb_free(gptGltfCtx->sbtModelFreeIndices);
        pl_sb_free(gptGltfCtx->sbtModelGenerations);
        pl_sb_free(gptGltfCtx->sbtModels);
        gptGltfCtx = NULL;
    }
        
    const plGltfI* ptApi = pl_get_api_latest(ptApiRegistry, plGltfI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define CGLTF_IMPLEMENTATION
    #include "cgltf.h"
    #undef CGLTF_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif