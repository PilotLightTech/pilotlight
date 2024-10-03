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
#include "pl_ds.h"
#include "pl_math.h"
#include "pl_profile.h"
#include "pl_log.h"

// extensions
#include "pl_job_ext.h"
#include "pl_script_ext.h"
#include "pl_ext.inc"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plComponentLibraryData
{
    plTransformComponent* sbtTransformsCopy; // used for inverse kinematics system
} plComponentLibraryData;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static uint64_t uLogChannelEcs = UINT64_MAX;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// setup/shutdown
static void     pl_ecs_init_component_library   (plComponentLibrary* ptLibrary);
static void     pl_ecs_cleanup_component_library(plComponentLibrary* ptLibrary);

// low level (most users shouldn't use this)
static plEntity pl_ecs_create_entity         (plComponentLibrary* ptLibrary);
static void     pl_ecs_remove_entity         (plComponentLibrary* ptLibrary, plEntity tEntity);
static bool     pl_ecs_is_entity_valid       (plComponentLibrary* ptLibrary, plEntity tEntity);
static plEntity pl_ecs_get_entity            (plComponentLibrary* ptLibrary, const char* pcName);
static size_t   pl_ecs_get_index             (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_get_component         (plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity);
static void*    pl_ecs_add_component         (plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity);

// components
static plEntity pl_ecs_create_tag                (plComponentLibrary*, const char* pcName);
static plEntity pl_ecs_create_mesh               (plComponentLibrary*, const char* pcName, plMeshComponent**);
static plEntity pl_ecs_create_object             (plComponentLibrary*, const char* pcName, plObjectComponent**);
static plEntity pl_ecs_create_transform          (plComponentLibrary*, const char* pcName, plTransformComponent**);
static plEntity pl_ecs_create_material           (plComponentLibrary*, const char* pcName, plMaterialComponent**);
static plEntity pl_ecs_create_skin               (plComponentLibrary*, const char* pcName, plSkinComponent**);
static plEntity pl_ecs_create_animation          (plComponentLibrary*, const char* pcName, plAnimationComponent**);
static plEntity pl_ecs_create_animation_data     (plComponentLibrary*, const char* pcName, plAnimationDataComponent**);
static plEntity pl_ecs_create_perspective_camera (plComponentLibrary*, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ, plCameraComponent**);
static plEntity pl_ecs_create_orthographic_camera(plComponentLibrary*, const char* pcName, plVec3 tPos, float fWidth, float fHeight, float fNearZ, float fFarZ, plCameraComponent**);
static plEntity pl_ecs_create_directional_light  (plComponentLibrary*, const char* pcName, plVec3 tDirection, plLightComponent**);
static plEntity pl_ecs_create_point_light        (plComponentLibrary*, const char* pcName, plVec3 tPosition, plLightComponent**);
static plEntity pl_ecs_create_script             (plComponentLibrary*, const char* pcFile, plScriptFlags, plScriptComponent**);
static void     pl_ecs_attach_script             (plComponentLibrary*, const char* pcFile, plScriptFlags, plEntity, plScriptComponent**);


// heirarchy
static void pl_ecs_attach_component (plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent);
static void pl_ecs_deattach_component(plComponentLibrary* ptLibrary, plEntity tEntity);

// update systems
static void pl_run_object_update_system            (plComponentLibrary* ptLibrary);
static void pl_run_transform_update_system         (plComponentLibrary* ptLibrary);
static void pl_run_skin_update_system              (plComponentLibrary* ptLibrary);
static void pl_run_hierarchy_update_system         (plComponentLibrary* ptLibrary);
static void pl_run_animation_update_system         (plComponentLibrary* ptLibrary, float fDeltaTime);
static void pl_run_inverse_kinematics_update_system(plComponentLibrary* ptLibrary);
static void pl_run_script_update_system            (plComponentLibrary* ptLibrary);

// misc.
static void pl_calculate_normals (plMeshComponent* atMeshes, uint32_t uComponentCount);
static void pl_calculate_tangents(plMeshComponent* atMeshes, uint32_t uComponentCount);

// camera
static void pl_camera_set_fov        (plCameraComponent* ptCamera, float fYFov);
static void pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ);
static void pl_camera_set_aspect     (plCameraComponent* ptCamera, float fAspect);
static void pl_camera_set_pos        (plCameraComponent* ptCamera, float fX, float fY, float fZ);
static void pl_camera_set_pitch_yaw  (plCameraComponent* ptCamera, float fPitch, float fYaw);
static void pl_camera_translate      (plCameraComponent* ptCamera, float fDx, float fDy, float fDz);
static void pl_camera_rotate         (plCameraComponent* ptCamera, float fDPitch, float fDYaw);
static void pl_camera_update         (plCameraComponent* ptCamera);
static void pl_camera_look_at        (plCameraComponent* ptCamera, plVec3 tEye, plVec3 tTarget);

static inline float
pl__wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

