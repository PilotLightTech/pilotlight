/*
   pl_model_loader_ext.c
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
#include "pl.h"
#include "pl_model_loader_ext.h"
#include "pl_stl.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_resource_ext.h"
#include "pl_ecs_ext.h"
#include "pl_file_ext.h"

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

    static const plResourceI* gptResource = NULL;
    static const plEcsI*  gptECS = NULL;
    static const plFileI* gptFile = NULL;
#endif

#include "pl_ds.h"

// misc
#include "cgltf.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plGltfLoadingData
{
    plComponentLibrary* ptLibrary;
    plHashMap64         tNodeHashmap;
    plHashMap64         tJointHashmap;
    plHashMap64         tSkinHashmap;
    plHashMap64         tMaterialHashMap;
    plEntity*           sbtMaterialEntities;
} plGltfLoadingData;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// internal gltf helpers
static void pl__load_gltf_texture(const char* pcPath, plTextureSlot tSlot, const cgltf_texture_view* ptTexture, const char* pcDirectory, const cgltf_material* ptMaterial, plMaterialComponent* ptMaterialOut);
static void pl__refr_load_material(const char* pcPath, const char* pcDirectory, plMaterialComponent* ptMaterial, const cgltf_material* ptGltfMaterial);
static void pl__refr_load_attributes(plMeshComponent* ptMesh, const cgltf_primitive* ptPrimitive);
static void pl__refr_load_gltf_object(const char* pcPath, plModelLoaderData* ptData, plGltfLoadingData* ptSceneData, const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode);
static void pl__refr_load_gltf_animation(plGltfLoadingData* ptSceneData, const cgltf_animation* ptAnimation);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__free_data(plModelLoaderData* ptData)
{
    pl_sb_free(ptData->atObjects);
    ptData->uObjectCount = 0;
    ptData->atObjects = NULL;
}

static bool
pl__load_stl(plComponentLibrary* ptLibrary, const char* pcPath, plVec4 tColor, const plMat4* ptTransform, plModelLoaderData* ptDataOut)
{

    // read in STL file
    size_t szFileSize = 0;
    gptFile->binary_read(pcPath, &szFileSize, NULL);
    uint8_t* pcBuffer = PL_ALLOC(szFileSize);
    memset(pcBuffer, 0, szFileSize);
    gptFile->binary_read(pcPath, &szFileSize, pcBuffer);

    // create ECS object component
    plEntity tEntity = gptECS->create_object(ptLibrary, pcPath, NULL);

    // retrieve actual components
    plMeshComponent*      ptMesh          = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tEntity);
    plTransformComponent* ptTransformComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);
    
    // set transform if present
    if(ptTransform)
    {
        ptTransformComp->tWorld = *ptTransform;
        pl_decompose_matrix(&ptTransformComp->tWorld, &ptTransformComp->tScale, &ptTransformComp->tRotation, &ptTransformComp->tTranslation);
    }

    // create simple material
    plMaterialComponent* ptMaterial = NULL;
    ptMesh->tMaterial = gptECS->create_material(ptLibrary, pcPath, &ptMaterial);
    ptMaterial->tBaseColor = tColor;
    ptMaterial->tBlendMode = PL_BLEND_MODE_ALPHA;
    // ptMaterial->tFlags |= PL_MATERIAL_FLAG_OUTLINE;
    
    // load STL model
    plStlInfo tInfo = {0};
    pl_load_stl((const char*)pcBuffer, szFileSize, NULL, NULL, NULL, &tInfo);

    ptMesh->ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL;

    gptECS->allocate_vertex_data(ptMesh, tInfo.szPositionStreamSize / 3, PL_MESH_FORMAT_FLAG_HAS_NORMAL, (uint32_t)tInfo.szIndexBufferSize);

    pl_load_stl((const char*)pcBuffer, szFileSize, (float*)ptMesh->ptVertexPositions, (float*)ptMesh->ptVertexNormals, (uint32_t*)ptMesh->puIndices, &tInfo);
    PL_FREE(pcBuffer);

    // calculate AABB
    ptMesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t i = 0; i < ptMesh->szVertexCount; i++)
    {
        if(ptMesh->ptVertexPositions[i].x > ptMesh->tAABB.tMax.x) ptMesh->tAABB.tMax.x = ptMesh->ptVertexPositions[i].x;
        if(ptMesh->ptVertexPositions[i].y > ptMesh->tAABB.tMax.y) ptMesh->tAABB.tMax.y = ptMesh->ptVertexPositions[i].y;
        if(ptMesh->ptVertexPositions[i].z > ptMesh->tAABB.tMax.z) ptMesh->tAABB.tMax.z = ptMesh->ptVertexPositions[i].z;
        if(ptMesh->ptVertexPositions[i].x < ptMesh->tAABB.tMin.x) ptMesh->tAABB.tMin.x = ptMesh->ptVertexPositions[i].x;
        if(ptMesh->ptVertexPositions[i].y < ptMesh->tAABB.tMin.y) ptMesh->tAABB.tMin.y = ptMesh->ptVertexPositions[i].y;
        if(ptMesh->ptVertexPositions[i].z < ptMesh->tAABB.tMin.z) ptMesh->tAABB.tMin.z = ptMesh->ptVertexPositions[i].z;
    }

    pl_sb_push(ptDataOut->atObjects, tEntity);
    ptDataOut->uObjectCount = pl_sb_size(ptDataOut->atObjects);
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

static bool
pl__load_gltf(plComponentLibrary* ptLibrary, const char* pcPath, const plMat4* ptTransform, plModelLoaderData* ptDataOut)
{
    plGltfLoadingData tLoadingData = {.ptLibrary = ptLibrary};
    cgltf_options tGltfOptions = {0};
    cgltf_data* ptGltfData = NULL;

    char acDirectory[1024] = {0};
    pl_str_get_directory(pcPath, acDirectory, 1024);

    cgltf_result tGltfResult = cgltf_parse_file(&tGltfOptions, pcPath, &ptGltfData);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    tGltfResult = cgltf_load_buffers(&tGltfOptions, ptGltfData, pcPath);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    for(size_t szSkinIndex = 0; szSkinIndex < ptGltfData->skins_count; szSkinIndex++)
    {
        const cgltf_skin* ptSkin = &ptGltfData->skins[szSkinIndex];

        plSkinComponent* ptSkinComponent = NULL;
        plEntity tSkinEntity = gptECS->create_skin(ptLibrary, ptSkin->name, &ptSkinComponent);
        plTransformComponent* ptSkinTransform = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tSkinEntity);
        pl_sb_resize(ptSkinComponent->sbtJoints, (uint32_t)ptSkin->joints_count);
        pl_sb_resize(ptSkinComponent->sbtInverseBindMatrices, (uint32_t)ptSkin->joints_count);


        if(ptSkin->joints[0]->name && pl_str_contains(ptSkin->joints[0]->name, "mixamorig"))
        {
            plHumanoidComponent* ptHumanoid = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_HUMANOID, tSkinEntity);
            for(size_t szJointIndex = 0; szJointIndex < ptSkin->joints_count; szJointIndex++)
            {
                const cgltf_node* ptJointNode = ptSkin->joints[szJointIndex];

                plEntity tTransformEntity = gptECS->create_transform(ptLibrary, ptJointNode->name, NULL);
                ptSkinComponent->sbtJoints[szJointIndex] = tTransformEntity;
                pl_hm_insert(&tLoadingData.tJointHashmap, (uint64_t)ptJointNode, tTransformEntity.ulData);
                pl__load_mixamorig(ptJointNode, ptHumanoid, tTransformEntity);
            }
        }
        else
        {
            for(size_t szJointIndex = 0; szJointIndex < ptSkin->joints_count; szJointIndex++)
            {
                const cgltf_node* ptJointNode = ptSkin->joints[szJointIndex];
                plEntity tTransformEntity = gptECS->create_transform(ptLibrary, ptJointNode->name, NULL);
                ptSkinComponent->sbtJoints[szJointIndex] = tTransformEntity;
                pl_hm_insert(&tLoadingData.tJointHashmap, (uint64_t)ptJointNode, tTransformEntity.ulData);
            }
        }
        if(ptSkin->inverse_bind_matrices)
        {
            const cgltf_buffer_view* ptInverseBindMatrixView = ptSkin->inverse_bind_matrices->buffer_view;
            const char* pcBufferData = ptInverseBindMatrixView->buffer->data;
            memcpy(ptSkinComponent->sbtInverseBindMatrices, &pcBufferData[ptInverseBindMatrixView->offset], sizeof(plMat4) * ptSkin->joints_count);
        }
        pl_hm_insert(&tLoadingData.tSkinHashmap, (uint64_t)ptSkin, tSkinEntity.ulData);
    }

    for(size_t i = 0; i < ptGltfData->scenes_count; i++)
    {
        const cgltf_scene* ptGScene = &ptGltfData->scenes[i];
        for(size_t j = 0; j < ptGScene->nodes_count; j++)
        {
            const cgltf_node* ptNode = ptGScene->nodes[j];
            plEntity tRoot = {UINT32_MAX, UINT32_MAX};
            if(ptTransform)
            {
                plTransformComponent* ptTransformComponent = NULL;
                tRoot = gptECS->create_transform(ptLibrary, "load transform", &ptTransformComponent);
                ptTransformComponent->tWorld = *ptTransform;
                pl_decompose_matrix(&ptTransformComponent->tWorld, &ptTransformComponent->tScale, &ptTransformComponent->tRotation, &ptTransformComponent->tTranslation);
            }
            pl__refr_load_gltf_object(pcPath, ptDataOut, &tLoadingData, acDirectory, tRoot, ptNode);
        }
    }

    for(size_t i = 0; i < ptGltfData->animations_count; i++)
    {
        const cgltf_animation* ptAnimation = &ptGltfData->animations[i];
        pl__refr_load_gltf_animation(&tLoadingData, ptAnimation);
    }

    pl_hm_free(&tLoadingData.tNodeHashmap);
    pl_hm_free(&tLoadingData.tJointHashmap);
    pl_hm_free(&tLoadingData.tSkinHashmap);
    pl_hm_free(&tLoadingData.tMaterialHashMap);
    pl_sb_free(tLoadingData.sbtMaterialEntities);
    ptDataOut->uObjectCount = pl_sb_size(ptDataOut->atObjects);
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] internal API implementation
//-----------------------------------------------------------------------------

static void
pl__load_gltf_texture(const char* pcPath, plTextureSlot tSlot, const cgltf_texture_view* ptTexture, const char* pcDirectory, const cgltf_material* ptGltfMaterial, plMaterialComponent* ptMaterial)
{
    ptMaterial->atTextureMaps[tSlot].uUVSet = ptTexture->texcoord;

    if(ptTexture->texture->image->buffer_view)
    {
        char* pucBufferData = ptTexture->texture->image->buffer_view->buffer->data;
        char* pucActualBuffer = &pucBufferData[ptTexture->texture->image->buffer_view->offset];
        
        char acResourceName[64] = {0};

        // int iOffset = pl_sprintf(acResourceName, "gltf_import_%p.", pucActualBuffer);
        int iOffset = pl_sprintf(acResourceName, "gltf_import_%u.", pl_str_hash_data(pucActualBuffer, ptTexture->texture->image->buffer_view->size, 0));
        char* pcNext = acResourceName;
        pcNext += iOffset;
        
        pl_str_get_file_name_only(ptTexture->texture->image->mime_type, pcNext, 4);
        strcpy(ptMaterial->atTextureMaps[tSlot].acName, acResourceName);
        ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_ex(acResourceName, 0, (uint8_t*)pucActualBuffer, ptTexture->texture->image->buffer_view->size, pcPath, 0);
    }
    else if(strncmp(ptTexture->texture->image->uri, "data:", 5) == 0)
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
        strncpy(ptMaterial->atTextureMaps[tSlot].acName, ptTexture->texture->image->uri, PL_MAX_PATH_LENGTH);
        char acFilepath[2048] = {0};
        strcpy(acFilepath, pcDirectory);
        pl_str_concatenate(acFilepath, ptMaterial->atTextureMaps[tSlot].acName, acFilepath, 2048);
        ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load(acFilepath, 0);
    }
}

static void
pl__refr_load_material(const char* pcPath, const char* pcDirectory, plMaterialComponent* ptMaterial, const cgltf_material* ptGltfMaterial)
{
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tFlags |= ptGltfMaterial->double_sided ? PL_MATERIAL_FLAG_DOUBLE_SIDED : PL_MATERIAL_FLAG_NONE;
    ptMaterial->fAlphaCutoff = ptGltfMaterial->alpha_cutoff;

    // blend mode
    if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_opaque)
        ptMaterial->tBlendMode = PL_BLEND_MODE_OPAQUE;
    else if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_blend)
        ptMaterial->tBlendMode = PL_BLEND_MODE_ALPHA;
    else
        ptMaterial->tBlendMode = PL_BLEND_MODE_CLIP_MASK;

	if(ptGltfMaterial->normal_texture.texture)
		pl__load_gltf_texture(pcPath, PL_TEXTURE_SLOT_NORMAL_MAP, &ptGltfMaterial->normal_texture, pcDirectory, ptGltfMaterial, ptMaterial);

    ptMaterial->tEmissiveColor.r = ptGltfMaterial->emissive_factor[0];
    ptMaterial->tEmissiveColor.g = ptGltfMaterial->emissive_factor[1];
    ptMaterial->tEmissiveColor.b = ptGltfMaterial->emissive_factor[2];
    if(ptMaterial->tEmissiveColor.r != 0.0f || ptMaterial->tEmissiveColor.g != 0.0f || ptMaterial->tEmissiveColor.b != 0.0f)
        ptMaterial->tEmissiveColor.a = 1.0f;
	if(ptGltfMaterial->emissive_texture.texture)
    {
		pl__load_gltf_texture(pcPath, PL_TEXTURE_SLOT_EMISSIVE_MAP, &ptGltfMaterial->emissive_texture, pcDirectory, ptGltfMaterial, ptMaterial);
    }

	if(ptGltfMaterial->occlusion_texture.texture)
    {
		pl__load_gltf_texture(pcPath, PL_TEXTURE_SLOT_OCCLUSION_MAP, &ptGltfMaterial->occlusion_texture, pcDirectory, ptGltfMaterial, ptMaterial);
    }

    if(ptGltfMaterial->has_pbr_metallic_roughness)
    {
        ptMaterial->tBaseColor.x = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[0];
        ptMaterial->tBaseColor.y = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[1];
        ptMaterial->tBaseColor.z = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[2];
        ptMaterial->tBaseColor.w = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[3];

		ptMaterial->fMetalness = ptGltfMaterial->pbr_metallic_roughness.metallic_factor;
		ptMaterial->fRoughness = ptGltfMaterial->pbr_metallic_roughness.roughness_factor;

        if(ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture)
			pl__load_gltf_texture(pcPath, PL_TEXTURE_SLOT_BASE_COLOR_MAP, &ptGltfMaterial->pbr_metallic_roughness.base_color_texture, pcDirectory, ptGltfMaterial, ptMaterial);

        if(ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture)
            pl__load_gltf_texture(pcPath, PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP, &ptGltfMaterial->pbr_metallic_roughness.metallic_roughness_texture, pcDirectory, ptGltfMaterial, ptMaterial);
        
    }
}

static void
pl__refr_load_attributes(plMeshComponent* ptMesh, const cgltf_primitive* ptPrimitive)
{
    const size_t szVertexCount = ptPrimitive->attributes[0].data->count;

    for(size_t szAttributeIndex = 0; szAttributeIndex < ptPrimitive->attributes_count; szAttributeIndex++)
    {
        const cgltf_attribute* ptAttribute = &ptPrimitive->attributes[szAttributeIndex];

        switch(ptAttribute->type)
        {
            case cgltf_attribute_type_position:break;
            case cgltf_attribute_type_normal: 
                ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL;
                break;
            case cgltf_attribute_type_tangent:
                ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT;
                break;
            case cgltf_attribute_type_texcoord:
                if(ptAttribute->index == 0 || ptAttribute->index == 1)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;
                else if(ptAttribute->index == 2 || ptAttribute->index == 3)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1;
                else if(ptAttribute->index == 4 || ptAttribute->index == 5)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2;
                else if(ptAttribute->index == 6 || ptAttribute->index == 7)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3;
                break;
            case cgltf_attribute_type_color:
                if(ptAttribute->index == 0)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
                else if(ptAttribute->index == 1)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1;
                break;
            case cgltf_attribute_type_joints:
                if(ptAttribute->index == 0)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0;
                else if(ptAttribute->index == 1)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1;
                break;
            case cgltf_attribute_type_weights:
                if(ptAttribute->index == 0)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0;
                else if(ptAttribute->index == 1)
                    ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1;
                break;
        }
    }

    if(ptPrimitive->indices)
        ptMesh->szIndexCount = (uint32_t)ptPrimitive->indices->count;

    gptECS->allocate_vertex_data(ptMesh, szVertexCount, ptMesh->ulVertexStreamMask, ptMesh->szIndexCount);

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
pl__refr_load_gltf_animation(plGltfLoadingData* ptSceneData, const cgltf_animation* ptAnimation)
{
    plComponentLibrary* ptLibrary = ptSceneData->ptLibrary;

    plAnimationComponent* ptAnimationComp = NULL;
    gptECS->create_animation(ptLibrary, ptAnimation->name, &ptAnimationComp);

    // load channels
    pl_sb_reserve(ptAnimationComp->sbtSamplers, ptAnimation->channels_count);
    pl_sb_reserve(ptAnimationComp->sbtChannels, ptAnimation->channels_count);
    for(size_t i = 0; i < ptAnimation->channels_count; i++)
    {
        const cgltf_animation_channel* ptChannel = &ptAnimation->channels[i];
        plAnimationChannel tChannel = {.uSamplerIndex = pl_sb_size(ptAnimationComp->sbtSamplers)};
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

        plAnimationDataComponent* ptAnimationDataComp = NULL;
        tSampler.tData = gptECS->create_animation_data(ptLibrary, ptSampler->input->name, &ptAnimationDataComp);

        ptAnimationComp->fEnd = pl_maxf(ptAnimationComp->fEnd, ptSampler->input->max[0]);

        const cgltf_buffer* ptInputBuffer = ptSampler->input->buffer_view->buffer;
        const cgltf_buffer* ptOutputBuffer = ptSampler->output->buffer_view->buffer;
        unsigned char* pucInputBufferStart = &((unsigned char*)ptInputBuffer->data)[ptSampler->input->buffer_view->offset + ptSampler->input->offset];
        unsigned char* pucOutputBufferStart = &((unsigned char*)ptOutputBuffer->data)[ptSampler->output->buffer_view->offset + ptSampler->output->offset];

        pl_sb_resize(ptAnimationDataComp->sbfKeyFrameTimes, (uint32_t)ptSampler->input->count);
        for(size_t j = 0; j < ptSampler->input->count; j++)
        {
            const float fValue = *(float*)&pucInputBufferStart[ptSampler->input->stride * j];
            ptAnimationDataComp->sbfKeyFrameTimes[j] = fValue;
        }

        if(ptSampler->output->type == cgltf_type_scalar)
        {
            pl_sb_resize(ptAnimationDataComp->sbfKeyFrameData, (uint32_t)ptSampler->input->count);
        }
        else if(ptSampler->output->type == cgltf_type_vec3)
        {
            pl_sb_reserve(ptAnimationDataComp->sbfKeyFrameData, (uint32_t)ptSampler->input->count * 3);
        }
        else if(ptSampler->output->type == cgltf_type_vec4)
        {
            pl_sb_reserve(ptAnimationDataComp->sbfKeyFrameData, (uint32_t)ptSampler->input->count * 4);
        }

        for(size_t j = 0; j < ptSampler->output->count; j++)
        {
            if(ptSampler->output->type == cgltf_type_scalar)
            {
                const float fValue0 = *(float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                ptAnimationDataComp->sbfKeyFrameData[j] =  fValue0;
            }
            else if(ptSampler->output->type == cgltf_type_vec3)
            {
                float* fFloatData = (float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                const float fValue0 = fFloatData[0];
                const float fValue1 = fFloatData[1];
                const float fValue2 = fFloatData[2];
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue0);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue1);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue2);
            }
            else if(ptSampler->output->type == cgltf_type_vec4)
            {
                float* fFloatData = (float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                const float fValue0 = fFloatData[0];
                const float fValue1 = fFloatData[1];
                const float fValue2 = fFloatData[2];
                const float fValue3 = fFloatData[3];
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue0);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue1);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue2);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue3);
            }
        }

        const uint64_t ulTargetEntity = pl_hm_lookup(&ptSceneData->tNodeHashmap, (uint64_t)ptChannel->target_node);
        tChannel.tTarget = *(plEntity*)&ulTargetEntity;

        pl_sb_push(ptAnimationComp->sbtSamplers, tSampler);
        pl_sb_push(ptAnimationComp->sbtChannels, tChannel);
    }
}

static void
pl__refr_load_gltf_object(const char* pcPath, plModelLoaderData* ptData, plGltfLoadingData* ptSceneData, const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode)
{
    plComponentLibrary* ptLibrary = ptSceneData->ptLibrary;

    plEntity tNewEntity = {UINT32_MAX, UINT32_MAX};
    plEntity tSkinEntity = {UINT32_MAX, UINT32_MAX};
    plTransformComponent* ptTransform = NULL;

    if(ptNode->skin)
    {
        tSkinEntity.ulData = pl_hm_lookup(&ptSceneData->tSkinHashmap, (uint64_t)ptNode->skin);
        PL_ASSERT(tSkinEntity.ulData != UINT64_MAX && "skin not preregistered");
    }

    const uint64_t ulObjectIndex = pl_hm_lookup(&ptSceneData->tJointHashmap, (uint64_t)ptNode);
    if(ulObjectIndex != UINT64_MAX)
    {
        tNewEntity.ulData = ulObjectIndex;
        ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
    }
    else
    {
        tNewEntity = gptECS->create_transform(ptLibrary, ptNode->name, &ptTransform);
    }
    pl_hm_insert(&ptSceneData->tNodeHashmap, (uint64_t)ptNode, tNewEntity.ulData);

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
        gptECS->attach_component(ptLibrary, tNewEntity, tParentEntity);

    // check if node has attached mesh
    if(ptNode->mesh)
    {
        // PL_ASSERT(ptNode->mesh->primitives_count == 1);
        for(size_t szPrimitiveIndex = 0; szPrimitiveIndex < ptNode->mesh->primitives_count; szPrimitiveIndex++)
        {
            // add mesh to our node
            plObjectComponent* ptObject = NULL;
            plMeshComponent* ptMesh = NULL;
            plEntity tNewObject = tNewEntity;
            if(szPrimitiveIndex == 0)
            {
                ptObject = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tNewEntity);
                ptMesh = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewEntity);
                ptObject->tMesh = tNewEntity;
                ptObject->tTransform = tNewEntity; // TODO: delete unused entities (old transform)
            }
            else
            {

                tNewObject = gptECS->create_tag(ptLibrary, ptNode->mesh->name);
                ptObject = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tNewObject);
                ptMesh = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewObject);

                ptObject->tMesh = tNewObject;

                plTransformComponent* ptSubTransform = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewObject);
                ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
                *ptSubTransform = *ptTransform;

                if(tParentEntity.uIndex != UINT32_MAX)
                    gptECS->attach_component(ptLibrary, tNewObject, tParentEntity);

                ptObject->tTransform = tNewObject;
            }
            ptMesh->tSkinComponent = tSkinEntity;

            const cgltf_primitive* ptPrimitive = &ptNode->mesh->primitives[szPrimitiveIndex];

            // load attributes
            pl__refr_load_attributes(ptMesh, ptPrimitive);

            ptMesh->tMaterial.uIndex      = UINT32_MAX;
            ptMesh->tMaterial.uGeneration = UINT32_MAX;

            // load material
            if(ptPrimitive->material)
            {
                plMaterialComponent* ptMaterial = NULL;
      
                // check if the material already exists
                uint64_t ulMaterialIndex = 0;
                if(pl_hm_has_key_ex(&ptSceneData->tMaterialHashMap, (uint64_t)ptPrimitive->material, &ulMaterialIndex))
                {
                    ptMesh->tMaterial = ptSceneData->sbtMaterialEntities[ulMaterialIndex];
                    ptMaterial = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);
                }
                else // create new material
                {
                    ulMaterialIndex = (uint64_t)pl_sb_size(ptSceneData->sbtMaterialEntities);
                    pl_hm_insert(&ptSceneData->tMaterialHashMap,(uint64_t)ptPrimitive->material, ulMaterialIndex);
                    ptMesh->tMaterial = gptECS->create_material(ptLibrary, ptPrimitive->material->name, &ptMaterial);
                    pl_sb_push(ptSceneData->sbtMaterialEntities, ptMesh->tMaterial);
                    pl__refr_load_material(pcPath, pcDirectory, ptMaterial, ptPrimitive->material);
                }

                if(ptPrimitive->material->has_transmission)
                    ptMaterial->tBlendMode = PL_BLEND_MODE_ALPHA;

                pl_sb_push(ptData->atObjects, tNewObject);
            }
        }
    }

    // recurse through children
    for(size_t i = 0; i < ptNode->children_count; i++)
    {
        pl__refr_load_gltf_object(pcPath, ptData, ptSceneData, pcDirectory, tNewEntity, ptNode->children[i]);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_model_loader_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plModelLoaderI tApi = {
        .load_stl  = pl__load_stl,
        .load_gltf = pl__load_gltf,
        .free_data = pl__free_data
    };
    pl_set_api(ptApiRegistry, plModelLoaderI, &tApi);

    gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);
    gptECS         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptResource    = pl_get_api_latest(ptApiRegistry, plResourceI);
}

PL_EXPORT void
pl_unload_model_loader_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plModelLoaderI* ptApi = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_STL_IMPLEMENTATION
    #include "pl_stl.h"
    #undef PL_STL_IMPLEMENTATION

    #define CGLTF_IMPLEMENTATION
    #include "cgltf.h"
    #undef CGLTF_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif