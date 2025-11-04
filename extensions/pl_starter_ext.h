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
// [SECTION] public api structs
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

        * plGraphicsI    (v1.x)
        * plScreenLogI   (v2.x)
        * plUiI          (v1.x)
        * plDrawBackendI (v1.x)
        * plShaderI      (v1.x)
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

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plStarterI_version {1, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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
typedef struct _plWindow               plWindow;                 // pl.h
typedef struct _plFont                 plFont;                   // pl_draw_ext.h
typedef struct _plDrawLayer2D          plDrawLayer2D;            // pl_draw_ext.h
typedef struct _plDevice               plDevice;                 // pl_graphics_ext.h
typedef struct _plCommandPool          plCommandPool;            // pl_graphics_ext.h
typedef struct _plTimelineSemaphore    plTimelineSemaphore;      // pl_graphics_ext.h
typedef struct _plSurface              plSurface;                // pl_graphics_ext.h
typedef struct _plRenderEncoder        plRenderEncoder;          // pl_graphics_ext.h
typedef struct _plSwapchain            plSwapchain;              // pl_graphics_ext.h
typedef struct _plCommandBuffer        plCommandBuffer;          // pl_graphics_ext.h
typedef struct _plBlitEncoder          plBlitEncoder;            // pl_graphics_ext.h
typedef union plRenderPassHandle       plRenderPassHandle;       // pl_graphics_ext.h
typedef union plRenderPassLayoutHandle plRenderPassLayoutHandle; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plStarterI
{

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~high level API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    //   - this will setup extensions that are included in the init flags and handle
    //     other tasks related to setup
    void (*initialize)(plStarterInit);

    // finalize
    //   - if you decide to handle any extension setup, calling this function after
    //     gives this extension a change to finish setup that relies on those
    //     extensions being initialized
    void (*finalize)(void);

    // cleanup
    //   - just handles all the cleanup need by the managed extensions
    void (*cleanup)(void);

    // begin frame
    //   - this absolutely must be called first inside "pl_app_update"
    //   - handles most of the "begin frame" calls & main swapchain stuff
    bool (*begin_frame)(void);

    // end frame
    //   - this absolutely must be called last inside "pl_app_update"
    //   - handles most of the "end frame" calls & will call main
    //     pass begin/end if you don't
    void (*end_frame)  (void);

    // main pass
    //   - if you need the encoder to submit your own work, use this
    //     and make sure to call "end_main_pass" before ending the frame
    plRenderEncoder* (*begin_main_pass)(void);
    void             (*end_main_pass)  (void);

    // resize
    //   - call inside "pl_app_resize" to have swapchain recreation handled
    void (*resize)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~mid level API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // command buffers
    //    - these command buffers are already in a recording state and
    //      syncronized with subsequent calls/main pass with the internal
    //      timeline semaphores. Use between "begin_frame" and before "begin_main_pass"
    //    - They do NOT block.
    plCommandBuffer* (*get_command_buffer)(void);
    void             (*submit_command_buffer)(plCommandBuffer*);

    // temporary command buffers
    //    - these command buffers are already in a recording state but are not
    //      syncronized.
    //    - these command buffers block when submitted and meant for setup work
    //      or resource loading 
    plCommandBuffer* (*get_temporary_command_buffer)(void);
    void             (*submit_temporary_command_buffer)(plCommandBuffer*);

    // raw command buffers
    //    - these command buffers serve the same purpose as the temporary
    //      command buffers but are NOT yet in a recording state.
    plCommandBuffer* (*get_raw_command_buffer)(void);
    void             (*return_raw_command_buffer)(plCommandBuffer*);

    // blit encoder
    //    - like the temporary command buffers above, this is just to
    //      help with resource loading. It handles the command buffer
    //      for you and blocks.
    plBlitEncoder* (*get_blit_encoder)(void);
    void           (*return_blit_encoder)(plBlitEncoder*);

    // VSync/MSAA/Depth Buffering
    //    - active/deactive MSAA & depth buffers & vsync
    //    - this will recreate the swapchain, render pass layouts
    //      and render passes, so be sure to recreate any shaders
    //      you created using these.
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
    plRenderPassHandle       (*get_render_pass)                 (void);
    plRenderPassLayoutHandle (*get_render_pass_layout)          (void);
    plCommandPool*           (*get_current_command_pool)        (void);
    plTimelineSemaphore*     (*get_current_timeline_semaphore)  (void);
    uint64_t                 (*get_current_timeline_value)      (void);
    uint64_t                 (*increment_current_timeline_value)(void);
    plTimelineSemaphore*     (*get_last_timeline_semaphore)     (void);
    uint64_t                 (*get_last_timeline_value)         (void);

    // drawing resources
    //    - this extension maintains some draw layers you use
    //      and the default font
    void           (*set_default_font)    (plFont*);
    plFont*        (*get_default_font)    (void);
    plDrawLayer2D* (*get_foreground_layer)(void);
    plDrawLayer2D* (*get_background_layer)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~helper API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // does the tedious process of enumerating and selecting a GPU by prioritizing
    // discrete, then integrated, then CPU.
    plDevice* (*create_device)(plSurface*);

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

    // main render pass options
    PL_STARTER_FLAGS_DEPTH_BUFFER = 1 << 9,
    PL_STARTER_FLAGS_MSAA         = 1 << 10,
    PL_STARTER_FLAGS_VSYNC_OFF    = 1 << 11,
    PL_STARTER_FLAGS_REVERSE_Z    = 1 << 12,

    PL_STARTER_FLAGS_ALL_EXTENSIONS = PL_STARTER_FLAGS_SHADER_EXT | PL_STARTER_FLAGS_DRAW_EXT | PL_STARTER_FLAGS_UI_EXT |
                            PL_STARTER_FLAGS_CONSOLE_EXT | PL_STARTER_FLAGS_PROFILE_EXT | PL_STARTER_FLAGS_STATS_EXT |
                            PL_STARTER_FLAGS_SCREEN_LOG_EXT | PL_STARTER_FLAGS_GRAPHICS_EXT | PL_STARTER_FLAGS_TOOLS_EXT

};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plStarterInit
{
    plStarterFlags tFlags;
    plWindow*      ptWindow;
} plStarterInit;

#endif // PL_STARTER_EXT_H