static inline bool
pl_ecs_has_entity(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    return pl_hm_has_key(ptManager->ptHashmap, tEntity.uIndex);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl_ecs_init_component_library(plComponentLibrary* ptLibrary)
{
    // initialize component managers
    ptLibrary->tTagComponentManager.tComponentType = PL_COMPONENT_TYPE_TAG;
    ptLibrary->tTagComponentManager.szStride = sizeof(plTagComponent);

    ptLibrary->tTransformComponentManager.tComponentType = PL_COMPONENT_TYPE_TRANSFORM;
    ptLibrary->tTransformComponentManager.szStride = sizeof(plTransformComponent);

    ptLibrary->tObjectComponentManager.tComponentType = PL_COMPONENT_TYPE_OBJECT;
    ptLibrary->tObjectComponentManager.szStride = sizeof(plObjectComponent);

    ptLibrary->tMeshComponentManager.tComponentType = PL_COMPONENT_TYPE_MESH;
    ptLibrary->tMeshComponentManager.szStride = sizeof(plMeshComponent);
    
    ptLibrary->tHierarchyComponentManager.tComponentType = PL_COMPONENT_TYPE_HIERARCHY;
    ptLibrary->tHierarchyComponentManager.szStride = sizeof(plHierarchyComponent);

    ptLibrary->tMaterialComponentManager.tComponentType = PL_COMPONENT_TYPE_MATERIAL;
    ptLibrary->tMaterialComponentManager.szStride = sizeof(plMaterialComponent);

    ptLibrary->tSkinComponentManager.tComponentType = PL_COMPONENT_TYPE_SKIN;
    ptLibrary->tSkinComponentManager.szStride = sizeof(plSkinComponent);

    ptLibrary->tCameraComponentManager.tComponentType = PL_COMPONENT_TYPE_CAMERA;
    ptLibrary->tCameraComponentManager.szStride = sizeof(plCameraComponent);

    ptLibrary->tAnimationComponentManager.tComponentType = PL_COMPONENT_TYPE_ANIMATION;
    ptLibrary->tAnimationComponentManager.szStride = sizeof(plAnimationComponent);

    ptLibrary->tAnimationDataComponentManager.tComponentType = PL_COMPONENT_TYPE_ANIMATION_DATA;
    ptLibrary->tAnimationDataComponentManager.szStride = sizeof(plAnimationDataComponent);

    ptLibrary->tInverseKinematicsComponentManager.tComponentType = PL_COMPONENT_TYPE_INVERSE_KINEMATICS;
    ptLibrary->tInverseKinematicsComponentManager.szStride = sizeof(plInverseKinematicsComponent);

    ptLibrary->tLightComponentManager.tComponentType = PL_COMPONENT_TYPE_LIGHT;
    ptLibrary->tLightComponentManager.szStride = sizeof(plLightComponent);

    ptLibrary->tScriptComponentManager.tComponentType = PL_COMPONENT_TYPE_SCRIPT;
    ptLibrary->tScriptComponentManager.szStride = sizeof(plScriptComponent);

    ptLibrary->tHumanoidComponentManager.tComponentType = PL_COMPONENT_TYPE_HUMANOID;
    ptLibrary->tHumanoidComponentManager.szStride = sizeof(plHumanoidComponent);

    ptLibrary->_ptManagers[0]  = &ptLibrary->tTagComponentManager;
    ptLibrary->_ptManagers[1]  = &ptLibrary->tTransformComponentManager;
    ptLibrary->_ptManagers[2]  = &ptLibrary->tMeshComponentManager;
    ptLibrary->_ptManagers[3]  = &ptLibrary->tObjectComponentManager;
    ptLibrary->_ptManagers[4]  = &ptLibrary->tHierarchyComponentManager;
    ptLibrary->_ptManagers[5]  = &ptLibrary->tMaterialComponentManager;
    ptLibrary->_ptManagers[6]  = &ptLibrary->tSkinComponentManager;
    ptLibrary->_ptManagers[7]  = &ptLibrary->tCameraComponentManager;
    ptLibrary->_ptManagers[8]  = &ptLibrary->tAnimationComponentManager;
    ptLibrary->_ptManagers[9]  = &ptLibrary->tAnimationDataComponentManager;
    ptLibrary->_ptManagers[10] = &ptLibrary->tInverseKinematicsComponentManager;
    ptLibrary->_ptManagers[11] = &ptLibrary->tLightComponentManager;
    ptLibrary->_ptManagers[12] = &ptLibrary->tScriptComponentManager;
    ptLibrary->_ptManagers[13] = &ptLibrary->tHumanoidComponentManager;

    for(uint32_t i = 0; i < PL_COMPONENT_TYPE_COUNT; i++)
        ptLibrary->_ptManagers[i]->ptParentLibrary = ptLibrary;

    ptLibrary->pInternal = PL_ALLOC(sizeof(plComponentLibraryData));
    memset(ptLibrary->pInternal, 0, sizeof(plComponentLibraryData));

    pl_log_info(uLogChannelEcs, "initialized component library");
}

static void
pl_ecs_cleanup_component_library(plComponentLibrary* ptLibrary)
{
    plMeshComponent* sbtMeshes = ptLibrary->tMeshComponentManager.pComponents;
    plSkinComponent* sbtSkins = ptLibrary->tSkinComponentManager.pComponents;
    plAnimationComponent* sbtAnimations = ptLibrary->tAnimationComponentManager.pComponents;
    plAnimationDataComponent* sbtAnimationDatas = ptLibrary->tAnimationDataComponentManager.pComponents;

    for(uint32_t i = 0; i < pl_sb_size(sbtAnimations); i++)
    {
        pl_sb_free(sbtAnimations[i].sbtChannels);
        pl_sb_free(sbtAnimations[i].sbtSamplers);
    }

    for(uint32_t i = 0; i < pl_sb_size(sbtAnimationDatas); i++)
    {
        pl_sb_free(sbtAnimationDatas[i].sbfKeyFrameData);
        pl_sb_free(sbtAnimationDatas[i].sbfKeyFrameTimes);
    }

    for(uint32_t i = 0; i < pl_sb_size(sbtSkins); i++)
    {
        pl_sb_free(sbtSkins[i].sbtTextureData);
        pl_sb_free(sbtSkins[i].sbtJoints);
        pl_sb_free(sbtSkins[i].sbtInverseBindMatrices);
    }

    for(uint32_t i = 0; i < pl_sb_size(sbtMeshes); i++)
    {
        pl_sb_free(sbtMeshes[i].sbtVertexPositions);
        pl_sb_free(sbtMeshes[i].sbtVertexNormals);
        pl_sb_free(sbtMeshes[i].sbtVertexTangents);
        pl_sb_free(sbtMeshes[i].sbtVertexColors[0]);
        pl_sb_free(sbtMeshes[i].sbtVertexColors[1]);
        pl_sb_free(sbtMeshes[i].sbtVertexWeights[0]);
        pl_sb_free(sbtMeshes[i].sbtVertexWeights[1]);
        pl_sb_free(sbtMeshes[i].sbtVertexJoints[0]);
        pl_sb_free(sbtMeshes[i].sbtVertexJoints[1]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[0]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[1]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[2]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[3]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[4]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[5]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[6]);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates[7]);
        pl_sb_free(sbtMeshes[i].sbuIndices);
    }

    for(uint32_t i = 0; i < PL_COMPONENT_TYPE_COUNT; i++)
    {
        pl_sb_free(ptLibrary->_ptManagers[i]->pComponents);
        pl_sb_free(ptLibrary->_ptManagers[i]->sbtEntities);
        pl_hm_free(ptLibrary->_ptManagers[i]->ptHashmap);
    }

    plComponentLibraryData* ptData = ptLibrary->pInternal;
    pl_sb_free(ptData->sbtTransformsCopy);

    // general
    pl_sb_free(ptLibrary->sbtEntityFreeIndices);
    pl_sb_free(ptLibrary->sbtEntityGenerations);
    pl_hm_free(ptLibrary->ptTagHashmap);
    PL_FREE(ptLibrary->pInternal);
    ptLibrary->pInternal = NULL;
}

static plEntity
pl_ecs_create_entity(plComponentLibrary* ptLibrary)
{
    plEntity tNewEntity = {0};
    if(pl_sb_size(ptLibrary->sbtEntityFreeIndices) > 0) // free slot available
    {
        tNewEntity.uIndex = pl_sb_pop(ptLibrary->sbtEntityFreeIndices);
        tNewEntity.uGeneration = ptLibrary->sbtEntityGenerations[tNewEntity.uIndex];
    }
    else // create new slot
    {
        tNewEntity.uIndex = pl_sb_size(ptLibrary->sbtEntityGenerations);
        pl_sb_push(ptLibrary->sbtEntityGenerations, 0);
    }
    return tNewEntity;
}

