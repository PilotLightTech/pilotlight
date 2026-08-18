/*
    example_renderer_0.cpp

    Minimal renderer-development example.

    This example intentionally uses the unstable renderer API so the current
    renderer workflow can be inspected without pulling it out of the sandbox
    application. Unlike most Pilot Light examples, it should be treated as a
    living example: it will change as the renderer API is refined.

    It demonstrates:
      - loading the extensions required by the current renderer stack
      - preserving application state across hot reloads
      - initializing the starter, ECS, shader-variant, and renderer systems
      - creating a renderer scene and view
      - loading renderable ECS objects from glTF
      - creating a camera and environment probe
      - preparing and rendering a single view each frame
      - presenting the renderer's output through the starter extension

    IMPORTANT:
        The renderer API used here is unstable by design. This file is intended
        to track that API while it develops rather than serve as a compatibility
        target for released applications.

        You will notice that things are still pretty verbose and many extensions
        required by the renderer extension are managed outside the renderer (
        i.e. shader variant, job, etc. have to be initialized). Many of these
        type things will be addressed obviously.
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helpers
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <string.h> // memset, strncpy
#include "pl.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// stable extensions used directly by this example
#include "pl_graphics_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_platform_ext.h"
#include "pl_starter_ext.h"
#include "pl_vfs_ext.h"
#include "pl_ecs_ext.h"
#include "pl_camera_ext.h"

// unstable extensions used by the current renderer stack
#include "pl_mesh_ext.h"
#include "pl_animation_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_physics_ext.h"
#include "pl_shader_variant_ext.h"
#include "pl_material_ext.h"
#include "pl_script_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;
    bool bResize;

    // The renderer scene references ECS-backed objects stored in this library.
    plComponentLibrary* ptComponentLibrary;
    plScene* ptScene;
    plView* ptView;

    // Keep entity handles instead of raw component pointers so they remain valid
    // across normal ECS updates and can be looked up when needed.
    plEntity tCamera;
    plEntity tProbe;
    plEntity tHelmet;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

// Only APIs used directly by this file are cached here. pl__load_apis() is
// called again after a hot reload because the application code now lives in a
// newly loaded DLL/shared object.
const plWindowI*        gptWindows       = nullptr;
const plGraphicsI*      gptGfx           = nullptr;
const plEcsI*           gptEcs           = nullptr;
const plCameraI*        gptCamera        = nullptr;
const plCameraEcsI*     gptCameraEcs     = nullptr;
const plRendererI*      gptRenderer      = nullptr;
const plRendererEcsI*   gptRendererEcs   = nullptr;
const plModelLoaderI*   gptModelLoader   = nullptr;
const plJobI*           gptJobs          = nullptr;
const plDrawI*          gptDraw          = nullptr;
const plIOI*            gptIO            = nullptr;
const plFileI*          gptFile          = nullptr;
const plStarterI*       gptStarter       = nullptr;
const plAnimationI*     gptAnimation     = nullptr;
const plMeshI*          gptMesh          = nullptr;
const plShaderVariantI* gptShaderVariant = nullptr;
const plVfsI*           gptVfs           = nullptr;
const plMaterialI*      gptMaterial      = nullptr;
const plScriptI*        gptScript        = nullptr;
const plPhysicsI*       gptPhysics       = nullptr;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

static void pl__load_apis(plApiRegistryI* ptApiRegistry);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // A non-null app pointer means the application DLL/shared object was hot
    // reloaded. Persistent application state is already valid, but API pointers
    // must be reacquired for this newly loaded module.
    if(ptAppData)
    {
        pl__load_apis(ptApiRegistry);
        return ptAppData;
    }

    // First load: allocate the persistent state returned to the runtime.
    ptAppData = (plAppData*)malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // Load the extension set used by the current renderer-development stack.
    // The renderer itself is unstable, so this list is intentionally allowed to
    // evolve along with it.
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    ptExtensionRegistry->load("pl_unity_ext", nullptr, nullptr, true);
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    ptExtensionRegistry->load("pl_dear_imgui_ext", "pl_load_dear_imgui_ext", "pl_unload_dear_imgui_ext", false);
    ptExtensionRegistry->load("pl_shader_ext", "pl_load_shader_ext", "pl_unload_shader_ext", true);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", true);

    pl__load_apis(ptApiRegistry);

    // The renderer and model loader use VFS paths rather than depending on the
    // application's physical directory layout.
    gptVfs->mount_directory("/gltf-samples",  "../assets/gltf-samples/Models",     PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/environments", "../assets/development/environments", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shaders",      "../shaders",                         PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shader-temp",  "../shader-temp",                     PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets",       "../assets",                          PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache",        "../cache",                           PL_VFS_MOUNT_FLAGS_NONE);
    gptFile->create_directory("../shader-temp");

    // Create the native window that will own the graphics presentation surface.
    plWindowDesc tWindowDesc = {
        PL_WINDOW_PRESENTATION_MODE_GRAPHICS_API,
        PL_WINDOW_FLAG_NONE,
        "Example Renderer 0",
        500,
        500,
        200,
        200
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    // initialize job extension (used by renderer during culling)
    gptJobs->initialize({});

    // The starter extension is used here only as the thin application/graphics
    // shell: device + swapchain setup, frame lifecycle, and final presentation.
    // The scene/view rendering below is driven directly through plRendererI.
    plStarterInit tStarterInit = {};
    tStarterInit.eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;
    gptStarter->initialize(tStarterInit);
    gptStarter->finalize();

    // setup shader variant extension (used by renderer to manage shaders)
    plShaderVariantInit tShaderVariantInit = {};
    tShaderVariantInit.ptDevice = gptStarter->get_device();
    gptShaderVariant->initialize(tShaderVariantInit);

    // The renderer consumes the graphics device and swapchain but manages its
    // own scene/view resources on top of them.
    plRendererSettings tRendererSettings = {};
    tRendererSettings.ptDevice    = gptStarter->get_device();
    tRendererSettings.ptSwapchain = gptStarter->get_swapchain();
    gptRenderer->initialize(&tRendererSettings);

    // Register every ECS system required by the objects used in this example
    // before finalizing the component library.
    gptEcs->initialize({});
    gptRendererEcs->register_system();
    gptScript->register_ecs_system();
    gptAnimation->register_ecs_system();
    gptCameraEcs->register_ecs_system();
    gptMesh->register_ecs_system();
    gptPhysics->register_ecs_system();
    gptMaterial->register_ecs_system();
    gptEcs->finalize();

    ptAppData->ptComponentLibrary = gptEcs->get_default_library(); // can only be called after "gptEcs->finalize()"

    // A scene owns renderer-wide state; views represent individual rendered
    // outputs from that scene (for example a main viewport or editor viewport).
    plSceneDesc tSceneDesc = {};
    tSceneDesc.ptComponentLibrary = ptAppData->ptComponentLibrary;
    ptAppData->ptScene = gptRenderer->create_scene(&tSceneDesc);

    plIO* ptIO = gptIO->get_io();

    plViewDesc tViewDesc = {};
    tViewDesc.uWidth  = (uint32_t)ptIO->tMainViewportSize.x;
    tViewDesc.uHeight = (uint32_t)ptIO->tMainViewportSize.y;
    ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc);

    // The camera is an ECS object so scripts and other systems can update it in
    // the same way as the rest of the scene.
    plCameraPerspectiveDesc tCameraDesc = {};
    tCameraDesc.eDepthMode    = PL_CAMERA_DEPTH_MODE_REVERSE_Z; // renderer uses reverse z depth buffers
    tCameraDesc.fNearZ        = 0.1f;
    tCameraDesc.fFarZ         = 75.0f;
    tCameraDesc.fYFov         = PL_PI_3;
    tCameraDesc.fAspectRatio  = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;

    plCamera* ptCamera = nullptr;
    ptAppData->tCamera = gptCameraEcs->create_perspective(
        ptAppData->ptComponentLibrary,
        "main camera",
        &tCameraDesc,
        &ptCamera); // do not store output, use immediately

    gptCamera->set_position(ptCamera, {-2.539, 3.003, 2.640});
    gptCamera->set_euler(ptCamera, -0.295f, 2.436f, 0.0f);
    gptCamera->update(ptCamera);

    // Reuse the normal camera-control script so the example remains interactive. (using Unreal camera controls)
    gptScript->attach(ptAppData->ptComponentLibrary, "pl_script_camera",
        PL_SCRIPT_FLAG_PLAYING | PL_SCRIPT_FLAG_RELOADABLE, ptAppData->tCamera, nullptr);

    // Populate the ECS library, then explicitly register those drawable objects
    // with the renderer scene.

    // Unfortunately for this example, the model loader extension is handling alot of the raw ECS stuff
    // you should really understand, but until we have a better asset system that makes it easier to
    // construct the scene, its easiest to use the model loader for now, which will properly create
    // the objects and all the various components (i.e. animation, skinning, materials, etc.)
    
    // helmet model
    {
        plMat4 tHelmetTransform = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
        // The model loader creates/populates ECS objects. The renderer scene still
        // needs to be told which of those objects should participate in rendering.
        plModelInstanceHandle tHandle = gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, "/assets/core/models/gltf/DamagedHelmet.glb", &tHelmetTransform);

        const plModelLoaderData* ptLoaderData = gptModelLoader->get_objects(tHandle);
        gptRendererEcs->add_drawable_objects_to_scene(ptAppData->ptScene, ptLoaderData->uObjectCount, ptLoaderData->atObjects);

        gptModelLoader->get_node_by_path(tHandle, "/load transform/node_damagedHelmet_-6514", &ptAppData->tHelmet);

        // The ECS objects are now owned by the component library/scene relationship;
        // the model loader's temporary result data is no longer needed.
        gptModelLoader->free_data(tHandle);
    }

    // floor model
    {
        // The model loader creates/populates ECS objects. The renderer scene still
        // needs to be told which of those objects should participate in rendering.
        plModelInstanceHandle tHandle = gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, "/assets/core/models/gltf/floor.gltf", nullptr);

        const plModelLoaderData* ptLoaderData = gptModelLoader->get_objects(tHandle);
        gptRendererEcs->add_drawable_objects_to_scene(ptAppData->ptScene, ptLoaderData->uObjectCount, ptLoaderData->atObjects);

        // The ECS objects are now owned by the component library/scene relationship;
        // the model loader's temporary result data is no longer needed.
        gptModelLoader->free_data(tHandle);
    }

    // Environment probes are also ECS objects. This one captures the sky and is
    // added explicitly to the renderer scene just like drawable objects above.
    plEnvironmentProbeComponent* ptProbe = nullptr;
    ptAppData->tProbe = gptRendererEcs->create_environment_probe(
        ptAppData->ptComponentLibrary,
        "Probe",
        {0.0f, 5.0f, 0.0f},
        &ptProbe);
    ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
    gptRendererEcs->add_probes_to_scene(ptAppData->ptScene, 1, &ptAppData->tProbe);

    // Configure a simple HDR skybox. Marking the skybox dirty tells the renderer
    // that the backing environment data needs to be rebuilt.
    plRendererSkyOptions tSkyOptions = {};
    gptRenderer->get_sky_options(ptAppData->ptScene, &tSkyOptions);
    strncpy(tSkyOptions.acSkyboxPath, "/environments/sky.hdr", sizeof(tSkyOptions.acSkyboxPath) - 1);
    tSkyOptions.tMode = PL_RENDERER_SKY_MODE_SKYBOX; // skybox instead of realistic sky renderer
    tSkyOptions.tFlags |= PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY;
    tSkyOptions.tSunDirection = {0.0f, -1.0f, 0.0f};
    gptRenderer->set_sky_options(ptAppData->ptScene, &tSkyOptions);

    // In addition to the main "sky light", you can also add additional lights (direction, spot, point) using the ECS
    // extension

    // NOTES:
    //   Many of the options are handled like the above "get_sky_options/set_sky_options" pattern. This includes:
    //     * fog options
    //     * shadow options
    //     * lighting options
    //     * bloom options
    //     * tonemap options
    //   Some of these are per view, per scene, or per renderer (eventually I'll make these more consistent)

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // Stop producers of renderer work before flushing/destroying GPU resources.
    gptJobs->cleanup();

    plDevice* ptDevice = gptStarter->get_device();
    gptGfx->flush_device(ptDevice);

    // Destroy resources in roughly the reverse order they were initialized.
    gptRenderer->destroy_view(ptAppData->ptView);
    gptRenderer->destroy_scene(ptAppData->ptScene);
    gptEcs->cleanup();
    gptRenderer->cleanup();
    gptShaderVariant->cleanup();
    gptStarter->cleanup();
    
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    (void)ptWindow;

    // Starter handles the presentation resources immediately. The renderer view
    // is resized at the beginning of the next valid frame, once the final
    // viewport dimensions are available through plIO.
    gptStarter->resize();
    ptAppData->bResize = true;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // begin_frame() may fail/skip while presentation resources are unavailable
    // (for example during a resize), so do not advance renderer work in that case.
    if(!gptStarter->begin_frame())
        return;

    plIO* ptIO = gptIO->get_io();

    gptRenderer->begin_frame();

    if(ptAppData->bResize)
    {
        gptRenderer->resize_view(ptAppData->ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // NOTES:
    //   Unlike most of the Pilot Light extensions, the renderer is not immediate mode.
    //   You interact with the things in the scene through the entity component system (see below).
    //
    //   At the moment its a little inconsistent on which things you can do at runtime. Right now you
    //   shouldn't try adding new objects or anything like that once the scene is created. This use to be
    //   possible and will be again ASAP but after some internal refactors, that hasn't been setup again.
    //   It is a priority though if you intend on having any editor type application.

    // Keep the camera projection in sync with the renderer view.
    plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tCamera); // don't store
    gptCamera->set_viewport(ptCamera, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);
    gptCamera->update(ptCamera);

    // update the helmet transform component
    plTransformComponent* ptHelmetTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), ptAppData->tHelmet); // don't store
    static float fRotation = 0.0f;
    fRotation += ptIO->fDeltaTime;
    ptHelmetTransform->tRotation = pl_quat_rotation(fRotation, 0.0f, 1.0f, 0.0f);
    ptHelmetTransform->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;

    // Update ECS state before asking the renderer to consume it. The ordering is
    // intentionally explicit because several renderer-facing systems depend on
    // results produced by earlier systems (probe update, for example, follows
    // the object update).
    //
    // Eventually these may be "tucked away" into the renderer extension but in many
    // cases I've found it useful to insert things between different systems running
    // and if this was tucked away... that would be more difficult. Sure we can add
    // a system to inject things at different points & eventually we will have that
    // but for now I've found it easier to just make this explicit.
    gptScript->run_update_system(ptAppData->ptComponentLibrary);
    gptAnimation->run_animation_update_system(ptAppData->ptComponentLibrary, ptIO->fDeltaTime);
    gptPhysics->update(ptIO->fDeltaTime, ptAppData->ptComponentLibrary);
    gptEcs->run_transform_update_system(ptAppData->ptComponentLibrary);
    gptEcs->run_hierarchy_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_light_update_system(ptAppData->ptComponentLibrary);
    gptCameraEcs->run_ecs(ptAppData->ptComponentLibrary);
    gptAnimation->run_inverse_kinematics_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_skin_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_object_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_environment_probe_update_system(ptAppData->ptComponentLibrary);

    // Scene preparation performs scene-wide work shared by all views. Supplying
    // the active cameras here lets that work account for every view that will be
    // rendered this frame.
    const plCamera* atCameras[] = {ptCamera};
    gptRenderer->prepare_scene(ptAppData->ptScene, atCameras, 1);

    // View preparation/rendering is per-output. The same camera is used for both
    // rendering and culling in this simple single-view example.
    plRenderViewDesc tRenderViewDesc = {};
    tRenderViewDesc.ptCamera     = ptCamera;
    tRenderViewDesc.ptCullCamera = ptCamera;

    gptRenderer->prepare_view(ptAppData->ptView, ptCamera);
    gptRenderer->render_view(ptAppData->ptView, &tRenderViewDesc);

    // The renderer produces an offscreen color image. Add that image to the
    // starter background layer so the starter's main pass copies/composites it
    // into the presentation target.
    plVec2 tUVMax = {};
    plBindGroupHandle tViewColor = gptRenderer->get_view_color_bind_group(ptAppData->ptView, &tUVMax);
    gptDraw->add_image_ex(gptStarter->get_background_layer(), tViewColor.uData,
        {0.0f, 0.0f}, ptIO->tMainViewportSize, {0.0f, 0.0f}, tUVMax, PL_COLOR_32_WHITE);

    // No explicit draw commands are needed here: the starter renders its draw
    // layers (including the renderer output above) as part of the main pass.
    gptStarter->begin_main_pass();
    gptStarter->end_main_pass();

    // Must be the final starter call for the frame; handles submission/present.
    gptStarter->end_frame();
}

static void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptCameraEcs     = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
    gptRenderer      = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptRendererEcs   = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
    gptModelLoader   = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptMaterial      = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript        = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
}
