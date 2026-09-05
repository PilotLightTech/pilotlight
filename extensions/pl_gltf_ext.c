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
#include "pl_texture_ext.h"
#include "pl_scene_ext.h"

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

    static const plEcsI*         gptEcs         = NULL;
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
    static const plTextureI*      gptTexture    = NULL;
    static const plSceneI*        gptScene    = NULL;
#endif

#define CGLTF_MALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define CGLTF_FREE(x)   gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#include "pl_ds.h"

// misc
#include "cgltf.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plGltfImportResult
{
    plAssetHandle* atAssets;
    uint32_t       uAssetCount;
} plGltfImportResult;

typedef struct _plGltfContext
{
    plHashMap64        tAssetHashmap;
    plHashMap64        tNewNodeHashmap;
    char*              sbcPathBuffer;
    const cgltf_node** sbptTempPath;
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
static plAssetHandle pl__import_gltf_animation(cgltf_data*, const char* pcFileNameOnly, const cgltf_animation*, uint32_t uAnimationIndex);

// internal gltf helpers
static void pl__import_attributes(plSubmesh* ptMesh, const cgltf_primitive* ptPrimitive);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__import_mixamorig(const cgltf_node* ptJointNode, plHumanoidComponent* ptHumanoid, plEntity tTransformEntity)
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

void
pl__gltf_blah(const char* pcPath, const cgltf_node* ptNode)
{
    // find root
    const cgltf_node* ptRoot = ptNode;
    while(ptRoot->parent)
    {
        pl_sb_push(gptGltfCtx->sbptTempPath, ptRoot);
        ptRoot = ptRoot->parent;
        
    }

    pl_sb_sprintf(gptGltfCtx->sbcPathBuffer, "/%s", pcPath);

    if(ptRoot == ptNode)
    {
        pl_sb_pop(gptGltfCtx->sbcPathBuffer);
        pl_sb_sprintf(gptGltfCtx->sbcPathBuffer, "/%s", ptNode->name ? ptNode->name : "unnamed");
        return;
    }

    for(uint32_t i = 0; i < pl_sb_size(gptGltfCtx->sbptTempPath); i++)
    {
        pl_sb_pop(gptGltfCtx->sbcPathBuffer);
        const char* pcName = gptGltfCtx->sbptTempPath[pl_sb_size(gptGltfCtx->sbptTempPath) - i - 1]->name;
        pl_sb_sprintf(gptGltfCtx->sbcPathBuffer, "/%s", pcName ? pcName : "unnamed");
    }
    pl_sb_reset(gptGltfCtx->sbptTempPath);
}

bool
pl_gltf_import_ex(const char* pcPath, const plGltfImportOptions* ptOptions, plGltfImportResult* ptResults)
{
    if(!gptVfs->does_file_exist(pcPath))
        return false;

    static const plGltfImportOptions tDefaultOptions = {
        .eFlags = PL_GLTF_IMPORT_FLAGS_IMPORT_ALL_ASSETS
    };
    if(ptOptions == NULL)
        ptOptions = &tDefaultOptions;

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

    for(size_t i = 0; i < ptGltfData->nodes_count; i++)
    {
        pl_hm_insert(&gptGltfCtx->tNewNodeHashmap, (uint64_t)&ptGltfData->nodes[i], (uint64_t)i);
    }

    uint32_t uCurrentAsset = 0;
    uint32_t uAssetCount = 0;

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES)   uAssetCount += (uint32_t)ptGltfData->textures_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS)  uAssetCount += (uint32_t)ptGltfData->materials_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES)     uAssetCount += (uint32_t)ptGltfData->meshes_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS) uAssetCount += (uint32_t)ptGltfData->animations_count;
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS)  uAssetCount += 2 * (uint32_t)ptGltfData->skins_count;
    
    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_SCENES)  uAssetCount += (uint32_t)ptGltfData->scenes_count;

    if(ptResults)
    {
        ptResults->atAssets = PL_ALLOC(uAssetCount * sizeof(plAssetHandle));
        ptResults->uAssetCount = uAssetCount;
    }

    size_t szCurrentDataOffset = 0;

    plAssetHandle* sbtHandles = NULL;

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES)
    {
        uint32_t uCurrentTexture = 0;

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
                    if(ptResults)
                    {
                        ptResults->atAssets[uCurrentAsset] = sbtHandles[i];
                        uCurrentAsset++;
                    }
                    uCurrentTexture++;
                }
            }

            pl_sb_reset(sbtHandles);
        }

    }
    pl_hm_free(&gptGltfCtx->tAssetHashmap);

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS)
    {
        for(size_t szMaterialIndex = 0; szMaterialIndex < ptGltfData->materials_count; szMaterialIndex++)
        {
            const cgltf_material* ptGltfMaterial = &ptGltfData->materials[szMaterialIndex];
            ptrdiff_t tMaterialIndex = ptGltfMaterial - ptGltfData->materials;
            plAssetHandle tAssetHandle = pl__import_gltf_material(ptGltfData, acFileNameOnly, acDirectory, ptGltfMaterial, (uint32_t)tMaterialIndex);
            if(ptResults)
            {
                ptResults->atAssets[uCurrentAsset] = tAssetHandle;
                uCurrentAsset++;
            }
        }
    }

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES)
    {
        for(size_t szMeshIndex = 0; szMeshIndex < ptGltfData->meshes_count; szMeshIndex++)
        {
            const cgltf_mesh* ptGltfMesh = &ptGltfData->meshes[szMeshIndex];
            ptrdiff_t tMeshIndex = ptGltfMesh - ptGltfData->meshes;
            plAssetHandle tAssetHandle = pl__import_gltf_mesh(ptGltfData, acFileNameOnly, acDirectory, ptGltfMesh, (uint32_t)tMeshIndex);
            if(ptResults)
            {
                ptResults->atAssets[uCurrentAsset] = tAssetHandle;
                uCurrentAsset++;
            }
        }
    }

    plAssetHandle* atAnimationAssets = NULL;
    if(ptGltfData->animations_count > 0)
        atAnimationAssets = PL_ALLOC(sizeof(plAssetHandle) * ptGltfData->animations_count);

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS)
    {
        for(size_t szAnimationIndex = 0; szAnimationIndex < ptGltfData->animations_count; szAnimationIndex++)
        {
            const cgltf_animation* ptGltfAnimation = &ptGltfData->animations[szAnimationIndex];
            ptrdiff_t tAnimationIndex = ptGltfAnimation - ptGltfData->animations;
            plAssetHandle tAssetHandle = pl__import_gltf_animation(ptGltfData, acFileNameOnly, ptGltfAnimation, (uint32_t)tAnimationIndex);
            atAnimationAssets[szAnimationIndex] = tAssetHandle;
            if(ptResults)
            {
                ptResults->atAssets[uCurrentAsset] = tAssetHandle;
                uCurrentAsset++;
            }
        }
    }

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS)
    {
        for(size_t szSkinIndex = 0; szSkinIndex < ptGltfData->skins_count; szSkinIndex++)
        {
            const cgltf_skin* ptSkin = &ptGltfData->skins[szSkinIndex];

            plSkin tSkin = {0};
            tSkin.uJointCount = (uint32_t)ptSkin->joints_count;
            tSkin.atJoints     = PL_ALLOC(ptSkin->joints_count * sizeof(plEntityId));
            memset(tSkin.atJoints, 0, ptSkin->joints_count * sizeof(plEntityId));

            tSkin.atInverseBindMatrices = PL_ALLOC(ptSkin->joints_count * sizeof(plMat4));
            if(ptSkin->inverse_bind_matrices)
            {
                const cgltf_buffer_view* ptInverseBindMatrixView = ptSkin->inverse_bind_matrices->buffer_view;
                const char* pcBufferData = ptInverseBindMatrixView->buffer->data;
                cgltf_accessor_unpack_floats(ptSkin->inverse_bind_matrices, (cgltf_float*)tSkin.atInverseBindMatrices, ptSkin->joints_count * 16);
                // memcpy(tSkin.atInverseBindMatrices, &pcBufferData[ptInverseBindMatrixView->offset], sizeof(plMat4) * ptSkin->joints_count);
            }
            else
            {
                for(uint32_t i = 0; i < ptSkin->joints_count; i++)
                {
                    tSkin.atInverseBindMatrices[i] = pl_identity_mat4();
                }
            }

            plSkeleton tSkeleton = {0};
            tSkeleton.uJointCount = (uint32_t)ptSkin->joints_count;
            tSkeleton.atJoints     = PL_ALLOC(ptSkin->joints_count * sizeof(plSkeletonJoint));
            memset(tSkeleton.atJoints, 0, ptSkin->joints_count * sizeof(plSkeletonJoint));

            // pass 1: build every skeleton joint
            for(uint32_t i = 0; i < ptSkin->joints_count; i++)
            {
                const cgltf_node* ptJointNode = ptSkin->joints[i];

                plSkeletonJoint* ptJoint = &tSkeleton.atJoints[i];

                ptJoint->pcName = gptString->intern(ptJointNode->name ? ptJointNode->name : "unnamed joint");
                ptJoint->uParent = UINT32_MAX;

                pl__gltf_blah(acFileNameOnly, ptJointNode);
                tSkin.atJoints[i] = gptEcs->generate_id(NULL, gptGltfCtx->sbcPathBuffer, 0);
                pl_sb_reset(gptGltfCtx->sbcPathBuffer);

                ptJoint->tRotation = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
                ptJoint->tScale = (plVec3){1.0f, 1.0f, 1.0f};

                if(ptJointNode->has_rotation)
                    memcpy(ptJoint->tRotation.d, ptJointNode->rotation, sizeof(plVec4));

                if(ptJointNode->has_scale)
                    memcpy(ptJoint->tScale.d, ptJointNode->scale, sizeof(plVec3));

                if(ptJointNode->has_translation)
                    memcpy(ptJoint->tTranslation.d, ptJointNode->translation, sizeof(plVec3));

                if(ptJointNode->has_matrix)
                {
                    plMat4 tLocal;
                    memcpy(tLocal.d, ptJointNode->matrix, sizeof(plMat4));

                    pl_decompose_matrix(
                        &tLocal,
                        &ptJoint->tScale,
                        &ptJoint->tRotation,
                        &ptJoint->tTranslation);
                }
            }

            // pass 2: resolve parents against ALL joints
            for(uint32_t i = 0; i < ptSkin->joints_count; i++)
            {
                const cgltf_node* ptParent = ptSkin->joints[i]->parent;

                while(ptParent)
                {
                    for(uint32_t j = 0; j < ptSkin->joints_count; j++)
                    {
                        if(ptSkin->joints[j] == ptParent)
                        {
                            tSkeleton.atJoints[i].uParent = j;
                            goto parent_found;
                        }
                    }

                    ptParent = ptParent->parent;
                }

            parent_found:;
            }

            if(ptSkin->name)
            {
                pl_sprintf(acTempBuffer, "/assets/skeletons/%s.plskeleton", ptSkin->name);
            }
            else
            {
                ptrdiff_t tSkeletonIndex = ptSkin - ptGltfData->skins;
                pl_sprintf(acTempBuffer, "/assets/skeletons/%s_%u.plskeleton", acFileNameOnly, (uint32_t)tSkeletonIndex);
            }

            plAssetDesc tAssetDesc = {
                .tType = gptSkeleton->get_asset_type_key_skeleton(),
                .pcPath = acTempBuffer
            };
            tSkin.tSkeleton = gptAsset->create(&tAssetDesc, &tSkeleton);

            if(ptResults)
            {
                ptResults->atAssets[uCurrentAsset] = tSkin.tSkeleton;
                uCurrentAsset++;
            }
            gptAsset->save(tSkin.tSkeleton, PL_ASSET_ENCODING_AUTO);

            if(ptSkin->name)
            {
                pl_sprintf(acTempBuffer, "/assets/skins/%s.plskin", ptSkin->name);
            }
            else
            {
                ptrdiff_t tSkeletonIndex = ptSkin - ptGltfData->skins;
                pl_sprintf(acTempBuffer, "/assets/skins/%s_%u.plskin", acFileNameOnly, (uint32_t)tSkeletonIndex);
            }

            plAssetDesc tSkinAssetDesc = {
                .tType = gptSkeleton->get_asset_type_key_skin(),
                .pcPath = acTempBuffer
            };
            plAssetHandle tAssetHandle = gptAsset->create(&tSkinAssetDesc, &tSkin);
            gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_AUTO);

            if(ptResults)
            {
                ptResults->atAssets[uCurrentAsset] = tAssetHandle;
                uCurrentAsset++;
            }
        }
        // pl_sb_free(sbtTempNodes);
    }

    if(ptOptions->eFlags & PL_GLTF_IMPORT_FLAGS_IMPORT_SCENES)
    {
        for(size_t i = 0; i < ptGltfData->scenes_count; i++) 
        {
            const cgltf_scene* ptGScene = &ptGltfData->scenes[i];

            // count scene nodes
            plScene tScene = {0};
            gptEcs->create_library(&tScene.ptLibrary);

            // first create all entities
            for(size_t szNodeIndex = 0; szNodeIndex < ptGltfData->nodes_count; szNodeIndex++)
            {
                const cgltf_node* ptNode = &ptGltfData->nodes[szNodeIndex];
                pl__gltf_blah(acFileNameOnly, ptNode);
                plEntityId tEntityId = gptEcs->generate_id(NULL, gptGltfCtx->sbcPathBuffer, 0);
                pl_sb_reset(gptGltfCtx->sbcPathBuffer);
                gptEcs->create_entity_with_id(tScene.ptLibrary, ptNode->name, tEntityId);
            }

            uint32_t uEntityCount = 0;
            gptEcs->get_entities(tScene.ptLibrary, NULL, &uEntityCount);
            plEntity* atEntities = PL_ALLOC(uEntityCount * sizeof(plEntity));
            gptEcs->get_entities(tScene.ptLibrary, atEntities, &uEntityCount);

            for(size_t szNodeIndex = 0; szNodeIndex < ptGltfData->nodes_count; szNodeIndex++)
            {
                const cgltf_node* ptNode = &ptGltfData->nodes[szNodeIndex];
                plEntity tEntity = atEntities[szNodeIndex];
                plEntityId tEntityId = gptEcs->get_entity_id(tScene.ptLibrary, tEntity);
                
                if(ptNode->parent)
                {
                    pl__gltf_blah(acFileNameOnly, ptNode->parent);
                    plEntityId tParentId = gptEcs->generate_id(NULL, gptGltfCtx->sbcPathBuffer, 0);
                    pl_sb_reset(gptGltfCtx->sbcPathBuffer);
                    plEntity tParentEntity = gptEcs->get_entity_by_id(tScene.ptLibrary, tParentId);
                    gptTransform->attach_component(tScene.ptLibrary, tEntity, tParentEntity);
                }

                // TODO: find nearest transform
                plTransformComponent* ptTransform = gptEcs->add_component(tScene.ptLibrary, gptTransform->get_ecs_type_key_transform(), tEntity);
                ptTransform->tScale = (plVec3){1.0f, 1.0f, 1.0f};
                ptTransform->tRotation = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
                if(ptNode->has_rotation)    memcpy(ptTransform->tRotation.d, ptNode->rotation, sizeof(plVec4));
                if(ptNode->has_scale)       memcpy(ptTransform->tScale.d, ptNode->scale, sizeof(plVec3));
                if(ptNode->has_translation) memcpy(ptTransform->tTranslation.d, ptNode->translation, sizeof(plVec3));

                if(ptNode->has_matrix)
                {
                    memcpy(ptTransform->tWorld.d, ptNode->matrix, sizeof(plMat4));
                    pl_decompose_matrix(&ptTransform->tWorld, &ptTransform->tScale, &ptTransform->tRotation, &ptTransform->tTranslation);
                }
                ptTransform->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;

                if(ptNode->mesh)
                {

                    plObjectComponent* ptObject = gptEcs->add_component(tScene.ptLibrary, gptRendererEcs->get_ecs_type_key_object(), tEntity);

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
                    // tSceneNode.tMesh = gptAsset->load(acBuffer);
                    ptObject->tMesh = gptAsset->load(acBuffer);
                    plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
                    ptObject->uFirstSubmesh = 0;
                    ptObject->uSubmeshCount = ptMesh->uSubmeshCount;
                    ptObject->tTransformId = tEntityId;
                    ptObject->tFlags = PL_OBJECT_FLAGS_RENDERABLE | PL_OBJECT_FLAGS_CAST_SHADOW | PL_OBJECT_FLAGS_RECEIVE_SHADOW;
                }

                if(ptNode->skin)
                {
                    plSkinComponent* ptSkinComp = gptEcs->add_component(tScene.ptLibrary, gptSkeleton->get_ecs_type_key_skin(), tEntity);

                    if(ptNode->skin->name)
                    {
                        pl_sprintf(acTempBuffer, "/assets/skeletons/%s.plskeleton", ptNode->skin->name);
                    }
                    else
                    {
                        ptrdiff_t tSkeletonIndex = ptNode->skin - ptGltfData->skins;
                        pl_sprintf(acTempBuffer, "/assets/skeletons/%s_%u.plskeleton", acFileNameOnly, (uint32_t)tSkeletonIndex);
                    }

                    plAssetHandle tSkeleton = gptAsset->load(acTempBuffer);

                    if(ptNode->skin->name)
                    {
                        pl_sprintf(acTempBuffer, "/assets/skins/%s.plskin", ptNode->skin->name);
                    }
                    else
                    {
                        ptrdiff_t tSkeletonIndex = ptNode->skin - ptGltfData->skins;
                        pl_sprintf(acTempBuffer, "/assets/skins/%s_%u.plskin", acFileNameOnly, (uint32_t)tSkeletonIndex);
                    }
                    ptSkinComp->tSkin = gptAsset->load(acTempBuffer);

                    plSkin* ptSkin = gptAsset->get_data(ptSkinComp->tSkin);
                    ptSkin->tSkeleton = tSkeleton;
                }
            }

            PL_FREE(atEntities);
            atEntities = NULL;
            for(size_t szAnimationIndex = 0; szAnimationIndex < ptGltfData->animations_count; szAnimationIndex++)
            {
                const cgltf_animation* ptGltfAnimation = &ptGltfData->animations[szAnimationIndex];
                ptrdiff_t tAnimationIndex = ptGltfAnimation - ptGltfData->animations;

                plEntityId tEntityId = gptEcs->generate_id(NULL, ptGltfAnimation->name, 0);
                plEntity tEntity = gptEcs->create_entity_with_id(tScene.ptLibrary, ptGltfAnimation->name, tEntityId);

                plAnimationComponent* ptAnimation = gptEcs->add_component(tScene.ptLibrary, gptAnimation->get_ecs_type_key_animation(), tEntity);

                ptAnimation->fSpeed = 1.0f;
                ptAnimation->fBlendAmount = 1.0f;
                ptAnimation->tFlags = PL_ANIMATION_FLAG_NONE;
                ptAnimation->fTimer = 0.0f;
                ptAnimation->tAnimation = atAnimationAssets[szAnimationIndex];
                ptAnimation->uTargetCount = (uint32_t)ptGltfAnimation->channels_count;
                ptAnimation->atTargetIds = PL_ALLOC(sizeof(plEntityId) * ptAnimation->uTargetCount);

                for(uint32_t jj = 0; jj < ptGltfAnimation->channels_count; jj++)
                {
                    const cgltf_node* ptTarget = ptGltfAnimation->channels[jj].target_node;

                    pl__gltf_blah(acFileNameOnly, ptTarget);
                    ptAnimation->atTargetIds[jj] = gptEcs->generate_id(NULL, gptGltfCtx->sbcPathBuffer, 0);
                    pl_sb_reset(gptGltfCtx->sbcPathBuffer);
                }
            }

            if(ptGScene->name)
            {
                pl_sprintf(acTempBuffer, "/assets/models/%s_%s.plscene", acFileNameOnly, ptGScene->name);
            }
            else
            {
                ptrdiff_t tSceneIndex = ptGScene - ptGltfData->scenes;
                pl_sprintf(acTempBuffer, "/assets/models/%s_%u.plscene", acFileNameOnly, (uint32_t)tSceneIndex);
            }

            plAssetDesc tAssetDesc = {
                .tType = gptScene->get_asset_type_key(),
                .pcPath = acTempBuffer
            };
            plAssetHandle tAsset = gptAsset->create(&tAssetDesc, &tScene);
            gptAsset->save(tAsset, PL_ASSET_ENCODING_AUTO);
            if(ptResults)
            {
                ptResults->atAssets[uCurrentAsset] = tAsset;
                uCurrentAsset++;
            }
        }
    }



    pl_hm_free(&gptGltfCtx->tNewNodeHashmap);
    pl_sb_free(sbtHandles);
    sbtHandles = NULL;

    cgltf_free(ptGltfData);
    PL_FREE(pcBuffer);
    if(atAnimationAssets)
    {
        PL_FREE(atAnimationAssets);
    }
    return true;
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

    plTextureAsset tTextureAsset = {
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
                tTextureAsset.eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
                tImageWrite.iByteStride = sizeof(float) * tImageInfo.iWidth;
                pucBuffer = (char*)gptImage->load_hdr((const unsigned char*)pucActualBuffer, (int)ptGltfTexture->image->buffer_view->size, &tImageInfo.iWidth, &tImageInfo.iHeight, &tImageInfo.iChannels, tImageInfo.iChannels);
            }
            else if(tImageInfo.b16Bit)
            {
                tTextureAsset.eFormat = PL_FORMAT_R16G16B16A16_UNORM;
                tImageWrite.iByteStride = sizeof(short) * tImageInfo.iWidth;
                pucBuffer = (char*)gptImage->load_16bit((const unsigned char*)pucActualBuffer, (int)ptGltfTexture->image->buffer_view->size, &tImageInfo.iWidth, &tImageInfo.iHeight, &tImageInfo.iChannels, tImageInfo.iChannels);
            }
            else
            {
                tTextureAsset.eFormat = PL_FORMAT_BC3_UNORM;
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
                tTextureAsset.eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
            }
            else if(tImageInfo.b16Bit)
            {
                tTextureAsset.eFormat = PL_FORMAT_R16G16B16A16_UNORM;
            }
            else
            {
                tTextureAsset.eFormat = PL_FORMAT_BC3_UNORM;
            }
        }
    }

    tTextureAsset.pcSourceFile = gptString->intern(acFilepath);
    plAssetDesc tAssetDesc = {
        .tType = gptTexture->get_asset_type_key(),
        .pcPath = acTempBuffer
    };
    plAssetHandle tAsset = gptAsset->create(&tAssetDesc, &tTextureAsset);
    gptAsset->save(tAsset, PL_ASSET_ENCODING_TEXT);
    return tAsset;
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
pl__import_gltf_animation(cgltf_data* ptGltfData, const char* pcFileNameOnly, const cgltf_animation* ptGltfAnimation, uint32_t uAnimationIndex)
{

    plAnimation tAnimation = {0};
    size_t szAllocationSize = (sizeof(plAnimationChannel) + sizeof(plAnimationData)) * ptGltfAnimation->channels_count;
    tAnimation.uChannelCount = (uint32_t)ptGltfAnimation->channels_count;
    tAnimation.uDataCount = (uint32_t)ptGltfAnimation->channels_count;
    tAnimation.puRawData = PL_ALLOC(szAllocationSize);
    memset(tAnimation.puRawData, 0, szAllocationSize);
    tAnimation.atChannels = (plAnimationChannel*)tAnimation.puRawData;
    tAnimation.atData = (plAnimationData*)&tAnimation.atChannels[ptGltfAnimation->channels_count];

    // load channels
    for(size_t i = 0; i < ptGltfAnimation->channels_count; i++)
    {
        const cgltf_animation_channel* ptChannel = &ptGltfAnimation->channels[i];
        plAnimationChannel tChannel = {.uDataIndex = (uint32_t)i};
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
        switch(ptSampler->interpolation)
        {
            case cgltf_interpolation_type_linear:
                tChannel.tMode = PL_ANIMATION_MODE_LINEAR;
                break;
            case cgltf_interpolation_type_step:
                tChannel.tMode = PL_ANIMATION_MODE_STEP;
                break;
            case cgltf_interpolation_type_cubic_spline:
                tChannel.tMode = PL_ANIMATION_MODE_CUBIC_SPLINE;
                break;
            default:
                tChannel.tMode = PL_ANIMATION_MODE_UNKNOWN;
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

        const cgltf_node* ptTargetNode = ptChannel->target_node;

        // tChannel.uTargetIndex = (uint32_t)(ptTargetNode - ptGltfData->nodes);
        tChannel.uTargetIndex = (uint32_t)i;
        tAnimation.atChannels[i] = tChannel;
    }

    char acFileNameOnly[128] = {0};

    char acBuffer[256] = {0};
    if(ptGltfAnimation->name)
    {
        pl_sprintf(acBuffer, "/assets/animations/%s.planimation", ptGltfAnimation->name);
    }
    else
    {
        pl_sprintf(acBuffer, "/assets/animations/%s_%u.planimation", acFileNameOnly, uAnimationIndex);
    }

    plAssetDesc tAssetDesc = {
        .tType = gptAnimation->get_asset_type_key(),
        .pcPath = acBuffer
    };
    plAssetHandle tAsset = gptAsset->create(&tAssetDesc, &tAnimation);
    gptAsset->save(tAsset, PL_ASSET_ENCODING_AUTO);
    return tAsset;
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
        pl__import_attributes(&tMesh.atSubmeshes[szPrimitiveIndex], ptPrimitive);

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
    gptMesh->calculate_bounds(&tMesh);

    char acBuffer[256] = {0};
    if(ptGltfMesh->name)
    {
        pl_sprintf(acBuffer, "/assets/meshes/%s.plmesh", ptGltfMesh->name);
    }
    else
    {
        pl_sprintf(acBuffer, "/assets/meshes/%s_%u.plmesh", pcFileNameOnly, uMeshIndex);
    }

    plAssetDesc tMeshDesc = {
        .pcPath = acBuffer,
        .tType = gptMesh->get_asset_type_key()
    };
    plAssetHandle tAsset =  gptAsset->create(&tMeshDesc, &tMesh);
    gptAsset->save(tAsset, PL_ASSET_ENCODING_AUTO);
    return tAsset;
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
		pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_NORMAL], &ptGltfMaterial->normal_texture, pcDirectory, (uint32_t)tTextureIndex);
        ptMaterial->fNormalMapStrength = ptGltfMaterial->normal_texture.scale;
    }

    if(ptGltfMaterial->has_emissive_strength)
        ptMaterial->fEmissiveStrength = ptGltfMaterial->emissive_strength.emissive_strength;
    ptMaterial->tEmissiveColor.r = ptGltfMaterial->emissive_factor[0];
    ptMaterial->tEmissiveColor.g = ptGltfMaterial->emissive_factor[1];
    ptMaterial->tEmissiveColor.b = ptGltfMaterial->emissive_factor[2];
	if(ptGltfMaterial->emissive_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->emissive_texture.texture - ptGltfData->textures;
		pl__import_gltf_material_texture(pcFileNameOnly, true, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_EMISSIVE], &ptGltfMaterial->emissive_texture, pcDirectory, (uint32_t)tTextureIndex);
    }

	if(ptGltfMaterial->occlusion_texture.texture)
    {
        ptrdiff_t tTextureIndex = ptGltfMaterial->occlusion_texture.texture - ptGltfData->textures;
		pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_OCCLUSION], &ptGltfMaterial->occlusion_texture, pcDirectory, (uint32_t)tTextureIndex);

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
            pl__import_gltf_material_texture(pcFileNameOnly, true, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_BASE_COLOR], &ptGltfMaterial->pbr_metallic_roughness.base_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_METAL_ROUGHNESS], &ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
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
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT], &ptGltfMaterial->clearcoat.clearcoat_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_roughness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS], &ptGltfMaterial->clearcoat.clearcoat_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        
        if(ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->clearcoat.clearcoat_normal_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_NORMAL], &ptGltfMaterial->clearcoat.clearcoat_normal_texture, pcDirectory, (uint32_t)tTextureIndex);
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
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_SHEEN_COLOR], &ptGltfMaterial->sheen.sheen_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }

        if(ptGltfMaterial->sheen.sheen_roughness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->sheen.sheen_roughness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_SHEEN_ROUGHNESS], &ptGltfMaterial->sheen.sheen_roughness_texture, pcDirectory, (uint32_t)tTextureIndex);
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
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE], &ptGltfMaterial->iridescence.iridescence_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        if(ptGltfMaterial->iridescence.iridescence_thickness_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->iridescence.iridescence_thickness_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS], &ptGltfMaterial->iridescence.iridescence_thickness_texture, pcDirectory, (uint32_t)tTextureIndex);
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
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_ANISOTROPY], &ptGltfMaterial->anisotropy.anisotropy_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    if(ptGltfMaterial->has_transmission)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_TRANSMISSION;
        ptMaterial->tTransmission.fFactor = ptGltfMaterial->transmission.transmission_factor;
        if(ptGltfMaterial->transmission.transmission_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->transmission.transmission_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_TRANSMISSION], &ptGltfMaterial->transmission.transmission_texture, pcDirectory, (uint32_t)tTextureIndex);
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
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_THICKNESS], &ptGltfMaterial->volume.thickness_texture, pcDirectory, (uint32_t)tTextureIndex);
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
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION], &ptGltfMaterial->diffuse_transmission.diffuse_transmission_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
        if(ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture)
        {
            ptrdiff_t tTextureIndex = ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture.texture - ptGltfData->textures;
            pl__import_gltf_material_texture(pcFileNameOnly, false, &ptMaterial->atTextures[PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION_COLOR], &ptGltfMaterial->diffuse_transmission.diffuse_transmission_color_texture, pcDirectory, (uint32_t)tTextureIndex);
        }
    }

    ptMaterial->tDispersion.fDispersion = 0;
    if(ptGltfMaterial->has_dispersion)
    {
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_DISPERSION;
        ptMaterial->tDispersion.fDispersion = ptGltfMaterial->dispersion.dispersion;
    }

    plAssetDesc tAssetDesc = {
        .tType = gptMaterial->get_asset_type_key(),
        .pcPath = acTempBuffer
    };
    plAssetHandle tAsset = gptAsset->create(&tAssetDesc, ptMaterial);
    gptAsset->save(tAsset, PL_ASSET_ENCODING_AUTO);
    return tAsset;
}

