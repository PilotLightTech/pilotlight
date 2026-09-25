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
#include "pl_asset_ext.inl"    // plAssetHandle
#include "pl_math.h"           // plVec3, plMat4

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plRendererSettings        plRendererSettings;
typedef struct _plSceneDesc               plSceneDesc;
typedef struct _plRendererFogOptions      plRendererFogOptions;
typedef struct _plRendererBloomOptions    plRendererBloomOptions;
typedef struct _plRendererTonemapOptions  plRendererTonemapOptions;
typedef struct _plRendererShadowOptions   plRendererShadowOptions;
typedef struct _plRendererLightingOptions plRendererLightingOptions;
typedef struct _plRendererSkyOptions      plRendererSkyOptions;
typedef struct _plViewDesc                plViewDesc;
typedef struct _plRenderViewDesc          plRenderViewDesc;
typedef struct _plScene                   plScene; // opaque type
typedef struct _plView                    plView;  // opaque type

// assets
typedef struct _plRenderEnvironment plRenderEnvironment;
typedef struct _plRenderSettings    plRenderSettings;

// terrain types
typedef struct _plTerrain plTerrain; // opaque type

// ecs components
typedef struct _plObjectComponent           plObjectComponent;
typedef struct _plLightComponent            plLightComponent;
typedef struct _plEnvironmentProbeComponent plEnvironmentProbeComponent;
typedef struct _plTerrainComponent          plTerrainComponent;
typedef struct _plEnvironmentComponent      plEnvironmentComponent;
typedef struct _plRendererComponent         plRendererComponent;

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
typedef int plRenderSceneFlags;
typedef int plRendererSkyFlags;
typedef int plRendererSkyMode;

// external 
typedef struct _plDevice           plDevice;           // pl_graphics_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h
typedef struct _plSwapchain        plSwapchain;        // pl_graphics_ext.h
typedef union  plBindGroupHandle   plBindGroupHandle;  // pl_graphics_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCamera           plCamera;           // pl_camera_ext.h

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

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plRendererI
{
    // setup/shutdown
    bool (*initialize)(const plRendererSettings*);
    void (*cleanup)   (void);

    // scenes
    plScene*             (*create_scene)   (const plSceneDesc*);
    void                 (*destroy_scene)  (plScene*);
    plRenderSceneFlags   (*get_scene_flags)(const plScene*);
    void                 (*set_scene_flags)(plScene*, plRenderSceneFlags);

    // scene modifications
    void (*load_component_library)  (plScene*, plComponentLibrary*);
    void (*add_entity_to_scene)     (plScene*, plEntity);
    bool (*remove_entity_from_scene)(plScene*, plEntity);
    void (*update_scene_asset)      (plScene*, plAssetHandle);

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
    void (*prepare_scene)(plScene*, const plCamera**, uint32_t cameraCount);
    void (*prepare_view) (plView*, const plCamera*);
    void (*render_view)      (plView*, const plRenderViewDesc*);
    void (*render_debug_view)(plView*, const plRenderViewDesc*);

    // assets
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key_environment)(void);
    plAssetTypeKey (*get_asset_type_key_settings)(void);

    // ecs
    void (*register_ecs_components)(void);
    
    // ecs system updates
    void (*run_object_update_system)           (plComponentLibrary*);
    void (*run_light_update_system)            (plComponentLibrary*);
    void (*run_environment_probe_update_system)(plComponentLibrary*);

    // editor helpers really
    plEcsTypeKey (*get_ecs_type_key_object)           (void);
    plEcsTypeKey (*get_ecs_type_key_light)            (void);
    plEcsTypeKey (*get_ecs_type_key_environment_probe)(void);
    plEcsTypeKey (*get_ecs_type_key_terrain)          (void);
    plEcsTypeKey (*get_ecs_type_key_environment)      (void);
    plEcsTypeKey (*get_ecs_type_key_renderer)         (void);

    // misc.
    plDrawList3D* (*get_drawlist)(plView*);

    // editor stuff
    void          (*update_hovered_entity)(plView*, plVec2 offset, plVec2 windowScale);
    bool          (*get_hovered_entity)   (plView*, plEntity*);
    void          (*outline_entities)     (plScene*, uint32_t count, const plEntity*);
    void          (*reload_scene_shaders) (plScene*);
    plDrawList3D* (*get_gizmo_drawlist)   (plView*);
    void          (*rebuild_scene_bvh)    (plScene*);
} plRendererI;

//-----------------------------------------------------------------------------
// [SECTION] stable structs
//-----------------------------------------------------------------------------

typedef struct _plRendererSettings
{
    plDevice*    ptDevice;
    plSwapchain* ptSwapchain;
} plRendererSettings;

typedef struct _plSceneDesc
{
    size_t   szIndexBufferSize;      // default: 64000000
    size_t   szVertexBufferSize;     // default: 64000000
    size_t   szDataBufferSize;       // default: 64000000
    size_t   szMaterialBufferSize;   // default:  8000000
    size_t   szSkinBufferSize;       // default:  8000000
    uint32_t uShadowAtlasResolution; // default:    4096
} plSceneDesc;