static void
pl_ecs_remove_entity(plComponentLibrary* ptLibrary, plEntity tEntity)
{

    pl_sb_push(ptLibrary->sbtEntityFreeIndices, tEntity.uIndex);

    // remove from tag hashmap
    plTagComponent* ptTag = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, tEntity);
    if(ptTag)
    {
        pl_hm_remove_str(ptLibrary->ptTagHashmap, ptTag->acName);
    }

    ptLibrary->sbtEntityGenerations[tEntity.uIndex]++;

    // remove from individual managers
    for(uint32_t i = 0; i < PL_COMPONENT_TYPE_COUNT; i++)
    {
        if(pl_hm_has_key(ptLibrary->_ptManagers[i]->ptHashmap, tEntity.uIndex))
        {
            pl_hm_remove(ptLibrary->_ptManagers[i]->ptHashmap, tEntity.uIndex);
            const uint64_t uEntityValue = pl_hm_get_free_index(ptLibrary->_ptManagers[i]->ptHashmap);
            

            // must keep valid entities contiguous (move last entity into removed slot)
            if(pl_sb_size(ptLibrary->_ptManagers[i]->sbtEntities) > 1)
            {
                plEntity tLastEntity = pl_sb_back(ptLibrary->_ptManagers[i]->sbtEntities);
                pl_hm_remove(ptLibrary->_ptManagers[i]->ptHashmap, tLastEntity.uIndex);
                pl_hm_get_free_index(ptLibrary->_ptManagers[i]->ptHashmap); // burn slot
                pl_hm_insert(ptLibrary->_ptManagers[i]->ptHashmap, tLastEntity.uIndex, uEntityValue);
            }

            pl_sb_del_swap(ptLibrary->_ptManagers[i]->sbtEntities, uEntityValue);
            switch(i)
            {
                case PL_COMPONENT_TYPE_TAG:
                {
                    plTagComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_TRANSFORM:
                {
                    plTransformComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_MESH:
                {
                    plMeshComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_OBJECT:
                {
                    plObjectComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_HIERARCHY:
                {
                    plHierarchyComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_MATERIAL:
                {
                    plMaterialComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_SKIN:
                {
                    plSkinComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_CAMERA:
                {
                    plCameraComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_ANIMATION:
                {
                    plAnimationComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_ANIMATION_DATA:
                {
                    plAnimationDataComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_INVERSE_KINEMATICS:
                {
                    plInverseKinematicsComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_LIGHT:
                {
                    plLightComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_SCRIPT:
                {
                    plScriptComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }

                case PL_COMPONENT_TYPE_HUMANOID:
                {
                    plHumanoidComponent* sbComponents = ptLibrary->_ptManagers[i]->pComponents;
                    pl_sb_del_swap(sbComponents, uEntityValue);
                    break;
                }
            }
        }
    }
}

static bool
pl_ecs_is_entity_valid(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX)
        return false;
    return ptLibrary->sbtEntityGenerations[tEntity.uIndex] == tEntity.uGeneration;
}

static plEntity
pl_ecs_get_entity(plComponentLibrary* ptLibrary, const char* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);
    if(pl_hm_has_key(ptLibrary->ptTagHashmap, ulHash))
    {
        uint64_t uIndex = pl_hm_lookup(ptLibrary->ptTagHashmap, ulHash);
        return (plEntity){.uIndex = (uint32_t)uIndex, .uGeneration = ptLibrary->sbtEntityGenerations[uIndex]};
    }
    return (plEntity){UINT32_MAX, UINT32_MAX};
}

static size_t
pl_ecs_get_index(plComponentManager* ptManager, plEntity tEntity)
{ 
    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    size_t szIndex = pl_hm_lookup(ptManager->ptHashmap, (uint64_t)tEntity.uIndex);
    return szIndex;
}

static void*
pl_ecs_get_component(plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX)
        return NULL;

    plComponentManager* ptManager = ptLibrary->_ptManagers[tType];

    if(ptManager->ptParentLibrary->sbtEntityGenerations[tEntity.uIndex] != tEntity.uGeneration)
        return NULL;

    size_t szIndex = pl_ecs_get_index(ptManager, tEntity);

    if(szIndex == UINT64_MAX)
        return NULL;

    unsigned char* pucData = ptManager->pComponents;
    return &pucData[szIndex * ptManager->szStride];
}

static void*
pl_ecs_add_component(plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity)
{
    if(tEntity.uIndex == UINT32_MAX)
        return NULL;

    plComponentManager* ptManager = ptLibrary->_ptManagers[tType];

    if(ptManager->ptParentLibrary->sbtEntityGenerations[tEntity.uIndex] != tEntity.uGeneration)
        return NULL;

    uint64_t uComponentIndex = pl_hm_get_free_index(ptManager->ptHashmap);
    bool bAddSlot = false; // can't add component with SB without correct type
    if(uComponentIndex == UINT64_MAX)
    {
        uComponentIndex = pl_sb_size(ptManager->sbtEntities);
        pl_sb_add(ptManager->sbtEntities);
        bAddSlot = true;
    }
    pl_hm_insert(ptManager->ptHashmap, (uint64_t)tEntity.uIndex, uComponentIndex);

    ptManager->sbtEntities[uComponentIndex] = tEntity;
    switch (ptManager->tComponentType)
    {
    case PL_COMPONENT_TYPE_TAG:
    {
        plTagComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plTagComponent){0};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_MESH:
    {
        plMeshComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plMeshComponent){.tSkinComponent = {UINT32_MAX, UINT32_MAX}};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_TRANSFORM:
    {
        plTransformComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plTransformComponent){.tWorld = pl_identity_mat4(), .tScale = {1.0f, 1.0f, 1.0f}, .tRotation = {0.0f, 0.0f, 0.0f, 1.0f}};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_OBJECT:
    {
        plObjectComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plObjectComponent){.tMesh= {UINT32_MAX, UINT32_MAX}, .tTransform ={UINT32_MAX, UINT32_MAX}};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_HIERARCHY:
    {
        plHierarchyComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plHierarchyComponent){0};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_MATERIAL:
    {
        plMaterialComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plMaterialComponent){
            .tBlendMode            = PL_BLEND_MODE_OPAQUE,
            .tFlags                = PL_MATERIAL_FLAG_NONE,
            .tShaderType           = PL_SHADER_TYPE_PBR,
            .tBaseColor            = {1.0f, 1.0f, 1.0f, 1.0f},
            .tEmissiveColor        = {0.0f, 0.0f, 0.0f, 0.0f},
            .fRoughness            = 1.0f,
            .fMetalness            = 1.0f,
            .fNormalMapStrength    = 1.0f,
            .fOcclusionMapStrength = 1.0f,
            .fAlphaCutoff          = 0.5f,
            .atTextureMaps         = {0}
        };
        for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
        {
            sbComponents[uComponentIndex].atTextureMaps[i].tResource.uIndex = UINT32_MAX;
            sbComponents[uComponentIndex].atTextureMaps[i].tResource.uGeneration = UINT32_MAX;
        }
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_SKIN:
    {
        plSkinComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plSkinComponent){.tMeshNode = {UINT32_MAX, UINT32_MAX}};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_CAMERA:
    {
        plCameraComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plCameraComponent){0};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_ANIMATION:
    {
        plAnimationComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plAnimationComponent){
            .fSpeed       = 1.0f,
            .fBlendAmount = 1.0f
        };
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_ANIMATION_DATA:
    {
        plAnimationDataComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plAnimationDataComponent){0};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_INVERSE_KINEMATICS:
    {
        plInverseKinematicsComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plInverseKinematicsComponent){.bEnabled = true, .tTarget = UINT32_MAX, .uIterationCount = 1};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_LIGHT:
    {
        plLightComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plLightComponent){
            .tPosition           = {0.0f, 0.0f, 0.0f},
            .tColor              = {1.0f, 1.0f, 1.0f},
            .tDirection          = {0.0f, -1.0f, 0.0f},
            .fIntensity          = 1.0f,
            .fRange              = 10.0f,
            .tType               = PL_LIGHT_TYPE_DIRECTIONAL,
            .uCascadeCount       = 0,
            .tFlags              = 0,
            .afCascadeSplits     = {0}
        };
        for(uint32_t i = 0; i < PL_MAX_SHADOW_CASCADES; i++)
            sbComponents[uComponentIndex].afCascadeSplits[i] = 8 * powf(10.0f, (float)i);
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_SCRIPT:
    {
        plScriptComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plScriptComponent){
            .tFlags = PL_SCRIPT_FLAG_NONE,
            .acFile = {0}
        };
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_HUMANOID:
    {
        plHumanoidComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plHumanoidComponent){
            .atBones = {0}
        };
        for(uint32_t i = 0; i < PL_HUMANOID_BONE_COUNT; i++)
        {
            sbComponents[uComponentIndex].atBones[i].ulData = UINT64_MAX;
        }
        return &sbComponents[uComponentIndex];
    }

    }

    return NULL;
}

static plEntity
pl_ecs_create_tag(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_TAG, tNewEntity);
    if(pcName)
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    else
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);

    if(pcName)
        pl_hm_insert_str(ptLibrary->ptTagHashmap, pcName, tNewEntity.uIndex);


    return tNewEntity;
}

static plEntity
pl_ecs_create_mesh(plComponentLibrary* ptLibrary, const char* pcName, plMeshComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed mesh";
    pl_log_debug_f(uLogChannelEcs, "created mesh: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);
    plMeshComponent* ptCompOut = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;
    return tNewEntity;
}

static plEntity
pl_ecs_create_directional_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tDirection, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed directional light";
    pl_log_debug_f(uLogChannelEcs, "created directional light: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);
    plLightComponent* ptLight =  pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_LIGHT, tNewEntity);
    ptLight->tDirection = tDirection;
    ptLight->tType = PL_LIGHT_TYPE_DIRECTIONAL;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

static plEntity
pl_ecs_create_point_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed point light";
    pl_log_debug_f(uLogChannelEcs, "created point light: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);
    plLightComponent* ptLight =  pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_LIGHT, tNewEntity);
    ptLight->tPosition = tPosition;
    ptLight->tType = PL_LIGHT_TYPE_POINT;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

static plEntity
pl_ecs_create_script(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plScriptComponent** pptCompOut)
{
    pl_log_debug_f(uLogChannelEcs, "created script: '%s'", pcFile);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcFile);
    plScriptComponent* ptScript =  pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_SCRIPT, tNewEntity);
    ptScript->tFlags = tFlags;
    strncpy(ptScript->acFile, pcFile, PL_MAX_NAME_LENGTH);

    gptExtensionRegistry->load(pcFile, "pl_load_script", "pl_unload_script", tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptI* ptScriptApi = gptApiRegistry->first(PL_API_SCRIPT);
    if(strncmp(pcFile, ptScriptApi->name(), PL_MAX_NAME_LENGTH) != 0)
    {
        while(ptScriptApi)
        {
            ptScriptApi = gptApiRegistry->next(ptScriptApi);
            if(strncmp(pcFile, ptScriptApi->name(), PL_MAX_NAME_LENGTH) == 0)
            {
                break;
            }
        }
    }
    ptScript->_ptApi = ptScriptApi;
    PL_ASSERT(ptScriptApi);

    if(ptScriptApi->setup)
        ptScriptApi->setup(ptLibrary, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptScript;
    return tNewEntity;
}

static void
pl_ecs_attach_script(plComponentLibrary* ptLibrary, const char* pcFile, plScriptFlags tFlags, plEntity tEntity, plScriptComponent** pptCompOut)
{
    pl_log_debug_f(uLogChannelEcs, "attach script: '%s'", pcFile);
    plScriptComponent* ptScript =  pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_SCRIPT, tEntity);
    ptScript->tFlags = tFlags;
    strncpy(ptScript->acFile, pcFile, PL_MAX_NAME_LENGTH);

    gptExtensionRegistry->load(pcFile, "pl_load_script", "pl_unload_script", tFlags & PL_SCRIPT_FLAG_RELOADABLE);

    const plScriptI* ptScriptApi = gptApiRegistry->first(PL_API_SCRIPT);
    if(strncmp(pcFile, ptScriptApi->name(), PL_MAX_NAME_LENGTH) != 0)
    {
        while(ptScriptApi)
        {
            ptScriptApi = gptApiRegistry->next(ptScriptApi);
            if(strncmp(pcFile, ptScriptApi->name(), PL_MAX_NAME_LENGTH) == 0)
            {
                break;
            }
        }
    }
    ptScript->_ptApi = ptScriptApi;
    PL_ASSERT(ptScriptApi);

    if(ptScriptApi->setup)
        ptScriptApi->setup(ptLibrary, tEntity);

    if(pptCompOut)
        *pptCompOut = ptScript;
}

static plEntity
pl_ecs_create_object(plComponentLibrary* ptLibrary, const char* pcName, plObjectComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed object";
    pl_log_debug_f(uLogChannelEcs, "created object: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plObjectComponent* ptObject = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tNewEntity);
    pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
    pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewEntity);

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tNewEntity;

    if(pptCompOut)
        *pptCompOut = ptObject;

    return tNewEntity;    
}

static plEntity
pl_ecs_create_transform(plComponentLibrary* ptLibrary, const char* pcName, plTransformComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed transform";
    pl_log_debug_f(uLogChannelEcs, "created transform: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plTransformComponent* ptTransform = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
    ptTransform->tScale = (plVec3){1.0f, 1.0f, 1.0f};
    ptTransform->tRotation = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptTransform->tWorld = pl_identity_mat4();

    if(pptCompOut)
        *pptCompOut = ptTransform;

    return tNewEntity;  
}

static plEntity
pl_ecs_create_material(plComponentLibrary* ptLibrary, const char* pcName, plMaterialComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed material";
    pl_log_debug_f(uLogChannelEcs, "created material: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plMaterialComponent* ptCompOut = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;    
}

static plEntity
pl_ecs_create_skin(plComponentLibrary* ptLibrary, const char* pcName, plSkinComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed skin";
    pl_log_debug_f(uLogChannelEcs, "created skin: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plSkinComponent* ptSkin = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptSkin;

    return tNewEntity;
}

static plEntity
pl_ecs_create_animation(plComponentLibrary* ptLibrary, const char* pcName, plAnimationComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed animation";
    pl_log_debug_f(uLogChannelEcs, "created animation: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plAnimationComponent* ptCompOut = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;
}

static plEntity
pl_ecs_create_animation_data(plComponentLibrary* ptLibrary, const char* pcName, plAnimationDataComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed animation data";
    pl_log_debug_f(uLogChannelEcs, "created animation data: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plAnimationDataComponent* ptCompOut = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION_DATA, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;
}

static plEntity
pl_ecs_create_perspective_camera(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ, plCameraComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed camera";
    pl_log_debug_f(uLogChannelEcs, "created camera: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    const plCameraComponent tCamera = {
        .tType        = PL_CAMERA_TYPE_PERSPECTIVE,
        .tPos         = tPos,
        .fNearZ       = fNearZ,
        .fFarZ        = fFarZ,
        .fFieldOfView = fYFov,
        .fAspectRatio = fAspect
    };

    plCameraComponent* ptCamera = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    if(pptCompOut)
        *pptCompOut = ptCamera;

    return tNewEntity; 
}

static plEntity
pl_ecs_create_orthographic_camera(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fWidth, float fHeight, float fNearZ, float fFarZ, plCameraComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed camera";
    pl_log_debug_f(uLogChannelEcs, "created camera: '%s'", pcName);
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    const plCameraComponent tCamera = {
        .tType   = PL_CAMERA_TYPE_ORTHOGRAPHIC,
        .tPos    = tPos,
        .fNearZ  = fNearZ,
        .fFarZ   = fFarZ,
        .fWidth   = fWidth,
        .fHeight  = fHeight
    };

    plCameraComponent* ptCamera = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    if(pptCompOut)
        *pptCompOut = ptCamera;

    return tNewEntity;    
}


static void
pl_ecs_attach_component(plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_entity(&ptLibrary->tHierarchyComponentManager, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tEntity);
    }
    else
    {
        ptHierarchyComponent = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tEntity);
    }
    ptHierarchyComponent->tParent = tParent;
}

static void
pl_ecs_deattach_component(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_entity(&ptLibrary->tHierarchyComponentManager, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tEntity);
    }
    else
    {
        ptHierarchyComponent = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tEntity);
    }
    ptHierarchyComponent->tParent.uIndex = UINT32_MAX;
}

static void
pl_run_skin_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plSkinComponent* sbtComponents = ptLibrary->tSkinComponentManager.pComponents;

    const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plSkinComponent* ptSkinComponent = &sbtComponents[i];
        plTransformComponent* ptParent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSkinComponent->tMeshNode);
        plMat4 tInverseWorldTransform = pl_mat4_invert(&ptParent->tWorld);
        for(uint32_t j = 0; j < pl_sb_size(ptSkinComponent->sbtJoints); j++)
        {
            plEntity tJointEntity = ptSkinComponent->sbtJoints[j];
            plTransformComponent* ptJointComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tJointEntity);

            const plMat4* ptIBM = &ptSkinComponent->sbtInverseBindMatrices[j];
            plMat4 tJointMatrix = pl_mul_mat4(&ptJointComponent->tWorld, ptIBM);
            tJointMatrix = pl_mul_mat4(&tInverseWorldTransform, &tJointMatrix);
            plMat4 tInvertJoint = pl_mat4_invert(&tJointMatrix);
            plMat4 tNormalMatrix = pl_mat4_transpose(&tInvertJoint);
            ptSkinComponent->sbtTextureData[j*2] = tJointMatrix;
            ptSkinComponent->sbtTextureData[j*2 + 1] = tNormalMatrix;
        }
    }

    pl_end_profile_sample(0);
}

static void
pl__object_update_job(uint32_t uJobIndex, void* pData)
{
    plComponentLibrary* ptLibrary = pData;
    plObjectComponent* sbtComponents = ptLibrary->tObjectComponentManager.pComponents;
    plObjectComponent* ptObject = &sbtComponents[uJobIndex];
    plTransformComponent* ptTransform = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
    plMeshComponent* ptMesh = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
    plSkinComponent* ptSkinComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, ptMesh->tSkinComponent);

    plMat4 tTransform = ptTransform->tWorld;

    if(ptSkinComponent)
    {
        plEntity tJointEntity = ptSkinComponent->sbtJoints[0];
        plTransformComponent* ptJointComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tJointEntity);

        const plMat4* ptIBM = &ptSkinComponent->sbtInverseBindMatrices[0];
        tTransform = pl_mul_mat4(&ptJointComponent->tWorld, ptIBM);
    }

    const plVec3 tVerticies[] = {
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMax.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMax.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMax.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMax.z }),
    };

    // calculate AABB
    ptMesh->tAABBFinal.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABBFinal.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t i = 0; i < 8; i++)
    {
        if(tVerticies[i].x > ptMesh->tAABBFinal.tMax.x) ptMesh->tAABBFinal.tMax.x = tVerticies[i].x;
        if(tVerticies[i].y > ptMesh->tAABBFinal.tMax.y) ptMesh->tAABBFinal.tMax.y = tVerticies[i].y;
        if(tVerticies[i].z > ptMesh->tAABBFinal.tMax.z) ptMesh->tAABBFinal.tMax.z = tVerticies[i].z;
        if(tVerticies[i].x < ptMesh->tAABBFinal.tMin.x) ptMesh->tAABBFinal.tMin.x = tVerticies[i].x;
        if(tVerticies[i].y < ptMesh->tAABBFinal.tMin.y) ptMesh->tAABBFinal.tMin.y = tVerticies[i].y;
        if(tVerticies[i].z < ptMesh->tAABBFinal.tMin.z) ptMesh->tAABBFinal.tMin.z = tVerticies[i].z;
    }
}

