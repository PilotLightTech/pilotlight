/*
   pl_ref_renderer_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_REF_RENDERER_EXT_H
#define PL_REF_RENDERER_EXT_H

#define PL_REF_RENDERER_EXT_VERSION    "0.9.0"
#define PL_REF_RENDERER_EXT_VERSION_NUM 000900

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_REF_RENDERER "PL_API_REF_RENDERER"
typedef struct _plRefRendererI plRefRendererI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plViewOptions plViewOptions;
typedef struct _plShaderVariant plShaderVariant;
typedef struct _plComputeShaderVariant plComputeShaderVariant;

// external 
typedef struct _plWindow           plWindow;           // pl_os.h
typedef struct _plGraphics         plGraphics;         // pl_graphics_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_graphics_ext.h
typedef struct _plCommandBuffer    plCommandBuffer;    // pl_graphics_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCameraComponent  plCameraComponent;  // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef union _plMat4              plMat4;             // pl_math.h
typedef union _plVec4              plVec4;             // pl_math.h
typedef union _plVec2              plVec2;             // pl_math.h
typedef void* plTextureId;                             // pl_ui.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plRefRendererI* pl_load_ref_renderer_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plRefRendererI
{
    // setup/shutdown
    void (*initialize)(plWindow* ptWindow);
    void (*cleanup)   (void);

    // scenes
    uint32_t (*create_scene)(void);
    void     (*add_drawable_objects_to_scene)(uint32_t uSceneHandle, uint32_t uOpaqueCount, const plEntity* atOpaqueObjects, uint32_t uTransparentCount, const plEntity* atTransparentObjects);

    // views
    uint32_t    (*create_view)(uint32_t uSceneHandle, plVec2 tDimensions);
    plTextureId (*get_view_texture_id)(uint32_t uSceneHandle, uint32_t uViewHandle);
    void        (*resize_view)(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions);
    
    // loading
    void (*load_skybox_from_panorama)(uint32_t uSceneHandle, const char* pcPath, int iResolution);
    void (*finalize_scene)(uint32_t uSceneHandle);

    // per frame
    void (*run_ecs)(uint32_t uSceneHandle);
    void (*update_skin_textures)(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle);
    void (*perform_skinning)(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle);
    void (*render_scene)(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions);


    void (*generate_cascaded_shadow_map)(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plEntity tCamera, plEntity tLight, float fCascadeSplitLambda);
    
    // misc
    void                (*select_entities)(uint32_t uSceneHandle, uint32_t uCount, plEntity*);
    plComponentLibrary* (*get_component_library)(uint32_t uSceneHandle);
    plGraphics*         (*get_graphics)(void);
    plDrawList3D*       (*get_debug_drawlist)(uint32_t uSceneHandle, uint32_t uViewHandle);

} plRefRendererI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plViewOptions
{
    bool               bShowVisibleBoundingBoxes;
    bool               bShowAllBoundingBoxes;
    bool               bShowOrigin;
    bool               bCullStats;
    plCameraComponent* ptViewCamera;
    plCameraComponent* ptCullCamera;
} plViewOptions;

#endif // PL_REF_RENDERER_EXT_H