/*
   pl_renderer_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] includes
// [SECTION] public api
// [SECTION] public api struct
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
        * plDrawI          (v2.x)
        * plShaderI        (v1.x)
        * plVfsI           (v2.x)
        * plLogI           (v1.x)
        * plRectPackI      (v1.x)
        * plConsoleI       (v1.x)
        * plScreenLogI     (v2.x)

        unstable APIs:
        * plCameraI
        * plResourceI
        * plEcsI
        * plBVHI
        * plAnimationI
        * plMeshI
        * plMaterialI
        * plTerrainProcessorI
        * plStageI
        * plFreeListI
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_EXT_H
#define PL_RENDERER_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plRendererI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_MAX_SHADOW_CASCADES 4

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include "pl_ecs_ext.inl"      // plEntity
#include "pl_resource_ext.inl" // plResourceHandle
#include "pl_math.h"           // plVec3, plMat4

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plRendererSettings       plRendererSettings;
typedef struct _plSceneInit              plSceneInit;
typedef struct _plRendererRuntimeOptions plRendererRuntimeOptions;
typedef struct _plScene                  plScene; // opaque type
typedef struct _plView                   plView;  // opaque type

// ecs components
typedef struct _plObjectComponent           plObjectComponent;
typedef struct _plSkinComponent             plSkinComponent;
typedef struct _plLightComponent            plLightComponent;
typedef struct _plEnvironmentProbeComponent plEnvironmentProbeComponent;

// enums & flags
typedef int plLightFlags;
typedef int plEnvironmentProbeFlags;
typedef int plObjectFlags;
typedef int plTonemapMode;

// external 
typedef struct _plWindow           plWindow;           // pl_platform_ext.h
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
typedef struct _plCamera           plCamera;           // pl_camera_ext.h
typedef void* plTextureId;                             // pl_ui_ext.h

// external enums & flags
typedef int plShaderDebugMode; // pl_shader_interop_renderer.h
typedef int plLightType;       // pl_shader_interop_renderer.h
typedef int plDrawFlags;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_renderer_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_renderer_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void              pl_renderer_initialize(plRendererSettings);
PL_API void              pl_renderer_cleanup   (void);

// scenes
PL_API plScene*          pl_renderer_create_scene (plSceneInit);
PL_API void              pl_renderer_cleanup_scene(plScene*);

// scene composition
PL_API void              pl_renderer_load_skybox_from_panorama    (plScene*, const char* path, int resolution);
PL_API bool              pl_renderer_add_drawable_objects_to_scene(plScene*, uint32_t count, const plEntity* objects);
PL_API void              pl_renderer_add_probe_to_scene           (plScene*, plEntity);
PL_API void              pl_renderer_add_light_to_scene           (plScene*, plEntity);
PL_API void              pl_renderer_add_materials_to_scene       (plScene*, uint32_t count, const plEntity* materials);

// scene - runtime
PL_API void              pl_renderer_reload_scene_shaders(plScene*);
PL_API void              pl_renderer_update_scene_material(plScene*, plEntity material);

// views
PL_API plView*           pl_renderer_create_view     (plScene*, plVec2 dims);
PL_API void              pl_renderer_cleanup_view    (plView*);
PL_API plBindGroupHandle pl_renderer_get_view_texture(plView*, plVec2* maxUVOut);
PL_API void              pl_renderer_resize_view     (plView*, plVec2 dims);

// per frame
PL_API bool              pl_renderer_begin_frame  (void);
PL_API void              pl_renderer_prepare_scene(plScene*);
PL_API void              pl_renderer_prepare_view (plView*, plCamera*);
PL_API void              pl_renderer_render_view  (plView*, plCamera* mainCamera, plCamera* cullCamera);

// per frame options
PL_API void              pl_renderer_show_skybox               (plView*);
PL_API void              pl_renderer_show_grid                 (plView*);
PL_API void              pl_renderer_debug_draw_lights         (plView*, const plLightComponent*, uint32_t lightCount);
PL_API void              pl_renderer_debug_draw_all_bound_boxes(plView*);
PL_API void              pl_renderer_debug_draw_bvh            (plView*);

// selection & highlighting
PL_API void             pl_renderer_update_hovered_entity(plView*, plVec2 offset, plVec2 windowScale);
PL_API bool             pl_renderer_get_hovered_entity   (plView*, plEntity*);
PL_API void             pl_renderer_outline_entities     (plScene*, uint32_t count, plEntity*);

// misc
PL_API plDrawList3D*             pl_renderer_get_debug_drawlist (plView*);
PL_API plDrawList3D*             pl_renderer_get_gizmo_drawlist (plView*);
PL_API plRendererRuntimeOptions* pl_renderer_get_runtime_options(void);
PL_API void                      pl_renderer_rebuild_scene_bvh  (plScene*);

//----------------------------ECS INTEGRATION----------------------------------

// system setup/shutdown/etc
PL_API void          pl_renderer_register_ecs_system(void);

// entity helpers (creates entity and necessary components)
//   - do NOT store out parameter; use it immediately
PL_API plEntity      pl_renderer_create_object           (plComponentLibrary*, const char* name, plObjectComponent**);
PL_API plEntity      pl_renderer_create_skin             (plComponentLibrary*, const char* name, plSkinComponent**);
PL_API plEntity      pl_renderer_create_directional_light(plComponentLibrary*, const char* name, plVec3 dir, plLightComponent**);
PL_API plEntity      pl_renderer_create_point_light      (plComponentLibrary*, const char* name, plVec3 pos, plLightComponent**);
PL_API plEntity      pl_renderer_create_spot_light       (plComponentLibrary*, const char* name, plVec3 pos, plVec3 dir, plLightComponent**);
PL_API plEntity      pl_renderer_create_environment_probe(plComponentLibrary*, const char* name, plVec3 pos, plEnvironmentProbeComponent**);

// object helpers
PL_API plEntity     pl_renderer_copy_object(plComponentLibrary*, const char* name, plEntity originalObject, plObjectComponent**);

// systems
PL_API void         pl_renderer_run_object_update_system           (plComponentLibrary*);
PL_API void         pl_renderer_run_skin_update_system             (plComponentLibrary*);
PL_API void         pl_renderer_run_light_update_system            (plComponentLibrary*);
PL_API void         pl_renderer_run_environment_probe_update_system(plComponentLibrary*);

// ecs types
PL_API plEcsTypeKey pl_renderer_get_ecs_type_key_object           (void);
PL_API plEcsTypeKey pl_renderer_get_ecs_type_key_skin             (void);
PL_API plEcsTypeKey pl_renderer_get_ecs_type_key_light            (void);
PL_API plEcsTypeKey pl_renderer_get_ecs_type_key_environment_probe(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plRendererI
{
    void                      (*initialize)                         (plRendererSettings);
    void                      (*cleanup)                            (void);
    plScene*                  (*create_scene)                       (plSceneInit);
    void                      (*cleanup_scene)                      (plScene*);
    void                      (*load_skybox_from_panorama)          (plScene*, const char* path, int resolution);
    bool                      (*add_drawable_objects_to_scene)      (plScene*, uint32_t count, const plEntity* objects);
    void                      (*add_probe_to_scene)                 (plScene*, plEntity);
    void                      (*add_light_to_scene)                 (plScene*, plEntity);
    void                      (*add_materials_to_scene)             (plScene*, uint32_t count, const plEntity* materials);
    void                      (*reload_scene_shaders)               (plScene*);
    void                      (*update_scene_material)              (plScene*, plEntity material);
    plView*                   (*create_view)                        (plScene*, plVec2 dims);
    void                      (*cleanup_view)                       (plView*);
    plBindGroupHandle         (*get_view_texture)                   (plView*, plVec2* maxUVOut);
    void                      (*resize_view)                        (plView*, plVec2 dims);
    bool                      (*begin_frame)                        (void);
    void                      (*prepare_scene)                      (plScene*);
    void                      (*prepare_view)                       (plView*, plCamera*);
    void                      (*render_view)                        (plView*, plCamera* mainCamera, plCamera* cullCamera);
    void                      (*show_skybox)                        (plView*);
    void                      (*show_grid)                          (plView*);
    void                      (*debug_draw_lights)                  (plView*, const plLightComponent*, uint32_t lightCount);
    void                      (*debug_draw_all_bound_boxes)         (plView*);
    void                      (*debug_draw_bvh)                     (plView*);
    void                      (*update_hovered_entity)              (plView*, plVec2 offset, plVec2 windowScale);
    bool                      (*get_hovered_entity)                 (plView*, plEntity*);
    void                      (*outline_entities)                   (plScene*, uint32_t count, plEntity*);
    plDrawList3D*             (*get_debug_drawlist)                 (plView*);
    plDrawList3D*             (*get_gizmo_drawlist)                 (plView*);
    plRendererRuntimeOptions* (*get_runtime_options)                (void);
    void                      (*rebuild_scene_bvh)                  (plScene*);
    void                      (*register_ecs_system)                (void);
    plEntity                  (*create_object)                      (plComponentLibrary*, const char* name, plObjectComponent**);
    plEntity                  (*create_skin)                        (plComponentLibrary*, const char* name, plSkinComponent**);
    plEntity                  (*create_directional_light)           (plComponentLibrary*, const char* name, plVec3 dir, plLightComponent**);
    plEntity                  (*create_point_light)                 (plComponentLibrary*, const char* name, plVec3 pos, plLightComponent**);
    plEntity                  (*create_spot_light)                  (plComponentLibrary*, const char* name, plVec3 pos, plVec3 dir, plLightComponent**);
    plEntity                  (*create_environment_probe)           (plComponentLibrary*, const char* name, plVec3 pos, plEnvironmentProbeComponent**);
    plEntity                  (*copy_object)                        (plComponentLibrary*, const char* name, plEntity originalObject, plObjectComponent**);
    void                      (*run_object_update_system)           (plComponentLibrary*);
    void                      (*run_skin_update_system)             (plComponentLibrary*);
    void                      (*run_light_update_system)            (plComponentLibrary*);
    void                      (*run_environment_probe_update_system)(plComponentLibrary*);
    plEcsTypeKey              (*get_ecs_type_key_object)           (void);
    plEcsTypeKey              (*get_ecs_type_key_skin)             (void);
    plEcsTypeKey              (*get_ecs_type_key_light)            (void);
    plEcsTypeKey              (*get_ecs_type_key_environment_probe)(void);
} plRendererI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSceneInit
{
    plComponentLibrary* ptComponentLibrary;
    size_t              szIndexBufferSize;    // default: 64000000
    size_t              szVertexBufferSize;   // default: 64000000
    size_t              szDataBufferSize;     // default: 64000000
    size_t              szMaterialBufferSize; // default:  8000000
    size_t              szSkinBufferSize;     // default:  8000000
} plSceneInit;

typedef struct _plRendererSettings
{
    plDevice*    ptDevice;
    plSwapchain* ptSwap;
    uint32_t     uMaxTextureResolution; // default 1024 (should be factor of 2)
} plRendererSettings;

typedef struct _plRendererRuntimeOptions
{
    bool              bShowProbes;
    bool              bWireframe;
    bool              bShowOrigin;
    bool              bShowSelectedBoundingBox;
    bool              bMultiViewportShadows;
    bool              bImageBasedLighting;
    bool              bNormalMapping;
    bool              bPunctualLighting;
    bool              bPcfShadows;
    float             fMaxShadowRange;
    float             fShadowConstantDepthBias;
    float             fShadowSlopeDepthBias;
    float             fTerrainShadowConstantDepthBias;
    float             fTerrainShadowSlopeDepthBias;
    uint32_t          uOutlineWidth;
    plShaderDebugMode tShaderDebugMode;

    // tonemapping
    plTonemapMode tTonemapMode;
    float         fExposure;
    float         fBrightness;
    float         fContrast;
    float         fSaturation;

    // bloom
    bool     bBloomActive;
    float    fBloomStrength;
    float    fBloomRadius;
    uint32_t uBloomChainLength;

    // grid
    float  fGridCellSize;
    float  fGridMinPixelsBetweenCells;
    plVec4 tGridColorThin;
    plVec4 tGridColorThick;

    // fog
    bool   bFog;
    bool   bLinearFog;
    float  fFogDensity;
    float  fFogHeight;
    float  fFogStart;
    float  fFogCutOffDistance;
    float  fFogMaxOpacity;
    float  fFogHeightFalloff;
    plVec3 tFogColor;

} plRendererRuntimeOptions;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plLightFlags
{
    PL_LIGHT_FLAG_NONE        = 0,
    PL_LIGHT_FLAG_CAST_SHADOW = 1 << 0,
    PL_LIGHT_FLAG_VISUALIZER  = 1 << 2,
};

enum _plEnvironmentProbeFlags
{
    PL_ENVIRONMENT_PROBE_FLAGS_NONE                    = 0,
    PL_ENVIRONMENT_PROBE_FLAGS_DIRTY                   = 1 << 0,
    PL_ENVIRONMENT_PROBE_FLAGS_REALTIME                = 1 << 1,
    PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY             = 1 << 2,
    PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX = 1 << 3,
};

enum _plObjectFlags
{
    PL_OBJECT_FLAGS_NONE        = 0,
    PL_OBJECT_FLAGS_RENDERABLE  = 1 << 0,
    PL_OBJECT_FLAGS_CAST_SHADOW = 1 << 1,
    PL_OBJECT_FLAGS_DYNAMIC     = 1 << 2,
    PL_OBJECT_FLAGS_FOREGROUND  = 1 << 3,
};

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plObjectComponent
{
    plObjectFlags tFlags;
    plEntity      tMesh;
    plEntity      tTransform;
    plAABB        tAABB;
} plObjectComponent;

typedef struct _plSkinComponent
{
    plMat4*   sbtInverseBindMatrices;
    plEntity* sbtJoints;
    plMat4*   sbtTextureData;
    plAABB    tAABB;
} plSkinComponent;

typedef struct _plEnvironmentProbeComponent
{
    plEnvironmentProbeFlags tFlags;
    uint32_t                uResolution; // default: 128 (must be power of two)
    uint32_t                uSamples;    // default: 128
    uint32_t                uInterval;   // default: 1 (1 to 6 for realtime probe)
    float                   fRange;
} plEnvironmentProbeComponent;

typedef struct _plLightComponent
{
    plLightType  tType;
    plLightFlags tFlags;
    plVec3       tColor;
    float        fIntensity;
    float        fRange;
    float        fRadius;
    float        fInnerConeAngle; // default: 0
    float        fOuterConeAngle; // default: 45
    plVec3       tPosition;
    plVec3       tDirection;
    uint32_t     uShadowResolution; // 0 -> automatic
    float        afCascadeSplits[PL_MAX_SHADOW_CASCADES];
    uint32_t     uCascadeCount;
    float        fShadowLambda;
} plLightComponent;

#ifdef __cplusplus
}
#endif

#endif // PL_RENDERER_EXT_H