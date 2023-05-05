/*
   pl_ecs_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_ecs_ext.h"
#include "pl_ds.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_vulkan_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plEntity pl_ecs_create_entity   (plComponentLibrary* ptLibrary);
static size_t   pl_ecs_get_index       (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_get_component   (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_create_component(plComponentManager* ptManager, plEntity tEntity);
static bool     pl_ecs_has_entity      (plComponentManager* ptManager, plEntity tEntity);

// components
static plEntity pl_ecs_create_mesh     (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_material (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_object   (plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_transform(plComponentLibrary* ptLibrary, const char* pcName);
static plEntity pl_ecs_create_camera   (plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

static void pl_ecs_attach_component (plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent);
static void pl_ecs_deattach_component(plComponentLibrary* ptLibrary, plEntity tEntity);

// material
static void pl_material_outline(plComponentLibrary* ptLibrary, plEntity tEntity);

// camera
static void     pl_camera_set_fov        (plCameraComponent* ptCamera, float fYFov);
static void     pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ);
static void     pl_camera_set_aspect     (plCameraComponent* ptCamera, float fAspect);
static void     pl_camera_set_pos        (plCameraComponent* ptCamera, float fX, float fY, float fZ);
static void     pl_camera_set_pitch_yaw  (plCameraComponent* ptCamera, float fPitch, float fYaw);
static void     pl_camera_translate      (plCameraComponent* ptCamera, float fDx, float fDy, float fDz);
static void     pl_camera_rotate         (plCameraComponent* ptCamera, float fDPitch, float fDYaw);
static void     pl_camera_update         (plCameraComponent* ptCamera);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plEcsI*
pl_load_ecs_api(void)
{
    static plEcsI tApi = {
        .create_entity      = pl_ecs_create_entity,
        .get_index          = pl_ecs_get_index,
        .get_component      = pl_ecs_get_component,
        .create_component   = pl_ecs_create_component,
        .has_entity         = pl_ecs_has_entity,
        .create_mesh        = pl_ecs_create_mesh,
        .create_material    = pl_ecs_create_material,
        .create_object      = pl_ecs_create_object,
        .create_transform   = pl_ecs_create_transform,
        .create_camera      = pl_ecs_create_camera,
        .material_outline   = pl_material_outline,
        .attach_component   = pl_ecs_attach_component,
        .deattach_component = pl_ecs_deattach_component
    };
    return &tApi;
}

plCameraI*
pl_load_camera_api(void)
{
    static plCameraI tApi = {
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

static plEntity
pl_ecs_create_entity(plComponentLibrary* ptLibrary)
{
    plEntity tNewEntity = ptLibrary->tNextEntity++;
    return tNewEntity;
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
    unsigned char* pucData = ptManager->pData;
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
        plTagComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plTagComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MESH:
    {
        plMeshComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plMeshComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_TRANSFORM:
    {
        plTransformComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, ((plTransformComponent){.tWorld = pl_identity_mat4(), .tFinalTransform = pl_identity_mat4()}));
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MATERIAL:
    {
        plMaterialComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plMaterialComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_OBJECT:
    {
        plObjectComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plObjectComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_CAMERA:
    {
        plCameraComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plCameraComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_HIERARCHY:
    {
        plHierarchyComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plHierarchyComponent){0});
        ptManager->pData = sbComponents;
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


static plEntity
pl_ecs_create_mesh(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plMeshComponent* ptMesh = pl_ecs_create_component(&ptLibrary->tMeshComponentManager, tNewEntity);
    return tNewEntity;
}

static plMaterialComponent*
pl_ecs_create_outline_material(plComponentLibrary* ptLibrary, plEntity tEntity)
{

    plMaterialComponent* ptMaterial = pl_ecs_create_component(&ptLibrary->tOutlineMaterialComponentManager, tEntity);
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

    return ptMaterial;
}

static plEntity
pl_ecs_create_material(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
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
pl_material_outline(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plMaterialComponent* ptMaterial = pl_ecs_get_component(&ptLibrary->tMaterialComponentManager, tEntity);
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_REPLACE;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_REPLACE;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_REPLACE;
    ptMaterial->bOutline                            = true;

    plMaterialComponent* ptOutlineMaterial = pl_ecs_create_outline_material(ptLibrary, tEntity);
    ptOutlineMaterial->tGraphicsState.ulVertexStreamMask   = ptMaterial->tGraphicsState.ulVertexStreamMask;
}

static plEntity
pl_ecs_create_object(plComponentLibrary* ptLibrary, const char* pcName)
{
    plEntity tNewEntity = pl_ecs_create_entity(ptLibrary);

    plTagComponent* ptTag = pl_ecs_create_component(&ptLibrary->tTagComponentManager, tNewEntity);
    if(pcName)
    {
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plObjectComponent* ptObject = pl_ecs_create_component(&ptLibrary->tObjectComponentManager, tNewEntity);
    memset(ptObject, 0, sizeof(plObjectComponent));

    plTransformComponent* ptTransform = pl_ecs_create_component(&ptLibrary->tTransformComponentManager, tNewEntity);
    memset(ptTransform, 0, sizeof(plTransformComponent));
    ptTransform->tInfo.tModel = pl_identity_mat4();
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
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plTransformComponent* ptTransform = pl_ecs_create_component(&ptLibrary->tTransformComponentManager, tNewEntity);
    memset(ptTransform, 0, sizeof(plTransformComponent));
    ptTransform->tInfo.tModel = pl_identity_mat4();
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
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
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
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ecs_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_ECS), pl_load_ecs_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_CAMERA), pl_load_camera_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_ECS, pl_load_ecs_api());
        ptApiRegistry->add(PL_API_CAMERA, pl_load_camera_api());
    }
}

PL_EXPORT void
pl_unload_ecs_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}