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

// external 
typedef struct _plGraphics         plGraphics;         // pl_graphics_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_graphics_ext.h
typedef struct _plCommandBuffer    plCommandBuffer;    // pl_graphics_ext.h
typedef struct _plPassRenderer     plPassRenderer;     // pl_graphics_ext.h
typedef struct plRenderPassHandle  plRenderPassHandle; // pl_ecs_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCameraComponent  plCameraComponent;  // pl_ecs_ext.h
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
    // setup/shutdown/resize
    void (*initialize)(void);
    void (*cleanup)   (void);
    void (*resize)    (void);

    // scenes
    uint32_t (*create_scene)(void);

    // views
    uint32_t    (*create_view)(uint32_t uSceneHandle, plVec2 tDimensions);
    plTextureId (*get_view_texture_id)(uint32_t uSceneHandle, uint32_t uViewHandle);
    void        (*resize_view)(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions);
    
    // loading
    void (*load_skybox_from_panorama)(uint32_t uSceneHandle, const char* pcPath, int iResolution);
    void (*load_stl)(uint32_t uSceneHandle, const char* pcPath, plVec4 tColor, const plMat4* ptTransform);
    void (*load_gltf)(uint32_t uSceneHandle, const char* pcPath, const plMat4* ptTransform);
    void (*finalize_scene)(uint32_t uSceneHandle);

    // per frame
    void (*run_ecs)(void);
    void (*update_scene)(uint32_t uSceneHandle);
    void (*render_scene)(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions);
    
    // misc
    plComponentLibrary* (*get_component_library)(void);
    plGraphics*         (*get_graphics)(void);

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