typedef struct _plViewDesc
{
    uint32_t uWidth;
    uint32_t uHeight;
} plViewDesc;

typedef struct _plRenderViewDesc
{
    const plCamera* ptCamera;
} plRenderViewDesc;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRenderEnvironment
{
    plRendererSkyMode  eMode;
    plRendererSkyFlags eFlags;
    plVec3             tSunColor;
    plVec3             tSunDirection;
    float              fSunIntensity;
    
    //------------skybox rendering options--------------

    plAssetHandle tSkyboxTexture;
    uint32_t      uSkyboxResolution;

    //----------realistic rendering options------------
    
    // general
    float  fAtmosphereConversion;
    float  fSunRadius;
    float  fPlanetRadius;
    float  fAtmosphereHeight;
    plVec3 tScatteringRayleighGround;
    plVec3 tExtinctionRayleighGround;
    plVec3 tOzoneExtinction;
    float  fScatteringMieGround;
    float  fExtinctionMieGround;
    float  fMieScatteringExponent; // used in mie phase function

    // aerial perspective
    float    fMaxAerialDistance; // in atmosphere units
    float    fAerialDepthExponent; // samples per slice
    uint32_t uAerialSamplesPerSlice; // samples per slice

} plRenderEnvironment;

typedef struct _plRendererSkyOptions
{
    plVec2 tTransmissionLutResolution;
    plVec2 tSkyLutResolution;
    plVec2 tMultiscatterLutResolution;
    plVec3 tAerialLutResolution;
} plRendererSkyOptions;

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
    uint32_t              uShadowCascadeCount;
    uint32_t              uShadowResolution;
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

typedef struct _plRenderSettings
{
    plRendererLightingOptions tLighting;
    plRendererShadowOptions   tShadows;
    plRendererFogOptions      tFog;
    plRendererBloomOptions    tBloom; // technically could be per view
    plRendererTonemapOptions  tTonemap; // technically could be per view
    plRendererSkyOptions      tSky;
} plRenderSettings;

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
    PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS       = 1 << 3
};

enum _plRendererSkyMode
{
    PL_RENDERER_SKY_MODE_NONE = 0,
    PL_RENDERER_SKY_MODE_SKYBOX,
    PL_RENDERER_SKY_MODE_REALISTIC,
};

enum _plRendererSkyFlags
{
    PL_RENDERER_SKY_FLAGS_NONE = 0,

    // general options
    PL_RENDERER_SKY_FLAGS_SHADOWS         = 1 << 1,
    PL_RENDERER_SKY_FLAGS_SHOW_VISUALIZER = 1 << 3,
    PL_RENDERER_SKY_FLAGS_DEBUG_CASCADES  = 1 << 4,

    // realistic options
    PL_RENDERER_SKY_FLAGS_MULTISCATTER       = 1 << 5,
    PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE = 1 << 6,
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
    PL_OBJECT_FLAGS_NONE           = 0,
    PL_OBJECT_FLAGS_RENDERABLE     = 1 << 0,
    PL_OBJECT_FLAGS_CAST_SHADOW    = 1 << 1,
    PL_OBJECT_FLAGS_RECEIVE_SHADOW = 1 << 2,
    PL_OBJECT_FLAGS_DYNAMIC        = 1 << 3,
    PL_OBJECT_FLAGS_FOREGROUND     = 1 << 4,
    PL_OBJECT_FLAGS_OUTLINE        = 1 << 5
};

enum _plRenderSceneFlags
{
    PL_RENDERER_SCENE_FLAGS_NONE             = 0,
    PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY = 1 << 0,
    PL_RENDERER_SCENE_FLAGS_SKY_LUTS_DIRTY   = 1 << 1,
    PL_RENDERER_SCENE_FLAGS_SKYBOX_DIRTY     = 1 << 2,
};

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plObjectComponent
{
    plObjectFlags tFlags;
    plAssetHandle tMesh;
    plEntityId    tTransformId;
    uint32_t      uFirstSubmesh;
    uint32_t      uSubmeshCount;
    plAABB        tAABB;
    plEntity      tTransform;
} plObjectComponent;

typedef struct _plTerrainComponent
{
    plAssetHandle  tTerrain;
    plTerrainFlags tFlags;
    float          fTau;
    float          fSlopeStart;
    float          fSlopeEnd;
    float          fTerrainShadowConstantDepthBias;
    float          fTerrainShadowSlopeDepthBias;
} plTerrainComponent;

typedef struct _plEnvironmentComponent
{
    plAssetHandle tEnvironment;
} plEnvironmentComponent;

typedef struct _plRendererComponent
{
    plAssetHandle tRenderer;
} plRendererComponent;

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
} plLightComponent;

#ifdef __cplusplus
}
#endif

#endif // PL_RENDERER_EXT_H