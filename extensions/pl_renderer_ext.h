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
// [SECTION] public api structs
// [SECTION] stable structs
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

#define plRendererI_version        {0, 3, 0}
#define plRendererTerrainI_version {0, 1, 0}
#define plRendererEcsI_version     {0, 1, 0}
#define plRendererDebugI_version   {0, 1, 0}
#define plRendererEditorI_version  {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_MAX_SHADOW_CASCADES 4
#define PL_MAX_TERRAIN_ELEVATION_ZONES 3

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
typedef struct _plRendererSettings           plRendererSettings;
typedef struct _plSceneDesc                  plSceneDesc;
typedef struct _plRendererEditorSceneOptions plRendererEditorSceneOptions;
typedef struct _plRendererEditorViewOptions  plRendererEditorViewOptions;
typedef struct _plRendererFogOptions         plRendererFogOptions;
typedef struct _plRendererBloomOptions       plRendererBloomOptions;
typedef struct _plRendererTonemapOptions     plRendererTonemapOptions;
typedef struct _plRendererShadowOptions      plRendererShadowOptions;
typedef struct _plRendererDebugSceneOptions  plRendererDebugSceneOptions;
typedef struct _plRendererDebugViewOptions   plRendererDebugViewOptions;
typedef struct _plRendererLightingOptions    plRendererLightingOptions;
typedef struct _plViewDesc                   plViewDesc;
typedef struct _plRenderViewDesc             plRenderViewDesc;
typedef struct _plScene                      plScene; // opaque type
typedef struct _plView                       plView;  // opaque type

// terrain types
typedef struct _plTerrain plTerrain; // opaque type
typedef struct _plTerrainRuntimeOptions plTerrainRuntimeOptions;
typedef struct _plTerrainMaterialLayer plTerrainMaterialLayer;
typedef struct _plTerrainElevationZone plTerrainElevationZone;

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
typedef int plTerrainFlags;
typedef int plRendererLightingFlags;
typedef int plRendererShadowFlags;
typedef int plRendererBloomFlags;
typedef int plRendererFogFlags;
typedef int plRendererFogMode;

// external 
typedef struct _plWindow             plWindow;             // pl_platform_ext.h
typedef struct _plGraphics           plGraphics;           // pl_graphics_ext.h
typedef struct _plDevice             plDevice;             // pl_graphics_ext.h
typedef struct _plDeviceInfo         plDeviceInfo;         // pl_graphics_ext.h
typedef struct _plDrawList3D         plDrawList3D;         // pl_draw_ext.h
typedef struct _plCommandBuffer      plCommandBuffer;      // pl_graphics_ext.h
typedef struct _plCommandPool        plCommandPool;        // pl_graphics_ext.h
typedef struct _plSwapchain          plSwapchain;          // pl_graphics_ext.h
typedef union  plTextureHandle       plTextureHandle;      // pl_graphics_ext.h
typedef struct _plRenderEncoder      plRenderEncoder;      // pl_graphics_ext.h
typedef union  plRenderPassHandle    plRenderPassHandle;   // pl_graphics_ext.h
typedef union  plBindGroupHandle     plBindGroupHandle;    // pl_graphics_ext.h
typedef struct _plComponentLibrary   plComponentLibrary;   // pl_ecs_ext.h
typedef struct _plCamera             plCamera;             // pl_camera_ext.h
typedef struct _plTerrainProcessInfo plTerrainProcessInfo; // pl_terrain_ext.h
typedef void* plTextureId;                                 // pl_ui_ext.h

// external enums & flags
typedef int plShaderDebugMode; // pl_shader_interop_renderer.h
typedef int plLightType;       // pl_shader_interop_renderer.h
typedef int plDrawFlags;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//-------------------------------extension-------------------------------------

// extension loading
PL_API void pl_load_renderer_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_renderer_ext(plApiRegistryI*, bool reload);

//----------------------------------main---------------------------------------

