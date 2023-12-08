/*
   pl_ecs_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pilotlight.h"
#include "pl_ecs_ext.h"
#include "pl_ds.h"
#include "pl_math.h"
#include "pl_profile.h"
#include "pl_log.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static uint32_t uLogChannel = UINT32_MAX;

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
static plEntity pl_ecs_create_tag             (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_mesh            (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_object          (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_transform       (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_material        (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_skin            (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_camera          (plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

// heirarchy
static void pl_ecs_attach_component (plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent);
static void pl_ecs_deattach_component(plComponentLibrary* ptLibrary, plEntity tEntity);

// update systems
static void pl_run_skin_update_system     (plComponentLibrary* ptLibrary);
static void pl_run_hierarchy_update_system(plComponentLibrary* ptLibrary);

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
    return pl_hm_has_key(&ptManager->tHashMap, tEntity.uIndex);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plEcsI*
pl_load_ecs_api(void)
{
    static const plEcsI tApi = {
        .init_component_library      = pl_ecs_init_component_library,
        .cleanup_component_library   = pl_ecs_cleanup_component_library,
        .create_entity               = pl_ecs_create_entity,
        .remove_entity               = pl_ecs_remove_entity,
        .get_entity                  = pl_ecs_get_entity,
        .is_entity_valid             = pl_ecs_is_entity_valid,
        .get_index                   = pl_ecs_get_index,
        .get_component               = pl_ecs_get_component,
        .add_component               = pl_ecs_add_component,
        .create_tag                  = pl_ecs_create_tag,
        .create_mesh                 = pl_ecs_create_mesh,
        .create_camera               = pl_ecs_create_camera,
        .create_object               = pl_ecs_create_object,
        .create_transform            = pl_ecs_create_transform,
        .create_material             = pl_ecs_create_material,
        .create_skin                 = pl_ecs_create_skin,
        .attach_component            = pl_ecs_attach_component,
        .deattach_component          = pl_ecs_deattach_component,
        .calculate_normals           = pl_calculate_normals,
        .calculate_tangents          = pl_calculate_tangents,
        .run_hierarchy_update_system = pl_run_hierarchy_update_system,
        .run_skin_update_system      = pl_run_skin_update_system
    };
    return &tApi;
}

const plCameraI*
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
        .update          = pl_camera_update
    };
    return &tApi;   
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

    ptLibrary->_ptManagers[0] = &ptLibrary->tTagComponentManager;
    ptLibrary->_ptManagers[1] = &ptLibrary->tTransformComponentManager;
    ptLibrary->_ptManagers[2] = &ptLibrary->tMeshComponentManager;
    ptLibrary->_ptManagers[3] = &ptLibrary->tObjectComponentManager;
    ptLibrary->_ptManagers[4] = &ptLibrary->tHierarchyComponentManager;
    ptLibrary->_ptManagers[5] = &ptLibrary->tMaterialComponentManager;
    ptLibrary->_ptManagers[6] = &ptLibrary->tSkinComponentManager;
    ptLibrary->_ptManagers[7] = &ptLibrary->tCameraComponentManager;

    for(uint32_t i = 0; i < PL_COMPONENT_TYPE_COUNT; i++)
        ptLibrary->_ptManagers[i]->ptParentLibrary = ptLibrary;

    pl_log_info_to(uLogChannel, "initialized component library");
}

static void
pl_ecs_cleanup_component_library(plComponentLibrary* ptLibrary)
{
    plMeshComponent* sbtMeshes = ptLibrary->tMeshComponentManager.pComponents;

    for(uint32_t i = 0; i < pl_sb_size(sbtMeshes); i++)
    {
        pl_sb_free(sbtMeshes[i].sbtVertexPositions);
        pl_sb_free(sbtMeshes[i].sbtVertexNormals);
        pl_sb_free(sbtMeshes[i].sbtVertexTangents);
        pl_sb_free(sbtMeshes[i].sbtVertexColors0);
        pl_sb_free(sbtMeshes[i].sbtVertexColors1);
        pl_sb_free(sbtMeshes[i].sbtVertexWeights0);
        pl_sb_free(sbtMeshes[i].sbtVertexWeights1);
        pl_sb_free(sbtMeshes[i].sbtVertexJoints0);
        pl_sb_free(sbtMeshes[i].sbtVertexJoints1);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates0);
        pl_sb_free(sbtMeshes[i].sbtVertexTextureCoordinates1);
        pl_sb_free(sbtMeshes[i].sbuIndices);
    }

    for(uint32_t i = 0; i < PL_COMPONENT_TYPE_COUNT; i++)
    {
        pl_sb_free(ptLibrary->_ptManagers[i]->pComponents);
        pl_sb_free(ptLibrary->_ptManagers[i]->sbtEntities);
        pl_hm_free(&ptLibrary->_ptManagers[i]->tHashMap);
    }

    // general
    pl_sb_free(ptLibrary->sbtEntityFreeIndices);
    pl_sb_free(ptLibrary->sbtEntityGenerations);
    pl_hm_free(&ptLibrary->tTagHashMap);
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
    ptLibrary->sbtEntityGenerations[tEntity.uIndex]++;

    // remove from tag hashmap
    plTagComponent* ptTag = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, tEntity);
    if(ptTag)
    {
        pl_hm_remove_str(&ptLibrary->tTagHashMap, ptTag->acName);
    }

    // remove from individual managers
    for(uint32_t i = 0; i < PL_COMPONENT_TYPE_COUNT; i++)
    {
        if(pl_hm_has_key(&ptLibrary->_ptManagers[i]->tHashMap, tEntity.uIndex))
            pl_hm_remove(&ptLibrary->_ptManagers[i]->tHashMap, tEntity.uIndex);
    }
}

static bool
pl_ecs_is_entity_valid(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    return ptLibrary->sbtEntityGenerations[tEntity.uIndex] == tEntity.uGeneration;
}

static plEntity
pl_ecs_get_entity(plComponentLibrary* ptLibrary, const char* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);
    if(pl_hm_has_key(&ptLibrary->tTagHashMap, ulHash))
    {
        uint64_t uIndex = pl_hm_lookup(&ptLibrary->tTagHashMap, ulHash);
        return (plEntity){.uIndex = (uint32_t)uIndex, .uGeneration = ptLibrary->sbtEntityGenerations[uIndex]};
    }
    return (plEntity){UINT32_MAX, UINT32_MAX};
}

static size_t
pl_ecs_get_index(plComponentManager* ptManager, plEntity tEntity)
{ 
    PL_ASSERT(tEntity.uIndex != UINT32_MAX);
    size_t szIndex = pl_hm_lookup(&ptManager->tHashMap, (uint64_t)tEntity.uIndex);
    PL_ASSERT(szIndex != UINT64_MAX);
    return szIndex;
}

static void*
pl_ecs_get_component(plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity)
{
    PL_ASSERT(tEntity.uIndex != UINT32_MAX);

    plComponentManager* ptManager = ptLibrary->_ptManagers[tType];

    if(ptManager->ptParentLibrary->sbtEntityGenerations[tEntity.uIndex] != tEntity.uGeneration)
        return NULL;

    size_t szIndex = pl_ecs_get_index(ptManager, tEntity);
    unsigned char* pucData = ptManager->pComponents;
    return &pucData[szIndex * ptManager->szStride];
}

static void*
pl_ecs_add_component(plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity)
{
    PL_ASSERT(tEntity.uIndex != UINT32_MAX);

    plComponentManager* ptManager = ptLibrary->_ptManagers[tType];

    if(ptManager->ptParentLibrary->sbtEntityGenerations[tEntity.uIndex] != tEntity.uGeneration)
        return NULL;

    PL_ASSERT(tEntity.uIndex != UINT32_MAX);

    uint64_t uComponentIndex = pl_hm_get_free_index(&ptManager->tHashMap);
    bool bAddSlot = false; // can't add component with SB without correct type
    if(uComponentIndex == UINT64_MAX)
    {
        uComponentIndex = pl_sb_size(ptManager->sbtEntities);
        pl_sb_add(ptManager->sbtEntities);
        bAddSlot = true;
    }
    pl_hm_insert(&ptManager->tHashMap, (uint64_t)tEntity.uIndex, uComponentIndex);

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
        sbComponents[uComponentIndex] = (plTransformComponent){.tWorld = pl_identity_mat4(), .tFinalTransform = pl_identity_mat4()};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_OBJECT:
    {
        plObjectComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plObjectComponent){0};
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
        sbComponents[uComponentIndex] = (plMaterialComponent){0};
        return &sbComponents[uComponentIndex];
    }

    case PL_COMPONENT_TYPE_SKIN:
    {
        plSkinComponent* sbComponents = ptManager->pComponents;
        if(bAddSlot)
            pl_sb_add(sbComponents);
        ptManager->pComponents = sbComponents;
        sbComponents[uComponentIndex] = (plSkinComponent){.tSkeleton = {UINT32_MAX, UINT32_MAX}};
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
        pl_hm_insert_str(&ptLibrary->tTagHashMap, pcName, tNewEntity.uIndex);
    return tNewEntity;
}

static plEntity
pl_ecs_create_mesh(plComponentLibrary* ptLibrary, const char* pcName)
{
    pl_log_debug_to_f(uLogChannel, "created mesh: '%s'", pcName ? pcName : "unnamed");
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);
    pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewEntity);
    return tNewEntity;
}

static plEntity
pl_ecs_create_object(plComponentLibrary* ptLibrary, const char* pcName)
{
    pl_log_debug_to_f(uLogChannel, "created object: '%s'", pcName ? pcName : "unnamed");
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plObjectComponent* ptObject = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tNewEntity);
    pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
    pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewEntity);

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tNewEntity;

    return tNewEntity;    
}

static plEntity
pl_ecs_create_transform(plComponentLibrary* ptLibrary, const char* pcName)
{
    pl_log_debug_to_f(uLogChannel, "created transform: '%s'", pcName ? pcName : "unnamed");
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plTransformComponent* ptTransform = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
    ptTransform->tWorld = pl_identity_mat4();

    return tNewEntity;  
}

static plEntity
pl_ecs_create_material(plComponentLibrary* ptLibrary, const char* pcName)
{
    pl_log_debug_to_f(uLogChannel, "created material: '%s'", pcName ? pcName : "unnamed");
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plMaterialComponent* ptMaterial = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, tNewEntity);
    ptMaterial->tAlbedo = (plVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ptMaterial->fAlphaCutoff = 0.1f;
    ptMaterial->bDoubleSided = false;
    return tNewEntity;    
}

static plEntity
pl_ecs_create_skin(plComponentLibrary* ptLibrary, const char* pcName)
{
    pl_log_debug_to_f(uLogChannel, "created skin: '%s'", pcName ? pcName : "unnamed");
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    plSkinComponent* ptSkin = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, tNewEntity);
    return tNewEntity;
}

static plEntity
pl_ecs_create_camera(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ)
{
    pl_log_debug_to_f(uLogChannel, "created camera: '%s'", pcName ? pcName : "unnamed");
    plEntity tNewEntity = pl_ecs_create_tag(ptLibrary, pcName);

    const plCameraComponent tCamera = {
        .tPos         = tPos,
        .fNearZ       = fNearZ,
        .fFarZ        = fFarZ,
        .fFieldOfView = fYFov,
        .fAspectRatio = fAspect
    };

    plCameraComponent* ptCamera = pl_ecs_add_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

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
    pl_begin_profile_sample(__FUNCTION__);
    plSkinComponent* sbtComponents = ptLibrary->tSkinComponentManager.pComponents;

    for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
    {
        plSkinComponent* ptSkinComponent = &sbtComponents[i];
        plMat4 tWorldTransform = pl_identity_mat4();
        plMat4 tInverseWorldTransform = pl_identity_mat4();
        if(ptSkinComponent->tSkeleton.uIndex != UINT32_MAX)
        {
            plTransformComponent* ptParentComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSkinComponent->tSkeleton);
            tWorldTransform = ptParentComponent->tFinalTransform;
            tInverseWorldTransform = pl_mat4_invert(&ptParentComponent->tFinalTransform);

        }
        for(uint32_t j = 0; j < pl_sb_size(ptSkinComponent->sbtJoints); j++)
        {
            plEntity tJointEntity = ptSkinComponent->sbtJoints[j];
            plTransformComponent* ptJointComponent = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tJointEntity);

            const plMat4* ptIBM = &ptSkinComponent->sbtInverseBindMatrices[j];
            plMat4 tJointMatrix = pl_mul_mat4(&ptJointComponent->tFinalTransform, ptIBM);
            tJointMatrix = pl_mul_mat4(&tInverseWorldTransform, &tJointMatrix);
            plMat4 tInvertJoint = pl_mat4_invert(&tJointMatrix);
            plMat4 tNormalMatrix = pl_mat4_transpose(&tInvertJoint);
            ptSkinComponent->sbtTextureData[j*2] = tJointMatrix;
            ptSkinComponent->sbtTextureData[j*2 + 1] = tNormalMatrix;
        }
    }

    pl_end_profile_sample();
}

static void
pl_run_hierarchy_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(__FUNCTION__);
    plHierarchyComponent* sbtComponents = ptLibrary->tHierarchyComponentManager.pComponents;

    for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
    {
        plHierarchyComponent* ptHierarchyComponent = &sbtComponents[i];
        plEntity tChildEntity = ptLibrary->tHierarchyComponentManager.sbtEntities[i];
        plTransformComponent* ptParentTransform = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptHierarchyComponent->tParent);
        plTransformComponent* ptChildTransform = pl_ecs_get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tChildEntity);
        ptChildTransform->tFinalTransform = pl_mul_mat4(&ptParentTransform->tFinalTransform, &ptChildTransform->tWorld);
    }

    pl_end_profile_sample();
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
pl_camera_update(plCameraComponent* ptCamera)
{
    pl_begin_profile_sample(__FUNCTION__);

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
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;  

    pl_end_profile_sample();  
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

        if(pl_sb_size(ptMesh->sbtVertexTangents) == 0 && pl_sb_size(ptMesh->sbtVertexTextureCoordinates0) > 0)
        {
            pl_sb_resize(ptMesh->sbtVertexTangents, pl_sb_size(ptMesh->sbtVertexPositions));
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbuIndices) - 2; i += 3)
            {
                const uint32_t uIndex0 = ptMesh->sbuIndices[i + 0];
                const uint32_t uIndex1 = ptMesh->sbuIndices[i + 1];
                const uint32_t uIndex2 = ptMesh->sbuIndices[i + 2];

                const plVec3 tP0 = ptMesh->sbtVertexPositions[uIndex0];
                const plVec3 tP1 = ptMesh->sbtVertexPositions[uIndex1];
                const plVec3 tP2 = ptMesh->sbtVertexPositions[uIndex2];

                const plVec2 tTex0 = ptMesh->sbtVertexTextureCoordinates0[uIndex0];
                const plVec2 tTex1 = ptMesh->sbtVertexTextureCoordinates0[uIndex1];
                const plVec2 tTex2 = ptMesh->sbtVertexTextureCoordinates0[uIndex2];

                const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                const float fDeltaU1 = tTex1.x - tTex0.x;
                const float fDeltaV1 = tTex1.y - tTex0.y;
                const float fDeltaU2 = tTex2.x - tTex0.x;
                const float fDeltaV2 = tTex2.y - tTex0.y;

                const float fDividend = (fDeltaU1 * fDeltaV2 - fDeltaU2 * fDeltaV1);
                const float fC = 1.0f / fDividend;

                const float fSx = fDeltaU1;
                const float fSy = fDeltaU2;
                const float fTx = fDeltaV1;
                const float fTy = fDeltaV2;
                const float fHandedness = ((fTx * fSy - fTy * fSx) < 0.0f) ? -1.0f : 1.0f;

                const plVec3 tTangent = 
                    pl_norm_vec3((plVec3){
                        fC * (fDeltaV2 * tEdge1.x - fDeltaV1 * tEdge2.x),
                        fC * (fDeltaV2 * tEdge1.y - fDeltaV1 * tEdge2.y),
                        fC * (fDeltaV2 * tEdge1.z - fDeltaV1 * tEdge2.z)
                });

                ptMesh->sbtVertexTangents[uIndex0] = (plVec4){tTangent.x, tTangent.y, tTangent.z, fHandedness};
                ptMesh->sbtVertexTangents[uIndex1] = (plVec4){tTangent.x, tTangent.y, tTangent.z, fHandedness};
                ptMesh->sbtVertexTangents[uIndex2] = (plVec4){tTangent.x, tTangent.y, tTangent.z, fHandedness};
            } 
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ecs_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_ECS), pl_load_ecs_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_CAMERA), pl_load_camera_api());

        // find log channel
        uint32_t uChannelCount = 0;
        plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
        for(uint32_t i = 0; i < uChannelCount; i++)
        {
            if(strcmp(ptChannels[i].pcName, "ECS") == 0)
            {
                uLogChannel = i;
                break;
            }
        }
    }
    else
    {
        ptApiRegistry->add(PL_API_ECS, pl_load_ecs_api());
        ptApiRegistry->add(PL_API_CAMERA, pl_load_camera_api());
        uLogChannel = pl_add_log_channel("ECS", PL_CHANNEL_TYPE_CYCLIC_BUFFER);
        // uLogChannel = pl_add_log_channel("ECS", PL_CHANNEL_TYPE_BUFFER | PL_CHANNEL_TYPE_CONSOLE);
    }
}

PL_EXPORT void
pl_unload_ecs_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}
