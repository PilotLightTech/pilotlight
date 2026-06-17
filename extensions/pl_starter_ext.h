/*
   pl_starter_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes notes
// [SECTION] quick notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plGraphicsI    (v2.x)
        * plScreenLogI   (v2.x)
        * plUiI          (v1.x)
        * plDrawI        (v3.x)
        * plShaderI      (v2.x)
        * plProfileI     (v1.x)
        * plConsoleI     (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    Background:
        This extension serves mostly as a helper extension to remove a
        significant amount of boilerplate for users who just need to minimum
        setup to begin using the drawing, UI, or other extensions that don't
        require the user to care about the lower level details of the graphics
        extension. It is also used in the examples to help users grasp concepts
        in a more isolated manner. It contains helper functions for common
        tasks (i.e. selecting a device). It may not utilize the most optimal
        techniques but should be "decent"
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STARTER_EXT_H
#define PL_STARTER_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plStarterI_version {2, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plStarterInit plStarterInit;

// enums/flags
typedef int plStarterFlags;

// external
typedef struct _plWindow               plWindow;               // pl.h
typedef struct _plFont                 plFont;                 // pl_draw_ext.h
typedef struct _plDrawLayer2D          plDrawLayer2D;          // pl_draw_ext.h
typedef struct _plDevice               plDevice;               // pl_graphics_ext.h
typedef struct _plCommandPool          plCommandPool;          // pl_graphics_ext.h
typedef struct _plTimelineSemaphore    plTimelineSemaphore;    // pl_graphics_ext.h
typedef struct _plSurface              plSurface;              // pl_graphics_ext.h
typedef struct _plSwapchain            plSwapchain;            // pl_graphics_ext.h
typedef struct _plCommandBuffer        plCommandBuffer;        // pl_graphics_ext.h
typedef struct _plRenderAttachmentInfo plRenderAttachmentInfo; // pl_graphics_ext.h
typedef struct _plTextureDesc          plTextureDesc;          // pl_graphics_ext.h
typedef struct _plBufferDesc           plBufferDesc;           // pl_graphics_ext.h
typedef union plBufferHandle           plBufferHandle;         // pl_graphics_ext.h
typedef union plTextureHandle          plTextureHandle;        // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_starter_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_starter_ext(plApiRegistryI*, bool reload);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~high level API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// initialize
//   - this will setup extensions that are included in the init flags and handle
//     other tasks related to setup
PL_API void pl_starter_initialize(plStarterInit);

// finalize
//   - if you decide to handle any extension setup, calling this function after
//     gives this extension a change to finish setup that relies on those
//     extensions being initialized
PL_API void pl_starter_finalize(void);

// cleanup
//   - just handles all the cleanup need by the managed extensions
PL_API void pl_starter_cleanup(void);

// begin frame
//   - this absolutely must be called first inside "pl_app_update"
//   - handles most of the "begin frame" calls & main swapchain stuff
PL_API bool pl_starter_begin_frame(void);

// end frame
//   - this absolutely must be called last inside "pl_app_update"
//   - handles most of the "end frame" calls & will call main
//     pass begin/end if you don't
PL_API void pl_starter_end_frame  (void);

// main pass
//   - if you need to submit your own work, use this
//     and make sure to call "end_main_pass" before ending the frame
PL_API plCommandBuffer* pl_starter_begin_main_pass(void);
PL_API void             pl_starter_end_main_pass  (void);

// resize
//   - call inside "pl_app_resize" to have swapchain recreation handled
PL_API void pl_starter_resize(void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~mid level API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// command buffers
//    - these command buffers are already in a recording state and
//      syncronized with subsequent calls/main pass with the internal
//      timeline semaphores. Use between "begin_frame" and before "begin_main_pass"
//    - They do NOT block.
PL_API plCommandBuffer* pl_starter_get_command_buffer(void);
PL_API void             pl_starter_submit_command_buffer(plCommandBuffer*);

// temporary command buffers
//    - these command buffers are already in a recording state but are not
//      syncronized.
//    - these command buffers block when submitted and meant for setup work
//      or resource loading 
PL_API plCommandBuffer* pl_starter_get_temporary_command_buffer(void);
PL_API void             pl_starter_submit_temporary_command_buffer(plCommandBuffer*);

// raw command buffers
//    - these command buffers serve the same purpose as the temporary
//      command buffers but are NOT yet in a recording state.
PL_API plCommandBuffer* pl_starter_get_raw_command_buffer(void);
PL_API void             pl_starter_return_raw_command_buffer(plCommandBuffer*);

// VSync/MSAA/Depth Buffering
//    - active/deactive MSAA & depth buffers & vsync
//    - this will recreate the swapchain, render pass layouts
//      and render passes, so be sure to recreate any shaders
//      you created using these.
PL_API void pl_starter_activate_msaa          (void);
PL_API void pl_starter_deactivate_msaa        (void);
PL_API void pl_starter_activate_depth_buffer  (void);
PL_API void pl_starter_deactivate_depth_buffer(void);
PL_API void pl_starter_activate_vsync         (void);
PL_API void pl_starter_deactivate_vsync       (void);

// resource retrieval
PL_API plDevice*                pl_starter_get_device                      (void);
PL_API plSwapchain*             pl_starter_get_swapchain                   (void);
PL_API plSurface*               pl_starter_get_surface                     (void);
PL_API plCommandPool*           pl_starter_get_current_command_pool        (void);
PL_API plTimelineSemaphore*     pl_starter_get_current_timeline_semaphore  (void);
PL_API uint64_t                 pl_starter_get_current_timeline_value      (void);
PL_API uint64_t                 pl_starter_increment_current_timeline_value(void);
PL_API plTimelineSemaphore*     pl_starter_get_last_timeline_semaphore     (void);
PL_API uint64_t                 pl_starter_get_last_timeline_value         (void);

// resource setting
PL_API void pl_starter_set_swapchain(plSwapchain*);

// drawing resources
//    - this extension maintains some draw layers you use
//      and the default font
PL_API void           pl_starter_set_default_font    (plFont*);
PL_API plFont*        pl_starter_get_default_font    (void);
PL_API plDrawLayer2D* pl_starter_get_foreground_layer(void);
PL_API plDrawLayer2D* pl_starter_get_background_layer(void);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~helper API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// does the tedious process of enumerating and selecting a GPU by prioritizing
// discrete, then integrated, then CPU.
PL_API plDevice* pl_starter_create_device(plSurface*);

// readback buffers
PL_API void pl_starter_get_readback_buffer   (uint64_t size, plBufferHandle*, const char* name); // allocates/resizes
PL_API void pl_starter_return_readback_buffer(plBufferHandle*);

// staging buffers
PL_API void pl_starter_get_staging_buffer   (uint64_t size, plBufferHandle*, const char* name); // allocates/resizes
PL_API void pl_starter_return_staging_buffer(plBufferHandle*);

// textures (local memory)
PL_API void pl_starter_create_texture(const plTextureDesc*, const void* data, uint64_t size, plTextureHandle*); // data only for 2D textures for now

// buffers (local memory)
PL_API void pl_starter_create_buffer(const plBufferDesc*, const void* data, plBufferHandle*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plStarterI
{

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~high level API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    void (*initialize)(plStarterInit);

    // finalize
    void (*finalize)(void);

    // cleanup
    void (*cleanup)(void);

    // begin frame
    bool (*begin_frame)(void);

    // end frame
    void (*end_frame)  (void);

    // main pass
    plCommandBuffer* (*begin_main_pass)(void);
    void             (*end_main_pass)  (void);

    // resize
    void (*resize)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~mid level API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // command buffers
    plCommandBuffer* (*get_command_buffer)(void);
    void             (*submit_command_buffer)(plCommandBuffer*);

    // temporary command buffers
    plCommandBuffer* (*get_temporary_command_buffer)(void);
    void             (*submit_temporary_command_buffer)(plCommandBuffer*);

    // raw command buffers
    plCommandBuffer* (*get_raw_command_buffer)(void);
    void             (*return_raw_command_buffer)(plCommandBuffer*);

    // VSync/MSAA/Depth Buffering
    void (*activate_msaa)          (void);
    void (*deactivate_msaa)        (void);
    void (*activate_depth_buffer)  (void);
    void (*deactivate_depth_buffer)(void);
    void (*activate_vsync)         (void);
    void (*deactivate_vsync)       (void);

    // resource retrieval
    plDevice*                (*get_device)                      (void);
    plSwapchain*             (*get_swapchain)                   (void);
    plSurface*               (*get_surface)                     (void);
    plCommandPool*           (*get_current_command_pool)        (void);
    plTimelineSemaphore*     (*get_current_timeline_semaphore)  (void);
    uint64_t                 (*get_current_timeline_value)      (void);
    uint64_t                 (*increment_current_timeline_value)(void);
    plTimelineSemaphore*     (*get_last_timeline_semaphore)     (void);
    uint64_t                 (*get_last_timeline_value)         (void);
    void                     (*get_render_attachment_info)      (plRenderAttachmentInfo*);

    // resource setting
    void (*set_swapchain)(plSwapchain*);

    // drawing resources
    void           (*set_default_font)    (plFont*);
    plFont*        (*get_default_font)    (void);
    plDrawLayer2D* (*get_foreground_layer)(void);
    plDrawLayer2D* (*get_background_layer)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~helper API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plDevice* (*create_device)(plSurface*);

    // readback buffer
    void (*get_readback_buffer)   (uint64_t size, plBufferHandle*, const char* name); // allocates/resizes
    void (*return_readback_buffer)(plBufferHandle*);

    // staging buffers
    void (*get_staging_buffer)   (uint64_t size, plBufferHandle*, const char* name); // allocates/resizes
    void (*return_staging_buffer)(plBufferHandle*);

    // textures (local memory)
    void (*create_texture)(const plTextureDesc*, const void* data, uint64_t size, plTextureHandle*); // data only for 2D textures for now

    // buffers (local memory)
    void (*create_buffer)(const plBufferDesc*, const void* data, plBufferHandle*);

} plStarterI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plStarterFlags
{
    PL_STARTER_FLAGS_NONE           = 0,

    // extension
    PL_STARTER_FLAGS_DRAW_EXT       = 1 << 0,
    PL_STARTER_FLAGS_UI_EXT         = 1 << 1,
    PL_STARTER_FLAGS_CONSOLE_EXT    = 1 << 2,
    PL_STARTER_FLAGS_PROFILE_EXT    = 1 << 3,
    PL_STARTER_FLAGS_STATS_EXT      = 1 << 4,
    PL_STARTER_FLAGS_SHADER_EXT     = 1 << 5,
    PL_STARTER_FLAGS_SCREEN_LOG_EXT = 1 << 6,
    PL_STARTER_FLAGS_GRAPHICS_EXT   = 1 << 7,
    PL_STARTER_FLAGS_TOOLS_EXT      = 1 << 8,
    PL_STARTER_FLAGS_RESOURCE_EXT   = 1 << 9,

    // main render pass options
    PL_STARTER_FLAGS_DEPTH_BUFFER = 1 << 10,
    PL_STARTER_FLAGS_MSAA         = 1 << 11,
    PL_STARTER_FLAGS_VSYNC_OFF    = 1 << 12,
    PL_STARTER_FLAGS_REVERSE_Z    = 1 << 13,

    PL_STARTER_FLAGS_ALL_EXTENSIONS = PL_STARTER_FLAGS_SHADER_EXT | PL_STARTER_FLAGS_DRAW_EXT | PL_STARTER_FLAGS_UI_EXT |
                            PL_STARTER_FLAGS_CONSOLE_EXT | PL_STARTER_FLAGS_PROFILE_EXT | PL_STARTER_FLAGS_STATS_EXT |
                            PL_STARTER_FLAGS_SCREEN_LOG_EXT | PL_STARTER_FLAGS_GRAPHICS_EXT | PL_STARTER_FLAGS_TOOLS_EXT |
                            PL_STARTER_FLAGS_RESOURCE_EXT

};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plStarterInit
{
    plStarterFlags eFlags;
    plWindow*      ptWindow;
} plStarterInit;

#ifdef __cplusplus
}
#endif

#endif // PL_STARTER_EXT_H