// setup/shutdown
PL_API bool pl_renderer_initialize(const plRendererSettings*);
PL_API void pl_renderer_cleanup   (void);

// scenes
PL_API plScene* pl_renderer_create_scene (const plSceneDesc*);
PL_API void     pl_renderer_destroy_scene(plScene*);

// views
PL_API plView*           pl_renderer_create_view     (plScene*, const plViewDesc*);
PL_API void              pl_renderer_destroy_view    (plView*);
PL_API void              pl_renderer_resize_view     (plView*, plVec2 dims);
PL_API plBindGroupHandle pl_renderer_get_view_color_bind_group(plView*, plVec2* maxUVOut); // for UI

// Per-frame order:
//   pl_renderer_begin_frame()
//   pl_renderer_prepare_scene(...) // once per scene
//   pl_renderer_prepare_view(...)  // once per visible view
//   pl_renderer_render_view(...)
PL_API bool pl_renderer_begin_frame  (void);
PL_API void pl_renderer_prepare_scene(plScene*);
PL_API void pl_renderer_prepare_view (plView*, const plCamera*);
PL_API void pl_renderer_render_view  (plView*, const plRenderViewDesc*);

// scene runtime options
PL_API void pl_renderer_get_lighting_options(plScene*, plRendererLightingOptions* out);
PL_API void pl_renderer_set_lighting_options(plScene*, const plRendererLightingOptions*);
PL_API void pl_renderer_get_shadow_options  (plScene*, plRendererShadowOptions* out);
PL_API void pl_renderer_set_shadow_options  (plScene*, const plRendererShadowOptions*);
PL_API void pl_renderer_get_fog_options     (plScene*, plRendererFogOptions* out);
PL_API void pl_renderer_set_fog_options     (plScene*, const plRendererFogOptions*);

// view runtime options
PL_API void pl_renderer_get_bloom_options  (plView*, plRendererBloomOptions* out);
PL_API void pl_renderer_set_bloom_options  (plView*, const plRendererBloomOptions*);
PL_API void pl_renderer_get_tonemap_options(plView*, plRendererTonemapOptions* out);
PL_API void pl_renderer_set_tonemap_options(plView*, const plRendererTonemapOptions*);

//---------------------------------editor--------------------------------------

// Editor API: authoring overlays, picking, highlighting, and editor-only helpers.

// misc.
PL_API void          pl_renderer_editor_reload_scene_shaders(plScene*);
PL_API plDrawList3D* pl_renderer_editor_get_gizmo_drawlist  (plView*);
PL_API void          pl_renderer_editor_rebuild_scene_bvh(plScene*);

// selection & highlighting
PL_API void pl_renderer_editor_update_hovered_entity(plView*, plVec2 offset, plVec2 windowScale);
PL_API bool pl_renderer_editor_get_hovered_entity   (plView*, plEntity*);
PL_API void pl_renderer_editor_outline_entities     (plScene*, uint32_t count, const plEntity*);

// scene runtime options
PL_API void pl_renderer_editor_get_scene_options(plScene*, plRendererEditorSceneOptions* out);
PL_API void pl_renderer_editor_set_scene_options(plScene*, const plRendererEditorSceneOptions*);

// view runtime options
PL_API void pl_renderer_editor_get_view_options(plView*, plRendererEditorViewOptions* out);
PL_API void pl_renderer_editor_set_view_options(plView*, const plRendererEditorViewOptions*);

//--------------------------------terrain--------------------------------------

PL_API plTerrain*               pl_renderer_terrain_create             (plCommandBuffer*, plTerrainProcessInfo*);
PL_API void                     pl_renderer_terrain_destroy            (plTerrain*);
PL_API plTerrainRuntimeOptions* pl_renderer_terrain_get_runtime_options(plTerrain*);
PL_API void                     pl_renderer_terrain_set                (plScene*, plTerrain*);

//---------------------------------debug---------------------------------------

// Debug API: renderer diagnostic visualization.

