/*
   pl_renderer_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plGraphicsI      (v1.x)
        * plImageI         (v1.x)
        * plStatsI         (v1.x)
        * plGPUAllocatorsI (v1.x)
        * plJobI           (v2.x)
        * plDrawI          (v1.x)
        * plDrawBackendI   (v1.x)
        * plShaderI        (v1.x)
        * plFileI          (v1.x)
        * plLogI           (v1.x)
        * plRectPackI      (v1.x)
        * plConsoleI       (v1.x)
        * plScreenLogI     (v2.x)

        unstable APIs:
        * plCameraI
        * plResourceI
        * plEcsI
        * plBVHI
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_EXT_H
#define PL_RENDERER_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plRendererI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderVariant          plShaderVariant;
typedef struct _plComputeShaderVariant   plComputeShaderVariant;
typedef struct _plRendererSettings       plRendererSettings;
typedef struct _plSceneInit              plSceneInit;
typedef struct _plRendererRuntimeOptions plRendererRuntimeOptions;
typedef struct _plScene                  plScene;
typedef struct _plView                   plView;

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
typedef union _plMat4              plMat4;             // pl_math.h
typedef union _plVec4              plVec4;             // pl_math.h
typedef union _plVec2              plVec2;             // pl_math.h
typedef void* plTextureId;                             // pl_ui.h
typedef int plDrawFlags;                               // pl_draw_ext.h

// external (pl_ecs_ext.h)
typedef struct _plComponentLibrary plComponentLibrary;
typedef struct _plCameraComponent  plCameraComponent;
typedef struct _plLightComponent   plLightComponent;
typedef union  _plEntity           plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plRendererI
{
    // setup/shutdown
    void (*initialize)(plRendererSettings);
    void (*cleanup)   (void);

    // scenes
    plScene* (*create_scene)(plSceneInit);
    void     (*cleanup_scene)(plScene*);
    void     (*load_skybox_from_panorama)(plScene*, const char* pcPath, int iResolution);
    void     (*add_drawable_objects_to_scene)(plScene*, uint32_t uCount, const plEntity* atObjects);
    void     (*finalize_scene)(plScene*);

    // scenes - runtime
    void (*reload_scene_shaders)(plScene*);
    void (*remove_objects_from_scene)(plScene*, uint32_t objectCount, const plEntity* objects);
    void (*update_scene_materials)(plScene*, uint32_t materialCount, const plEntity* materials);
    void (*update_scene_objects)(plScene*, uint32_t objectCount, const plEntity* objects); // call if you change flags for objects

    // scenes - not ready
    void (*add_materials_to_scene)(plScene*, uint32_t materialCount, const plEntity* materials);

    // views
    plView*           (*create_view)(plScene*, plVec2 tDimensions);
    void              (*cleanup_view)(plView*);
    plBindGroupHandle (*get_view_color_texture)(plView*);
    void              (*resize_view)(plView*, plVec2 tDimensions);
    void              (*resize)(void);

    // per frame
    void (*prepare_scene)(plScene*);
    void (*prepare_view) (plView*, plCameraComponent*);
    void (*render_view) (plView*, plCameraComponent*, plCameraComponent* cullCamera);
    bool (*begin_frame)     (void);
    void (*begin_final_pass)(plRenderEncoder**, plCommandBuffer**);
    void (*end_final_pass)  (plRenderEncoder*, plCommandBuffer*);
    
    // per frame options
    void (*show_skybox)               (plView*);
    void (*debug_draw_lights)         (plView*, const plLightComponent*, uint32_t lightCount);
    void (*debug_draw_all_bound_boxes)(plView*);
    void (*debug_draw_bvh)            (plView*);

    // selection & highlighting
    void (*update_hovered_entity)(plView*, plVec2 tOffset, plVec2 tWindowScale);
    bool (*get_hovered_entity)   (plView*, plEntity*);
    void (*outline_entities)     (plScene*, uint32_t uCount, plEntity*);

    // misc
    plDrawList3D*       (*get_debug_drawlist)(plView*);
    plDrawList3D*       (*get_gizmo_drawlist)(plView*);
    plCommandPool*      (*get_command_pool)(void);
    plRenderPassHandle  (*get_main_render_pass)(void);
    plRendererRuntimeOptions* (*get_runtime_options)(void);
    void (*rebuild_scene_bvh)(plScene*);

} plRendererI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSceneInit
{
    plComponentLibrary* ptComponentLibrary;
} plSceneInit;

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
    bool     bWireframe;
    bool     bReloadSwapchain;
    bool     bReloadMSAA;
    bool     bVSync;
    bool     bShowOrigin;
    bool     bShowSelectedBoundingBox;
    bool     bMultiViewportShadows;
    bool     bImageBasedLighting;
    bool     bPunctualLighting;
    float    fShadowConstantDepthBias;
    float    fShadowSlopeDepthBias;
    uint32_t uOutlineWidth;
} plRendererRuntimeOptions;

#endif // PL_RENDERER_EXT_H