static void
pl_run_object_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    
    plObjectComponent* sbtComponents = ptLibrary->tObjectComponentManager.pComponents;
    const uint32_t uComponentCount = pl_sb_size(sbtComponents);

    plAtomicCounter* ptCounter = NULL;
    plJobDesc tJobDesc = {
        .task = pl__object_update_job,
        .pData = ptLibrary
    };
    gptJob->dispatch_batch(uComponentCount, 0, tJobDesc, &ptCounter);
    gptJob->wait_for_counter(ptCounter);

    pl_end_profile_sample(0);
}

static void
pl_run_transform_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plTransformComponent* sbtComponents = ptLibrary->tTransformComponentManager.pComponents;

    const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plTransformComponent* ptTransform = &sbtComponents[i];
        ptTransform->tWorld = pl_rotation_translation_scale(ptTransform->tRotation, ptTransform->tTranslation, ptTransform->tScale);
    }

    pl_end_profile_sample(0);
}

static void
pl_run_hierarchy_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    const uint32_t uComponentCount = pl_sb_size(ptLibrary->tHierarchyComponentManager.sbtEntities);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        const plEntity tChildEntity = ptLibrary->tHierarchyComponentManager.sbtEntities[i];
        plHierarchyComponent* ptHierarchyComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tChildEntity);
        plTransformComponent* ptParentTransform = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptHierarchyComponent->tParent);
        plTransformComponent* ptChildTransform = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tChildEntity);
        if(ptParentTransform && ptChildTransform)
            ptChildTransform->tWorld = pl_mul_mat4(&ptParentTransform->tWorld, &ptChildTransform->tWorld);
    }

    pl_end_profile_sample(0);
}

