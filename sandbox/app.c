/*
   editor.c

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * look at examples for more stable APIs
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "app.h"

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plEditorData* ptEditorData)
{
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(ptEditorData) // reload
    {

        // reload global apis
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDebug       = pl_get_api_latest(ptApiRegistry, plDebugApiI);
        gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptJobs        = pl_get_api_latest(ptApiRegistry, plJobI);
        gptModelLoader = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptNetwork     = pl_get_api_latest(ptApiRegistry, plNetworkI);
        gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);

        return ptEditorData;
    }

    // load extensions
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    
    // load apis
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDebug       = pl_get_api_latest(ptApiRegistry, plDebugApiI);
    gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs        = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork     = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);

    gptProfile->begin_frame();
    
    // add some context to data registry
    ptEditorData = PL_ALLOC(sizeof(plEditorData));
    memset(ptEditorData, 0, sizeof(plEditorData));
    ptEditorData->tSelectedEntity.ulData = UINT64_MAX;

    // initialize shader extension
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        #ifndef PL_OFFLINE_SHADERS_ONLY
        .tFlags = PL_SHADER_FLAGS_ALWAYS_COMPILE
        #endif
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // initialize gizmo stuff
    ptEditorData->ptGizmoData = pl_initialize_gizmo_data();

    // initialize job system
    gptJobs->initialize(0);

    plWindowDesc tWindowDesc = {
        .pcTitle = "Pilot Light Sandbox",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptEditorData->ptWindow);

    plIO* ptIO = gptIO->get_io();

    // setup reference renderer
    gptRenderer->initialize(ptEditorData->ptWindow);
    ptEditorData->ptSwap = gptRenderer->get_swapchain();

    // setup draw
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(gptRenderer->get_device());

    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();

    plFontRange tFontRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    plFontConfig tFontConfig0 = {
        .bSdf = false,
        .fSize = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptRanges = &tFontRange,
        .uRangeCount = 1
    };
    // ptEditorData->tDefaultFont = gptDraw->add_default_font();

    ptEditorData->tDefaultFont = gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    const plFontRange tIconRange = {
        .iFirstCodePoint = ICON_MIN_FA,
        .uCharCount = ICON_MAX_16_FA - ICON_MIN_FA
    };

    plFontConfig tFontConfig1 = {
        .bSdf           = false,
        .fSize          = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptMergeFont    = ptEditorData->tDefaultFont,
        .ptRanges       = &tIconRange,
        .uRangeCount    = 1
    };
    gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig1, "../data/pilotlight-assets-master/fonts/fa-solid-900.otf");

    plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(gptRenderer->get_command_pool());
    gptDrawBackend->build_font_atlas(ptCmdBuffer, ptAtlas);
    gptGfx->return_command_buffer(ptCmdBuffer);
    gptDraw->set_font_atlas(ptAtlas);

    // setup ui
    gptUi->initialize();
    gptUi->set_default_font(ptEditorData->tDefaultFont);

    ptEditorData->uSceneHandle0 = gptRenderer->create_scene();

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptEditorData->ptDrawLayer = gptDraw->request_2d_layer(gptUi->get_draw_list());

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptEditorData->uSceneHandle0);

    // create main camera
    plCameraComponent* ptMainCamera = NULL;
    ptEditorData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", (plVec3){-9.6f, 2.096f, 0.86f}, PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 48.0f, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, -0.245f, 1.816f);
    gptCamera->update(ptMainCamera);
    #ifdef PL_CONFIG_DEBUG
        gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camerad", PL_SCRIPT_FLAG_PLAYING, ptEditorData->tMainCamera, NULL);
    #else
        gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING, ptEditorData->tMainCamera, NULL);
    #endif

    // create cull camera
    plCameraComponent* ptCullCamera = NULL;
    ptEditorData->tCullCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "cull camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 25.0f, &ptCullCamera);
    gptCamera->set_pitch_yaw(ptCullCamera, 0.0f, PL_PI);
    gptCamera->update(ptCullCamera);

    // create lights
    gptEcs->create_point_light(ptMainComponentLibrary, "light", (plVec3){6.0f, 4.0f, -3.0f}, NULL);

    plLightComponent* ptLight = NULL;
    ptEditorData->tSunlight = gptEcs->create_directional_light(ptMainComponentLibrary, "sunlight", (plVec3){-0.375f, -1.0f, -0.085f}, &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW;

    // load models


    // plTransformComponent* ptTargetTransform = NULL;
    // ptEditorData->tTrackPoint = gptEcs->create_transform(ptMainComponentLibrary, "track 0", &ptTargetTransform);
    // ptTargetTransform->tTranslation = (plVec3){0.1f, 0.017f};

    // plHumanoidComponent* ptHuman =  gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_HUMANOID, (plEntity){.uIndex = 6});

    // plInverseKinematicsComponent* ptIK = gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_INVERSE_KINEMATICS, ptHuman->atBones[PL_HUMANOID_BONE_LEFT_FOOT]);
    // ptIK->bEnabled = true;
    // ptIK->tTarget = ptEditorData->tTrackPoint;
    // ptIK->uChainLength = 2;
    // ptIK->uIterationCount = 5;

    return ptEditorData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plEditorData* ptEditorData)
{
    gptJobs->cleanup();
    // ensure GPU is finished before cleanup
    gptGfx->flush_device(gptRenderer->get_device());
    gptDrawBackend->cleanup_font_atlas(gptDraw->get_current_font_atlas());
    gptUi->cleanup();
    gptDrawBackend->cleanup();
    gptRenderer->cleanup();
    gptWindows->destroy_window(ptEditorData->ptWindow);
    PL_FREE(ptEditorData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plEditorData* ptEditorData)
{
    plIO* ptIO = gptIO->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = gptGfx->get_swapchain_info(ptEditorData->ptSwap).bVSync,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y,
        .tSampleCount = gptGfx->get_swapchain_info(ptEditorData->ptSwap).tSampleCount,
    };
    gptGfx->recreate_swapchain(ptEditorData->ptSwap, &tDesc);
    gptCamera->set_aspect(gptEcs->get_component(gptRenderer->get_component_library(ptEditorData->uSceneHandle0), PL_COMPONENT_TYPE_CAMERA, ptEditorData->tMainCamera), ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y);
    ptEditorData->bResize = true;
    gptRenderer->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plEditorData* ptEditorData)
{
    // begin profiling frame
    gptProfile->begin_frame();
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    gptIO->new_frame();

    // for convience
    plIO* ptIO = gptIO->get_io();

    if(ptEditorData->bResize || ptEditorData->bAlwaysResize)
    {
        // gptOS->sleep(32);
        if(ptEditorData->bSceneLoaded)
            gptRenderer->resize_view(ptEditorData->uSceneHandle0, ptEditorData->uViewHandle0, ptIO->tMainViewportSize);
        ptEditorData->bResize = false;
    }

    if(!gptRenderer->begin_frame())
    {
        pl_end_cpu_sample(gptProfile, 0);
        gptProfile->end_frame();
        return;
    }

    gptDrawBackend->new_frame();
    gptUi->new_frame();

    // update statistics
    gptStats->new_frame();
    static double* pdFrameTimeCounter = NULL;
    static double* pdMemoryCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frametime (ms)");
    if(!pdMemoryCounter)
        pdMemoryCounter = gptStats->get_counter("CPU memory");
    *pdFrameTimeCounter = (double)ptIO->fDeltaTime * 1000.0;
    *pdMemoryCounter = (double)gptMemory->get_memory_usage();

    // handle input
    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptEditorData->uSceneHandle0);

    plCameraComponent* ptCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptEditorData->tMainCamera);
    plCameraComponent* ptCullCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptEditorData->tCullCamera);
    gptCamera->update(ptCullCamera);

    if(ptEditorData->bSceneLoaded)
    {
        // run ecs system
        gptRenderer->run_ecs(ptEditorData->uSceneHandle0);


        plEntity tNextEntity = gptRenderer->get_picked_entity();
        if(tNextEntity.ulData == 0)
        {
            ptEditorData->tSelectedEntity.ulData = UINT64_MAX;
            gptRenderer->select_entities(ptEditorData->uSceneHandle0, 0, NULL);
        }
        else if(tNextEntity.ulData != UINT64_MAX && ptEditorData->tSelectedEntity.ulData != tNextEntity.ulData)
        {
            ptEditorData->tSelectedEntity = tNextEntity;
            gptRenderer->select_entities(ptEditorData->uSceneHandle0, 1, &ptEditorData->tSelectedEntity);
        }

        if(gptIO->is_key_pressed(PL_KEY_M, true))
            pl_change_gizmo_mode(ptEditorData->ptGizmoData);

        if(ptEditorData->bShowEntityWindow)
        {
            if(pl_show_ecs_window(&ptEditorData->tSelectedEntity, ptMainComponentLibrary, &ptEditorData->bShowEntityWindow))
            {
                if(ptEditorData->tSelectedEntity.ulData == UINT64_MAX)
                    gptRenderer->select_entities(ptEditorData->uSceneHandle0, 0, NULL);
                else
                    gptRenderer->select_entities(ptEditorData->uSceneHandle0, 1, &ptEditorData->tSelectedEntity);
            }
        }

        if(ptEditorData->tSelectedEntity.uIndex != UINT32_MAX)
        {
            plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptEditorData->uSceneHandle0, ptEditorData->uViewHandle0);
            pl_gizmo(ptEditorData->ptGizmoData, ptGizmoDrawlist, ptMainComponentLibrary, ptCamera, ptEditorData->tSelectedEntity);
        }
    
        const plViewOptions tViewOptions = {
            .ptViewCamera = &ptEditorData->tMainCamera,
            .ptCullCamera = ptEditorData->bFreezeCullCamera ? &ptEditorData->tCullCamera : NULL,
            .ptSunLight = &ptEditorData->tSunlight
        };
        gptRenderer->render_scene(ptEditorData->uSceneHandle0, ptEditorData->uViewHandle0, tViewOptions);
    }

    gptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(gptUi->begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {

        const float pfRatios[] = {1.0f};
        const float pfRatios2[] = {0.5f, 0.5f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->begin_collapsing_header(ICON_FA_CIRCLE_INFO " Information", 0))
        {
            gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_METAL_BACKEND
            gptUi->text("Graphics Backend: Metal");
            #elif PL_VULKAN_BACKEND
            gptUi->text("Graphics Backend: Vulkan");
            #else
            gptUi->text("Graphics Backend: Unknown");
            #endif
            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header(ICON_FA_SLIDERS " App Options", 0))
        {
            if(gptUi->checkbox("Freeze Culling Camera", &ptEditorData->bFreezeCullCamera))
            {
                *ptCullCamera = *ptCamera;
            }

            gptUi->end_collapsing_header();
        }
        
        gptRenderer->show_graphics_options(ICON_FA_DICE_D6 " Graphics");

        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);
        if(gptUi->begin_collapsing_header(ICON_FA_SCREWDRIVER_WRENCH " Tools", 0))
        {
            gptUi->checkbox("Device Memory Analyzer", &ptEditorData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            gptUi->checkbox("Memory Allocations", &ptEditorData->tDebugInfo.bShowMemoryAllocations);
            gptUi->checkbox("Profiling", &ptEditorData->tDebugInfo.bShowProfiling);
            gptUi->checkbox("Statistics", &ptEditorData->tDebugInfo.bShowStats);
            gptUi->checkbox("Logging", &ptEditorData->tDebugInfo.bShowLogging);
            gptUi->checkbox("Entities", &ptEditorData->bShowEntityWindow);
            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header(ICON_FA_USER_GEAR " User Interface", 0))
        {
            gptUi->checkbox("UI Debug", &ptEditorData->bShowUiDebug);
            gptUi->checkbox("UI Style", &ptEditorData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }

        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->begin_collapsing_header(ICON_FA_BUG " Debug", 0))
        {
            if(gptUi->button("resize"))
                ptEditorData->bResize = true;
            gptUi->checkbox("Always Resize", &ptEditorData->bAlwaysResize);

            plLightComponent* ptLight = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_LIGHT, ptEditorData->tSunlight);
            gptUi->slider_float("x", &ptLight->tDirection.x, -1.0f, 1.0f, 0);
            gptUi->slider_float("y", &ptLight->tDirection.y, -1.0f, 1.0f, 0);
            gptUi->slider_float("z", &ptLight->tDirection.z, -1.0f, 1.0f, 0);
            gptUi->end_collapsing_header();
        }

        if(gptUi->begin_collapsing_header(ICON_FA_PHOTO_FILM " Renderer", 0))
        {

            plLightComponent* ptLight = gptEcs->get_component(ptMainComponentLibrary,  PL_COMPONENT_TYPE_LIGHT, ptEditorData->tSunlight);
            int iCascadeCount  = (int)ptLight->uCascadeCount;
            if(gptUi->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0))
            {
                ptLight->uCascadeCount = (uint32_t)iCascadeCount;
            }

            if(gptUi->button("Reload Shaders"))
            {
                gptRenderer->reload_scene_shaders(ptEditorData->uSceneHandle0);
            }

            if(!ptEditorData->bSceneLoaded)
            {
                if(gptUi->button("Load Sponza"))
                {
                    gptProfile->begin_sample(0, "load environments");
                    gptRenderer->load_skybox_from_panorama(ptEditorData->uSceneHandle0, "../data/pilotlight-assets-master/environments/helipad.hdr", 256);
                    gptProfile->end_sample(0);

                    gptProfile->begin_sample(0, "create scene views");
                    ptEditorData->uViewHandle0 = gptRenderer->create_view(ptEditorData->uSceneHandle0, ptIO->tMainViewportSize);
                    gptProfile->end_sample(0);

                    plModelLoaderData tLoaderData0 = {0};

                    gptProfile->begin_sample(0, "load models 0");
                    // const plMat4 tTransform = pl_mat4_translate_xyz(0.0f, 1.0f, 0.0f);
                    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", &tTransform, &tLoaderData0);
                    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/Sponza/glTF/Sponza.gltf", NULL, &tLoaderData0);
                    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/NormalTangentTest/glTF/NormalTangentTest.gltf", NULL, &tLoaderData0);
                    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/pilotlight-assets-master/models/gltf/humanoid/floor.gltf", NULL, &tLoaderData0);
                    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/pilotlight-assets-master/models/gltf/humanoid/model.gltf", NULL, &tLoaderData0);
                    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/kenny.glb", NULL, &tLoaderData0);
                    gptRenderer->add_drawable_objects_to_scene(ptEditorData->uSceneHandle0, tLoaderData0.uDeferredCount, tLoaderData0.atDeferredObjects, tLoaderData0.uForwardCount, tLoaderData0.atForwardObjects);
                    gptModelLoader->free_data(&tLoaderData0);
                    gptProfile->end_sample(0);

                    gptProfile->begin_sample(0, "finalize scene 0");
                    gptRenderer->finalize_scene(ptEditorData->uSceneHandle0);
                    gptProfile->end_sample(0);

                    ptEditorData->bSceneLoaded = true;
                }

                if(gptUi->button("Load Dance Floor"))
                {
                    gptProfile->begin_sample(0, "load environments");
                    gptRenderer->load_skybox_from_panorama(ptEditorData->uSceneHandle0, "../data/pilotlight-assets-master/environments/helipad.hdr", 256);
                    gptProfile->end_sample(0);

                    gptProfile->begin_sample(0, "create scene views");
                    ptEditorData->uViewHandle0 = gptRenderer->create_view(ptEditorData->uSceneHandle0, ptIO->tMainViewportSize);
                    gptProfile->end_sample(0);

                    plModelLoaderData tLoaderData0 = {0};

                    gptProfile->begin_sample(0, "load models 0");
                    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/pilotlight-assets-master/models/gltf/humanoid/floor.gltf", NULL, &tLoaderData0);
                    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/pilotlight-assets-master/models/gltf/humanoid/model.gltf", NULL, &tLoaderData0);
                    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/kenny.glb", NULL, &tLoaderData0);
                    gptRenderer->add_drawable_objects_to_scene(ptEditorData->uSceneHandle0, tLoaderData0.uDeferredCount, tLoaderData0.atDeferredObjects, tLoaderData0.uForwardCount, tLoaderData0.atForwardObjects);
                    gptModelLoader->free_data(&tLoaderData0);
                    gptProfile->end_sample(0);

                    gptProfile->begin_sample(0, "finalize scene 0");
                    gptRenderer->finalize_scene(ptEditorData->uSceneHandle0);
                    gptProfile->end_sample(0);

                    ptEditorData->bSceneLoaded = true;
                }
            }

            gptUi->end_collapsing_header();
        }


        gptUi->end_window();
    }

    gptDebug->show_debug_windows(&ptEditorData->tDebugInfo);
        
    if(ptEditorData->bShowUiStyle)
        gptUi->show_style_editor_window(&ptEditorData->bShowUiStyle);

    if(ptEditorData->bShowUiDebug)
        gptUi->show_debug_window(&ptEditorData->bShowUiDebug);

    // add full screen quad for offscreen render
    if(ptEditorData->bSceneLoaded)
        gptDraw->add_image(ptEditorData->ptDrawLayer, gptRenderer->get_view_color_texture(ptEditorData->uSceneHandle0, ptEditorData->uViewHandle0).uData, (plVec2){0}, ptIO->tMainViewportSize);

    gptDraw->submit_2d_layer(ptEditorData->ptDrawLayer);

    gptRenderer->end_frame();

    pl_end_cpu_sample(gptProfile, 0);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_gizmo.c"
#include "pl_ecs_tools.c"

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif