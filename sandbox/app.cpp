/*
   app.cpp

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * probably better to look at the examples
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper implementations
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
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "ptAppData" will be nullptr but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);



    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData) // reload
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        pl__load_apis(ptApiRegistry);

        ImGuiContext* ptImguiContext = (ImGuiContext*)ptDataRegistry->get_data("imgui");
        ImGui::SetCurrentContext(ptImguiContext);

        ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)ptDataRegistry->get_data("imgui allocate");
        ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)ptDataRegistry->get_data("imgui free");
        ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);

        ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", nullptr, nullptr, true);
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    ptExtensionRegistry->load("pl_dear_imgui_ext", "pl_load_dear_imgui_ext", "pl_unload_dear_imgui_ext", false);
    ptExtensionRegistry->load("pl_shader_ext", "pl_load_shader_ext", "pl_unload_shader_ext", true);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", true);

    // load apis
    pl__load_apis(ptApiRegistry);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset((void*)ptAppData, 0, sizeof(plAppData));

    gptVfs->mount_directory("/gltf-samples", "../assets/gltf-samples/Models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/environments", "../assets/development/environments", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shader-temp", "../shader-temp", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets", "../assets", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache", "../cache", PL_VFS_MOUNT_FLAGS_NONE);
    gptFile->create_directory("../shader-temp");
    gptFile->create_directory("../shader-temp");

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;
    ptAppData->bVSync = true;
    ptAppData->iSelectedSceneCore = -1;
    ptAppData->iSelectedSceneDev = -1;
    ptAppData->iSelectedSceneUser = -1;
    ptAppData->iSelectedEnvironment = 0;

    gptConfig->load_from_disk(nullptr);
    ptAppData->bShowEntityWindow = gptConfig->load_bool("bShowEntityWindow", false);
    ptAppData->bPhysicsDebugDraw = gptConfig->load_bool("bPhysicsDebugDraw", false);

    // add console variables
    gptConsole->initialize({PL_CONSOLE_FLAGS_POPUP});
    gptConsole->add_toggle_variable("a.Entities", &ptAppData->bShowEntityWindow, "shows ecs tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

    // initialize APIs that require it
    gptEcsTools->initialize();
    gptPhysics->initialize({});

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        PL_WINDOW_PRESENTATION_MODE_GRAPHICS_API,
        PL_WINDOW_FLAG_NONE,
        "Pilot Light Sandbox",
        1500,
        900,
        50,
        50
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plFullScreenDesc tFullScreen = {};
    tFullScreen.iMonitor = 0;
    // tFullScreen.tMode = PL_FULLSCREEN_MODE_EXCLUSIVE;
    gptWindows->set_fullscreen(ptAppData->ptWindow, &tFullScreen);

    plStarterInit tStarterInit = {};
    tStarterInit.eFlags   = PL_STARTER_FLAGS_NONE;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // extensions handled by starter
    tStarterInit.eFlags |= PL_STARTER_FLAGS_GRAPHICS_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_PROFILE_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_STATS_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_CONSOLE_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_TOOLS_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_DRAW_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_UI_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_RESOURCE_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_SHADER_EXT;
    // tStarterInit.eFlags |= PL_STARTER_FLAGS_SCREEN_LOG_EXT;

    // initial flags
    // tStarterInit.eFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);
    
    ptAppData->ptDevice = gptStarter->get_device();

    // initialize job system
    gptJobs->initialize({});

    const plShaderVariantInit tShaderVariantInit = {
        ptAppData->ptDevice
    };
    gptShaderVariant->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = PL_ZERO_INIT;
    tRenderSettings.ptDevice              = ptAppData->ptDevice;
    tRenderSettings.ptSwapchain           = gptStarter->get_swapchain();
    gptRenderer->initialize(&tRenderSettings);

    // initialize ecs component library
    gptEcs->initialize({});
    gptRendererEcs->register_system();
    gptScript->register_ecs_system();
    gptAnimation->register_ecs_system();
    gptCameraEcs->register_ecs_system();
    gptMesh->register_ecs_system();
    gptPhysics->register_ecs_system();
    gptMaterial->register_ecs_system();
    gptEcs->finalize();
    ptAppData->ptCompLibrary = gptEcs->get_default_library();

    // plIO* ptIO = gptIO->get_io();

    // for(int i = 0; i < ptIO->iArgc; i++)
    // {
    //     if(strcmp(ptIO->apArgv[i], "-s") == 0)
    //     {
    //         pl_sprintf(ptAppData->acCurrentScene, "../assets/core/scenes/scene-%s.json", ptIO->apArgv[i + 1]);
    //         gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptCompLibrary, &ptAppData->tTestWorld);

    //         if(ptAppData->tTestWorld.bMSAA)
    //             gptStarter->activate_msaa();
    //         else
    //             gptStarter->deactivate_msaa();

    //         gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
    //     }

    // }

    gptTools->initialize({ptAppData->ptDevice});

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", nullptr, nullptr);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", nullptr, nullptr);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", nullptr, nullptr);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", nullptr, nullptr);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", nullptr, nullptr);

    *ptAppData->pbShowLogging = gptConfig->load_bool("pbShowLogging", *ptAppData->pbShowLogging);
    *ptAppData->pbShowStats = gptConfig->load_bool("pbShowStats", *ptAppData->pbShowStats);
    *ptAppData->pbShowProfiling = gptConfig->load_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    *ptAppData->pbShowMemoryAllocations = gptConfig->load_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    *ptAppData->pbShowDeviceMemoryAnalyzer = gptConfig->load_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create fonts
    plFontRange tFontRange = PL_ZERO_INIT;
    tFontRange.iFirstCodePoint = 0x0020;
    tFontRange.uCharCount = 0x00FF - 0x0020;

    plFontConfig tFontConfig0 = PL_ZERO_INIT;
    tFontConfig0.bSdf = false;
    tFontConfig0.fSize = 16.0f;
    tFontConfig0.uHOverSampling = 1;
    tFontConfig0.uVOverSampling = 1;
    tFontConfig0.ptRanges = &tFontRange;
    tFontConfig0.uRangeCount = 1;
    ptAppData->tDefaultFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "/assets/core/fonts/Cousine-Regular.ttf");

    plFontRange tIconRange = PL_ZERO_INIT;
    tIconRange.iFirstCodePoint = ICON_MIN_FA;
    tIconRange.uCharCount = ICON_MAX_16_FA - ICON_MIN_FA;

    plFontConfig tFontConfig1 = PL_ZERO_INIT;
    tFontConfig1.bSdf           = false;
    tFontConfig1.fSize          = 16.0f;
    tFontConfig1.uHOverSampling = 1;
    tFontConfig1.uVOverSampling = 1;
    tFontConfig1.ptMergeFont    = ptAppData->tDefaultFont;
    tFontConfig1.ptRanges       = &tIconRange;
    tFontConfig1.uRangeCount    = 1;
    gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "/assets/core/fonts/fa-solid-900.otf");
    gptStarter->set_default_font(ptAppData->tDefaultFont);
    gptUI->set_default_font(ptAppData->tDefaultFont);

    gptStarter->finalize();

    pl__refresh_files(ptAppData);

    gptScreenLog->initialize({ptAppData->tDefaultFont});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

    gptDearImGui->initialize(nullptr);

    ImGuiContext* ptImguiContext = (ImGuiContext*)ptDataRegistry->get_data("imgui");
    ImGui::SetCurrentContext(ptImguiContext);

    ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)ptDataRegistry->get_data("imgui allocate");
    ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)ptDataRegistry->get_data("imgui free");
    ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);

    // ImGui::GetIO().ConfigFlags &= ~ImGuiBackendFlags_PlatformHasViewports;
    ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));
    ImGuiIO& tImGuiIO = ImGui::GetIO();
    tImGuiIO.IniFilename = nullptr;
    ImGui::LoadIniSettingsFromDisk("../sandbox/pl_imgui.ini");
    tImGuiIO.Fonts->AddFontFromFileTTF("../assets/core/fonts/Cousine-Regular.ttf", 16.0f);
    auto tImGuiFontConfig = ImFontConfig();
    tImGuiFontConfig.MergeMode = true;
    static ImWchar atFontRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA};
    tImGuiIO.FontDefault = tImGuiIO.Fonts->AddFontFromFileTTF("../assets/core/fonts/fa-solid-900.otf", 16.0f, &tImGuiFontConfig, atFontRanges);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptJobs->cleanup();
    pl_sb_free(ptAppData->sbcTempBuffer);
    pl_sb_free(ptAppData->sbtSceneFilesCore);
    pl_sb_free(ptAppData->sbtSceneFilesDev);
    pl_sb_free(ptAppData->sbtSceneFilesUser);
    pl_sb_free(ptAppData->sbtSceneEnvironments);

    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);

    gptDearImGui->cleanup();

    gptConfig->set_bool("bShowEntityWindow", ptAppData->bShowEntityWindow);
    gptConfig->set_bool("bPhysicsDebugDraw", ptAppData->bPhysicsDebugDraw);
    gptConfig->set_bool("pbShowLogging", *ptAppData->pbShowLogging);
    gptConfig->set_bool("pbShowStats", *ptAppData->pbShowStats);
    gptConfig->set_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    gptConfig->set_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    gptConfig->set_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    gptConfig->save_to_disk(nullptr);
    gptConfig->cleanup();
    gptEcsTools->cleanup();
    gptPhysics->cleanup();
    gptScreenLog->cleanup();

    if(ptAppData->tTestWorld.ptScene)
        gptRenderer->unload_test_world(&ptAppData->tTestWorld);

    gptEcs->cleanup();
    gptRenderer->cleanup();
    gptShaderVariant->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow*, plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    ptAppData->bResize = true;
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;
        
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    
    // for convience
    plIO* ptIO = gptIO->get_io();

    if(gptIO->is_key_pressed(PL_KEY_F2, false))
    {
        if(ptAppData->tMode != PL_SANDBOX_MODE_EDITOR)
        {
            ptAppData->tMode = PL_SANDBOX_MODE_EDITOR;
            if(ptAppData->tTestWorld.ptScene)
                ptAppData->bResize = true;
        }
    }

    if(gptIO->is_key_pressed(PL_KEY_F3, false))
    {
        if(ptAppData->tMode != PL_SANDBOX_MODE_GAME_DEBUG)
        {
            ptAppData->bMainViewHovered = true;
            ptAppData->tMode = PL_SANDBOX_MODE_GAME_DEBUG;
            if(ptAppData->tTestWorld.ptScene)
                ptAppData->bResize = true;
        }
    }

    if(gptIO->is_key_pressed(PL_KEY_F4, false))
    {
        if(ptAppData->tMode != PL_SANDBOX_MODE_GAME)
        {
            ptAppData->bMainViewHovered = true;
            ptAppData->tMode = PL_SANDBOX_MODE_GAME;
            if(ptAppData->tTestWorld.ptScene)
            {
                ptAppData->bResize = true;
                ptAppData->tSelectedEntity.uData = UINT64_MAX;
                gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 0, nullptr);
            }
        }
    }

    gptRenderer->begin_frame();

    gptDearImGui->new_frame(ptAppData->ptDevice);

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->tTestWorld.ptScene)
            gptRenderer->resize_view(ptAppData->tTestWorld.ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // update statistics
    gptShaderVariant->update_stats();

    if(ptAppData->tTestWorld.ptScene)
    {
        plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tTestWorld.tMainCamera);

        if(ptAppData->tMode == PL_SANDBOX_MODE_EDITOR)
            gptCamera->set_viewport(ptCamera, (ptIO->tMainViewportSize.x * ptAppData->tView0Scale.x), (ptIO->tMainViewportSize.y * ptAppData->tView0Scale.y));
        else
            gptCamera->set_viewport(ptCamera, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        plVec2 tMousePos = gptIO->get_mouse_pos();

        if(ptAppData->bMainViewHovered && !gptUI->wants_mouse_capture() && !gptGizmo->active())
        {
            static plVec2 tClickPos = {0};
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                tClickPos = tMousePos;
            }
            else if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                plVec2 tReleasePos = tMousePos;

                if(tReleasePos.x == tClickPos.x && tReleasePos.y == tClickPos.y)
                    gptRendererEditor->update_hovered_entity(ptAppData->tTestWorld.ptView, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
        }

        if(!ptAppData->bMainViewHovered)
        {
            if(ImGui::GetIO().WantCaptureKeyboard)
                gptUI->set_wants_keyboard_capture_next_frame(true);

            if(ImGui::GetIO().WantCaptureMouse)
                gptUI->set_wants_mouse_capture_next_frame(true);
        }

        // run ecs system
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Run ECS");
        gptScript->run_update_system(ptAppData->ptCompLibrary);
        gptAnimation->run_animation_update_system(ptAppData->ptCompLibrary, ptIO->fDeltaTime);
        gptPhysics->update(ptIO->fDeltaTime, ptAppData->ptCompLibrary);
        gptEcs->run_transform_update_system(ptAppData->ptCompLibrary);
        gptEcs->run_hierarchy_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_light_update_system(ptAppData->ptCompLibrary);
        gptCameraEcs->run_ecs(ptAppData->ptCompLibrary);
        gptAnimation->run_inverse_kinematics_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_skin_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_object_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_environment_probe_update_system(ptAppData->ptCompLibrary); // run after object update
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);

        if(ptAppData->tMode != PL_SANDBOX_MODE_GAME)
        {
            plEntity tNextEntity = {0};
            if(gptRendererEditor->get_hovered_entity(ptAppData->tTestWorld.ptView, &tNextEntity))
            {
                
                if(tNextEntity.uData == 0)
                {
                    ptAppData->tSelectedEntity.uData = UINT64_MAX;
                    gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 0, nullptr);
                }
                else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
                {
                    gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
                    gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 1, &tNextEntity);
                    ptAppData->tSelectedEntity = tNextEntity;
                    gptPhysics->set_angular_velocity(ptAppData->ptCompLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                    gptPhysics->set_linear_velocity(ptAppData->ptCompLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                }

            }

            if(gptIO->is_key_pressed(PL_KEY_M, true))
                gptGizmo->next_mode();

            if(ptAppData->bShowEntityWindow)
            {
                if(gptEcsTools->show_window(ptAppData->ptCompLibrary, &ptAppData->tSelectedEntity, ptAppData->tTestWorld.ptScene, &ptAppData->bShowEntityWindow))
                {
                    if(ptAppData->tSelectedEntity.uData == UINT64_MAX)
                    {
                        gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 0, nullptr);
                    }
                    else
                    {
                        gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 1, &ptAppData->tSelectedEntity);
                    }
                }
            }

            if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
            {
                plDrawList3D* ptGizmoDrawlist =  gptRendererEditor->get_gizmo_drawlist(ptAppData->tTestWorld.ptView);
                plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptRendererEcs->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
                plTransformComponent* ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptAppData->tSelectedEntity);
                plTransformComponent* ptParentTransform = nullptr;
                plHierarchyComponent* ptHierarchyComp = (plHierarchyComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_hierarchy(), ptAppData->tSelectedEntity);
                if(ptHierarchyComp)
                {
                    ptParentTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptHierarchyComp->tParent);
                }
                if(ptSelectedTransform)
                {
                    gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, ptAppData->tView0Offset, ptAppData->tView0Scale);
                }
                else if(ptSelectedObject)
                {
                    ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptSelectedObject->tTransform);
                    gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, ptAppData->tView0Offset, ptAppData->tView0Scale);
                }
            }

            if(ptAppData->bPhysicsDebugDraw)
            {
                plDrawList3D* ptDrawlist = gptRendererDebug->get_drawlist(ptAppData->tTestWorld.ptView);
                gptPhysics->draw(ptAppData->ptCompLibrary, ptDrawlist);
            }

            // debug rendering
            if(ptAppData->tTestWorld.bShowDebugLights)
            {
                plLightComponent* ptLights = nullptr;
                const uint32_t uLightCount = gptEcs->get_components(ptAppData->ptCompLibrary, gptRendererEcs->get_ecs_type_key_light(), (void**)&ptLights, nullptr);
                gptRendererDebug->draw_lights(ptAppData->tTestWorld.ptView, ptLights, uLightCount);
                // gptRendererDebug->draw_lights(ptAppData->ptSecondaryView, ptLights, uLightCount);
            }

            if(ptAppData->tTestWorld.bDrawAllBoundingBoxes)
            {
                gptRendererDebug->draw_all_bound_boxes(ptAppData->tTestWorld.ptView);
            }

            if(ptAppData->tTestWorld.bShowBVH)
            {
                gptRendererDebug->draw_bvh(ptAppData->tTestWorld.ptView);
            }
        }

        // render scene
        const plCamera* atCameras[] = {ptCamera}; //ptSecondaryCamera};
        gptRenderer->prepare_scene(ptAppData->tTestWorld.ptScene, atCameras, 1);
        
        // single view
        plRenderViewDesc tViewDesc0 = {};
        tViewDesc0.ptCamera = ptCamera;
        tViewDesc0.ptCullCamera = ptAppData->tTestWorld.bFrustumCulling ? ptCamera : nullptr;
        gptRenderer->prepare_view(ptAppData->tTestWorld.ptView, ptCamera);
        gptRenderer->render_view(ptAppData->tTestWorld.ptView, &tViewDesc0);
    }

    ImVec2 tLogOffset = {};

    if(ptAppData->tMode != PL_SANDBOX_MODE_EDITOR)
    {
        if(ptAppData->tTestWorld.ptScene)
        {
            plVec2 tStartPos = {0};
            plVec2 tEndPos = ptIO->tMainViewportSize;
            plVec2 tUV = {0};
            plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->tTestWorld.ptView, &tUV);
            gptDraw->add_image_ex(ptAppData->ptDrawLayer,
                tTexture.uData,
                tStartPos,
                tEndPos,
                {0},
                tUV,
                PL_COLOR_32_WHITE);
        }
    }
    else if(ptAppData->tMode == PL_SANDBOX_MODE_EDITOR)
    {
        ImGui::DockSpaceOverViewport(0, 0, ImGuiDockNodeFlags_PassthruCentralNode);

        if(ImGui::BeginMainMenuBar())
        {
            if(ImGui::BeginMenu("File", true))
            {
                if(ImGui::MenuItem("Save Layout"))
                    ImGui::SaveIniSettingsToDisk("../internal/demo/pl_imgui.ini");
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Edit", false))
            {
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Tools", true))
            {
                ImGui::SeparatorText("Pilot Light");
                ImGui::MenuItem("Log Tool", nullptr, ptAppData->pbShowLogging);
                ImGui::MenuItem("Stat Tool", nullptr, ptAppData->pbShowStats);
                ImGui::MenuItem("Profile Tool", nullptr, ptAppData->pbShowProfiling);
                ImGui::MenuItem("Allocation Tool", nullptr, ptAppData->pbShowMemoryAllocations);
                ImGui::MenuItem("Device Memory Tool", nullptr, ptAppData->pbShowDeviceMemoryAnalyzer);
                ImGui::SeparatorText("Dear ImGui");
                ImGui::MenuItem("Dear ImGui Demo", nullptr, &ptAppData->bShowImGuiDemo);
                ImGui::MenuItem("Dear ImPlot Demo", nullptr, &ptAppData->bShowPlotDemo);
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Help", false))
            {
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // main "editor" debug window
        pl__show_editor_window(ptAppData);

        if(ptAppData->tTestWorld.ptScene)
        {
            pl__show_entity_components(ptAppData, ptAppData->tTestWorld.ptScene, ptAppData->tSelectedEntity);

            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ptAppData->bMainViewHovered = false;
            if(ImGui::Begin("Offscreen", nullptr, ImGuiWindowFlags_NoTitleBar))
            {
                if(ImGui::IsWindowHovered())
                    ptAppData->bMainViewHovered = true;

                ImVec2 tContextSize = ImGui::GetContentRegionAvail();
                ImVec2 tCursorStart = ImGui::GetCursorScreenPos();
                ImVec2 tHoverMousePos = ImGui::GetMousePos();
                
                ptAppData->tView0Offset = {
                    tCursorStart.x - ImGui::GetWindowViewport()->Pos.x,
                    tCursorStart.y - ImGui::GetWindowViewport()->Pos.y
                };

                tLogOffset.x = ptAppData->tView0Offset.x;
                tLogOffset.y = ptAppData->tView0Offset.y;

                if(ptAppData->tTestWorld.ptScene)
                {
                    ptAppData->tView0Scale = {
                        tContextSize.x / ImGui::GetWindowViewport()->Size.x,
                        tContextSize.y / ImGui::GetWindowViewport()->Size.y,
                    };

                    plVec2 tUV = {};
                    plBindGroupHandle tTextureHandle = gptRenderer->get_view_color_bind_group(ptAppData->tTestWorld.ptView, &tUV);
                    ImTextureRef tTexture = ImTextureRef(tTextureHandle.uData);
                    ImGui::Image(tTexture, tContextSize, ImVec2(0, 0), ImVec2(tUV.x, tUV.y));

                }
            }
            ImGui::End();
        }
    }

    if(ptAppData->tMode != PL_SANDBOX_MODE_GAME)
    {
        if(ptAppData->bShowUiDemo)
            pl__show_ui_demo_window(ptAppData);

        if(ptAppData->bShowUiStyle)
            gptUI->show_style_editor_window(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            gptUI->show_debug_window(&ptAppData->bShowUiDebug);

        if(ptAppData->bShowPlotDemo)
            ImPlot::ShowDemoWindow(&ptAppData->bShowPlotDemo);

        if(ptAppData->bShowImGuiDemo)
            ImGui::ShowDemoWindow(&ptAppData->bShowImGuiDemo);
    }

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    plCommandBuffer* ptCommandBuffer = gptStarter->begin_main_pass();
    gptDearImGui->render(ptCommandBuffer);

    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;
    plDrawList2D* ptMessageDrawlist = gptScreenLog->get_drawlist(tLogOffset.x, tLogOffset.y, fWidth * 0.2f, fHeight);
    plRenderAttachmentInfo tRenderInfo = {};
    gptStarter->get_render_attachment_info(&tRenderInfo);
    gptDraw->submit_2d_drawlist(ptMessageDrawlist, ptCommandBuffer, fWidth, fHeight, gptGfx->get_swapchain_info(gptStarter->get_swapchain()).eSampleCount, &tRenderInfo);
    gptStarter->end_main_pass();
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    gptStarter->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
pl__show_editor_window(plAppData* ptAppData)
{

    plRendererEditorSceneOptions tEditorSceneOptions = PL_ZERO_INIT;
    plRendererEditorViewOptions tEditorViewOptions = PL_ZERO_INIT;
    plRendererDebugSceneOptions tDebugOptions = PL_ZERO_INIT;
    plRendererTonemapOptions tTonemapOptions = PL_ZERO_INIT;
    plRendererLightingOptions tLightingOptions = PL_ZERO_INIT;
    plRendererShadowOptions tShadowOptions = PL_ZERO_INIT;
    plRendererBloomOptions tBloomOptions = PL_ZERO_INIT;
    plRendererFogOptions tFogOptions = PL_ZERO_INIT;
    plRendererSkyOptions tSkyOptions = PL_ZERO_INIT;
    plTerrainRuntimeOptions tRuntimeOptions= PL_ZERO_INIT;
    
    gptRenderer->get_bloom_options(ptAppData->tTestWorld.ptView, &tBloomOptions);
    gptRenderer->get_shadow_options(ptAppData->tTestWorld.ptScene, &tShadowOptions);
    gptRenderer->get_lighting_options(ptAppData->tTestWorld.ptScene, &tLightingOptions);
    gptRenderer->get_tonemap_options(ptAppData->tTestWorld.ptView, &tTonemapOptions);
    gptRendererEditor->get_scene_options(ptAppData->tTestWorld.ptScene, &tEditorSceneOptions);
    gptRendererEditor->get_view_options(ptAppData->tTestWorld.ptView, &tEditorViewOptions);
    gptRendererDebug->get_scene_options(ptAppData->tTestWorld.ptScene, &tDebugOptions);
    gptRenderer->get_fog_options(ptAppData->tTestWorld.ptScene, &tFogOptions);
    gptRenderer->get_sky_options(ptAppData->tTestWorld.ptScene, &tSkyOptions);

    bool bReloadShaders = false;
    bool bReloadScene = false;
    bool bLoadScene = false;

    plTerrainRuntimeOptions* ptRuntimeOptions = &tRuntimeOptions;
    if(ptAppData->tTestWorld.ptTerrain)
        ptRuntimeOptions = gptRendererTerrain->get_runtime_options(ptAppData->tTestWorld.ptTerrain);

    bool bSceneExists = ptAppData->tTestWorld.ptScene != nullptr;

    if(!bSceneExists)
    {
        plIO* ptIO = gptIO->get_io();
        ImGui::SetNextWindowPos({ptIO->tMainViewportSize.x * 0.25f, ptIO->tMainViewportSize.y * 0.25f});
        ImGui::SetNextWindowSize({ptIO->tMainViewportSize.x / 2.0f, ptIO->tMainViewportSize.y / 2.0f});
        if(ImGui::Begin("Select Scene", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
        {
            if(ImGui::Button("Refresh"))
                pl__refresh_files(ptAppData);

            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
                ptAppData->filter.Clear();
            }
            ptAppData->filter.Draw(ICON_FA_MAGNIFYING_GLASS);

            static int iSceneGroup = 0;
            // ImGui::GetTextLineHeight
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Scene Group: ");
            ImGui::SameLine();
            ImGui::RadioButton("Core", &iSceneGroup, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Dev", &iSceneGroup, 1);
            ImGui::SameLine();
            ImGui::RadioButton("User", &iSceneGroup, 2);

            if(iSceneGroup == 0 && ImGui::BeginListBox("Scenes", {-75.0f, -1.0f}))
            {
                
                uint32_t uSceneCount = pl_sb_size(ptAppData->sbtSceneFilesCore);

                for (uint32_t n = 0; n < uSceneCount; n++)
                {
                    if (ptAppData->filter.PassFilter(ptAppData->sbtSceneFilesCore[n].acName))
                    {
                        bool bPlaceHolder = ptAppData->iSelectedSceneCore == n;
                        if(ImGui::Selectable(ptAppData->sbtSceneFilesCore[n].acName, &bPlaceHolder))
                        {
                            if(ptAppData->tTestWorld.ptScene)
                                gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                            ptAppData->iSelectedSceneCore = n;
                            ptAppData->iSelectedSceneDev = -1;
                            ptAppData->iSelectedSceneUser = -1;
                            pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFilesCore[n].acTemplate);
                            gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptCompLibrary, &ptAppData->tTestWorld);

                            // if(ptAppData->tTestWorld.bMSAA)
                            //     gptStarter->activate_msaa();
                            // else
                            //     gptStarter->deactivate_msaa();

                            gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
                            bLoadScene = true;
                        }
                    }
                }

                ImGui::EndListBox();
            }

            if(iSceneGroup == 1 && ImGui::BeginListBox("Scenes", {-75.0f, -1.0f}))
            {
                
                uint32_t uSceneCount = pl_sb_size(ptAppData->sbtSceneFilesDev);

                for (uint32_t n = 0; n < uSceneCount; n++)
                {
                    if (ptAppData->filter.PassFilter(ptAppData->sbtSceneFilesDev[n].acName))
                    {
                        bool bPlaceHolder = ptAppData->iSelectedSceneDev == n;
                        if(ImGui::Selectable(ptAppData->sbtSceneFilesDev[n].acName, &bPlaceHolder))
                        {
                            if(ptAppData->tTestWorld.ptScene)
                                gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                            ptAppData->iSelectedSceneDev = n;
                            ptAppData->iSelectedSceneCore = -1;
                            ptAppData->iSelectedSceneUser = -1;
                            pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFilesDev[n].acTemplate);
                            gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptCompLibrary, &ptAppData->tTestWorld);

                            // if(ptAppData->tTestWorld.bMSAA)
                            //     gptStarter->activate_msaa();
                            // else
                            //     gptStarter->deactivate_msaa();

                            gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
                            bLoadScene = true;
                        }
                    }
                }

                ImGui::EndListBox();
            }

            if(iSceneGroup == 2 && ImGui::BeginListBox("Scenes", {-75.0f, -1.0f}))
            {
                
                uint32_t uSceneCount = pl_sb_size(ptAppData->sbtSceneFilesUser);

                for (uint32_t n = 0; n < uSceneCount; n++)
                {
                    if (ptAppData->filter.PassFilter(ptAppData->sbtSceneFilesUser[n].acName))
                    {
                        bool bPlaceHolder = ptAppData->iSelectedSceneUser == n;
                        if(ImGui::Selectable(ptAppData->sbtSceneFilesUser[n].acName, &bPlaceHolder))
                        {
                            if(ptAppData->tTestWorld.ptScene)
                                gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                            ptAppData->iSelectedSceneUser = n;
                            ptAppData->iSelectedSceneCore = -1;
                            ptAppData->iSelectedSceneDev = -1;
                            pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFilesUser[n].acTemplate);
                            gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptCompLibrary, &ptAppData->tTestWorld);

                            // if(ptAppData->tTestWorld.bMSAA)
                            //     gptStarter->activate_msaa();
                            // else
                            //     gptStarter->deactivate_msaa();

                            gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
                            bLoadScene = true;
                        }
                    }
                }

                ImGui::EndListBox();
            }
            ImGui::End();
        }
        
    }
    
    if(bSceneExists)
    {
        if(ImGui::Begin("Pilot Light", nullptr, ImGuiWindowFlags_None))
        {
            ImGui::Dummy({25.0f, 15.0f});
            // ImGui::PushStyleColor(ImGuiCol_Header, {0.29f, 0.09f, 0.13f, 1.0f});
            if(ImGui::CollapsingHeader(ICON_FA_CIRCLE_INFO " Information"))
            {
                ImGui::Text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                ImGui::Text("Graphics Backend: %s", gptGfx->get_backend_string());

                ImGui::SeparatorText("Controls");
                ImGui::BulletText("F1 - bring up console");
                ImGui::BulletText("F2 - change to editor mode");
                ImGui::BulletText("F3 - change to game debug mode");
                ImGui::BulletText("F4 - change to game mode");
                ImGui::BulletText("M  - change gizmo mode");

                if(ImGui::Button("Show Camera Controls"))
                {
                    const char* acMouseInfo = "Camera Controls\n"
                    "_______________\n"
                    "LMB + Drag: Moves camera forward & backward and rotates left & right.\n\n"
                    "RMB + Drag: Rotates camera.\n\n"
                    "LMB + RMB + Drag: Pans Camera\n\n"
                    "Mouse Wheel: Speed\n\n"
                    "Game style (when holding RMB)\n"
                    "_____________________________\n"
                    "W    Moves the camera forward.\n"
                    "S    Moves the camera backward.\n"
                    "A    Moves the camera left.\n"
                    "D    Moves the camera right.\n"
                    "E    Moves the camera up.\n"
                    "Q    Moves the camera down.\n"
                    "Z    Zooms the camera out (raises FOV).\n"
                    "C    Zooms the camera in (lowers FOV).\n";
                    gptScreenLog->add_message_ex(651984984, 45.0, PL_COLOR_32_GREEN, 1.5f, acMouseInfo);
                }
            }
            // ImGui::PopStyleColor(1);

            if(ImGui::CollapsingHeader(ICON_FA_SLIDERS " App Options"))
            {

                plScreenLogFlags tScreenLogFlags = gptScreenLog->get_flags();

                if(ImGui::Checkbox("VSync", &ptAppData->bVSync))
                {
                    if(ptAppData->bVSync)
                        gptStarter->activate_vsync();
                    else
                        gptStarter->deactivate_vsync();
                }

                // if(ImGui::Checkbox("UI MSAA", &ptAppData->tTestWorld.bMSAA))
                // {
                //     if(ptAppData->tTestWorld.bMSAA)
                //         gptStarter->activate_msaa();
                //     else
                //         gptStarter->deactivate_msaa();
                // }

                ImGui::Checkbox("Frustum Culling", &ptAppData->tTestWorld.bFrustumCulling);
                if(ImGui::CheckboxFlags("Hide Screen Log", &tScreenLogFlags, PL_SCREEN_LOG_FLAGS_HIDE_MESSAGES))
                    gptScreenLog->set_flags(tScreenLogFlags);
            }

            if(ImGui::CollapsingHeader(ICON_FA_PHOTO_FILM " Scene"))
            {
                if(ImGui::Button("Reload"))
                {
                    gptPhysics->reset();
                    gptEcs->reset_library(ptAppData->ptCompLibrary);
                    gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                    gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptCompLibrary, &ptAppData->tTestWorld);
                    bReloadScene = true;
                }
                ImGui::SameLine();

                if(ImGui::Button("Unload"))
                {
                    gptPhysics->reset();
                    gptEcs->reset_library(ptAppData->ptCompLibrary);
                    gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                    pl__refresh_files(ptAppData);
                    ptAppData->iSelectedSceneCore = -1;
                    ptAppData->iSelectedSceneDev = -1;
                    ptAppData->iSelectedSceneUser = -1;
                }

                ImGui::Checkbox("Dynamic BVH", &ptAppData->bContinuousBVH);
                if((ImGui::Button("Build BVH") || ptAppData->bContinuousBVH))
                    gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
            }

            if(ImGui::CollapsingHeader(ICON_FA_DICE_D6 " Renderer"))
            {
                if(ImGui::CheckboxFlags("Image Based Lighting", &tLightingOptions.tFlags, PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED))
                    bReloadShaders = true;

                if(ImGui::CheckboxFlags("Punctual Lighting", &tLightingOptions.tFlags, PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS))
                    bReloadShaders = true;
                
                if(ImGui::CheckboxFlags("Normal Mapping", &tLightingOptions.tFlags, PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING))
                    bReloadShaders = true;

                if(ImGui::CheckboxFlags("No Shadows", &tLightingOptions.tFlags, PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS))
                    bReloadShaders = true;

                if(ImGui::CheckboxFlags("MultiViewport Shadows", &tShadowOptions.tFlags, PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT))
                    bReloadShaders = true;

                if(ImGui::CheckboxFlags("PCF Shadows", &tShadowOptions.tFlags, PL_RENDERER_SHADOW_FLAGS_PCF))
                    bReloadShaders = true;

                ImGui::InputFloat("Depth Bias", &tShadowOptions.fConstantDepthBias);
                ImGui::InputFloat("Slope Depth Bias", &tShadowOptions.fSlopeDepthBias);
                ImGui::InputFloat("Max Shadow Range", &tShadowOptions.fMaxShadowRange);

                if(ImGui::Button("Reload Shaders"))
                    bReloadShaders = true;

            }

            if(ImGui::CollapsingHeader(ICON_FA_CLOUD_SUN " Sky Options"))
            {
                bool bProbesDirty = false;
                if(ImGui::RadioButton("Method: None", &tSkyOptions.tMode, PL_RENDERER_SKY_MODE_NONE)) bProbesDirty = true;
                if(ImGui::RadioButton("Method: Skybox", &tSkyOptions.tMode, PL_RENDERER_SKY_MODE_SKYBOX)) bProbesDirty = true;
                if(ImGui::RadioButton("Method: Realistic", &tSkyOptions.tMode, PL_RENDERER_SKY_MODE_REALISTIC)) bProbesDirty = true;


                if(bProbesDirty)
                {
                    plRendererSceneFlags tSceneFlags = gptRenderer->get_scene_flags(ptAppData->tTestWorld.ptScene);
                    tSceneFlags |= PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY;
                    gptRenderer->set_scene_flags(ptAppData->tTestWorld.ptScene, tSceneFlags);
                }

                if(tSkyOptions.tMode != PL_RENDERER_SKY_MODE_NONE)
                {

                    static int saiSkyLutRes[2] = {0};
                    static int saiTransmissionLutRes[2] = {0};
                    static int saiMultiscatterLutRes[2] = {0};
                    static int saiAerialLutRes[3] = {0};
                    if(saiSkyLutRes[0] == 0) // first run
                    {
                        saiSkyLutRes[0] = (int)tSkyOptions.tSkyLutResolution.x;
                        saiSkyLutRes[1] = (int)tSkyOptions.tSkyLutResolution.y;

                        saiTransmissionLutRes[0] = (int)tSkyOptions.tTransmissionLutResolution.x;
                        saiTransmissionLutRes[1] = (int)tSkyOptions.tTransmissionLutResolution.y;

                        saiMultiscatterLutRes[0] = (int)tSkyOptions.tMultiscatterLutResolution.x;
                        saiMultiscatterLutRes[1] = (int)tSkyOptions.tMultiscatterLutResolution.y;

                        saiAerialLutRes[0] = (int)tSkyOptions.tAerialLutResolution.x;
                        saiAerialLutRes[1] = (int)tSkyOptions.tAerialLutResolution.y;
                        saiAerialLutRes[2] = (int)tSkyOptions.tAerialLutResolution.z;
                    }

                    ImGui::InputFloat("Sun Intensity", &tSkyOptions.fSunIntensity);
                    ImGui::InputFloat3("Sun Color", tSkyOptions.tSunColor.d);
                    ImGui::ColorPicker3("Sun Color", tSkyOptions.tSunColor.d);

                    tSkyOptions.tSunDirection = pl_norm_vec3(tSkyOptions.tSunDirection);
                    float fSunPitch = asinf(pl_clampf(-1.0f, tSkyOptions.tSunDirection.y, 1.0f));
                    float fSunYaw   = atan2f(tSkyOptions.tSunDirection.x, tSkyOptions.tSunDirection.z);

                    bool bChanged = false;
                    bChanged |= ImGui::SliderAngle("Sun Pitch", &fSunPitch, -89.9f, 89.9f, 0);
                    bChanged |= ImGui::SliderAngle("Sun Yaw", &fSunYaw, -180.0f, 180.0f, 0);
                    if(bChanged)
                    {
                        const float fCosPitch = cosf(fSunPitch);
                        tSkyOptions.tSunDirection.x = fCosPitch * sinf(fSunYaw);
                        tSkyOptions.tSunDirection.y = sinf(fSunPitch);
                        tSkyOptions.tSunDirection.z = fCosPitch * cosf(fSunYaw);
                        tSkyOptions.tSunDirection = pl_norm_vec3(tSkyOptions.tSunDirection);
                    }

                    ImGui::CheckboxFlags("Shadow Mapping", &tSkyOptions.tFlags, PL_RENDERER_SKY_FLAGS_SHADOWS);
                    if(tSkyOptions.tFlags & PL_RENDERER_SKY_FLAGS_SHADOWS)
                    {
                        ImGui::SeparatorText("Shadows");
                        int iSunResolution = (int)tSkyOptions.uShadowResolution;
                        ImGui::RadioButton("Shadow Resolution: Low", &iSunResolution, 1024);
                        ImGui::RadioButton("Shadow Resolution: Medium", &iSunResolution, 2048);
                        ImGui::RadioButton("Shadow Resolution: High", &iSunResolution, 4096);
                        tSkyOptions.uShadowResolution = (uint32_t)iSunResolution;
                        int iShadowCascadeCount = (int)tSkyOptions.uShadowCascadeCount;
                        ImGui::SliderInt("Cascades", &iShadowCascadeCount, 1, 4, 0);
                        tSkyOptions.uShadowCascadeCount = (uint32_t)iShadowCascadeCount;
                        ImGui::CheckboxFlags("Debug Cascades", &tSkyOptions.tFlags, PL_RENDERER_SKY_FLAGS_DEBUG_CASCADES);
                    }

                    if(tSkyOptions.tMode == PL_RENDERER_SKY_MODE_SKYBOX)
                    {
                        // static uint32_t uComboSelect = 0;
                        // static const char* apcEnvMaps[] = {
                        //     "none",
                        //     "helipad",
                        //     "chromatic",
                        //     "directional",
                        //     "doge2",
                        //     "ennis",
                        //     "field",
                        //     "footprint_court",
                        //     "neutral",
                        //     "papermill",
                        //     "pisa",
                        //     "asteroid_field",
                        //     "brown_dwarf",
                        //     "galaxy",
                        //     "nebulae",
                        //     "planet",
                        //     "ringed_planet",
                        //     "hay_bales",
                        //     "sunset",
                        //     "sunset2",
                        //     "sunset3",
                        //     "sunrise",
                        //     "sky",
                        //     "country_road",
                        // };
                        // bool abCombo[24] = {0};
                        // abCombo[uComboSelect] = true;
                        if(ImGui::BeginCombo("Environment", ptAppData->sbtSceneEnvironments[ptAppData->iSelectedEnvironment].acName))
                        {
                            for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtSceneEnvironments); i++)
                            {
                                if(ImGui::Selectable(ptAppData->sbtSceneEnvironments[i].acName, i == ptAppData->iSelectedEnvironment, 0))
                                {
                                    if(i == 0)
                                    {
                                        tSkyOptions.tMode = PL_RENDERER_SKY_MODE_NONE;
                                        bProbesDirty = true;
                                    }
                                    else
                                    {
                                        ptAppData->iSelectedEnvironment = i;
                                        tSkyOptions.uSkyboxResolution = 1024;
                                        tSkyOptions.tFlags |= PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY;
                                        strncpy(tSkyOptions.acSkyboxPath, ptAppData->sbtSceneEnvironments[i].acPath, 256);
                                    }
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if(tSkyOptions.tMode == PL_RENDERER_SKY_MODE_REALISTIC)
                    {
                        ImGui::CheckboxFlags("Feature: Visualizer", &tSkyOptions.tFlags, PL_RENDERER_SKY_FLAGS_SHOW_VISUALIZER);
                        ImGui::CheckboxFlags("Feature: Multiscattering", &tSkyOptions.tFlags, PL_RENDERER_SKY_FLAGS_MULTISCATTER);
                        ImGui::CheckboxFlags("Feature: Aerial Perspective", &tSkyOptions.tFlags, PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE);
                        
                        ImGui::InputFloat("Atmosphere Thickness", &tSkyOptions.fAtmosphereHeight, 0.0f, 0.0f, "%0.6f", 0);
                        ImGui::InputFloat("Atmosphere Conversion", &tSkyOptions.fAtmosphereConversion, 0.0f, 0.0f, "%0.6f", 0);
                        ImGui::InputFloat("Sun Radius", &tSkyOptions.fSunRadius, 0.0f, 0.0f, "%0.6f", 0);
                        ImGui::InputFloat("Planet Radius", &tSkyOptions.fPlanetRadius, 0.0f, 0.0f, "%0.6f", 0);
                        ImGui::InputFloat3("Rayleigh Scattering", tSkyOptions.tScatteringRayleighGround.d, "%0.6f", 0);
                        ImGui::InputFloat3("Rayleigh Absorption", tSkyOptions.tExtinctionRayleighGround.d, "%0.6f", 0);
                        ImGui::InputFloat3("Ozone Absorption", tSkyOptions.tOzoneExtinction.d, "%0.6f", 0);
                        ImGui::InputFloat("Mie Scattering", &tSkyOptions.fScatteringMieGround, 0.0f, 0.0f, "%0.6f", 0);
                        ImGui::InputFloat("Mie Absorption", &tSkyOptions.fExtinctionMieGround, 0.0f, 0.0f, "%0.6f", 0);
                        ImGui::InputFloat("Mie Scatter Asymmetry", &tSkyOptions.fMieScatteringExponent, 0.0f, 0.0f, "%0.6f", 0);

                        ImGui::InputInt2("Sky LUT Res", saiSkyLutRes, 0);
                        ImGui::InputInt2("Transmission LUT Res", saiTransmissionLutRes, 0);

                        if(tSkyOptions.tFlags & PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE)
                        {
                            ImGui::SeparatorText("Aerial Perspective");
                            ImGui::InputFloat("Max. Distance", &tSkyOptions.fMaxAerialDistance);
                            ImGui::InputFloat("Depth Exponent", &tSkyOptions.fAerialDepthExponent);
                            int iAerialSamplesPerSlice = tSkyOptions.uAerialSamplesPerSlice;
                            ImGui::InputInt("Samples per Slice", &iAerialSamplesPerSlice, 0);
                            tSkyOptions.uAerialSamplesPerSlice = (uint32_t)iAerialSamplesPerSlice;
                            ImGui::InputInt3("Aerial LUT Res", saiAerialLutRes, 0);
                        }

                        if(tSkyOptions.tFlags & PL_RENDERER_SKY_FLAGS_MULTISCATTER)
                        {
                            ImGui::InputInt2("Multiscatter LUT Res", saiMultiscatterLutRes, 0);
                        }

                        if(ImGui::Button("Update LUTS"))
                        {
                            tSkyOptions.tSkyLutResolution.x = (float)saiSkyLutRes[0];
                            tSkyOptions.tSkyLutResolution.y = (float)saiSkyLutRes[1];
                            tSkyOptions.tTransmissionLutResolution.x = (float)saiTransmissionLutRes[0];
                            tSkyOptions.tTransmissionLutResolution.y = (float)saiTransmissionLutRes[1];
                            tSkyOptions.tMultiscatterLutResolution.x = (float)saiMultiscatterLutRes[0];
                            tSkyOptions.tMultiscatterLutResolution.y = (float)saiMultiscatterLutRes[1];
                            tSkyOptions.tAerialLutResolution.x = (float)saiAerialLutRes[0];
                            tSkyOptions.tAerialLutResolution.y = (float)saiAerialLutRes[1];
                            tSkyOptions.tAerialLutResolution.z = (float)saiAerialLutRes[2];
                            tSkyOptions.tFlags |= PL_RENDERER_SKY_FLAGS_LUTS_DIRTY;
                        }
                    }
                }
            }

            if(ImGui::CollapsingHeader(ICON_FA_INDUSTRY " Terrain Options"))
            {
                ImGui::SliderFloat("fTau", &ptRuntimeOptions->fTau, 0.0f, 1.0f);

                ImGui::CheckboxFlags("Wireframe", &ptRuntimeOptions->tFlags, PL_TERRAIN_FLAGS_WIREFRAME);
                ImGui::CheckboxFlags("Show Levels", &ptRuntimeOptions->tFlags, PL_TERRAIN_FLAGS_SHOW_LEVELS);

                ImGui::SliderFloat("fSlopeStart", &ptRuntimeOptions->fSlopeStart, 0.0f, 1.0f);
                ImGui::SliderFloat("fSlopeEnd", &ptRuntimeOptions->fSlopeEnd, 0.0f, 1.0f);

                ImGui::InputFloat("Terrain Depth Bias", &ptRuntimeOptions->fTerrainShadowConstantDepthBias);
                ImGui::InputFloat("Terrain Slope Depth Bias", &ptRuntimeOptions->fTerrainShadowSlopeDepthBias);

                for(uint32_t i = 0; i < PL_MAX_TERRAIN_ELEVATION_ZONES; i++)
                {
                    if(ImGui::TreeNode(&ptRuntimeOptions->atElevationZones[i], "Zone: %d", i))
                    {
                        ImGui::InputFloat("fMinElevation", &ptRuntimeOptions->atElevationZones[i].fMinElevation);
                        ImGui::InputFloat("fMaxElevation", &ptRuntimeOptions->atElevationZones[i].fMaxElevation);
                        ImGui::InputFloat("fBlendSize", &ptRuntimeOptions->atElevationZones[i].fBlendSize);
                        ImGui::ColorEdit4("Flat Material", ptRuntimeOptions->atElevationZones[i].tFlatMaterial.tBaseColor.d);
                        ImGui::ColorEdit4("Steep Material", ptRuntimeOptions->atElevationZones[i].tSteepMaterial.tBaseColor.d);
                        ImGui::TreePop();
                    }
                }
            }

            if(ImGui::CollapsingHeader(ICON_FA_FILE_IMAGE " Post Process"))
            {

                static const char* apcTonemapText[] = {
                    "None",
                    "Simple",
                    "ACES Filmic (Narkowicz)",
                    "ACES Filmic (Hill)",
                    "ACES Filmic (Hill Exposure Boost)",
                    "Reinhard",
                    "Khronos PBR Neutral",
                };
                ImGui::Combo("Tonemapping", &tTonemapOptions.tMode, apcTonemapText, PL_ARRAYSIZE(apcTonemapText));

                ImGui::SliderFloat("Exposure", &tTonemapOptions.fExposure, 0.0f, 3.0f);
                ImGui::SliderFloat("Brightness", &tTonemapOptions.fBrightness, -1.0f, 1.0f);
                ImGui::SliderFloat("Contrast", &tTonemapOptions.fContrast, 0.0f, 2.0f);
                ImGui::SliderFloat("Saturation", &tTonemapOptions.fSaturation, 0.0f, 2.0f);

                ImGui::SeparatorText("Bloom");
                bool bBloomActive = tBloomOptions.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                ImGui::Checkbox("Bloom", &bBloomActive);

                if(bBloomActive)
                {
                    ImGui::SliderFloat("Bloom Radius", &tBloomOptions.fRadius, 0.0f, 10.0f, 0);
                    ImGui::SliderFloat("Bloom Strength", &tBloomOptions.fStrength, 0.0f, 1.0f, 0);
                    int iBloomChainLength = (int)tBloomOptions.uChainLength;
                    if(ImGui::SliderInt("Bloom Chain", &iBloomChainLength, 2, 10, 0))
                        tBloomOptions.uChainLength = (uint32_t)iBloomChainLength;
                    tBloomOptions.tFlags |= PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                }
                else
                    tBloomOptions.tFlags &= ~PL_RENDERER_BLOOM_FLAGS_ACTIVE;

                ImGui::SeparatorText("Fog");

                bool bFog = tFogOptions.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE;
                ImGui::Checkbox("Fog", &bFog);
                if(bFog)
                {
                    tFogOptions.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
                    ImGui::RadioButton("Linear Fog", &tFogOptions.tMode, 0);
                    ImGui::RadioButton("Exponential Fog", &tFogOptions.tMode, 1);
                    ImGui::SliderFloat("Fog Start", &tFogOptions.fStart, 0.0f, 100.0f);
                    ImGui::SliderFloat("Fog End", &tFogOptions.fCutOffDistance, 0.0f, 10000.0f);
                    ImGui::ColorEdit3("Fog Color", tFogOptions.tColor.d);
                    if(tFogOptions.tMode == PL_RENDERER_FOG_MODE_EXPONENTIAL)
                    {
                        ImGui::SliderFloat("Fog Max Opacity", &tFogOptions.fMaxOpacity, 0.0f, 1.0f);
                        ImGui::SliderFloat("Fog Density", &tFogOptions.fDensity, 0.0f, 1.0f);
                        ImGui::SliderFloat("Fog Height", &tFogOptions.fHeight, -100.0f, 100.0f);
                        ImGui::SliderFloat("Fog Height Falloff", &tFogOptions.fHeightFalloff, 0.0f, 1.0f);
                    }  
                }
                else
                    tFogOptions.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;
            }

            if(ImGui::CollapsingHeader(ICON_FA_BOXES_STACKED " Physics", 0))
            {
                plPhysicsEngineSettings tPhysicsSettings = {};
                gptPhysics->get_settings(&tPhysicsSettings);

                ImGui::Checkbox("Enabled", &tPhysicsSettings.bEnabled);
                ImGui::Checkbox("Debug Draw", &ptAppData->bPhysicsDebugDraw);
                ImGui::SliderFloat("Simulation Speed", &tPhysicsSettings.fSimulationMultiplier, 0.01f, 3.0f);
                ImGui::InputFloat("Sleep Epsilon", &tPhysicsSettings.fSleepEpsilon);
                ImGui::InputFloat("Position Epsilon", &tPhysicsSettings.fPositionEpsilon);
                ImGui::InputFloat("Velocity Epsilon", &tPhysicsSettings.fVelocityEpsilon);
                ImGui::InputScalar("Max Position Its.", ImGuiDataType_U32, &tPhysicsSettings.uMaxPositionIterations);
                ImGui::InputScalar("Max Velocity Its.", ImGuiDataType_U32, &tPhysicsSettings.uMaxVelocityIterations);
                ImGui::InputFloat("Frame Rate", &tPhysicsSettings.fSimulationFrameRate);
                if(ImGui::Button("Wake All")) gptPhysics->wake_up_all();
                if(ImGui::Button("Sleep All")) gptPhysics->sleep_all();

                gptPhysics->set_settings(tPhysicsSettings);

            }

            if(ImGui::CollapsingHeader(ICON_FA_SCREWDRIVER_WRENCH " Tools"))
            {
                ImGui::Checkbox("Device Memory", ptAppData->pbShowDeviceMemoryAnalyzer);
                ImGui::Checkbox("Memory Allocations", ptAppData->pbShowMemoryAllocations);
                ImGui::Checkbox("Profiling", ptAppData->pbShowProfiling);
                ImGui::Checkbox("Statistics", ptAppData->pbShowStats);
                ImGui::Checkbox("Logging", ptAppData->pbShowLogging);
                ImGui::Checkbox("Entities", &ptAppData->bShowEntityWindow);
            }
            if(ImGui::CollapsingHeader(ICON_FA_USER_GEAR " User Interface"))
            {
                ImGui::Checkbox("UI Demo", &ptAppData->bShowUiDemo);
                ImGui::Checkbox("UI Debug", &ptAppData->bShowUiDebug);
                ImGui::Checkbox("UI Style", &ptAppData->bShowUiStyle);
            }

            if(ImGui::CollapsingHeader(ICON_FA_BUG " Debug Options"))
            {
                ImGui::Checkbox("Show Debug Lights", &ptAppData->tTestWorld.bShowDebugLights);
                ImGui::Checkbox("Show Bounding Boxes", &ptAppData->tTestWorld.bDrawAllBoundingBoxes);
                ImGui::Checkbox("Show Probes", &tDebugOptions.bShowProbes);
                ImGui::Checkbox("Show Probe Ranges", &tDebugOptions.bShowProbeRange);
                    ImGui::Checkbox("Show Origin", &tDebugOptions.bShowOrigin);
                    ImGui::Checkbox("Show Grid", &tEditorViewOptions.bShowGrid);
                    ImGui::Checkbox("Selected Bounding Box", &tEditorViewOptions.bShowSelectedBoundingBox);
                    uint32_t uMinOutline = 2;
                    uint32_t uMaxOutline = 50;
                    ImGui::SliderScalar("Outline Width", ImGuiDataType_U32, &tEditorViewOptions.uOutlineWidth, &uMinOutline, &uMaxOutline, 0);
                
                ImGui::Checkbox("Show BVH", &ptAppData->tTestWorld.bShowBVH);
                if(ImGui::Checkbox("Wireframe", &tDebugOptions.bWireframe))
                    bReloadShaders = true;

                    static const char* apcShaderDebugModeText[] = {
                        "None",
                        "Base Color",
                        "Metallic",
                        "Roughness",
                        "Alpha",
                        "Emissive",
                        "Occlusion",
                        "Shading Normal",
                        "Texture Normal",
                        "Geometry Normal",
                        "Geometry Tangent",
                        "Geometry Bitangent",
                        "UV 0",
                        "Clearcoat",
                        "Clearcoat Roughness",
                        "Clearcoat Normal",
                        "Sheen Color",
                        "Sheen Roughness",
                        "Iridescence Factor",
                        "Iridescence Thickness",
                        "Anisotropy Strength",
                        "Anisotropy Direction",
                        "Transmission Strength",
                        "Volume Thickness",
                        "Diffuse Transmission Strength",
                        "Diffuse Transmission Color",
                    };
                    if(ImGui::Combo("Shader Debug Mode", &tDebugOptions.tShaderDebugMode, apcShaderDebugModeText, PL_ARRAYSIZE(apcShaderDebugModeText)))
                        bReloadShaders = true;
            }
        }
        ImGui::End();
    }

    if(!bReloadScene && !bLoadScene)
    {
        gptRenderer->set_tonemap_options(ptAppData->tTestWorld.ptView, &tTonemapOptions);
        gptRenderer->set_lighting_options(ptAppData->tTestWorld.ptScene, &tLightingOptions);
        gptRendererEditor->set_scene_options(ptAppData->tTestWorld.ptScene, &tEditorSceneOptions);
        gptRendererEditor->set_view_options(ptAppData->tTestWorld.ptView, &tEditorViewOptions);
        gptRenderer->set_bloom_options(ptAppData->tTestWorld.ptView, &tBloomOptions);
        gptRenderer->set_fog_options(ptAppData->tTestWorld.ptScene, &tFogOptions);
        gptRenderer->set_shadow_options(ptAppData->tTestWorld.ptScene, &tShadowOptions);
        gptRenderer->set_sky_options(ptAppData->tTestWorld.ptScene, &tSkyOptions);
        gptRendererDebug->set_scene_options(ptAppData->tTestWorld.ptScene, &tDebugOptions);
    }

    if(bReloadShaders)
    {
        gptRendererEditor->reload_scene_shaders(ptAppData->tTestWorld.ptScene);
    }
}

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows          = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats            = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx              = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools            = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs              = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera           = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptCameraEcs        = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
    gptRenderer         = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs             = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader      = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw             = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI               = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork          = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString           = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile          = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile             = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools         = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo            = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole          = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog        = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics          = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision        = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh              = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig           = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptDearImGui        = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
    gptResource         = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter          = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation        = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh             = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant    = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs              = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak              = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime         = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress         = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptMaterial         = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript           = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptRendererDebug    = pl_get_api_latest(ptApiRegistry, plRendererDebugI);
    gptRendererEcs      = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
    gptRendererEditor   = pl_get_api_latest(ptApiRegistry, plRendererEditorI);
    gptRendererTerrain  = pl_get_api_latest(ptApiRegistry, plRendererTerrainI);
}

void
pl__refresh_files(plAppData* ptAppData)
{
    pl_sb_reset(ptAppData->sbtSceneFilesCore);
    pl_sb_reset(ptAppData->sbtSceneFilesDev);
    pl_sb_reset(ptAppData->sbtSceneFilesUser);

    pl_sb_add(ptAppData->sbtSceneEnvironments);
    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acName, "None", 5);
    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acPath, "None", 5);

    // local
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/core/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFilesCore, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "../assets/core/scenes/%s.json", acFileNameOnly);
                    pl_sprintf(acFullPath, "../assets/core/scenes/%s.json", acFileNameOnly);
                    if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFilesCore);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesCore).acName, &acFileNameOnly[6], PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesCore).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/core/environments/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                // if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acFullPath, "../assets/core/environments/%s.hdr", acFileNameOnly);
                    pl_sb_add(ptAppData->sbtSceneEnvironments);
                    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acName, acFileNameOnly, PL_MAX_PATH_LENGTH);
                    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acPath, acFullPath, PL_MAX_PATH_LENGTH);
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    // development
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/development/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFilesDev, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                // if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "../assets/development/scenes/%s.json", acFileNameOnly);
                    pl_sprintf(acFullPath, "../assets/development/scenes/%s.json", acFileNameOnly);
                    if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFilesDev);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesDev).acName, acFileNameOnly, PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesDev).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/development/environments", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                // if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acFullPath, "../assets/development/environments/%s.hdr", acFileNameOnly);
                    pl_sb_add(ptAppData->sbtSceneEnvironments);
                    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acName, acFileNameOnly, PL_MAX_PATH_LENGTH);
                    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acPath, acFullPath, PL_MAX_PATH_LENGTH);
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    // user
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/user/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFilesUser, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "../assets/user/scenes/%s.json", acFileNameOnly);
                    pl_sprintf(acFullPath, "../assets/user/scenes/%s.json", acFileNameOnly);
                    if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFilesUser);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesUser).acName, &acFileNameOnly[6], PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesUser).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/user/environments/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acFullPath, "../assets/user/environments/%s.hdr", acFileNameOnly);
                    pl_sb_add(ptAppData->sbtSceneEnvironments);
                    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acName, &acFileNameOnly[6], PL_MAX_PATH_LENGTH);
                    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acPath, acFullPath, PL_MAX_PATH_LENGTH);
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }
}

bool
pl__verify_scene(plAppData* ptAppData, const char* pcPath)
{
    bool bResult = true;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcPath);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tHandle);

    plJsonObject* ptRootJsonObject = NULL;
    pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

    plJsonObject* ptAppObject = pl_json_member(ptRootJsonObject, "app");

    char acFlag0[256] = {0};
    char acFlag1[256] = {0};
    char* aacFlags[] = {acFlag0, acFlag1};
    uint32_t auLengths[] = {256, 256};
    uint32_t uDependencyCount = 0;
    pl_json_string_array_member(ptRootJsonObject, "dependencies", aacFlags, &uDependencyCount, auLengths);
    for(uint32_t k = 0; k < uDependencyCount; k++)
    {
        if(!gptFile->directory_exists(acFlag0))
        {
            bResult = false;
            break;
        }
    }

    pl_unload_json(&ptRootJsonObject);
    PL_FREE(puFileBuffer);
    return bResult;
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl__entities.cpp"
#include "pl__ui_demo.cpp"

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"