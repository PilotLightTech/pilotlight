/*
   pl_ref_renderer_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api
// [SECTION] public api structs
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

// external 
typedef struct _plGraphics         plGraphics;         // pl_graphics_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_graphics_ext.h
typedef struct plRenderPassHandle  plRenderPassHandle; // pl_ecs_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCameraComponent  plCameraComponent;  // pl_ecs_ext.h
typedef union _plMat4              plMat4;             // pl_math.h
typedef union _plVec4              plVec4;             // pl_math.h
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

    // loading
    void (*load_skybox_from_panorama)(const char* pcPath, int iResolution);
    void (*load_stl)(const char* pcPath, plVec4 tColor, const plMat4* ptTransform);
    void (*load_gltf)(const char* pcPath, const plMat4* ptTransform);
    void (*finalize_scene)(void);

    // per frame
    void (*run_ecs)(void);
    void (*submit_ui)(void);
    void (*cull_objects)(plCameraComponent*);
    void (*uncull_objects)(plCameraComponent*);
    void (*draw_all_bound_boxes)(plDrawList3D*);
    void (*draw_visible_bound_boxes)(plDrawList3D*);
    void (*submit_draw_stream)(plCameraComponent*);
    
    // misc
    plComponentLibrary* (*get_component_library)(void);
    plGraphics*         (*get_graphics)(void);

    // temporary
    plTextureId         (*get_offscreen_texture_id)(void);
    plRenderPassHandle  (*get_offscreen_render_pass)(void);

} plRefRendererI;

#endif // PL_REF_RENDERER_EXT_H