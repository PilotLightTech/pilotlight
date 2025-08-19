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
        * plVfsI           (v1.x)
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

#define plRendererI_version {0, 2, 1}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_MAX_SHADOW_CASCADES 4

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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
typedef struct _plScene                  plScene;
typedef struct _plView                   plView;

// ecs components
typedef struct _plObjectComponent           plObjectComponent;
typedef struct _plSkinComponent             plSkinComponent;
typedef struct _plMaterialComponent         plMaterialComponent;
typedef struct _plLightComponent            plLightComponent;
typedef struct _plEnvironmentProbeComponent plEnvironmentProbeComponent;

// enums & flags
typedef int plShaderType;
typedef int plMaterialFlags;
typedef int plBlendMode;
typedef int plLightFlags;
typedef int plEnvironmentProbeFlags;
typedef int plObjectFlags;
typedef int plTextureSlot;
typedef int plTonemapMode;

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
typedef void* plTextureId;                             // pl_ui.h


// external (pl_ecs_ext.h)
typedef struct _plComponentLibrary plComponentLibrary;
typedef struct _plCamera           plCamera;
typedef struct _plLightComponent   plLightComponent;

// external
typedef int plShaderDebugMode; // pl_shader_interop_renderer.h
typedef int plLightType;       // pl_shader_interop_renderer.h
typedef int plDrawFlags;       // pl_draw_ext.h

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
    plVec2            (*get_view_color_texture_max_uv)(plView*);
    void              (*resize_view)(plView*, plVec2 tDimensions);

    // per frame
    void (*prepare_scene)(plScene*);
    void (*prepare_view) (plView*, plCamera*);
    void (*render_view) (plView*, plCamera*, plCamera* cullCamera);
    bool (*begin_frame)     (void);
    
    // per frame options
    void (*show_skybox)               (plView*);
    void (*show_grid)                 (plView*);
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
    plRendererRuntimeOptions* (*get_runtime_options)(void);
    void (*rebuild_scene_bvh)(plScene*);

    //----------------------------ECS INTEGRATION----------------------------------

    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // entity helpers (creates entity and necessary components)
    //   - do NOT store out parameter; use it immediately
    plEntity (*create_object)             (plComponentLibrary*, const char* pcName, plObjectComponent**);
    plEntity (*create_material)           (plComponentLibrary*, const char* pcName, plMaterialComponent**);
    plEntity (*create_skin)               (plComponentLibrary*, const char* pcName, plSkinComponent**);
    plEntity (*create_directional_light)  (plComponentLibrary*, const char* pcName, plVec3 tDirection, plLightComponent**);
    plEntity (*create_point_light)        (plComponentLibrary*, const char* pcName, plVec3 tPosition, plLightComponent**);
    plEntity (*create_spot_light)         (plComponentLibrary*, const char* pcName, plVec3 tPosition, plVec3 tDirection, plLightComponent**);
    plEntity (*create_environment_probe)  (plComponentLibrary*, const char* pcName, plVec3 tPosition, plEnvironmentProbeComponent**);

    // object helpers
    plEntity (*copy_object) (plComponentLibrary*, const char* pcName, plEntity tOriginalObject, plObjectComponent**);

    // systems
    void (*run_object_update_system)            (plComponentLibrary*);
    void (*run_skin_update_system)              (plComponentLibrary*);
    void (*run_light_update_system)             (plComponentLibrary*);
    void (*run_environment_probe_update_system) (plComponentLibrary*);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key_object)(void);
    plEcsTypeKey (*get_ecs_type_key_material)(void);
    plEcsTypeKey (*get_ecs_type_key_skin)(void);
    plEcsTypeKey (*get_ecs_type_key_light)(void);
    plEcsTypeKey (*get_ecs_type_key_environment_probe)(void);

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
    bool              bShowProbes;
    bool              bWireframe;
    bool              bShowOrigin;
    bool              bShowSelectedBoundingBox;
    bool              bMultiViewportShadows;
    bool              bImageBasedLighting;
    bool              bNormalMapping;
    bool              bPunctualLighting;
    float             fShadowConstantDepthBias;
    float             fShadowSlopeDepthBias;
    uint32_t          uOutlineWidth;
    plShaderDebugMode tShaderDebugMode;

    // tonemapping
    plTonemapMode   tTonemapMode;
    float           fExposure;
    float           fBrightness;
    float           fContrast;
    float           fSaturation;

    // grid
    float fGridCellSize;
    float fGridMinPixelsBetweenCells;
    plVec4 tGridColorThin;
    plVec4 tGridColorThick;

} plRendererRuntimeOptions;

typedef struct _plTextureMap
{
    char             acName[PL_MAX_PATH_LENGTH];
    plResourceHandle tResource;
    uint32_t         uUVSet;
    uint32_t         uWidth;
    uint32_t         uHeight;
} plTextureMap;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTextureSlot
{
    PL_TEXTURE_SLOT_BASE_COLOR_MAP,
    PL_TEXTURE_SLOT_NORMAL_MAP,
    PL_TEXTURE_SLOT_EMISSIVE_MAP,
    PL_TEXTURE_SLOT_OCCLUSION_MAP,
    PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP,
    
    PL_TEXTURE_SLOT_COUNT
};

enum _plShaderType
{
    PL_SHADER_TYPE_PBR,
    PL_SHADER_TYPE_CUSTOM,
    
    PL_SHADER_TYPE_COUNT
};

enum _plMaterialFlags
{
    PL_MATERIAL_FLAG_NONE                = 0,
    PL_MATERIAL_FLAG_DOUBLE_SIDED        = 1 << 0,
    PL_MATERIAL_FLAG_OUTLINE             = 1 << 1,
    PL_MATERIAL_FLAG_CAST_SHADOW         = 1 << 2,
    PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW = 1 << 3
};

enum _plBlendMode
{
    PL_BLEND_MODE_OPAQUE,
    PL_BLEND_MODE_ALPHA,
    PL_BLEND_MODE_PREMULTIPLIED,
    PL_BLEND_MODE_ADDITIVE,
    PL_BLEND_MODE_MULTIPLY,
    PL_BLEND_MODE_CLIP_MASK,

    PL_BLEND_MODE_COUNT
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
} plLightComponent;

typedef struct _plMaterialComponent
{
    plBlendMode     tBlendMode;
    plMaterialFlags tFlags;
    plShaderType    tShaderType;
    plVec4          tBaseColor;
    plVec4          tEmissiveColor;
    float           fAlphaCutoff;
    float           fRoughness;
    float           fMetalness;
    float           fNormalMapStrength;
    float           fOcclusionMapStrength;
    plTextureMap    atTextureMaps[PL_TEXTURE_SLOT_COUNT];
    uint32_t        uLayerMask;
} plMaterialComponent;

#endif // PL_RENDERER_EXT_H