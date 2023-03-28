/*
   pl_prototype.h

   * temporary locations for testing new APIs
   * things here will change wildly!
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PROTOTYPE_H
#define PL_PROTOTYPE_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_INVALID_ENTITY_HANDLE
    #define PL_INVALID_ENTITY_HANDLE 0
#endif


//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>    // uint*_t
#include <stdbool.h>   // bool
#include "pl_math.h"
#include "pl_graphics_vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plObjectInfo    plObjectInfo;
typedef struct _plMaterialInfo  plMaterialInfo;
typedef struct _plGlobalInfo    plGlobalInfo;

// graphics
typedef struct _plRenderPass       plRenderPass;
typedef struct _plRenderPassDesc   plRenderPassDesc;
typedef struct _plRenderTarget     plRenderTarget;
typedef struct _plRenderTargetDesc plRenderTargetDesc;

// renderer
typedef struct _plRenderer plRenderer;
typedef struct _plScene       plScene;

// entity component system
typedef struct _plComponentLibrary plComponentLibrary;
typedef struct _plComponentManager plComponentManager;

// ecs components
typedef struct _plTagComponent       plTagComponent;
typedef struct _plMeshComponent      plMeshComponent;
typedef struct _plSubMesh            plSubMesh;
typedef struct _plTransformComponent plTransformComponent;
typedef struct _plMaterialComponent  plMaterialComponent;
typedef struct _plObjectComponent    plObjectComponent;
typedef struct _plCameraComponent    plCameraComponent;
typedef struct _plHierarchyComponent plHierarchyComponent;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

typedef int      plShaderType;
typedef int      plComponentType;
typedef uint64_t plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// graphics
void pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut);
void pl_create_render_pass  (plGraphics* ptGraphics, const plRenderPassDesc* ptDesc, plRenderPass* ptPassOut);
void pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
void pl_begin_render_target (plGraphics* ptGraphics, plRenderTarget* ptTarget);
void pl_end_render_target   (plGraphics* ptGraphics);
void pl_cleanup_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget);
void pl_cleanup_render_pass(plGraphics* ptGraphics, plRenderPass* ptPass);

// new renderer
void pl_setup_renderer  (plGraphics* ptGraphics, plRenderer* ptRenderer);
void pl_cleanup_renderer(plRenderer* ptRenderer);

void pl_draw_sky        (plScene* ptScene);

// scene
void pl_create_scene      (plRenderer* ptRenderer, plScene* ptSceneOut);
void pl_reset_scene       (plScene* ptScene);
void pl_draw_scene        (plScene* ptScene);
void pl_scene_bind_camera (plScene* ptScene, const plCameraComponent* ptCamera);
void pl_scene_update_ecs  (plScene* ptScene);
void pl_scene_bind_target (plScene* ptScene, plRenderTarget* ptTarget);

// entity component system
plEntity pl_ecs_create_entity   (plRenderer* ptRenderer);
size_t   pl_ecs_get_index       (plComponentManager* ptManager, plEntity tEntity);
void*    pl_ecs_get_component   (plComponentManager* ptManager, plEntity tEntity);
void*    pl_ecs_create_component(plComponentManager* ptManager, plEntity tEntity);
bool     pl_ecs_has_entity      (plComponentManager* ptManager, plEntity tEntity);
void     pl_ecs_update          (plScene* ptScene, plComponentManager* ptManager);

// components
plEntity pl_ecs_create_mesh     (plScene* ptScene, const char* pcName);
plEntity pl_ecs_create_material (plScene* ptScene, const char* pcName);
plEntity pl_ecs_create_object   (plScene* ptScene, const char* pcName);
plEntity pl_ecs_create_transform(plScene* ptScene, const char* pcName);
plEntity pl_ecs_create_camera   (plScene* ptScene, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

// hierarchy
void     pl_ecs_attach_component  (plScene* ptScene, plEntity tEntity, plEntity tParent);
void     pl_ecs_deattach_component(plScene* ptScene, plEntity tEntity);

// material
void     pl_material_outline(plScene* ptScene, plEntity tEntity);

// camera
void     pl_camera_set_fov        (plCameraComponent* ptCamera, float fYFov);
void     pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ);
void     pl_camera_set_aspect     (plCameraComponent* ptCamera, float fAspect);
void     pl_camera_set_pos        (plCameraComponent* ptCamera, float fX, float fY, float fZ);
void     pl_camera_set_pitch_yaw  (plCameraComponent* ptCamera, float fPitch, float fYaw);
void     pl_camera_translate      (plCameraComponent* ptCamera, float fDx, float fDy, float fDz);
void     pl_camera_rotate         (plCameraComponent* ptCamera, float fDPitch, float fDYaw);
void     pl_camera_update         (plCameraComponent* ptCamera);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRenderPassDesc
{
    VkFormat tColorFormat;
    VkFormat tDepthFormat;
} plRenderPassDesc;

typedef struct _plRenderPass
{
    plRenderPassDesc tDesc;
    VkRenderPass     _tRenderPass;
} plRenderPass;

typedef struct _plRenderTargetDesc
{
    plRenderPass tRenderPass;
    plVec2       tSize;
} plRenderTargetDesc;

typedef struct _plRenderTarget
{
    plRenderTargetDesc tDesc;
    uint32_t*          sbuColorTextures;
    uint32_t           uDepthTexture;
    VkFramebuffer*     sbtFrameBuffers;
    bool               bMSAA;
} plRenderTarget;

typedef struct _plGlobalInfo
{
    plVec4 tAmbientColor;
    plVec4 tCameraPos;
    plMat4 tCameraView;
    plMat4 tCameraViewProj;
    float  fTime;
    int    _unused[3];
} plGlobalInfo;

typedef struct _plMaterialInfo
{
    plVec4 tAlbedo;
} plMaterialInfo;

typedef struct _plObjectInfo
{
    plMat4   tModel;
    uint32_t uVertexOffset;
    int      _unused[3];
} plObjectInfo;

typedef struct _plObjectComponent
{
    plEntity tMesh;
    plEntity tTransform;
} plObjectComponent;

typedef struct _plHierarchyComponent
{
    plEntity tParent;
} plHierarchyComponent;

typedef struct _plAssetRegistry
{
    plGraphics*          ptGraphics;
    plMaterialComponent* sbtMaterials;
    uint32_t             uDummyTexture;
} plAssetRegistry;

typedef struct _plTagComponent
{
    char acName[PL_MAX_NAME_LENGTH];
} plTagComponent;

typedef struct _plTransformComponent
{
    plVec3 tScale;
    plVec4 tRotation;
    plVec3 tTranslation;
    plMat4 tFinalTransform;
    plMat4 tWorld;

    plBindGroup tBindGroup2;
    uint32_t    uBufferOffset;
    plObjectInfo tInfo;
    bool         bDirty;
} plTransformComponent;

typedef struct _plSubMesh
{
    plMesh   tMesh;
    plEntity tMaterial;
    uint32_t uStorageOffset;
} plSubMesh;

typedef struct _plMeshComponent
{
    plSubMesh* sbtSubmeshes;
    plVec3*    sbtVertexPositions;
    plVec3*    sbtVertexNormals;
    plVec4*    sbtVertexTangents;
    plVec4*    sbtVertexColors0;
    plVec4*    sbtVertexColors1;
    plVec4*    sbtVertexWeights0;
    plVec4*    sbtVertexWeights1;
    plVec2*    sbtVertexTextureCoordinates0;
    plVec2*    sbtVertexTextureCoordinates1;
    uint32_t*  sbuIndices;
} plMeshComponent;

typedef struct _plMaterialComponent
{
    plShaderType tShaderType;
    
    // properties
    plVec4 tAlbedo;
    float  fAlphaCutoff;
    bool   bDoubleSided;
    bool   bOutline;

    // maps
    uint32_t uAlbedoMap;
    uint32_t uNormalMap;
    uint32_t uEmissiveMap;

    // misc
    uint32_t        uShader;
    uint32_t        uShaderVariant;
    plGraphicsState tGraphicsState;
    plBindGroup     tMaterialBindGroup;
    uint64_t        ulShaderTextureFlags;

    // internal
    uint32_t    uBufferOffset;
    bool        bDirty;
} plMaterialComponent;

typedef struct _plCameraComponent
{
    plVec3       tPos;
    float        fNearZ;
    float        fFarZ;
    float        fFieldOfView;
    float        fAspectRatio;  // width/height
    plMat4       tViewMat;      // cached
    plMat4       tProjMat;      // cached
    plMat4       tTransformMat; // cached

    // rotations
    float        fPitch; // rotation about right vector
    float        fYaw;   // rotation about up vector
    float        fRoll;  // rotation about forward vector

    // direction vectors
    plVec3       _tUpVec;
    plVec3       _tForwardVec;
    plVec3       _tRightVec;
} plCameraComponent;

typedef struct _plComponentManager
{
    plComponentType tComponentType;
    plEntity*       sbtEntities;
    void*           pData;
    size_t          szStride;
} plComponentManager;

typedef struct _plComponentLibrary
{
    plComponentManager tTagComponentManager;
    plComponentManager tTransformComponentManager;
    plComponentManager tMeshComponentManager;
    plComponentManager tMaterialComponentManager;
    plComponentManager tOutlineMaterialComponentManager;
    plComponentManager tObjectComponentManager;
    plComponentManager tCameraComponentManager;
    plComponentManager tHierarchyComponentManager;
} plComponentLibrary;

typedef struct _plScene
{
    plRenderer*              ptRenderer;
    plRenderTarget*          ptRenderTarget;
    const plCameraComponent* ptCamera;

    // skybox
    plBindGroup     tSkyboxBindGroup0;
    uint32_t        uSkyboxTexture;
    
    plMeshComponent tSkyboxMesh;
    
    uint32_t            uDynamicBufferSize;
    uint32_t            uDynamicBuffer0_Offset;
    uint32_t            uDynamicBuffer1_Offset;
    uint32_t            uDynamicBuffer2_Offset;
    uint32_t            uDynamicBuffer0;
    uint32_t            uDynamicBuffer1;
    uint32_t            uDynamicBuffer2;
    plComponentLibrary  tComponentLibrary;
    plComponentManager* ptTagComponentManager;
    plComponentManager* ptTransformComponentManager;
    plComponentManager* ptMeshComponentManager;
    plComponentManager* ptMaterialComponentManager;
    plComponentManager* ptOutlineMaterialComponentManager;
    plComponentManager* ptObjectComponentManager;
    plComponentManager* ptCameraComponentManager;
    plComponentManager* ptHierarchyComponentManager;
} plScene;

typedef struct _plRenderer
{
    plGraphics*              ptGraphics;
    plEntity*                sbtObjectEntities;
    float*                   sbfStorageBuffer;
    plBindGroup              tGlobalBindGroup;
    uint32_t                 uGlobalStorageBuffer;
    size_t                   tNextEntity;
    uint32_t                 uLogChannel;

    // draw stream
    plDraw*     sbtDraws;
    plDraw*     sbtOutlineDraws;
    plDrawArea* sbtDrawAreas;

    // shaders
    uint32_t uMainShader;
    uint32_t uOutlineShader;
    uint32_t uSkyboxShader;

} plRenderer;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plComponentType
{
    PL_COMPONENT_TYPE_NONE,
    PL_COMPONENT_TYPE_TAG,
    PL_COMPONENT_TYPE_TRANSFORM,
    PL_COMPONENT_TYPE_MESH,
    PL_COMPONENT_TYPE_MATERIAL,
    PL_COMPONENT_TYPE_CAMERA,
    PL_COMPONENT_TYPE_OBJECT,
    PL_COMPONENT_TYPE_HIERARCHY
};

enum _plShaderType
{
    PL_SHADER_TYPE_PBR,
    PL_SHADER_TYPE_UNLIT,
    PL_SHADER_TYPE_CUSTOM,
    
    PL_SHADER_TYPE_COUNT
};

#endif // PL_PROTOTYPE_H