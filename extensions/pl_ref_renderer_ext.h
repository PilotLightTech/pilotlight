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
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h
typedef struct _plCommandBuffer    plCommandBuffer;    // pl_graphics_ext.h
typedef union  plTextureHandle     plTextureHandle;    // pl_graphics_ext.h
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

typedef struct _plRefRendererI
{
    // setup/shutdown
    void (*initialize)(plWindow* ptWindow);
    void (*cleanup)   (void);

    // scenes
    uint32_t (*create_scene)(void);
    void     (*add_drawable_objects_to_scene)(uint32_t uSceneHandle, uint32_t uOpaqueCount, const plEntity* atOpaqueObjects, uint32_t uTransparentCount, const plEntity* atTransparentObjects);

    // views
    uint32_t        (*create_view)(uint32_t uSceneHandle, plVec2 tDimensions);
    plTextureHandle (*get_view_color_texture)(uint32_t uSceneHandle, uint32_t uViewHandle);
    void            (*resize_view)(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions);
    
    // loading
    void (*load_skybox_from_panorama)(uint32_t uSceneHandle, const char* pcPath, int iResolution);
    void (*finalize_scene)(uint32_t uSceneHandle);

    // ui
    void (*show_graphics_options)(void);

    // per frame
    void (*run_ecs)     (uint32_t uSceneHandle);
    void (*render_scene)(uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions);
    bool (*begin_frame) (void);
    void (*end_frame)   (void);
    plEntity (*get_picked_entity)(void);

    // misc
    void                (*select_entities)(uint32_t uSceneHandle, uint32_t uCount, plEntity*);
    plComponentLibrary* (*get_component_library)(uint32_t uSceneHandle);
    plGraphics*         (*get_graphics)(void);
    plDrawList3D*       (*get_debug_drawlist)(uint32_t uSceneHandle, uint32_t uViewHandle);
    plDrawList3D*       (*get_gizmo_drawlist)(uint32_t uSceneHandle, uint32_t uViewHandle);

} plRefRendererI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plViewOptions
{
    plEntity* ptViewCamera;
    plEntity* ptCullCamera;
    plEntity* ptSunLight;
} plViewOptions;

#endif // PL_REF_RENDERER_EXT_H