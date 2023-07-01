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

// extensions
#include "pl_vulkan_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------
static uint32_t uLogChannel = UINT32_MAX;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void     pl_ecs_init_component_library(const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptLibrary);
static plEntity pl_ecs_create_entity         (plComponentLibrary* ptLibrary);
static plEntity pl_ecs_get_entity            (plComponentLibrary* ptLibrary, const char* pcName);
static size_t   pl_ecs_get_index             (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_get_component         (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_create_component      (plComponentManager* ptManager, plEntity tEntity);
static bool     pl_ecs_has_entity            (plComponentManager* ptManager, plEntity tEntity);

static plVec4   pl_entity_to_color(plEntity tEntity);
static plEntity pl_color_to_entity(const plVec4* ptColor);

// components
static plEntity pl_ecs_create_mesh            (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_material        (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_outline_material(plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_object          (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_transform       (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_camera          (plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);
static plEntity pl_ecs_create_light           (plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, plVec3 tColor);

static void pl_ecs_attach_component (plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent);
static void pl_ecs_deattach_component(plComponentLibrary* ptLibrary, plEntity tEntity);

// material
static void pl_add_mesh_outline(plComponentLibrary* ptLibrary, plEntity tEntity);
static void pl_remove_mesh_outline(plComponentLibrary* ptLibrary, plEntity tEntity);

// update systems
static void pl_ecs_cleanup_systems        (const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptLibrary);
static void pl_run_object_update_system   (plComponentLibrary* ptLibrary);
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

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plEcsI*
pl_load_ecs_api(void)
{
    static const plEcsI tApi = {
        .init_component_library      = pl_ecs_init_component_library,
        .create_entity               = pl_ecs_create_entity,
        .get_entity                  = pl_ecs_get_entity,
        .get_index                   = pl_ecs_get_index,
        .get_component               = pl_ecs_get_component,
        .create_component            = pl_ecs_create_component,
        .has_entity                  = pl_ecs_has_entity,
        .create_mesh                 = pl_ecs_create_mesh,
        .create_material             = pl_ecs_create_material,
        .create_object               = pl_ecs_create_object,
        .create_transform            = pl_ecs_create_transform,
        .create_camera               = pl_ecs_create_camera,
        .create_light                = pl_ecs_create_light,
        .add_mesh_outline            = pl_add_mesh_outline,
        .remove_mesh_outline         = pl_remove_mesh_outline,
        .attach_component            = pl_ecs_attach_component,
        .deattach_component          = pl_ecs_deattach_component,
        .cleanup_systems             = pl_ecs_cleanup_systems,
        .run_object_update_system    = pl_run_object_update_system,
        .calculate_normals           = pl_calculate_normals,
        .calculate_tangents          = pl_calculate_tangents,
        .run_hierarchy_update_system = pl_run_hierarchy_update_system,
        .entity_to_color             = pl_entity_to_color,
        .color_to_entity             = pl_color_to_entity
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

static float
pl__wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

static void
pl_ecs_init_component_library(const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptLibrary)
{

    ptLibrary->tNextEntity = 1;

    // initialize component managers
    ptLibrary->tTagComponentManager.tComponentType = PL_COMPONENT_TYPE_TAG;
    ptLibrary->tTagComponentManager.szStride = sizeof(plTagComponent);

    ptLibrary->tTransformComponentManager.tComponentType = PL_COMPONENT_TYPE_TRANSFORM;
    ptLibrary->tTransformComponentManager.szStride = sizeof(plTransformComponent);

    ptLibrary->tObjectComponentManager.tComponentType = PL_COMPONENT_TYPE_OBJECT;
    ptLibrary->tObjectComponentManager.szStride = sizeof(plObjectComponent);
    ptLibrary->tObjectComponentManager.pSystemData = PL_ALLOC(sizeof(plObjectSystemData));
    memset(ptLibrary->tObjectComponentManager.pSystemData, 0, sizeof(plObjectSystemData));

    ptLibrary->tMaterialComponentManager.tComponentType = PL_COMPONENT_TYPE_MATERIAL;
    ptLibrary->tMaterialComponentManager.szStride = sizeof(plMaterialComponent);

    ptLibrary->tMeshComponentManager.tComponentType = PL_COMPONENT_TYPE_MESH;
    ptLibrary->tMeshComponentManager.szStride = sizeof(plMeshComponent);
    
    ptLibrary->tCameraComponentManager.tComponentType = PL_COMPONENT_TYPE_CAMERA;
    ptLibrary->tCameraComponentManager.szStride = sizeof(plCameraComponent);

    ptLibrary->tHierarchyComponentManager.tComponentType = PL_COMPONENT_TYPE_HIERARCHY;
    ptLibrary->tHierarchyComponentManager.szStride = sizeof(plHierarchyComponent);

    ptLibrary->tLightComponentManager.tComponentType = PL_COMPONENT_TYPE_LIGHT;
    ptLibrary->tLightComponentManager.szStride = sizeof(plLightComponent);

    pl_log_info_to(uLogChannel, "initialized component library");

}

static plEntity
pl_ecs_create_entity(plComponentLibrary* ptLibrary)
{
    plEntity tNewEntity = ptLibrary->tNextEntity++;
    return tNewEntity;
}

static plEntity
pl_ecs_get_entity(plComponentLibrary* ptLibrary, const char* pcName)
{
    plTagComponent* sbComponents = ptLibrary->tTagComponentManager.pComponents;
    for(uint32_t i = 0; i < pl_sb_size(sbComponents); i++)
    {
        if(strcmp(sbComponents[i].acName, pcName) == 0)
        {
            return ptLibrary->tTagComponentManager.sbtEntities[i];
        }
    }

    return PL_INVALID_ENTITY_HANDLE;
}

static size_t
pl_ecs_get_index(plComponentManager* ptManager, plEntity tEntity)
{ 
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);
    bool bFound = false;
    size_t szIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptManager->sbtEntities); i++)
    {
        if(ptManager->sbtEntities[i] == tEntity)
        {
            szIndex = (size_t)i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);
    return szIndex;
}

static void*
pl_ecs_get_component(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);
    size_t szIndex = pl_ecs_get_index(ptManager, tEntity);
    unsigned char* pucData = ptManager->pComponents;
    return &pucData[szIndex * ptManager->szStride];
}

static void*
pl_ecs_create_component(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);

    switch (ptManager->tComponentType)
    {
    case PL_COMPONENT_TYPE_TAG:
    {
        plTagComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, (plTagComponent){0});
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MESH:
    {
        plMeshComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, (plMeshComponent){0});
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_TRANSFORM:
    {
        plTransformComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, ((plTransformComponent){.tWorld = pl_identity_mat4(), .tFinalTransform = pl_identity_mat4()}));
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MATERIAL:
    {
        plMaterialComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, (plMaterialComponent){0});
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_OBJECT:
    {
        plObjectComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, (plObjectComponent){0});
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_CAMERA:
    {
        plCameraComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, (plCameraComponent){0});
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_HIERARCHY:
    {
        plHierarchyComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, (plHierarchyComponent){0});
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_LIGHT:
    {
        plLightComponent* sbComponents = ptManager->pComponents;
        pl_sb_push(sbComponents, ((plLightComponent){.tColor = {1.0f, 1.0f, 1.0f}}));
        ptManager->pComponents = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    }

    return NULL;
}

static bool
pl_ecs_has_entity(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);

    for(uint32_t i = 0; i < pl_sb_size(ptManager->sbtEntities); i++)
    {
        if(ptManager->sbtEntities[i] == tEntity)
            return true;
    }
    return false;
}

static plVec4
pl_entity_to_color(plEntity tEntity)
{
    const uint32_t uId = (uint32_t)tEntity;
    return (plVec4){
        ((float)(uId & 0xff) / 255.0f),
        ((float)((uId >> 8) & 0xff) / 255.0f),
        ((float)((uId >> 16) & 0xff) / 255.0f),
        1.0f};
}

static plEntity
pl_color_to_entity(const plVec4* ptColor)
{
    unsigned char* pucMapping = (unsigned char*)ptColor;
    return (plEntity)(pucMapping[0] | pucMapping[1] << 8 | pucMapping[2] << 16);
}

static plEntity
pl_ecs_create_mesh(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created mesh '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed mesh");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plMeshComponent* ptMesh = pl_ecs_create_component(&ptLibrary->tMeshComponentManager, tNewEntity);
    return tNewEntity;
}

static plEntity
pl_ecs_create_outline_material(plComponentLibrary* ptLibrary, const char* pcName)
{

    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created outline material '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed outline material");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plMaterialComponent* ptMaterial = pl_ecs_create_component(&ptLibrary->tMaterialComponentManager, tNewEntity);
    memset(ptMaterial, 0, sizeof(plMaterialComponent));
    ptMaterial->uShader                             = UINT32_MAX;
    ptMaterial->tAlbedo                             = (plVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ptMaterial->fAlphaCutoff                        = 0.1f;
    ptMaterial->bDoubleSided                        = false;
    ptMaterial->tShaderType                         = PL_SHADER_TYPE_UNLIT;
    ptMaterial->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    ptMaterial->tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_ALWAYS;
    ptMaterial->tGraphicsState.ulDepthWriteEnabled  = false;
    ptMaterial->tGraphicsState.ulCullMode           = VK_CULL_MODE_FRONT_BIT;
    ptMaterial->tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA;
    ptMaterial->tGraphicsState.ulShaderTextureFlags = 0;
    ptMaterial->tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_NOT_EQUAL;
    ptMaterial->tGraphicsState.ulStencilRef         = 0xff;
    ptMaterial->tGraphicsState.ulStencilMask        = 0xff;
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_REPLACE;
    return tNewEntity;   
}

static plEntity
pl_ecs_create_material(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created material '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed material");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plMaterialComponent* ptMaterial = pl_ecs_create_component(&ptLibrary->tMaterialComponentManager, tNewEntity);
    memset(ptMaterial, 0, sizeof(plMaterialComponent));
    ptMaterial->uShader = UINT32_MAX;
    ptMaterial->tAlbedo = (plVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ptMaterial->fAlphaCutoff = 0.1f;
    ptMaterial->bDoubleSided = false;
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    ptMaterial->tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL;
    ptMaterial->tGraphicsState.ulDepthWriteEnabled  = true;
    ptMaterial->tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE;
    ptMaterial->tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA;
    ptMaterial->tGraphicsState.ulShaderTextureFlags = 0;
    ptMaterial->tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS;
    ptMaterial->tGraphicsState.ulStencilRef         = 0xff;
    ptMaterial->tGraphicsState.ulStencilMask        = 0xff;
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_KEEP;

    return tNewEntity;    
}

static void
pl_add_mesh_outline(plComponentLibrary* ptLibrary, plEntity tEntity)
{

    pl_log_debug_to_f(uLogChannel, "add mesh outline to %u", tEntity);


    if(pl_ecs_has_entity(&ptLibrary->tMeshComponentManager, tEntity))
    {
        plMeshComponent* ptMesh = pl_ecs_get_component(&ptLibrary->tMeshComponentManager, tEntity);

        plMaterialComponent* ptMaterial = pl_ecs_get_component(&ptLibrary->tMaterialComponentManager, ptMesh->tMaterial);
        ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_REPLACE;
        ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_REPLACE;
        ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_REPLACE;

        if(ptMesh->tOutlineMaterial == 0)
        {
            ptMesh->tOutlineMaterial = pl_ecs_create_outline_material(ptLibrary, "outline material");
            plMaterialComponent* ptOutlineMaterial = pl_ecs_get_component(&ptLibrary->tMaterialComponentManager, ptMesh->tOutlineMaterial);
            ptMaterial = pl_ecs_get_component(&ptLibrary->tMaterialComponentManager, ptMesh->tMaterial);
            ptOutlineMaterial->tGraphicsState.ulVertexStreamMask = ptMaterial->tGraphicsState.ulVertexStreamMask;
        }
    }
}

static void
pl_remove_mesh_outline(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    pl_log_debug_to_f(uLogChannel, "remove mesh outline to %u", tEntity);

    if(pl_ecs_has_entity(&ptLibrary->tMeshComponentManager, tEntity))
    {
        plMeshComponent* ptMesh = pl_ecs_get_component(&ptLibrary->tMeshComponentManager, tEntity);
        plMaterialComponent* ptMaterial = pl_ecs_get_component(&ptLibrary->tMaterialComponentManager, ptMesh->tMaterial);
        ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_KEEP;
        ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_KEEP;
        ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_KEEP;
    }
}

static void
pl_ecs_cleanup_systems(const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptLibrary)
{

    plObjectSystemData* ptObjectSystemData = ptLibrary->tObjectComponentManager.pSystemData;

    for(uint32_t i = 0; i < pl_sb_size(ptObjectSystemData->sbtSubmeshes); i++)
    {
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexPositions);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexNormals);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexTangents);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexColors0);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexColors1);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexWeights0);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexWeights1);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexJoints0);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexJoints1);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexTextureCoordinates0);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbtVertexTextureCoordinates1);
        pl_sb_free(ptObjectSystemData->sbtSubmeshes[i]->sbuIndices);
    }
    pl_sb_free(ptObjectSystemData->sbtSubmeshes);
    PL_FREE(ptObjectSystemData);
    ptLibrary->tObjectComponentManager.pSystemData = NULL;

    plMeshComponent* ptMeshComponents = ptLibrary->tMeshComponentManager.pComponents;

    // components
    pl_sb_free(ptLibrary->tTagComponentManager.pComponents);
    pl_sb_free(ptLibrary->tTransformComponentManager.pComponents);
    pl_sb_free(ptLibrary->tMeshComponentManager.pComponents);
    pl_sb_free(ptLibrary->tMaterialComponentManager.pComponents);
    pl_sb_free(ptLibrary->tObjectComponentManager.pComponents);
    pl_sb_free(ptLibrary->tCameraComponentManager.pComponents);
    pl_sb_free(ptLibrary->tHierarchyComponentManager.pComponents);

    // entities
    pl_sb_free(ptLibrary->tTagComponentManager.sbtEntities);
    pl_sb_free(ptLibrary->tTransformComponentManager.sbtEntities);
    pl_sb_free(ptLibrary->tMeshComponentManager.sbtEntities);
    pl_sb_free(ptLibrary->tMaterialComponentManager.sbtEntities);
    pl_sb_free(ptLibrary->tObjectComponentManager.sbtEntities);
    pl_sb_free(ptLibrary->tCameraComponentManager.sbtEntities);
    pl_sb_free(ptLibrary->tHierarchyComponentManager.sbtEntities);
}

