/*
   pl_renderer_ext.h

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

#ifndef PL_RENDERER_EXT_H
#define PL_RENDERER_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_RENDERER "PL_API_RENDERER"
typedef struct _plRendererI plRendererI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h"
#include "pl_ds.h" // hashmap
#include "pl_vulkan_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plMaterialInfo  plMaterialInfo;
typedef struct _plGlobalInfo    plGlobalInfo;

// graphics
typedef struct _plRenderTarget     plRenderTarget;
typedef struct _plRenderTargetDesc plRenderTargetDesc;

// renderer
typedef struct _plRenderer plRenderer;
typedef struct _plScene       plScene;

// external apis
typedef struct _plApiRegistryApiI  plApiRegistryApiI;  // pilotlight.h
typedef struct _plDataRegistryApiI plDataRegistryApiI; // pilotlight.h
typedef struct _plImageApiI        plImageApiI;        // pl_image_ext.h
typedef struct _plEcsI             plEcsI;             // pl_ecs_ext.h

// external
typedef struct _plComponentLibrary plComponentLibrary;
typedef struct _plCameraComponent plCameraComponent;
typedef uint64_t plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plRendererI* pl_load_renderer_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

// apis
typedef struct _plRendererI
{

    // graphics
    void (*create_main_render_target)(plGraphics* ptGraphics, plRenderTarget* ptTargetOut);
    void (*create_render_target)     (plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
    void (*begin_render_target)      (plGraphicsApiI* ptGfx, plGraphics* ptGraphics, plRenderTarget* ptTarget);
    void (*end_render_target)        (plGraphicsApiI* ptGfx, plGraphics* ptGraphics);
    void (*cleanup_render_target)    (plGraphics* ptGraphics, plRenderTarget* ptTarget);

    // new renderer
    void (*setup_renderer)  (plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptComponentLibrary, plGraphics* ptGraphics, plRenderer* ptRenderer);
    void (*resize)          (plRenderer* ptRenderer, float fWidth, float fHeight);
    void (*cleanup_renderer)(plRenderer* ptRenderer);
    void (*draw_sky)        (plScene* ptScene);

    // scene
    void (*create_scene)          (plRenderer* ptRenderer, plComponentLibrary* ptComponentLibrary, plScene* ptSceneOut);
    void (*reset_scene)           (plScene* ptScene);
    void (*draw_scene)            (plScene* ptScene);
    void (*draw_pick_scene)       (plScene* ptScene);
    void (*scene_bind_camera)     (plScene* ptScene, const plCameraComponent* ptCamera);
    void (*scene_bind_target)     (plScene* ptScene, plRenderTarget* ptTarget);
    void (*scene_prepare)         (plScene* ptScene);   // creates global vertex/index buffer + other setup when dirty
    void (*prepare_scene_gpu_data)(plScene* ptScene);  // updates dynamic buffers for materials + objects
} plRendererI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plPickInfo
{
    plVec4 tColor;
} plPickInfo;

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

typedef struct _plRenderTargetDesc
{
    uint32_t     uRenderPass;
    plVec2       tSize;
} plRenderTargetDesc;

typedef struct _plRenderTarget
{
    plRenderTargetDesc tDesc;
    uint32_t*          sbuColorTextureViews;
    uint32_t           uDepthTextureView;
    uint32_t*          sbuFrameBuffers;
    bool               bMSAA;
} plRenderTarget;

typedef struct _plScene
{
    plRenderer*              ptRenderer;
    plRenderTarget*          ptRenderTarget;
    const plCameraComponent* ptCamera;
    bool                     bMaterialsNeedUpdate;
    bool                     bMeshesNeedUpdate;
    
    // global data
    plBindGroup     tGlobalBindGroup;
    uint32_t        uGlobalVertexData;
    float*          sbfGlobalVertexData;
    uint32_t        uGlobalMaterialData;
    plMaterialInfo* sbtGlobalMaterialData;

    plBindGroup     tGlobalPickBindGroup;
    uint32_t        uGlobalPickData;
    plPickInfo*     sbtGlobalPickData;

    // global vertex/index buffers
    plVec3*   sbtVertexData;
    uint32_t* sbuIndexData;
    uint32_t  uIndexBuffer;
    uint32_t  uVertexBuffer;
    
    // skybox
    plBindGroup tSkyboxBindGroup0;
    uint32_t    uSkyboxTextureView;
    plMesh      tSkyboxMesh;
    
    uint32_t            uDynamicBuffer0_Offset;
    uint32_t            uDynamicBuffer0;
    plComponentLibrary* ptComponentLibrary;
} plScene;

typedef struct _plRenderer
{
    plGraphics* ptGraphics;

    plEntity*   sbtVisibleMeshes;
    plEntity*   sbtVisibleOutlinedMeshes;
    uint32_t    uLogChannel;

    // apis
    plGraphicsApiI*     ptGfx;
    plMemoryApiI*       ptMemoryApi;
    plDataRegistryApiI* ptDataRegistry;
    plDeviceApiI*       ptDeviceApi;
    plRendererI*        ptRendererApi;
    plImageApiI*        ptImageApi;
    plEcsI*             ptEcs;

    // material bind groups
    plBindGroup* sbtMaterialBindGroups;
    plHashMap    tMaterialBindGroupdHashMap;

    // object bind groups
    plBindGroup* sbtObjectBindGroups;
    plHashMap    tObjectBindGroupdHashMap;

    // draw stream
    plDraw*     sbtDraws;
    plDrawArea* sbtDrawAreas;

    // shaders
    uint32_t uMainShader;
    uint32_t uOutlineShader;
    uint32_t uSkyboxShader;
    uint32_t uPickShader;

    // picking
    uint32_t         uPickPass;
    plRenderTarget   tPickTarget;
    VkDescriptorSet* sbtTextures;
    uint32_t         uPickMaterial;

} plRenderer;

#endif // PL_RENDERER_EXT_H