static void
pl_run_script_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    plScriptComponent* sbtComponents = ptLibrary->tScriptComponentManager.pComponents;

    const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        const plEntity tEnitity = ptLibrary->tScriptComponentManager.sbtEntities[i];
        if(sbtComponents[i].tFlags == 0)
            continue;

        if(sbtComponents[i].tFlags & PL_SCRIPT_FLAG_PLAYING)
            sbtComponents[i]._ptApi->run(ptLibrary, tEnitity);
        if(sbtComponents[i].tFlags & PL_SCRIPT_FLAG_PLAY_ONCE)
            sbtComponents[i].tFlags = PL_SCRIPT_FLAG_NONE;
    }
    pl_end_profile_sample(0);
}

static void
pl_run_animation_update_system(plComponentLibrary* ptLibrary, float fDeltaTime)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plAnimationComponent* sbtComponents = ptLibrary->tAnimationComponentManager.pComponents;
    
    const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plAnimationComponent* ptAnimationComponent = &sbtComponents[i];

        if(!(ptAnimationComponent->tFlags & PL_ANIMATION_FLAG_PLAYING))
            continue;

        ptAnimationComponent->fTimer += fDeltaTime;

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

        const uint32_t uChannelCount = pl_sb_size(ptAnimationComponent->sbtChannels);
        for(uint32_t j = 0; j < uChannelCount; j++)
        {
            
            const plAnimationChannel* ptChannel = &ptAnimationComponent->sbtChannels[j];
            const plAnimationSampler* ptSampler = &ptAnimationComponent->sbtSamplers[ptChannel->uSamplerIndex];
            const plAnimationDataComponent* ptData = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION_DATA, ptSampler->tData);
            plTransformComponent* ptTransform = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptChannel->tTarget);

            // wrap t around, so the animation loops.
            // make sure that t is never earlier than the first keyframe and never later then the last keyframe.
            const float fModTime = pl_clampf(ptData->sbfKeyFrameTimes[0], ptAnimationComponent->fTimer, pl_sb_top(ptData->sbfKeyFrameTimes));
            int iNextKey = 0;

            const uint32_t uInputDataCount = pl_sb_size(ptData->sbfKeyFrameTimes);

            for(uint32_t k = 0; k < uInputDataCount; k++)
            {
                
                if(fModTime <= ptData->sbfKeyFrameTimes[k])
                {
                    iNextKey = pl_clampi(1, k, uInputDataCount - 1);
                    break;
                }
            }
            const int iPrevKey = pl_clampi(0, iNextKey - 1, uInputDataCount - 1);

            const float fKeyDelta = ptData->sbfKeyFrameTimes[iNextKey] - ptData->sbfKeyFrameTimes[iPrevKey];

            // normalize t: [t0, t1] -> [0, 1]
            const float fTn = (fModTime - ptData->sbfKeyFrameTimes[iPrevKey]) / fKeyDelta;

            const float fTSq = fTn * fTn;
            const float fTCub = fTSq * fTn;

            const int iA = 0;

            switch(ptChannel->tPath)
            {
                case PL_ANIMATION_PATH_TRANSLATION:
                {

                    if(ptSampler->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec3 tPrev = *(plVec3*)&ptData->sbfKeyFrameData[iPrevKey * 3];
                        const plVec3 tNext = *(plVec3*)&ptData->sbfKeyFrameData[iNextKey * 3];
                        const plVec3 tTranslation = (plVec3){
                            .x = tPrev.x * (1.0f - fTn) + tNext.x * fTn,
                            .y = tPrev.y * (1.0f - fTn) + tNext.y * fTn,
                            .z = tPrev.z * (1.0f - fTn) + tNext.z * fTn,
                        };
                        ptTransform->tTranslation = pl_lerp_vec3(ptTransform->tTranslation, tTranslation, ptAnimationComponent->fBlendAmount);
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        const plVec3 tTranslation = *(plVec3*)&ptData->sbfKeyFrameData[iPrevKey * 3];
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
                            const float iV0 = *(float*)&ptData->sbfKeyFrameData[iPrevIndex + k + iV];
                            const float a = fKeyDelta * *(float*)&ptData->sbfKeyFrameData[iNextIndex + k + iA];
                            const float b = fKeyDelta * *(float*)&ptData->sbfKeyFrameData[iPrevIndex + k + iB];
                            const float v1 = *(float*)&ptData->sbfKeyFrameData[iNextIndex + k + iV];
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
                        const plVec3 tPrev = *(plVec3*)&ptData->sbfKeyFrameData[iPrevKey * 3];
                        const plVec3 tNext = *(plVec3*)&ptData->sbfKeyFrameData[iNextKey * 3];
                        ptTransform->tScale = (plVec3){
                            .x = tPrev.x * (1.0f - fTn) + tNext.x * fTn,
                            .y = tPrev.y * (1.0f - fTn) + tNext.y * fTn,
                            .z = tPrev.z * (1.0f - fTn) + tNext.z * fTn,
                        };
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        ptTransform->tScale = *(plVec3*)&ptData->sbfKeyFrameData[iPrevKey * 3];
                    }

                    else if(ptSampler->tMode == PL_ANIMATION_MODE_CUBIC_SPLINE)
                    {
                        const int iPrevIndex = iPrevKey * 3 * 3;
                        const int iNextIndex = iNextKey * 3 * 3;
                        const int iV = 1 * 3;
                        const int iB = 2 * 3;

                        for(uint32_t k = 0; k < 3; k++)
                        {
                            float v0 = *(float*)&ptData->sbfKeyFrameData[iPrevIndex + k + iV];
                            float a = fKeyDelta * *(float*)&ptData->sbfKeyFrameData[iNextIndex + k + iA];
                            float b = fKeyDelta * *(float*)&ptData->sbfKeyFrameData[iPrevIndex + k + iB];
                            float v1 = *(float*)&ptData->sbfKeyFrameData[iNextIndex + k + iV];
                            ptTransform->tScale.d[k] = ((2 * fTCub - 3 * fTSq + 1) * v0) + ((fTCub - 2 * fTSq + fTn) * b) + ((-2 * fTCub + 3 * fTSq) * v1) + ((fTCub - fTSq) * a);
                        }
                    }
                    break;
                }

                case PL_ANIMATION_PATH_ROTATION:
                {

                    if(ptSampler->tMode == PL_ANIMATION_MODE_LINEAR)
                    {
                        const plVec4 tQ0 = *(plVec4*)&ptData->sbfKeyFrameData[iPrevKey * 4];
                        const plVec4 tQ1 = *(plVec4*)&ptData->sbfKeyFrameData[iNextKey * 4];
                        const plVec4 tRotation = pl_quat_slerp(tQ0, tQ1, fTn);
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tRotation, ptAnimationComponent->fBlendAmount);
                    }
                    else if(ptSampler->tMode == PL_ANIMATION_MODE_STEP)
                    {
                        const plVec4 tRotation = *(plVec4*)&ptData->sbfKeyFrameData[iPrevKey * 4];
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
                            const float iV0 = *(float*)&ptData->sbfKeyFrameData[iPrevIndex + k + iV];
                            const float a = fKeyDelta * *(float*)&ptData->sbfKeyFrameData[iNextIndex + k + iA];
                            const float b = fKeyDelta * *(float*)&ptData->sbfKeyFrameData[iPrevIndex + k + iB];
                            const float iV1 = *(float*)&ptData->sbfKeyFrameData[iNextIndex + k + iV];

                            tResult.d[k] = ((2 * fTCub - 3 * fTSq + 1) * iV0) + ((fTCub - 2 * fTSq + fTn) * b) + ((-2 * fTCub + 3 * fTSq) * iV1) + ((fTCub - fTSq) * a);
                        }
                        ptTransform->tRotation = pl_quat_slerp(ptTransform->tRotation, tResult, ptAnimationComponent->fBlendAmount);
                    }
                    break;
                }
            }
        }
    }

    pl_end_profile_sample(0);
}