static void
pl_run_object_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_profile_sample(__FUNCTION__);
    plObjectComponent* sbtComponents = ptLibrary->tObjectComponentManager.pComponents;
    plObjectSystemData* ptObjectSystemData = ptLibrary->tObjectComponentManager.pSystemData;
    pl_sb_reset(ptObjectSystemData->sbtSubmeshes);

    for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
    {
        plMeshComponent* ptMeshComponent = pl_ecs_get_component(&ptLibrary->tMeshComponentManager, sbtComponents[i].tMesh);
        plTransformComponent* ptTransform = pl_ecs_get_component(&ptLibrary->tTransformComponentManager, sbtComponents[i].tTransform);
        ptMeshComponent->tInfo.tModel = ptTransform->tFinalTransform;
        pl_sb_push(ptObjectSystemData->sbtSubmeshes, ptMeshComponent);
    }
    pl_end_profile_sample();
}

static void
pl_calculate_normals(plMeshComponent* atMeshes, uint32_t uComponentCount)
{
    pl_begin_profile_sample(__FUNCTION__);

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
    pl_end_profile_sample();
}

static void
pl_calculate_tangents(plMeshComponent* atMeshes, uint32_t uComponentCount)
{
    pl_begin_profile_sample(__FUNCTION__);

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
        plTransformComponent* ptParentTransform = pl_ecs_get_component(&ptLibrary->tTransformComponentManager, ptHierarchyComponent->tParent);
        plTransformComponent* ptChildTransform = pl_ecs_get_component(&ptLibrary->tTransformComponentManager, tChildEntity);
        ptChildTransform->tFinalTransform = pl_mul_mat4(&ptParentTransform->tFinalTransform, &ptChildTransform->tWorld);
    }

    pl_end_profile_sample();
}


