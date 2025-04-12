/*
   pl_ref_renderer_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_EXT_H
#define PL_RENDERER_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plRendererI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plViewOptions            plViewOptions;
typedef struct _plShaderVariant          plShaderVariant;
typedef struct _plComputeShaderVariant   plComputeShaderVariant;
typedef struct _plRendererSettings       plRendererSettings;
typedef struct _plRendererRuntimeOptions plRendererRuntimeOptions;
typedef struct _plSceneRuntimeOptions    plSceneRuntimeOptions;

// external 
typedef struct _plWindow           plWindow;           // pl_os.h
typedef struct _plGraphics         plGraphics;         // pl_graphics_ext.h
typedef struct _plDevice           plDevice;           // pl_graphics_ext.h
typedef struct _plDeviceInfo       plDeviceInfo;       // pl_graphics_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h
typedef struct _plCommandBuffer    plCommandBuffer;    // pl_graphics_ext.h
typedef struct _plCommandPool      plCommandPool;      // pl_graphics_ext.h
typedef struct _plSwapchain        plSwapchain;        // pl_graphics_ext.h
typedef union  plTextureHandle     plTextureHandle;    // pl_graphics_ext.h
typedef struct _plRenderEncoder    plRenderEncoder;    // pl_graphics_ext.h
typedef union  plRenderPassHandle  plRenderPassHandle; // pl_graphics_ext.h
typedef union  plBindGroupHandle   plBindGroupHandle;  // pl_graphics_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCameraComponent  plCameraComponent;  // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef union _plMat4              plMat4;             // pl_math.h
typedef union _plVec4              plVec4;             // pl_math.h
typedef union _plVec2              plVec2;             // pl_math.h
typedef void* plTextureId;                             // pl_ui.h
typedef int plDrawFlags;                               // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plRendererI
{
    // setup/shutdown
    void (*initialize)(plRendererSettings);
    void (*cleanup)   (void);

    // scenes
    uint32_t (*create_scene)(void);
    void (*cleanup_scene)(uint32_t);
    void (*load_skybox_from_panorama)(uint32_t uSceneHandle, const char* pcPath, int iResolution);
    void (*add_drawable_objects_to_scene)(uint32_t uSceneHandle, uint32_t uCount, const plEntity* atObjects);
    void (*finalize_scene)(uint32_t uSceneHandle);

    // scenes - runtime
    void (*reload_scene_shaders)(uint32_t uSceneHandle);
    void (*remove_objects_from_scene)(uint32_t sceneHandle, uint32_t objectCount, const plEntity* objects);
    void (*update_scene_materials)(uint32_t sceneHandle, uint32_t materialCount, const plEntity* materials);
    void (*update_scene_objects)(uint32_t sceneHandle, uint32_t objectCount, const plEntity* objects); // call if you change flags for objects

    // scenes - not ready
    void (*add_materials_to_scene)(uint32_t sceneHandle, uint32_t materialCount, const plEntity* materials);

    // views
    uint32_t          (*create_view)(uint32_t uSceneHandle, plVec2 tDimensions);
    void              (*cleanup_view)(uint32_t uSceneHandle, uint32_t uViewHandle);
    plBindGroupHandle (*get_view_color_texture)(uint32_t uSceneHandle, uint32_t uViewHandle);
    void              (*resize_view)(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions);
    void              (*resize)(void);

    // per frame
    void (*run_ecs)         (uint32_t uSceneHandle);
    void (*render_scene)    (uint32_t uSceneHandle, const uint32_t* auViewHandles, const plViewOptions* atOptions, uint32_t uViewCount);
    bool (*begin_frame)     (void);
    void (*begin_final_pass)(plRenderEncoder**, plCommandBuffer**);
    void (*end_final_pass)  (plRenderEncoder*, plCommandBuffer*);

    // selection & highlighting
    void (*update_hovered_entity)(uint32_t uSceneHandle, uint32_t uViewHandle);
    bool (*get_hovered_entity)   (uint32_t uSceneHandle, uint32_t uViewHandle, plEntity*);
    void (*outline_entities)     (uint32_t uSceneHandle, uint32_t uCount, plEntity*);

    // misc
    plComponentLibrary* (*get_component_library)(uint32_t uSceneHandle);
    plDevice*           (*get_device)(void);
    plSwapchain*        (*get_swapchain)(void);
    plDrawList3D*       (*get_debug_drawlist)(uint32_t uSceneHandle, uint32_t uViewHandle);
    plDrawList3D*       (*get_gizmo_drawlist)(uint32_t uSceneHandle, uint32_t uViewHandle);
    plCommandPool*      (*get_command_pool)(void);
    plRenderPassHandle  (*get_main_render_pass)(void);
    plRendererRuntimeOptions* (*get_runtime_options)(void);
    plSceneRuntimeOptions* (*get_scene_runtime_options)(uint32_t uSceneHandle);
    void (*rebuild_scene_bvh)(uint32_t uSceneHandle);

} plRendererI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plViewOptions
{
    plEntity* ptViewCamera;
    plEntity* ptCullCamera;
} plViewOptions;

typedef struct _plRendererSettings
{
    plDevice* ptDevice;
    plSwapchain* ptSwap;
    uint32_t  uMaxTextureResolution; // default 1024 (should be factor of 2)
} plRendererSettings;

typedef struct _plRendererRuntimeOptions
{
    bool     bMSAA;
    bool     bShowProbes;
    bool     bShowBVH;
    bool     bWireframe;
    bool     bReloadSwapchain;
    bool     bReloadMSAA;
    bool     bVSync;
    bool     bShowOrigin;
    bool     bFrustumCulling;
    bool     bDrawAllBoundingBoxes;
    bool     bDrawVisibleBoundingBoxes;
    bool     bShowSelectedBoundingBox;
    bool     bMultiViewportShadows;
    bool     bImageBasedLighting;
    bool     bPunctualLighting;
    float    fShadowConstantDepthBias;
    float    fShadowSlopeDepthBias;
    uint32_t uOutlineWidth;
} plRendererRuntimeOptions;

typedef struct _plSceneRuntimeOptions
{
    bool bShowSkybox;
    bool bContinuousBVH;
} plSceneRuntimeOptions;

#endif // PL_RENDERER_EXT_H