static void
pl_run_inverse_kinematics_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    plInverseKinematicsComponent* sbtComponents = ptLibrary->tInverseKinematicsComponentManager.pComponents;
    plTransformComponent* sbtTransforms = ptLibrary->tTransformComponentManager.pComponents;

    plComponentLibraryData* ptData = ptLibrary->pInternal;
    pl_sb_resize(ptData->sbtTransformsCopy, pl_sb_size(sbtTransforms));
    memcpy(ptData->sbtTransformsCopy, sbtTransforms, pl_sb_size(sbtTransforms) * sizeof(plTransformComponent));
    
    bool bRecomputeHierarchy = false;
    const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {

        const plEntity tIKEntity = ptLibrary->tInverseKinematicsComponentManager.sbtEntities[i];
        const size_t uIKIndex = pl_ecs_get_index(&ptLibrary->tInverseKinematicsComponentManager, tIKEntity);

        const plInverseKinematicsComponent* ptInverseKinematicsComponent = &sbtComponents[uIKIndex];
        
        if(!ptInverseKinematicsComponent->bEnabled)
            continue;

        const size_t uTransformIndex = pl_ecs_get_index(&ptLibrary->tTransformComponentManager, tIKEntity);
        const size_t uTargetIndex = pl_ecs_get_index(&ptLibrary->tTransformComponentManager, ptInverseKinematicsComponent->tTarget);

        plTransformComponent* ptTransform = &ptData->sbtTransformsCopy[uTransformIndex];
        plTransformComponent* ptTarget = &ptData->sbtTransformsCopy[uTargetIndex];
        plHierarchyComponent* ptHierComp = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tIKEntity);
        
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
                const size_t uParentIndex = pl_ecs_get_index(&ptLibrary->tTransformComponentManager, tParentEntity);
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
                tQ = pl_norm_vec4(pl_quat_rotation_normal_vec3(fAngle, tAxis));

                // parent to world space
                pl_decompose_matrix(&ptParentTransform->tWorld, &ptParentTransform->tScale, &ptParentTransform->tRotation, &ptParentTransform->tTranslation);

                // rotate parent
                ptParentTransform->tRotation = pl_norm_vec4(pl_mul_quat(tQ, ptParentTransform->tRotation));
                ptParentTransform->tWorld = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);

                // parent back to local space (if parent has parent)
                plHierarchyComponent* ptHierParentComp = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tParentEntity);
                if(ptHierParentComp)
                {
                    plEntity tParentOfParentEntity = ptHierParentComp->tParent;
                    const size_t uGrandParentIndex = pl_ecs_get_index(&ptLibrary->tTransformComponentManager, tParentOfParentEntity);
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

        const uint32_t uHierarchyCount = pl_sb_size(ptLibrary->tHierarchyComponentManager.sbtEntities);
        for(uint32_t i = 0; i < uHierarchyCount; i++)
        {
            const plEntity tChildEntity = ptLibrary->tHierarchyComponentManager.sbtEntities[i];
            
            const size_t uChildIndex = pl_ecs_get_index(&ptLibrary->tTransformComponentManager, tChildEntity);
            PL_ASSERT(uChildIndex != UINT64_MAX);

            const plTransformComponent* ptTransformChild = &ptData->sbtTransformsCopy[uChildIndex];

            plMat4 tWorldMatrix = pl_rotation_translation_scale(ptTransformChild->tRotation, ptTransformChild->tTranslation, ptTransformChild->tScale);

            plHierarchyComponent* ptHierarchyComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tChildEntity);
            
            plEntity tParentID = ptHierarchyComponent->tParent;
            while(tParentID.uIndex != UINT32_MAX)
            {
                const size_t uParentIndex = pl_ecs_get_index(&ptLibrary->tTransformComponentManager, tParentID);
                if(uParentIndex == UINT64_MAX)
                    break;
                plTransformComponent* ptTransformParent = &ptData->sbtTransformsCopy[uParentIndex];
                plMat4 tLocalMatrix = pl_rotation_translation_scale(ptTransformParent->tRotation, ptTransformParent->tTranslation, ptTransformParent->tScale);
                tWorldMatrix = pl_mul_mat4(&tLocalMatrix, &tWorldMatrix);

                const plHierarchyComponent* ptHierarchyRecursive = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tParentID);
                if(ptHierarchyRecursive)
                    tParentID = ptHierarchyRecursive->tParent;
                else
                    tParentID.uIndex = UINT32_MAX;
            }

            sbtTransforms[uChildIndex].tWorld = tWorldMatrix;
        }

    }

    pl_sb_reset(ptData->sbtTransformsCopy);

    pl_end_profile_sample(0);
}