static plEntity
pl_ecs_create_object(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created object '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed object");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plObjectComponent* ptObject = pl_ecs_create_component(&ptLibrary->tObjectComponentManager, tNewEntity);
    memset(ptObject, 0, sizeof(plObjectComponent));

    plTransformComponent* ptTransform = pl_ecs_create_component(&ptLibrary->tTransformComponentManager, tNewEntity);
    memset(ptTransform, 0, sizeof(plTransformComponent));
    ptTransform->tWorld = pl_identity_mat4();

    plMeshComponent* ptMesh = pl_ecs_create_component(&ptLibrary->tMeshComponentManager, tNewEntity);
    memset(ptMesh, 0, sizeof(plMeshComponent));

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tNewEntity;

    return tNewEntity;    
}

static plEntity
pl_ecs_create_transform(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created transform '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed transform");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plTransformComponent* ptTransform = pl_ecs_create_component(&ptLibrary->tTransformComponentManager, tNewEntity);
    memset(ptTransform, 0, sizeof(plTransformComponent));
    ptTransform->tWorld = pl_identity_mat4();

    return tNewEntity;  
}

static plEntity
pl_ecs_create_camera(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created camera '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed camera");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    const plCameraComponent tCamera = {
        .tPos         = tPos,
        .fNearZ       = fNearZ,
        .fFarZ        = fFarZ,
        .fFieldOfView = fYFov,
        .fAspectRatio = fAspect
    };

    plCameraComponent* ptCamera = pl_ecs_create_component(&ptLibrary->tCameraComponentManager, tNewEntity);
    memset(ptCamera, 0, sizeof(plCameraComponent));
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    return tNewEntity; 
}

static plEntity
pl_ecs_create_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, plVec3 tColor)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(uLogChannel, "created light '%s'", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to(uLogChannel, "created unnamed light");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plLightComponent* ptLight = pl_ecs_create_component(&ptLibrary->tLightComponentManager, tNewEntity);
    memset(ptLight, 0, sizeof(plLightComponent));
    
    ptLight->tColor = tColor;
    ptLight->tPosition = tPos;
    return tNewEntity;   
}

static void
pl_ecs_attach_component(plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_entity(&ptLibrary->tHierarchyComponentManager, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(&ptLibrary->tHierarchyComponentManager, tEntity);
    }
    else
    {
        ptHierarchyComponent = pl_ecs_create_component(&ptLibrary->tHierarchyComponentManager, tEntity);
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
        ptHierarchyComponent = pl_ecs_get_component(&ptLibrary->tHierarchyComponentManager, tEntity);
    }
    else
    {
        ptHierarchyComponent = pl_ecs_create_component(&ptLibrary->tHierarchyComponentManager, tEntity);
    }
    ptHierarchyComponent->tParent = PL_INVALID_ENTITY_HANDLE;
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
    }
}

PL_EXPORT void
pl_unload_ecs_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}