PL_API void          pl_renderer_debug_draw_lights         (plView*, const plLightComponent*, uint32_t lightCount);
PL_API void          pl_renderer_debug_draw_all_bound_boxes(plView*);
PL_API void          pl_renderer_debug_draw_bvh            (plView*);
PL_API plDrawList3D* pl_renderer_debug_get_drawlist        (plView*);

// scene runtime options
PL_API void pl_renderer_debug_get_scene_options (plScene*, plRendererDebugSceneOptions* out);
PL_API void pl_renderer_debug_set_scene_options (plScene*, const plRendererDebugSceneOptions*);

// view runtime options
PL_API void pl_renderer_debug_get_view_options(plView*, plRendererDebugViewOptions* out);
PL_API void pl_renderer_debug_set_view_options(plView*, const plRendererDebugViewOptions*);

//----------------------------ECS INTEGRATION----------------------------------

// system setup/shutdown/etc
PL_API void pl_renderer_ecs_register_system(void);

// entity helpers (creates entity and necessary components)
//   - do NOT store out parameter; use it immediately
PL_API plEntity pl_renderer_ecs_create_object           (plComponentLibrary*, const char* name, plObjectComponent**);
PL_API plEntity pl_renderer_ecs_create_skin             (plComponentLibrary*, const char* name, plSkinComponent**);
PL_API plEntity pl_renderer_ecs_create_directional_light(plComponentLibrary*, const char* name, plVec3 dir, plLightComponent**);
PL_API plEntity pl_renderer_ecs_create_point_light      (plComponentLibrary*, const char* name, plVec3 pos, plLightComponent**);
PL_API plEntity pl_renderer_ecs_create_spot_light       (plComponentLibrary*, const char* name, plVec3 pos, plVec3 dir, plLightComponent**);
PL_API plEntity pl_renderer_ecs_create_environment_probe(plComponentLibrary*, const char* name, plVec3 pos, plEnvironmentProbeComponent**);

// object helpers
PL_API plEntity pl_renderer_ecs_copy_object(plComponentLibrary*, const char* name, plEntity originalObject, plObjectComponent**);

// systems
PL_API void pl_renderer_ecs_run_object_update_system           (plComponentLibrary*);
PL_API void pl_renderer_ecs_run_skin_update_system             (plComponentLibrary*);
PL_API void pl_renderer_ecs_run_light_update_system            (plComponentLibrary*);
PL_API void pl_renderer_ecs_run_environment_probe_update_system(plComponentLibrary*);

// ecs types
PL_API plEcsTypeKey pl_renderer_ecs_get_type_key_object           (void);
PL_API plEcsTypeKey pl_renderer_ecs_get_type_key_skin             (void);
PL_API plEcsTypeKey pl_renderer_ecs_get_type_key_light            (void);
PL_API plEcsTypeKey pl_renderer_ecs_get_type_key_environment_probe(void);

