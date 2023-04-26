/*
   pl_proto_ext.h

   * temporary locations for testing new APIs
   * things here will change wildly!
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PROTO_EXT_H
#define PL_PROTO_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_PROTO "PL_API_PROTO"
typedef struct _plProtoApiI plProtoApiI;

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
#include "pl_ds.h" // hashmap
#include "pl_vulkan_ext.h"

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

// external apis
typedef struct _plApiRegistryApiI plApiRegistryApiI; // pilotlight.h
typedef struct _plImageApiI plImageApiI;             // pl_image_ext.h

// enums
typedef int      plShaderType;
typedef int      plComponentType;
typedef uint64_t plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plProtoApiI* pl_load_proto_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

// apis
typedef struct _plProtoApiI
{

    // graphics
    void (*create_main_render_target)(plGraphics* ptGraphics, plRenderTarget* ptTargetOut);
    void (*create_render_pass)       (plGraphics* ptGraphics, const plRenderPassDesc* ptDesc, plRenderPass* ptPassOut);
    void (*create_render_target)     (plResourceManager0ApiI* ptResourceApi, plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
    void (*begin_render_target)      (plGraphicsApiI* ptGfx, plGraphics* ptGraphics, plRenderTarget* ptTarget);
    void (*end_render_target)        (plGraphicsApiI* ptGfx, plGraphics* ptGraphics);
    void (*cleanup_render_target)    (plGraphics* ptGraphics, plRenderTarget* ptTarget);
    void (*cleanup_render_pass)      (plGraphics* ptGraphics, plRenderPass* ptPass);

    // new renderer
    void (*setup_renderer)  (plApiRegistryApiI* ptApiRegistry, plGraphics* ptGraphics, plRenderer* ptRenderer);
    void (*cleanup_renderer)(plRenderer* ptRenderer);
    void (*draw_sky)        (plScene* ptScene);

    // scene
    void (*create_scene)      (plRenderer* ptRenderer, plScene* ptSceneOut);
    void (*reset_scene)       (plScene* ptScene);
    void (*draw_scene)        (plScene* ptScene);
    void (*scene_bind_camera) (plScene* ptScene, const plCameraComponent* ptCamera);
    void (*scene_update_ecs)  (plScene* ptScene);
    void (*scene_bind_target) (plScene* ptScene, plRenderTarget* ptTarget);
    void (*scene_prepare)     (plScene* ptScene);

    // entity component system
    plEntity (*ecs_create_entity)    (plRenderer* ptRenderer);
    size_t   (*ecs_get_index)        (plComponentManager* ptManager, plEntity tEntity);
    void*    (*ecs_get_component)    (plComponentManager* ptManager, plEntity tEntity);
    void*    (*ecs_create_component) (plComponentManager* ptManager, plEntity tEntity);
    bool     (*ecs_has_entity)       (plComponentManager* ptManager, plEntity tEntity);
    void     (*ecs_update)           (plScene* ptScene, plComponentManager* ptManager);

    // components
    plEntity (*ecs_create_mesh)     (plScene* ptScene, const char* pcName);
    plEntity (*ecs_create_material) (plScene* ptScene, const char* pcName);
    plEntity (*ecs_create_object)   (plScene* ptScene, const char* pcName);
    plEntity (*ecs_create_transform)(plScene* ptScene, const char* pcName);
    plEntity (*ecs_create_camera)   (plScene* ptScene, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

    // hierarchy
    void (*ecs_attach_component)   (plScene* ptScene, plEntity tEntity, plEntity tParent);
    void (*ecs_deattach_component) (plScene* ptScene, plEntity tEntity);

    // material
    void (*material_outline)(plScene* ptScene, plEntity tEntity);

    // camera
    void (*camera_set_fov        )(plCameraComponent* ptCamera, float fYFov);
    void (*camera_set_clip_planes)(plCameraComponent* ptCamera, float fNearZ, float fFarZ);
    void (*camera_set_aspect     )(plCameraComponent* ptCamera, float fAspect);
    void (*camera_set_pos        )(plCameraComponent* ptCamera, float fX, float fY, float fZ);
    void (*camera_set_pitch_yaw  )(plCameraComponent* ptCamera, float fPitch, float fYaw);
    void (*camera_translate      )(plCameraComponent* ptCamera, float fDx, float fDy, float fDz);
    void (*camera_rotate         )(plCameraComponent* ptCamera, float fDPitch, float fDYaw);
    void (*camera_update         )(plCameraComponent* ptCamera);

} plProtoApiI;

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
    uint32_t*          sbuColorTextureViews;
    uint32_t           uDepthTextureView;
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

    uint64_t     uBindGroup2;
    uint32_t     uBufferOffset;
    plObjectInfo tInfo;
} plTransformComponent;

typedef struct _plSubMesh
{
    plMesh    tMesh;
    plEntity  tMaterial;
    uint32_t  uStorageOffset;

    plVec3*   sbtVertexPositions;
    plVec3*   sbtVertexNormals;
    plVec4*   sbtVertexTangents;
    plVec4*   sbtVertexColors0;
    plVec4*   sbtVertexColors1;
    plVec4*   sbtVertexWeights0;
    plVec4*   sbtVertexWeights1;
    plVec4*   sbtVertexJoints0;
    plVec4*   sbtVertexJoints1;
    plVec2*   sbtVertexTextureCoordinates0;
    plVec2*   sbtVertexTextureCoordinates1;
    uint32_t* sbuIndices;
} plSubMesh;

typedef struct _plMeshComponent
{
    plSubMesh* sbtSubmeshes;

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
    uint64_t        uBindGroup1;
    uint64_t        ulShaderTextureFlags;

    // internal
    uint32_t    uBufferOffset;
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
    float*                   sbfStorageBuffer;
    uint32_t                 uGlobalStorageBuffer;
    plBindGroup              tGlobalBindGroup;
    bool                     bFirstEcsUpdate;
    bool                     bMaterialsNeedUpdate;

    // skybox
    plBindGroup     tSkyboxBindGroup0;
    uint32_t        uSkyboxTextureView;
    
    plMeshComponent tSkyboxMesh;
    
    
    uint32_t             uDynamicBuffer0_Offset;
    uint32_t             uDynamicBuffer0;
    plComponentLibrary   tComponentLibrary;
    plComponentManager*  ptTagComponentManager;
    plComponentManager*  ptTransformComponentManager;
    plComponentManager*  ptMeshComponentManager;
    plComponentManager*  ptMaterialComponentManager;
    plComponentManager*  ptOutlineMaterialComponentManager;
    plComponentManager*  ptObjectComponentManager;
    plComponentManager*  ptCameraComponentManager;
    plComponentManager*  ptHierarchyComponentManager;
} plScene;

typedef struct _plRenderer
{
    plGraphics* ptGraphics;
    plGraphicsApiI* ptGfx;
    plMemoryApiI*   ptMemoryApi;
    plDataRegistryApiI* ptDataRegistry;
    plResourceManager0ApiI* ptResourceApi;
    plProtoApiI* ptProtoApi;
    plImageApiI* ptImageApi;
    plEntity*   sbtObjectEntities;
    size_t   tNextEntity;
    uint32_t uLogChannel;

    // material bind groups
    plBindGroup*         sbtMaterialBindGroups;
    plHashMap            tMaterialBindGroupdHashMap;

    // object bind groups
    plBindGroup*         sbtObjectBindGroups;
    plHashMap            tObjectBindGroupdHashMap;

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

#endif // PL_PROTO_EXT_H