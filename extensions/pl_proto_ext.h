/*
   pl_proto_ext.h

   * temporary locations for testing new APIs
   * things here will change wildly!
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
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
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h"
#include "pl_ds.h" // hashmap
#include "pl_vulkan_ext.h"

#include "pl_ecs_ext.h"

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

// external apis
typedef struct _plApiRegistryApiI plApiRegistryApiI; // pilotlight.h
typedef struct _plImageApiI plImageApiI;             // pl_image_ext.h
typedef struct _plDataRegistryApiI plDataRegistryApiI;

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
    void (*create_render_target)     (plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
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
    void (*ecs_update)(plScene* ptScene, plComponentManager* ptManager);
} plProtoApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRenderPassDesc
{
    plFormat tColorFormat;
    plFormat tDepthFormat;
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

typedef struct _plAssetRegistry
{
    plGraphics*          ptGraphics;
    plMaterialComponent* sbtMaterials;
    uint32_t             uDummyTexture;
} plAssetRegistry;

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
    plDeviceApiI* ptDeviceApi;
    plProtoApiI* ptProtoApi;
    plImageApiI* ptImageApi;
    plEcsI* ptEcs;
    plEntity*   sbtObjectEntities;
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

#endif // PL_PROTO_EXT_H