// scene interaction
PL_API bool pl_renderer_ecs_add_drawable_objects_to_scene(plScene*, uint32_t count, const plEntity* objects);
PL_API void pl_renderer_ecs_add_probes_to_scene          (plScene*, uint32_t count, const plEntity* probes);
PL_API void pl_renderer_ecs_add_lights_to_scene          (plScene*, uint32_t count, const plEntity* lights);
PL_API void pl_renderer_ecs_add_materials_to_scene       (plScene*, uint32_t count, const plEntity* materials);
PL_API void pl_renderer_ecs_update_scene_materials       (plScene*, uint32_t count, const plEntity* materials);
PL_API void pl_renderer_ecs_load_skybox_from_panorama    (plScene*, const char* path, int resolution);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plRendererI
{
    // setup/shutdown
    bool (*initialize)(const plRendererSettings*);
    void (*cleanup)   (void);

    // scenes
    plScene* (*create_scene) (const plSceneDesc*);
    void     (*destroy_scene)(plScene*);

    // views
    plView*           (*create_view)               (plScene*, const plViewDesc*);
    void              (*destroy_view)              (plView*);
    plBindGroupHandle (*get_view_color_bind_group) (plView*, plVec2* maxUVOut); // for UI
    void              (*resize_view)               (plView*, plVec2 dims);

    // Per-frame order:
    //   begin_frame()
    //   prepare_scene(...) // once per scene
    //   prepare_view(...)  // once per visible view
    //   render_view(...)
    bool (*begin_frame)  (void);
    void (*prepare_scene)(plScene*);
    void (*prepare_view) (plView*, const plCamera*);
    void (*render_view)  (plView*, const plRenderViewDesc*);

    // scene options
    void (*get_fog_options)     (plScene*, plRendererFogOptions* out);
    void (*set_fog_options)     (plScene*, const plRendererFogOptions*);
    void (*get_shadow_options)  (plScene*, plRendererShadowOptions* out);
    void (*set_shadow_options)  (plScene*, const plRendererShadowOptions*);
    void (*get_lighting_options)(plScene*, plRendererLightingOptions* out);
    void (*set_lighting_options)(plScene*, const plRendererLightingOptions*);

    // view options
    void (*get_bloom_options)  (plView*, plRendererBloomOptions* out);
    void (*set_bloom_options)  (plView*, const plRendererBloomOptions*);
    void (*get_tonemap_options)(plView*, plRendererTonemapOptions* out);
    void (*set_tonemap_options)(plView*, const plRendererTonemapOptions*);
} plRendererI;

typedef struct _plRendererEcsI
{
    void (*register_system)(void);
    
    // create entities for scene
    plEntity (*create_object)           (plComponentLibrary*, const char* name, plObjectComponent**);
    plEntity (*create_skin)             (plComponentLibrary*, const char* name, plSkinComponent**);
    plEntity (*create_directional_light)(plComponentLibrary*, const char* name, plVec3 dir, plLightComponent**);
    plEntity (*create_point_light)      (plComponentLibrary*, const char* name, plVec3 pos, plLightComponent**);
    plEntity (*create_spot_light)       (plComponentLibrary*, const char* name, plVec3 pos, plVec3 dir, plLightComponent**);
    plEntity (*create_environment_probe)(plComponentLibrary*, const char* name, plVec3 pos, plEnvironmentProbeComponent**);

    // "constructing" scenes
    bool (*add_drawable_objects_to_scene)(plScene*, uint32_t count, const plEntity* objects);
    void (*add_probes_to_scene)          (plScene*, uint32_t count, const plEntity* probes);
    void (*add_lights_to_scene)          (plScene*, uint32_t count, const plEntity* lights);
    void (*add_materials_to_scene)       (plScene*, uint32_t count, const plEntity* materials);
    void (*load_skybox_from_panorama)    (plScene*, const char* path, int resolution);

    // ecs system updates
    void (*run_object_update_system)           (plComponentLibrary*);
    void (*run_skin_update_system)             (plComponentLibrary*);
    void (*run_light_update_system)            (plComponentLibrary*);
    void (*run_environment_probe_update_system)(plComponentLibrary*);

    // editor helpers really
    plEntity     (*copy_object)                       (plComponentLibrary*, const char* name, plEntity originalObject, plObjectComponent**);
    plEcsTypeKey (*get_ecs_type_key_object)           (void);
    plEcsTypeKey (*get_ecs_type_key_skin)             (void);
    plEcsTypeKey (*get_ecs_type_key_light)            (void);
    plEcsTypeKey (*get_ecs_type_key_environment_probe)(void);
    void         (*update_scene_materials)            (plScene*, uint32_t count, const plEntity* materials);

} plRendererEcsI;

typedef struct _plRendererTerrainI
{
    plTerrain*               (*create)             (plCommandBuffer*, plTerrainProcessInfo*);
    void                     (*destroy)            (plTerrain*);
    void                     (*set)                (plScene*, plTerrain*);
    plTerrainRuntimeOptions* (*get_runtime_options)(plTerrain*);
} plRendererTerrainI;