bool
pl_gltf_import(const char* pcPath, const plGltfImportOptions* ptOptions)
{
    return pl_gltf_import_ex(pcPath, ptOptions, NULL);
}

static void
pl__import_attributes(plSubmesh* ptMesh, const cgltf_primitive* ptPrimitive)
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
                    cgltf_accessor_unpack_floats(ptAttribute->data, (float*)ptMesh->ptVertexWeights[ptAttribute->index], szVertexCount * 4);
                    // for(size_t i = 0; i < szVertexCount; i++)
                    // {
                    //     uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    // }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    cgltf_accessor_unpack_floats(ptAttribute->data, (float*)ptMesh->ptVertexWeights[ptAttribute->index], szVertexCount * 4);
                    // for(size_t i = 0; i < szVertexCount; i++)
                    // {
                    //     uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                    //     (ptMesh->ptVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    // }
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

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_gltf_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGltfI tApi = {
        .import                = pl_gltf_import
    };
    pl_set_api(ptApiRegistry, plGltfI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
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
        gptScene       = pl_get_api_latest(ptApiRegistry, plSceneI);
        gptTexture     = pl_get_api_latest(ptApiRegistry, plTextureI);
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
    }
}

void
pl_unload_gltf_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    if(gptGltfCtx)
    {
        pl_sb_free(gptGltfCtx->sbptTempPath);
        pl_sb_free(gptGltfCtx->sbcPathBuffer);
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