static void
pl_camera_set_fov(plCameraComponent* ptCamera, float fYFov)
{
    ptCamera->fFieldOfView = fYFov;
}

static void
pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ)
{
    ptCamera->fNearZ = fNearZ;
    ptCamera->fFarZ = fFarZ;
}

static void
pl_camera_set_aspect(plCameraComponent* ptCamera, float fAspect)
{
    ptCamera->fAspectRatio = fAspect;
}

static void
pl_camera_set_pos(plCameraComponent* ptCamera, float fX, float fY, float fZ)
{
    ptCamera->tPos.x = fX;
    ptCamera->tPos.y = fY;
    ptCamera->tPos.z = fZ;
}

static void
pl_camera_set_pitch_yaw(plCameraComponent* ptCamera, float fPitch, float fYaw)
{
    ptCamera->fPitch = fPitch;
    ptCamera->fYaw = fYaw;
}

static void
pl_camera_translate(plCameraComponent* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

static void
pl_camera_rotate(plCameraComponent* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = pl__wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

static void
pl_camera_look_at(plCameraComponent* ptCamera, plVec3 tEye, plVec3 tTarget)
{
    const plVec3 tDirection = pl_norm_vec3(pl_sub_vec3(tTarget, tEye));
    ptCamera->fYaw = atan2f(tDirection.x, tDirection.z);
    ptCamera->fPitch = asinf(tDirection.y);
    ptCamera->tPos = tEye;
}

static void
pl_camera_update(plCameraComponent* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(ptCamera->tType == PL_CAMERA_TYPE_PERSPECTIVE)
    {
        const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
        ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
        ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
        ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
        ptCamera->tProjMat.col[2].w = 1.0f;
        ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
        ptCamera->tProjMat.col[3].w = 0.0f;  
    }
    else if(ptCamera->tType == PL_CAMERA_TYPE_ORTHOGRAPHIC)
    {
        ptCamera->tProjMat.col[0].x = 2.0f / ptCamera->fWidth;
        ptCamera->tProjMat.col[1].y = 2.0f / ptCamera->fHeight;
        ptCamera->tProjMat.col[2].z = 1 / (ptCamera->fFarZ - ptCamera->fNearZ);
        ptCamera->tProjMat.col[3].w = 1.0f;
    }
}

static void
pl_calculate_normals(plMeshComponent* atMeshes, uint32_t uComponentCount)
{

    for(uint32_t uMeshIndex = 0; uMeshIndex < uComponentCount; uMeshIndex++)
    {
        plMeshComponent* ptMesh = &atMeshes[uMeshIndex];

        if(pl_sb_size(ptMesh->sbtVertexNormals) == 0)
        {
            pl_sb_resize(ptMesh->sbtVertexNormals, pl_sb_size(ptMesh->sbtVertexPositions));
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbuIndices) - 2; i += 3)
            {
                const uint32_t uIndex0 = ptMesh->sbuIndices[i + 0];
                const uint32_t uIndex1 = ptMesh->sbuIndices[i + 1];
                const uint32_t uIndex2 = ptMesh->sbuIndices[i + 2];

                const plVec3 tP0 = ptMesh->sbtVertexPositions[uIndex0];
                const plVec3 tP1 = ptMesh->sbtVertexPositions[uIndex1];
                const plVec3 tP2 = ptMesh->sbtVertexPositions[uIndex2];

                const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                const plVec3 tNorm = pl_cross_vec3(tEdge1, tEdge2);

                ptMesh->sbtVertexNormals[uIndex0] = tNorm;
                ptMesh->sbtVertexNormals[uIndex1] = tNorm;
                ptMesh->sbtVertexNormals[uIndex2] = tNorm;
            }
        }
    }
}