typedef struct _plRendererDebugI
{
    void          (*draw_lights)         (plView*, const plLightComponent*, uint32_t lightCount);
    void          (*draw_all_bound_boxes)(plView*);
    void          (*draw_bvh)            (plView*);
    plDrawList3D* (*get_drawlist)        (plView*);

    // scene options
    void (*get_scene_options)(plScene*, plRendererDebugSceneOptions* out);
    void (*set_scene_options)(plScene*, const plRendererDebugSceneOptions*);

    // view options
    void (*get_view_options)(plView*, plRendererDebugViewOptions* out);
    void (*set_view_options)(plView*, const plRendererDebugViewOptions*);
} plRendererDebugI;

typedef struct _plRendererEditorI
{
    void          (*update_hovered_entity)(plView*, plVec2 offset, plVec2 windowScale);
    bool          (*get_hovered_entity)   (plView*, plEntity*);
    void          (*outline_entities)     (plScene*, uint32_t count, const plEntity*);
    void          (*reload_scene_shaders) (plScene*);
    plDrawList3D* (*get_gizmo_drawlist)   (plView*);
    void          (*rebuild_scene_bvh)    (plScene*);

    // scene options
    void (*get_scene_options)  (plScene*, plRendererEditorSceneOptions* out);
    void (*set_scene_options)  (plScene*, const plRendererEditorSceneOptions*);

    // view options
    void (*get_view_options)   (plView*, plRendererEditorViewOptions* out);
    void (*set_view_options)   (plView*, const plRendererEditorViewOptions*);
} plRendererEditorI;

//-----------------------------------------------------------------------------
// [SECTION] stable structs
//-----------------------------------------------------------------------------

typedef struct _plRendererSettings
{
    plDevice*    ptDevice;
    plSwapchain* ptSwapchain;
    uint32_t     uMaxTextureResolution; // default 1024 (should be factor of 2)
} plRendererSettings;

typedef struct _plSceneDesc
{
    plComponentLibrary* ptComponentLibrary;
    size_t              szIndexBufferSize;    // default: 64000000
    size_t              szVertexBufferSize;   // default: 64000000
    size_t              szDataBufferSize;     // default: 64000000
    size_t              szMaterialBufferSize; // default:  8000000
    size_t              szSkinBufferSize;     // default:  8000000
} plSceneDesc;

typedef struct _plViewDesc
{
    uint32_t uWidth;
    uint32_t uHeight;
} plViewDesc;

typedef struct _plRenderViewDesc
{
    const plCamera* ptCamera;
    const plCamera* ptCullCamera; // optional, NULL => ptCamera
} plRenderViewDesc;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRendererLightingOptions
{
    plRendererLightingFlags tFlags;
} plRendererLightingOptions;

typedef struct _plRendererShadowOptions
{
    plRendererShadowFlags tFlags;
    float                 fMaxShadowRange; // world units
    float                 fConstantDepthBias;
    float                 fSlopeDepthBias;
} plRendererShadowOptions;

typedef struct _plRendererTonemapOptions
{
    plTonemapMode tMode;
    float         fExposure;   // default: 1.0
    float         fBrightness; // default: 0.0
    float         fContrast;   // default: 1.0
    float         fSaturation; // default: 1.0
} plRendererTonemapOptions;

typedef struct _plRendererBloomOptions
{
    plRendererBloomFlags tFlags;
    float                fStrength;    // default: 0.05
    float                fRadius;      // default: 1.5
    uint32_t             uChainLength; // default: 5
} plRendererBloomOptions;

typedef struct _plRendererFogOptions
{
    plRendererFogFlags tFlags;
    plRendererFogMode  tMode;

    // common for all modes
    float  fStart;          // fog start distance
    float  fCutOffDistance; // max fog distance / cutoff
    plVec3 tColor;          // 

    // exponential fog mode only
    float fDensity;       // exponential/height fog density
    float fHeight;        // world-space height reference
    float fMaxOpacity;    // 0..1
    float fHeightFalloff; // fog falloff
    
} plRendererFogOptions;

typedef struct _plRendererDebugSceneOptions
{
    bool bWireframe;
    bool bShowOrigin;
    bool bShowProbes;
    plShaderDebugMode tShaderDebugMode;
} plRendererDebugSceneOptions;

typedef struct _plRendererDebugViewOptions
{
    int _iUnused;
} plRendererDebugViewOptions;

typedef struct _plRendererEditorViewOptions
{
    bool bShowSkybox;
    bool bShowGrid;
    bool bShowSelectedBoundingBox;
    uint32_t uOutlineWidth;

    // grid
    float  fGridCellSize;
    float  fGridMinPixelsBetweenCells;
    plVec4 tGridColorThin;
    plVec4 tGridColorThick;
} plRendererEditorViewOptions;

typedef struct _plRendererEditorSceneOptions
{
    int _iUnused;
} plRendererEditorSceneOptions;

typedef struct _plTerrainMaterialLayer
{
    plVec4           tBaseColor;      // default/debug fallback
    // plResourceHandle tAlbedoTexture;  // optional
    // plResourceHandle tNormalTexture;  // optional
    // plResourceHandle tAOMRTexture;    // optional

    float            fUVScale;        // default: 1.0f
    float            fRoughness;      // default: 1.0f
    float            fMetalness;      // default: 0.0f
    float            fAO;             // default: 1.0f
} plTerrainMaterialLayer;

typedef struct _plTerrainElevationZone
{
    float fMinElevation;
    float fMaxElevation;
    float fBlendSize;

    plTerrainMaterialLayer tFlatMaterial;
    plTerrainMaterialLayer tSteepMaterial;
} plTerrainElevationZone;

typedef struct _plTerrainRuntimeOptions
{
    plTerrainFlags         tFlags;
    float                  fTau;
    float                  fSlopeStart;
    float                  fSlopeEnd;
    plTerrainElevationZone atElevationZones[PL_MAX_TERRAIN_ELEVATION_ZONES];
    float                  fTerrainShadowConstantDepthBias;
    float                  fTerrainShadowSlopeDepthBias;
} plTerrainRuntimeOptions;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plRendererFogFlags
{
    PL_RENDERER_FOG_FLAGS_NONE   = 0,
    PL_RENDERER_FOG_FLAGS_ACTIVE = 1 << 0,
};

enum _plRendererFogMode
{
    PL_RENDERER_FOG_MODE_LINEAR = 0,
    PL_RENDERER_FOG_MODE_EXPONENTIAL
};

enum _plRendererBloomFlags
{
    PL_RENDERER_BLOOM_FLAGS_NONE   = 0,
    PL_RENDERER_BLOOM_FLAGS_ACTIVE = 1 << 0,
};

enum _plRendererLightingFlags
{
    PL_RENDERER_LIGHTING_FLAGS_NONE             = 0,
    PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED      = 1 << 0,
    PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING   = 1 << 1,
    PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS  = 1 << 2,
};

enum _plRendererShadowFlags
{
    PL_RENDERER_SHADOW_FLAGS_NONE           = 0,
    PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT = 1 << 0,
    PL_RENDERER_SHADOW_FLAGS_PCF            = 1 << 1,
};

enum _plTerrainFlags
{
    PL_TERRAIN_FLAGS_NONE        = 0,
    PL_TERRAIN_FLAGS_WIREFRAME   = 1 << 0,
    PL_TERRAIN_FLAGS_SHOW_LEVELS = 1 << 1
};

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
    plMat4*   atInverseBindMatrices;
    plEntity* atJoints;
    uint32_t  uJointCount;
    plAABB    tAABB;

    // [INTERNAL]
    plMat4* _atTextureData;
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