static void
pl_calculate_tangents(plMeshComponent* atMeshes, uint32_t uComponentCount)
{

    for(uint32_t uMeshIndex = 0; uMeshIndex < uComponentCount; uMeshIndex++)
    {
        plMeshComponent* ptMesh = &atMeshes[uMeshIndex];

        if(pl_sb_size(ptMesh->sbtVertexTangents) == 0 && pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0)
        {
            pl_sb_resize(ptMesh->sbtVertexTangents, pl_sb_size(ptMesh->sbtVertexPositions));
            memset(ptMesh->sbtVertexTangents, 0, pl_sb_size(ptMesh->sbtVertexPositions) * sizeof(plVec4));
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbuIndices) - 2; i += 3)
            {
                const uint32_t uIndex0 = ptMesh->sbuIndices[i + 0];
                const uint32_t uIndex1 = ptMesh->sbuIndices[i + 1];
                const uint32_t uIndex2 = ptMesh->sbuIndices[i + 2];

                const plVec3 tP0 = ptMesh->sbtVertexPositions[uIndex0];
                const plVec3 tP1 = ptMesh->sbtVertexPositions[uIndex1];
                const plVec3 tP2 = ptMesh->sbtVertexPositions[uIndex2];

                const plVec2 tTex0 = ptMesh->sbtVertexTextureCoordinates[0][uIndex0];
                const plVec2 tTex1 = ptMesh->sbtVertexTextureCoordinates[0][uIndex1];
                const plVec2 tTex2 = ptMesh->sbtVertexTextureCoordinates[0][uIndex2];

                const plVec3 atNormals[3] = { 
                    ptMesh->sbtVertexNormals[uIndex0],
                    ptMesh->sbtVertexNormals[uIndex1],
                    ptMesh->sbtVertexNormals[uIndex2],
                };

                const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                const float fDeltaU1 = tTex1.x - tTex0.x;
                const float fDeltaV1 = tTex1.y - tTex0.y;
                const float fDeltaU2 = tTex2.x - tTex0.x;
                const float fDeltaV2 = tTex2.y - tTex0.y;

                const float fSx = fDeltaU1;
                const float fSy = fDeltaU2;
                const float fTx = fDeltaV1;
                const float fTy = fDeltaV2;
                const float fHandedness = ((fSx * fTy - fTx * fSy) < 0.0f) ? -1.0f : 1.0f;

                const plVec3 tTangent = {
                        fHandedness * (fDeltaV2 * tEdge1.x - fDeltaV1 * tEdge2.x),
                        fHandedness * (fDeltaV2 * tEdge1.y - fDeltaV1 * tEdge2.y),
                        fHandedness * (fDeltaV2 * tEdge1.z - fDeltaV1 * tEdge2.z)
                };

                plVec4 atFinalTangents[3] = {0};
                for(uint32_t j = 0; j < 3; j++)
                {
                    atFinalTangents[j].xyz = pl_mul_vec3(tTangent, atNormals[j]);
                    atFinalTangents[j].xyz = pl_mul_vec3(atNormals[j], atFinalTangents[j].xyz);
                    atFinalTangents[j].xyz = pl_norm_vec3(pl_sub_vec3(tTangent, atFinalTangents[j].xyz));
                    atFinalTangents[j].w = fHandedness;
                }

                ptMesh->sbtVertexTangents[uIndex0] = atFinalTangents[0];
                ptMesh->sbtVertexTangents[uIndex1] = atFinalTangents[1];
                ptMesh->sbtVertexTangents[uIndex2] = atFinalTangents[2];
            } 
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plEcsI*
pl_load_ecs_api(void)
{
    static const plEcsI tApi = {
        .init_component_library               = pl_ecs_init_component_library,
        .cleanup_component_library            = pl_ecs_cleanup_component_library,
        .create_entity                        = pl_ecs_create_entity,
        .remove_entity                        = pl_ecs_remove_entity,
        .get_entity                           = pl_ecs_get_entity,
        .is_entity_valid                      = pl_ecs_is_entity_valid,
        .get_index                            = pl_ecs_get_index,
        .get_component                        = pl_ecs_get_component,
        .add_component                        = pl_ecs_add_component,
        .create_tag                           = pl_ecs_create_tag,
        .create_mesh                          = pl_ecs_create_mesh,
        .create_perspective_camera            = pl_ecs_create_perspective_camera,
        .create_orthographic_camera           = pl_ecs_create_orthographic_camera,
        .create_object                        = pl_ecs_create_object,
        .create_transform                     = pl_ecs_create_transform,
        .create_material                      = pl_ecs_create_material,
        .create_skin                          = pl_ecs_create_skin,
        .create_animation                     = pl_ecs_create_animation,
        .create_animation_data                = pl_ecs_create_animation_data,
        .create_directional_light             = pl_ecs_create_directional_light,
        .create_point_light                   = pl_ecs_create_point_light,
        .create_script                        = pl_ecs_create_script,
        .attach_script                        = pl_ecs_attach_script,
        .attach_component                     = pl_ecs_attach_component,
        .deattach_component                   = pl_ecs_deattach_component,
        .calculate_normals                    = pl_calculate_normals,
        .calculate_tangents                   = pl_calculate_tangents,
        .run_object_update_system             = pl_run_object_update_system,
        .run_transform_update_system          = pl_run_transform_update_system,
        .run_hierarchy_update_system          = pl_run_hierarchy_update_system,
        .run_skin_update_system               = pl_run_skin_update_system,
        .run_animation_update_system          = pl_run_animation_update_system,
        .run_inverse_kinematics_update_system = pl_run_inverse_kinematics_update_system,
        .run_script_update_system             = pl_run_script_update_system
    };
    return &tApi;
}

static const plCameraI*
pl_load_camera_api(void)
{
    static const plCameraI tApi = {
        .set_fov         = pl_camera_set_fov,
        .set_clip_planes = pl_camera_set_clip_planes,
        .set_aspect      = pl_camera_set_aspect,
        .set_pos         = pl_camera_set_pos,
        .set_pitch_yaw   = pl_camera_set_pitch_yaw,
        .translate       = pl_camera_translate,
        .rotate          = pl_camera_rotate,
        .update          = pl_camera_update,
        .look_at         = pl_camera_look_at,
    };
    return &tApi;   
}

static void
pl_load_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->add(PL_API_ECS, pl_load_ecs_api());
    ptApiRegistry->add(PL_API_CAMERA, pl_load_camera_api());

    if(bReload)
    {
        uLogChannelEcs = pl_get_log_channel_id("ECS");
    }
    else // first load
    {
        plLogChannelInit tLogInit = {
            .tType       = PL_CHANNEL_TYPE_CYCLIC_BUFFER,
            .uEntryCount = 256
        };
        uLogChannelEcs = pl_add_log_channel("ECS", tLogInit);
    }
}

static void
pl_unload_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->remove(pl_load_ecs_api());
    ptApiRegistry->remove(pl_load_camera_api());
}
