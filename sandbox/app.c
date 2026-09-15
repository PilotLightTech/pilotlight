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
    // NOTE: on first load, "ptAppData" will be NULL but on reloads
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

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    ptExtensionRegistry->load("pl_shader_ext", "pl_load_shader_ext", "pl_unload_shader_ext", true);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", true);

    // load apis
    pl__load_apis(ptApiRegistry);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset((void*)ptAppData, 0, sizeof(plAppData));
    ptAppData->bFrustumCulling = true;
    ptAppData->bAttached = true;

    // mount required directories
    gptVfs->mount_directory("/shaders",   "../shaders",   PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/resources", "../resources", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache",     "../cache",     PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets",    "../assets",    PL_VFS_MOUNT_FLAGS_NONE);

    // create cache directories
    gptFile->create_directory("../cache");
    gptFile->create_directory("../cache/shaders");
    gptFile->create_directory("../cache/imports");
    gptFile->create_directory("../cache/textures");
    gptFile->create_directory("../cache/terrain");

    // create asset directories
    gptFile->create_directory("../assets/materials");
    gptFile->create_directory("../assets/textures");
    gptFile->create_directory("../assets/models");
    gptFile->create_directory("../assets/meshes");
    gptFile->create_directory("../assets/animations");
    gptFile->create_directory("../assets/skeletons");
    gptFile->create_directory("../assets/skins");
    gptFile->create_directory("../assets/scenes");
    // gptFile->create_directory("../assets/shaders");
    // gptFile->create_directory("../assets/fonts");

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;
    ptAppData->bVSync = true;
    ptAppData->iSelectedSceneCore = -1;
    ptAppData->iSelectedEnvironment = 0;

    gptConfig->load_from_disk(NULL);
    ptAppData->bShowEntityWindow = gptConfig->load_bool("bShowEntityWindow", false);
    ptAppData->bPhysicsDebugDraw = gptConfig->load_bool("bPhysicsDebugDraw", false);

    // add console variables
    plConsoleSettings tConsoleSettings = {
        .eFlags = PL_CONSOLE_FLAGS_POPUP
    };
    gptConsole->initialize(tConsoleSettings);
    gptConsole->add_toggle_variable("a.Entities", &ptAppData->bShowEntityWindow, "shows ecs tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

    // initialize APIs that require it
    gptEcsTools->initialize();

    plPhysicsEngineSettings tPhysicsSettings = {0};
    gptPhysics->initialize(tPhysicsSettings);

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        .tMode = PL_WINDOW_PRESENTATION_MODE_GRAPHICS_API,
        .tFlags = PL_WINDOW_FLAG_NONE,
        .pcTitle = "Pilot Light Sandbox",
        .uWidth = 1500,
        .uHeight = 900,
        .iXPos = 50,
        .iYPos = 50
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plFullScreenDesc tFullScreen = {
        .iMonitor = 0
    };
    gptWindows->set_fullscreen(ptAppData->ptWindow, &tFullScreen);

    plStarterInit tStarterInit = {
        .eFlags = PL_STARTER_FLAGS_NONE,
        .ptWindow = ptAppData->ptWindow
    };

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
    tStarterInit.eFlags |= PL_STARTER_FLAGS_SCREEN_LOG_EXT;

    // initial flags
    // tStarterInit.eFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);
    
    ptAppData->ptDevice = gptStarter->get_device();

    // initialize job system
    plJobSystemInit tJobInit = {0};
    gptJobs->initialize(tJobInit);

    const plShaderVariantInit tShaderVariantInit = {
        ptAppData->ptDevice
    };
    gptShaderVariant->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = PL_ZERO_INIT;
    tRenderSettings.ptDevice              = ptAppData->ptDevice;
    tRenderSettings.ptSwapchain           = gptStarter->get_swapchain();
    gptRenderer->initialize(&tRenderSettings);

    plAssetInit tAssetInit = {0};
    gptAsset->initialize(tAssetInit);
    gptTexture->register_asset_types();
    gptMaterial->register_asset_types();
    gptAnimation->register_asset_types();
    gptMesh->register_asset_types();
    gptSkeleton->register_asset_types();
    gptEcs->register_asset_types();
    gptTerrain->register_asset_types();
    gptRenderer->register_asset_types();
    gptAsset->finalize();

    // initialize ecs component library
    plEcsInit tEcsInit = {0};
    gptEcs->initialize(tEcsInit);
    gptIk->register_ecs_components();
    gptSkeleton->register_ecs_components();
    gptTransform->register_ecs_components();
    gptRenderer->register_ecs_components();
    gptScript->register_ecs_components();
    gptAnimation->register_ecs_components();
    gptCameraEcs->register_ecs_components();
    gptPhysics->register_ecs_components();
    gptEcs->finalize();

    if(!gptVfs->does_file_exist("/assets/models/humanoid_Scene.plscene"))
    {
        gptStl->import("/resources/cube.stl");
        gptGltf->import("/resources/DamagedHelmet.glb", NULL);
        gptGltf->import("/resources/humanoid.gltf", NULL);
    }

    // gptGltf->import("/resources/gltf-samples/Models/Sponza/glTF/sponza.gltf", NULL);

    // animations
    // gptGltf->import("/resources/gltf-samples/Models/InterpolationTest/glTF/InterpolationTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/CesiumMan/glTF/CesiumMan.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/BrainStem/glTF/BrainStem.gltf", NULL); // broke
    // gptGltf->import("/resources/gltf-samples/Models/CommercialRefrigerator/glTF/CommercialRefrigerator.gltf", NULL);

    // anisotropy
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyBarnLamp/glTF/AnisotropyBarnLamp.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyDiscTest/glTF/AnisotropyDiscTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyStrengthTest/glTF/AnisotropyStrengthTest.gltf", NULL); // broke
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyRotationTest/glTF/AnisotropyRotationTest.gltf", NULL);
    
    // basic tests
    // gptGltf->import("/resources/gltf-samples/Models/AlphaBLendModeTest/glTF/AlphaBLendModeTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/OrientationTest/glTF/OrientationTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/EnvironmentTest/glTF/EnvironmentTest.gltf", NULL);

    // car
    // gptGltf->import("/resources/gltf-samples/Models/CarConcept/glTF/CarConcept.gltf", NULL);
    
    // chess
    // gptGltf->import("/resources/gltf-samples/Models/ABeautifulGame/glTF/ABeautifulGame.gltf", NULL);

    // clear coat
    // gptGltf->import("/resources/gltf-samples/Models/ClearCoatWicker/glTF/ClearCoatWicker.gltf", NULL);

    plToolsInit tToolsInit = {
        .ptDevice = ptAppData->ptDevice
    };
    gptTools->initialize(tToolsInit);

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);
    ptAppData->pbShowAssets               = (bool*)gptConsole->get_variable("t.AssetTool", NULL, NULL);

    *ptAppData->pbShowLogging = gptConfig->load_bool("pbShowLogging", *ptAppData->pbShowLogging);
    *ptAppData->pbShowStats = gptConfig->load_bool("pbShowStats", *ptAppData->pbShowStats);
    *ptAppData->pbShowProfiling = gptConfig->load_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    *ptAppData->pbShowMemoryAllocations = gptConfig->load_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    *ptAppData->pbShowDeviceMemoryAnalyzer = gptConfig->load_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);
    *ptAppData->pbShowAssets = gptConfig->load_bool("pbShowAssets", *ptAppData->pbShowDeviceMemoryAnalyzer);

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
    ptAppData->tDefaultFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "/resources/Cousine-Regular.ttf");

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
    gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "/resources/fa-solid-900.otf");
    gptStarter->set_default_font(ptAppData->tDefaultFont);
    gptUI->set_default_font(ptAppData->tDefaultFont);

    gptStarter->finalize();

    pl__refresh_files(ptAppData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

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
    pl_sb_free(ptAppData->sbtSceneEnvironments);

    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);

    gptConfig->set_bool("bShowEntityWindow", ptAppData->bShowEntityWindow);
    gptConfig->set_bool("bPhysicsDebugDraw", ptAppData->bPhysicsDebugDraw);
    gptConfig->set_bool("pbShowLogging", *ptAppData->pbShowLogging);
    gptConfig->set_bool("pbShowAssets", *ptAppData->pbShowAssets);
    gptConfig->set_bool("pbShowStats", *ptAppData->pbShowStats);
    gptConfig->set_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    gptConfig->set_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    gptConfig->set_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    gptConfig->save_to_disk(NULL);
    gptConfig->cleanup();
    gptEcsTools->cleanup();
    gptPhysics->cleanup();

    if(ptAppData->ptScene)
    {
        gptRenderer->destroy_view(ptAppData->ptView);
        gptRenderer->destroy_scene(ptAppData->ptScene);
    }
    //     gptRenderer->unload_test_world(&ptAppData->tTestWorld);

    gptAsset->cleanup();
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
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
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

    gptRenderer->begin_frame();

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->ptScene)
            gptRenderer->resize_view(ptAppData->ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // update statistics
    gptShaderVariant->update_stats();

    if(ptAppData->ptScene)
    {
        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);
        plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);

        gptCamera->set_viewport(ptCamera, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        plVec2 tMousePos = gptIO->get_mouse_pos();

        if(ptAppData->bMainViewHovered && !gptUI->wants_mouse_capture() && !gptGizmo->active())
        {
            static plVec2 tClickPos = {0};
            if(gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            {
                tClickPos = tMousePos;
            }
            else if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
            {
                plVec2 tReleasePos = tMousePos;

                if(tReleasePos.x == tClickPos.x && tReleasePos.y == tClickPos.y)
                    gptRendererEditor->update_hovered_entity(ptAppData->ptView, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
        }

        // if(!ptAppData->bMainViewHovered)
        // {
        //     if(ImGui::GetIO().WantCaptureKeyboard)
        //         gptUI->set_wants_keyboard_capture_next_frame(true);

        //     if(ImGui::GetIO().WantCaptureMouse)
        //         gptUI->set_wants_mouse_capture_next_frame(true);
        // }

        // run ecs system
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Run ECS");
        gptScript->run_update_system(ptLibrary);
        gptAnimation->run_animation_update_system(ptLibrary, ptIO->fDeltaTime);
        gptPhysics->update(ptIO->fDeltaTime, ptLibrary);
        gptTransform->run_transform_update_system(ptLibrary);
        gptTransform->run_hierarchy_update_system(ptLibrary);
        gptRenderer->run_light_update_system(ptLibrary);
        gptCameraEcs->run_ecs(ptLibrary);
        gptIk->run_ecs_update_system(ptLibrary);
        gptSkeleton->run_skin_update_system(ptLibrary);
        gptRenderer->run_object_update_system(ptLibrary);
        gptRenderer->run_environment_probe_update_system(ptLibrary); // run after object update
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);

        plEntity tNextEntity = {0};
        if(gptRendererEditor->get_hovered_entity(ptAppData->ptView, &tNextEntity))
        {
            
            if(tNextEntity.uData == 0)
            {
                ptAppData->tSelectedEntity.uData = UINT64_MAX;
                gptRendererEditor->outline_entities(ptAppData->ptScene, 0, NULL);
            }
            else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
            {
                gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
                gptRendererEditor->outline_entities(ptAppData->ptScene, 1, &tNextEntity);
                ptAppData->tSelectedEntity = tNextEntity;
                gptPhysics->set_angular_velocity(ptLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                gptPhysics->set_linear_velocity(ptLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
            }

        }

        if(gptIO->is_key_pressed(PL_KEY_M, true))
            gptGizmo->next_mode();

        if(ptAppData->bShowEntityWindow)
        {
            if(gptEcsTools->show_window(ptLibrary, &ptAppData->tSelectedEntity, ptAppData->ptScene, &ptAppData->bShowEntityWindow))
            {
                if(ptAppData->tSelectedEntity.uData == UINT64_MAX)
                {
                    gptRendererEditor->outline_entities(ptAppData->ptScene, 0, NULL);
                }
                else
                {
                    gptRendererEditor->outline_entities(ptAppData->ptScene, 1, &ptAppData->tSelectedEntity);
                }
            }
        }

        if(gptEcs->is_entity_valid(ptLibrary, ptAppData->tSelectedEntity))
        {
            plDrawList3D* ptGizmoDrawlist =  gptRendererEditor->get_gizmo_drawlist(ptAppData->ptView);
            plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
            plTransformComponent* ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptAppData->tSelectedEntity);
            plTransformComponent* ptParentTransform = NULL;
            plHierarchyComponent* ptHierarchyComp = (plHierarchyComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_hierarchy(), ptAppData->tSelectedEntity);
            if(ptHierarchyComp)
            {
                ptParentTransform = (plTransformComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptHierarchyComp->tParent);
            }
            if(ptSelectedTransform)
            {
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
            else if(ptSelectedObject)
            {
                ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptSelectedObject->tTransform);
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
        }

        if(ptAppData->bPhysicsDebugDraw)
        {
            plDrawList3D* ptDrawlist = gptRendererDebug->get_drawlist(ptAppData->ptView);
            gptPhysics->draw(ptLibrary, ptDrawlist);
        }

        // debug rendering
        if(ptAppData->bShowDebugLights)
        {
            plLightComponent* ptLights = NULL;
            const uint32_t uLightCount = gptEcs->get_components(ptLibrary, gptRenderer->get_ecs_type_key_light(), (void**)&ptLights, NULL);
            gptRendererDebug->draw_lights(ptAppData->ptView, ptLights, uLightCount);
            // gptRendererDebug->draw_lights(ptAppData->ptSecondaryView, ptLights, uLightCount);
        }

        if(ptAppData->bDrawAllBoundingBoxes)
        {
            gptRendererDebug->draw_all_bound_boxes(ptAppData->ptView);
        }

        if(ptAppData->bShowBVH)
        {
            gptRendererDebug->draw_bvh(ptAppData->ptView);
        }

        // render scene
        const plCamera* atCameras[] = {ptCamera}; //ptSecondaryCamera};
        gptRenderer->prepare_scene(ptAppData->ptScene, atCameras, 1);
        
        // single view
        plRenderViewDesc tViewDesc0 = {
            .ptCamera     = ptCamera,
            .ptCullCamera = ptAppData->bFrustumCulling ? ptCamera : NULL
        };
        gptRenderer->prepare_view(ptAppData->ptView, ptCamera);
        gptRenderer->render_view(ptAppData->ptView, &tViewDesc0);
    }

    if(ptAppData->ptScene)
    {
        plVec2 tStartPos = {0};
        plVec2 tEndPos = ptIO->tMainViewportSize;
        plVec2 tUV = {0};
        plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->ptView, &tUV);
        gptDraw->add_image_ex(ptAppData->ptDrawLayer,
            tTexture.uData,
            tStartPos,
            tEndPos,
            (plVec2){0},
            tUV,
            PL_COLOR_32_WHITE);
    }

    pl__show_editor_window(ptAppData);

    if(ptAppData->bShowUiDemo)
        pl__show_ui_demo_window(ptAppData);

    if(ptAppData->bShowUiStyle)
        gptUI->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUI->show_debug_window(&ptAppData->bShowUiDebug);

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    plCommandBuffer* ptCommandBuffer = gptStarter->begin_main_pass();

    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;
    plRenderAttachmentInfo tRenderInfo = {0};
    gptStarter->get_render_attachment_info(&tRenderInfo);
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
    
    gptRendererEditor->get_scene_options(ptAppData->ptScene, &tEditorSceneOptions);
    gptRendererEditor->get_view_options(ptAppData->ptView, &tEditorViewOptions);
    gptRendererDebug->get_scene_options(ptAppData->ptScene, &tDebugOptions);

    bool bReloadShaders = false;
    bool bReloadScene = false;
    bool bLoadScene = false;

    bool bSceneExists = ptAppData->ptScene != NULL;
    plIO* ptIO = gptIO->get_io();

    if(!bSceneExists)
    {
        
        gptUI->set_next_window_pos((plVec2){ptIO->tMainViewportSize.x * 0.25f, ptIO->tMainViewportSize.y * 0.25f}, PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size((plVec2){ptIO->tMainViewportSize.x * 0.5f, ptIO->tMainViewportSize.y * 0.5f}, PL_UI_COND_ALWAYS);
        if(gptUI->begin_window("Select Scene", NULL, PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        { 
            gptUI->layout_static(0.0f, 100.0f, 1);
            if(gptUI->button("Refresh"))
                pl__refresh_files(ptAppData);

            gptUI->layout_dynamic(ptIO->tMainViewportSize.y * 0.4f, 1);

            if(gptUI->begin_child("Scenes", 0, 0))
            {
                uint32_t uSceneCount = pl_sb_size(ptAppData->sbtSceneFilesCore);

                for (uint32_t n = 0; n < uSceneCount; n++)
                {
                    // if (ptAppData->filter.PassFilter(ptAppData->sbtSceneFilesCore[n].acName))
                    bool bPlaceHolder = ptAppData->iSelectedSceneCore == (int)n;
                    if(gptUI->selectable(ptAppData->sbtSceneFilesCore[n].acName, &bPlaceHolder, 0))
                    {
                        ptAppData->iSelectedSceneCore = (int)n;
                        pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFilesCore[n].acTemplate);

                        ptAppData->tSceneHandle = gptAsset->load(ptAppData->acCurrentScene);
                        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);

                        plSceneDesc tSceneInit = {0};

                        ptAppData->ptScene = gptRenderer->create_scene(&tSceneInit);
                        plViewDesc tViewDesc = PL_ZERO_INIT;
                        tViewDesc.uWidth = (uint32_t)ptIO->tMainViewportSize.x;
                        tViewDesc.uHeight = (uint32_t)ptIO->tMainViewportSize.y;
                        ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc);
                        
                        gptRenderer->load_component_library(ptAppData->ptScene, ptLibrary);

                        const plEntity* ptCameraEntities = NULL;
                        uint32_t uCameraCount = gptEcs->get_components(ptLibrary, gptCameraEcs->get_ecs_type_key(), NULL, &ptCameraEntities);
                        if(uCameraCount > 0)
                        {
                            ptAppData->tMainCamera = ptCameraEntities[0];
                        }

                        const plEntity* ptTerrainEntities = NULL;
                        uint32_t uTerrainCount = gptEcs->get_components(ptLibrary, gptRenderer->get_ecs_type_key_terrain(), NULL, &ptTerrainEntities);
                        if(uTerrainCount > 0)
                        {
                            ptAppData->bHasTerrain = true;
                        }

                        bLoadScene = true;
                    }
                }

                gptUI->end_child();
            }

            gptUI->end_window();
        }  
    }
    
    if(bSceneExists)
    {
        plTerrainRuntimeOptions* ptTerrainOptions = gptRenderer->get_terrain_options(ptAppData->ptScene);
        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);

        const plEntity* ptRendererEntities = NULL;
        uint32_t uEnvironmentCount = gptEcs->get_components(ptLibrary, gptRenderer->get_ecs_type_key_environment(), NULL, &ptRendererEntities);
        plEnvironmentComponent* ptEnvironmentComp = (plEnvironmentComponent*)gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_environment(), ptRendererEntities[0]);
        plRendererComponent* ptRendererComp = (plRendererComponent*)gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_renderer(), ptRendererEntities[0]);


        plRenderEnvironment* ptEnvironment = (plRenderEnvironment*)gptAsset->get_data(ptEnvironmentComp->tEnvironment);
        plRenderSettings* ptSettings = (plRenderSettings*)gptAsset->get_data(ptRendererComp->tRenderer);

        plUiWindowFlags tWindowFlags = PL_UI_WINDOW_FLAGS_NONE;
        static bool bJustUnattached = false;
        if(ptAppData->bAttached)
        {
            float fWidth = ptIO->tMainViewportSize.x * 0.5f;
            fWidth = pl_clampf(150.0f, fWidth, 500.0f);
            gptUI->set_next_window_pos((plVec2){0.0f, 0.0f}, PL_UI_COND_ALWAYS);
            gptUI->set_next_window_size((plVec2){fWidth, ptIO->tMainViewportSize.y}, PL_UI_COND_ALWAYS);
            tWindowFlags= PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE | PL_UI_WINDOW_FLAGS_NO_TITLE_BAR;
        }

        if(bJustUnattached)
        {
            gptUI->set_next_window_pos((plVec2){ptIO->tMainViewportSize.x * 0.25f, ptIO->tMainViewportSize.y * 0.25f}, PL_UI_COND_ALWAYS);
            gptUI->set_next_window_size((plVec2){ptIO->tMainViewportSize.x * 0.5f, ptIO->tMainViewportSize.y * 0.5f}, PL_UI_COND_ALWAYS);
            bJustUnattached = false;
        }

        if(gptUI->begin_window("Pilot Light", NULL, tWindowFlags))
        {
            gptUI->layout_dynamic(0.0f, 1);

            if(gptUI->begin_collapsing_header(ICON_FA_CIRCLE_INFO " Information", 0))
            {
                gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());

                gptUI->separator_text("Controls");
                gptUI->text("* F1 - bring up console");
                gptUI->text("* F2 - change to editor mode");
                gptUI->text("* F3 - change to game debug mode");
                gptUI->text("* F4 - change to game mode");
                gptUI->text("* M  - change gizmo mode");

                if(gptUI->button("Show Camera Controls"))
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
                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_SLIDERS " App Options", 0))
            {

                plScreenLogFlags tScreenLogFlags = gptScreenLog->get_flags();

                if(gptUI->checkbox("Attached", &ptAppData->bAttached))
                {
                    if(!ptAppData->bAttached)
                        bJustUnattached = true;
                }

                if(gptUI->checkbox("VSync", &ptAppData->bVSync))
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

                gptUI->checkbox("Frustum Culling", &ptAppData->bFrustumCulling);
                if(gptUI->checkbox_flags("Hide Screen Log", &tScreenLogFlags, PL_SCREEN_LOG_FLAGS_HIDE_MESSAGES))
                    gptScreenLog->set_flags(tScreenLogFlags);

                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_PHOTO_FILM " Scene", 0))
            {
                // if(ImGui::Button("Reload"))
                // {
                //     gptPhysics->reset();
                //     gptAsset->destroy(ptAppData->tSceneHandle);
                //     ptAppData->tSceneHandle = gptAsset->load(ptAppData->acCurrentScene);
                //     // gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                //     // gptRenderer->load_test_world(ptAppData->acCurrentScene, ptScene->ptLibrary, &ptAppData->tTestWorld);
                //     bReloadScene = true;
                // }
                // ImGui::SameLine();

                gptUI->layout_dynamic(0.0f, 2);

                if(gptUI->button("Unload"))
                {
                    gptPhysics->reset();
                    gptRenderer->destroy_view(ptAppData->ptView);
                    gptRenderer->destroy_scene(ptAppData->ptScene);
                    ptAppData->ptView = NULL;
                    ptAppData->ptScene = NULL;
                    gptAsset->destroy(ptAppData->tSceneHandle);
                    // gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                    pl__refresh_files(ptAppData);
                    ptAppData->iSelectedSceneCore = -1;
                }

                if(gptUI->button("Save"))
                {
                    gptAsset->save(ptAppData->tSceneHandle, PL_ASSET_ENCODING_TEXT);
                }

                gptUI->checkbox("Dynamic BVH", &ptAppData->bContinuousBVH);
                if((gptUI->button("Build BVH") || ptAppData->bContinuousBVH))
                    gptRendererEditor->rebuild_scene_bvh(ptAppData->ptScene);

                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_DICE_D6 " Renderer", 0))
            {
                if(gptUI->checkbox_flags("Image Based Lighting", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED))
                    bReloadShaders = true;

                if(gptUI->checkbox_flags("Punctual Lighting", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS))
                    bReloadShaders = true;
                
                if(gptUI->checkbox_flags("Normal Mapping", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING))
                    bReloadShaders = true;

                if(gptUI->checkbox_flags("No Shadows", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS))
                    bReloadShaders = true;

                if(gptUI->checkbox_flags("MultiViewport Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT))
                    bReloadShaders = true;

                if(gptUI->checkbox_flags("PCF Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_PCF))
                    bReloadShaders = true;

                gptUI->input_float("Depth Bias", &ptSettings->tShadows.fConstantDepthBias, "%g", 0);
                gptUI->input_float("Slope Depth Bias", &ptSettings->tShadows.fSlopeDepthBias, "%g", 0);
                gptUI->input_float("Max Shadow Range", &ptSettings->tShadows.fMaxShadowRange, "%g", 0);

                if(gptUI->button("Reload Shaders"))
                    bReloadShaders = true;

                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_CLOUD_SUN " Sky Options", 0))
            {
                bool bProbesDirty = false;
                if(gptUI->radio_button("Method: None", &ptEnvironment->eMode, PL_RENDERER_SKY_MODE_NONE)) bProbesDirty = true;
                if(gptUI->radio_button("Method: Skybox", &ptEnvironment->eMode, PL_RENDERER_SKY_MODE_SKYBOX)) bProbesDirty = true;
                if(gptUI->radio_button("Method: Realistic", &ptEnvironment->eMode, PL_RENDERER_SKY_MODE_REALISTIC)) bProbesDirty = true;


                if(bProbesDirty)
                {
                    plRenderSceneFlags tSceneFlags = gptRenderer->get_scene_flags(ptAppData->ptScene);
                    tSceneFlags |= PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY;
                    gptRenderer->set_scene_flags(ptAppData->ptScene, tSceneFlags);
                }

                if(ptEnvironment->eMode != PL_RENDERER_SKY_MODE_NONE)
                {

                    static int saiSkyLutRes[2] = {0};
                    static int saiTransmissionLutRes[2] = {0};
                    static int saiMultiscatterLutRes[2] = {0};
                    static int saiAerialLutRes[3] = {0};
                    if(saiSkyLutRes[0] == 0) // first run
                    {
                        saiSkyLutRes[0] = (int)ptSettings->tSky.tSkyLutResolution.x;
                        saiSkyLutRes[1] = (int)ptSettings->tSky.tSkyLutResolution.y;

                        saiTransmissionLutRes[0] = (int)ptSettings->tSky.tTransmissionLutResolution.x;
                        saiTransmissionLutRes[1] = (int)ptSettings->tSky.tTransmissionLutResolution.y;

                        saiMultiscatterLutRes[0] = (int)ptSettings->tSky.tMultiscatterLutResolution.x;
                        saiMultiscatterLutRes[1] = (int)ptSettings->tSky.tMultiscatterLutResolution.y;

                        saiAerialLutRes[0] = (int)ptSettings->tSky.tAerialLutResolution.x;
                        saiAerialLutRes[1] = (int)ptSettings->tSky.tAerialLutResolution.y;
                        saiAerialLutRes[2] = (int)ptSettings->tSky.tAerialLutResolution.z;
                    }

                    gptUI->input_float("Sun Intensity", &ptEnvironment->fSunIntensity, "%g", 0);
                    gptUI->input_float3("Sun Color", ptEnvironment->tSunColor.d, "%g", 0);
                    gptUI->input_float3("Sun Color", ptEnvironment->tSunColor.d, "%g", 0);

                    ptEnvironment->tSunDirection = pl_norm_vec3(ptEnvironment->tSunDirection);
                    float fSunPitch = asinf(pl_clampf(-1.0f, ptEnvironment->tSunDirection.y, 1.0f));
                    float fSunYaw   = atan2f(ptEnvironment->tSunDirection.x, ptEnvironment->tSunDirection.z);

                    bool bChanged = false;
                    bChanged |= gptUI->slider_angle("Sun Pitch", &fSunPitch, -89.9f, 89.9f, NULL, 0);
                    bChanged |= gptUI->slider_angle("Sun Yaw", &fSunYaw, -180.0f, 180.0f, NULL, 0);
                    if(bChanged)
                    {
                        const float fCosPitch = cosf(fSunPitch);
                        ptEnvironment->tSunDirection.x = fCosPitch * sinf(fSunYaw);
                        ptEnvironment->tSunDirection.y = sinf(fSunPitch);
                        ptEnvironment->tSunDirection.z = fCosPitch * cosf(fSunYaw);
                        ptEnvironment->tSunDirection = pl_norm_vec3(ptEnvironment->tSunDirection);
                    }

                    gptUI->checkbox_flags("Shadow Mapping", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_SHADOWS);
                    if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_SHADOWS)
                    {
                        gptUI->separator_text("Shadows");
                        int iSunResolution = (int)ptSettings->tShadows.uShadowResolution;
                        gptUI->radio_button("Shadow Resolution: Low", &iSunResolution, 1024);
                        gptUI->radio_button("Shadow Resolution: Medium", &iSunResolution, 2048);
                        gptUI->radio_button("Shadow Resolution: High", &iSunResolution, 4096);
                        ptSettings->tShadows.uShadowResolution = (uint32_t)iSunResolution;
                        int iShadowCascadeCount = (int)ptSettings->tShadows.uShadowCascadeCount;
                        gptUI->slider_int("Cascades", &iShadowCascadeCount, 1, 4, 0);
                        ptSettings->tShadows.uShadowCascadeCount = (uint32_t)iShadowCascadeCount;
                        gptUI->checkbox_flags("Debug Cascades", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_DEBUG_CASCADES);
                    }

                    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX)
                    {
                        // static uint32_t uComboSelect = 0;
                    }

                    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
                    {
                        gptUI->checkbox_flags("Feature: Visualizer", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_SHOW_VISUALIZER);
                        gptUI->checkbox_flags("Feature: Multiscattering", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_MULTISCATTER);
                        gptUI->checkbox_flags("Feature: Aerial Perspective", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE);
                        
                        gptUI->input_float("Atmosphere Thickness", &ptEnvironment->fAtmosphereHeight, "%0.6f", 0);
                        gptUI->input_float("Atmosphere Conversion", &ptEnvironment->fAtmosphereConversion, "%0.6f", 0);
                        gptUI->input_float("Sun Radius", &ptEnvironment->fSunRadius, "%0.6f", 0);
                        gptUI->input_float("Planet Radius", &ptEnvironment->fPlanetRadius, "%0.6f", 0);
                        gptUI->input_float3("Rayleigh Scattering", ptEnvironment->tScatteringRayleighGround.d, "%0.6f", 0);
                        gptUI->input_float3("Rayleigh Absorption", ptEnvironment->tExtinctionRayleighGround.d, "%0.6f", 0);
                        gptUI->input_float3("Ozone Absorption", ptEnvironment->tOzoneExtinction.d, "%0.6f", 0);
                        gptUI->input_float("Mie Scattering", &ptEnvironment->fScatteringMieGround, "%0.6f", 0);
                        gptUI->input_float("Mie Absorption", &ptEnvironment->fExtinctionMieGround, "%0.6f", 0);
                        gptUI->input_float("Mie Scatter Asymmetry", &ptEnvironment->fMieScatteringExponent, "%0.6f", 0);

                        gptUI->input_int2("Sky LUT Res", saiSkyLutRes, 0);
                        gptUI->input_int2("Transmission LUT Res", saiTransmissionLutRes, 0);

                        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE)
                        {
                            gptUI->separator_text("Aerial Perspective");
                            gptUI->input_float("Max. Distance", &ptEnvironment->fMaxAerialDistance, "%g", 0);
                            gptUI->input_float("Depth Exponent", &ptEnvironment->fAerialDepthExponent, "%g", 0);
                            int iAerialSamplesPerSlice = ptEnvironment->uAerialSamplesPerSlice;
                            gptUI->input_int("Samples per Slice", &iAerialSamplesPerSlice, 0);
                            ptEnvironment->uAerialSamplesPerSlice = (uint32_t)iAerialSamplesPerSlice;
                            gptUI->input_int3("Aerial LUT Res", saiAerialLutRes, 0);
                        }

                        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_MULTISCATTER)
                        {
                            gptUI->input_int2("Multiscatter LUT Res", saiMultiscatterLutRes, 0);
                        }

                        if(gptUI->button("Update LUTS"))
                        {
                            ptSettings->tSky.tSkyLutResolution.x = (float)saiSkyLutRes[0];
                            ptSettings->tSky.tSkyLutResolution.y = (float)saiSkyLutRes[1];
                            ptSettings->tSky.tTransmissionLutResolution.x = (float)saiTransmissionLutRes[0];
                            ptSettings->tSky.tTransmissionLutResolution.y = (float)saiTransmissionLutRes[1];
                            ptSettings->tSky.tMultiscatterLutResolution.x = (float)saiMultiscatterLutRes[0];
                            ptSettings->tSky.tMultiscatterLutResolution.y = (float)saiMultiscatterLutRes[1];
                            ptSettings->tSky.tAerialLutResolution.x = (float)saiAerialLutRes[0];
                            ptSettings->tSky.tAerialLutResolution.y = (float)saiAerialLutRes[1];
                            ptSettings->tSky.tAerialLutResolution.z = (float)saiAerialLutRes[2];
                            ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_LUTS_DIRTY;
                        }
                    }
                }

                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_FILE_IMAGE " Post Process", 0))
            {
                // plScene* ptSceneAsset = (plScene*)gptAsset->get_data(ptAppData->tSceneHandle);
                // plRenderSettings* ptSettings = (plRenderSettings*)gptAsset->get_data(ptSceneAsset->tRendererSettings);

                static const char* apcTonemapText[] = {
                    "None",
                    "Simple",
                    "ACES Filmic (Narkowicz)",
                    "ACES Filmic (Hill)",
                    "ACES Filmic (Hill Exposure Boost)",
                    "Reinhard",
                    "Khronos PBR Neutral",
                };

                if(gptUI->begin_combo("Tonemapping", apcTonemapText[ptSettings->tTonemap.tMode], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < PL_ARRAYSIZE(apcTonemapText); i++)
                    {
                        bool bPlaceHolder = ptSettings->tTonemap.tMode == (int)i;
                        if(gptUI->selectable(apcTonemapText[i], &bPlaceHolder, 0))
                        {
                            ptSettings->tTonemap.tMode = (int)i;
                            gptUI->close_current_popup();
                            bReloadShaders = true;
                        }
                    }
                    gptUI->end_combo();
                }

                gptUI->slider_float("Exposure", &ptSettings->tTonemap.fExposure, 0.0f, 3.0f, 0);
                gptUI->slider_float("Brightness", &ptSettings->tTonemap.fBrightness, -1.0f, 1.0f, 0);
                gptUI->slider_float("Contrast", &ptSettings->tTonemap.fContrast, 0.0f, 2.0f, 0);
                gptUI->slider_float("Saturation", &ptSettings->tTonemap.fSaturation, 0.0f, 2.0f, 0);

                gptUI->separator_text("Bloom");
                bool bBloomActive = ptSettings->tBloom.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                gptUI->checkbox("Bloom", &bBloomActive);

                if(bBloomActive)
                {
                    gptUI->slider_float("Bloom Radius", &ptSettings->tBloom.fRadius, 0.0f, 10.0f, 0);
                    gptUI->slider_float("Bloom Strength", &ptSettings->tBloom.fStrength, 0.0f, 1.0f, 0);
                    int iBloomChainLength = (int)ptSettings->tBloom.uChainLength;
                    if(gptUI->slider_int("Bloom Chain", &iBloomChainLength, 2, 10, 0))
                        ptSettings->tBloom.uChainLength = (uint32_t)iBloomChainLength;
                    ptSettings->tBloom.tFlags |= PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                }
                else
                    ptSettings->tBloom.tFlags &= ~PL_RENDERER_BLOOM_FLAGS_ACTIVE;

                gptUI->separator_text("Fog");

                bool bFog = ptSettings->tFog.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE;
                gptUI->checkbox("Fog", &bFog);
                if(bFog)
                {
                    ptSettings->tFog.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
                    gptUI->radio_button("Linear Fog", &ptSettings->tFog.tMode, 0);
                    gptUI->radio_button("Exponential Fog", &ptSettings->tFog.tMode, 1);
                    gptUI->slider_float("Fog Start", &ptSettings->tFog.fStart, 0.0f, 100.0f, 0);
                    gptUI->slider_float("Fog End", &ptSettings->tFog.fCutOffDistance, 0.0f, 10000.0f, 0);
                    gptUI->input_float3("Fog Color", ptSettings->tFog.tColor.d, "%g", 0);
                    if(ptSettings->tFog.tMode == PL_RENDERER_FOG_MODE_EXPONENTIAL)
                    {
                        gptUI->slider_float("Fog Max Opacity", &ptSettings->tFog.fMaxOpacity, 0.0f, 1.0f, 0);
                        gptUI->slider_float("Fog Density", &ptSettings->tFog.fDensity, 0.0f, 1.0f, 0);
                        gptUI->slider_float("Fog Height", &ptSettings->tFog.fHeight, -100.0f, 100.0f, 0);
                        gptUI->slider_float("Fog Height Falloff", &ptSettings->tFog.fHeightFalloff, 0.0f, 1.0f, 0);
                    }  
                }
                else
                    ptSettings->tFog.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;

                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_INDUSTRY " Terrain Options", 0))
            {
                if(ptTerrainOptions)
                {
                    gptUI->slider_float("fTau", &ptTerrainOptions->fTau, 0.0f, 1.0f, 0);

                    gptUI->checkbox_flags("Wireframe", &ptTerrainOptions->tFlags, PL_TERRAIN_FLAGS_WIREFRAME);
                    gptUI->checkbox_flags("Show Levels", &ptTerrainOptions->tFlags, PL_TERRAIN_FLAGS_SHOW_LEVELS);

                    gptUI->slider_float("fSlopeStart", &ptTerrainOptions->fSlopeStart, 0.0f, 1.0f, 0);
                    gptUI->slider_float("fSlopeEnd", &ptTerrainOptions->fSlopeEnd, 0.0f, 1.0f, 0);

                    gptUI->input_float("Terrain Depth Bias", &ptTerrainOptions->fTerrainShadowConstantDepthBias, "%g", 0);
                    gptUI->input_float("Terrain Slope Depth Bias", &ptTerrainOptions->fTerrainShadowSlopeDepthBias, "%g", 0);

                    for(uint32_t i = 0; i < PL_MAX_TERRAIN_ELEVATION_ZONES; i++)
                    {
                        if(gptUI->tree_node_f("Zone: %d", i))
                        {
                            gptUI->input_float("fMinElevation", &ptTerrainOptions->atElevationZones[i].fMinElevation, "%g", 0);
                            gptUI->input_float("fMaxElevation", &ptTerrainOptions->atElevationZones[i].fMaxElevation, "%g", 0);
                            gptUI->input_float("fBlendSize", &ptTerrainOptions->atElevationZones[i].fBlendSize, "%g", 0);
                            gptUI->input_float4("Flat Material", ptTerrainOptions->atElevationZones[i].tFlatMaterial.tBaseColor.d, "%g", 0);
                            gptUI->input_float4("Steep Material", ptTerrainOptions->atElevationZones[i].tSteepMaterial.tBaseColor.d, "%g", 0);
                            gptUI->tree_pop();
                        }
                    }
                }

                gptUI->end_collapsing_header();
            }

            if(gptUI->begin_collapsing_header(ICON_FA_BOXES_STACKED " Physics", 0))
            {
                plPhysicsEngineSettings tPhysicsSettings = {0};
                gptPhysics->get_settings(&tPhysicsSettings);

                gptUI->checkbox("Enabled", &tPhysicsSettings.bEnabled);
                gptUI->checkbox("Debug Draw", &ptAppData->bPhysicsDebugDraw);
                gptUI->slider_float("Simulation Speed", &tPhysicsSettings.fSimulationMultiplier, 0.01f, 3.0f, 0);
                gptUI->input_float("Sleep Epsilon", &tPhysicsSettings.fSleepEpsilon, "%g", 0);
                gptUI->input_float("Position Epsilon", &tPhysicsSettings.fPositionEpsilon, "%g", 0);
                gptUI->input_float("Velocity Epsilon", &tPhysicsSettings.fVelocityEpsilon, "%g", 0);
                gptUI->input_uint("Max Position Its.", &tPhysicsSettings.uMaxPositionIterations, 0);
                gptUI->input_uint("Max Velocity Its.", &tPhysicsSettings.uMaxVelocityIterations, 0);
                gptUI->input_float("Frame Rate", &tPhysicsSettings.fSimulationFrameRate, "%g", 0);
                if(gptUI->button("Wake All")) gptPhysics->wake_up_all();
                if(gptUI->button("Sleep All")) gptPhysics->sleep_all();

                gptPhysics->set_settings(tPhysicsSettings);
                gptUI->end_collapsing_header();
            }

            gptUI->layout_dynamic(0.0f, 2);

            if(gptUI->begin_collapsing_header(ICON_FA_SCREWDRIVER_WRENCH " Tools", 0))
            {
                gptUI->checkbox("Device Memory", ptAppData->pbShowDeviceMemoryAnalyzer);
                gptUI->checkbox("Memory Allocations", ptAppData->pbShowMemoryAllocations);
                gptUI->checkbox("Profiling", ptAppData->pbShowProfiling);
                gptUI->checkbox("Statistics", ptAppData->pbShowStats);
                gptUI->checkbox("Logging", ptAppData->pbShowLogging);
                gptUI->checkbox("Assets", ptAppData->pbShowAssets);
                gptUI->checkbox("Entities", &ptAppData->bShowEntityWindow);
                gptUI->end_collapsing_header();
            }
            if(gptUI->begin_collapsing_header(ICON_FA_USER_GEAR " User Interface", 0))
            {
                gptUI->checkbox("UI Demo", &ptAppData->bShowUiDemo);
                gptUI->checkbox("UI Debug", &ptAppData->bShowUiDebug);
                gptUI->checkbox("UI Style", &ptAppData->bShowUiStyle);
                gptUI->end_collapsing_header();
            }

            gptUI->layout_dynamic(0.0f, 1);

            if(gptUI->begin_collapsing_header(ICON_FA_BUG " Debug Options", 0))
            {
                gptUI->checkbox("Show Debug Lights", &ptAppData->bShowDebugLights);
                gptUI->checkbox("Show Bounding Boxes", &ptAppData->bDrawAllBoundingBoxes);
                gptUI->checkbox("Show Probes", &tDebugOptions.bShowProbes);
                gptUI->checkbox("Show Probe Ranges", &tDebugOptions.bShowProbeRange);
                gptUI->checkbox("Show Origin", &tDebugOptions.bShowOrigin);
                gptUI->checkbox("Show Grid", &tEditorViewOptions.bShowGrid);
                gptUI->checkbox("Selected Bounding Box", &tEditorViewOptions.bShowSelectedBoundingBox);
                gptUI->slider_uint("Outline Width", &tEditorViewOptions.uOutlineWidth, 2, 50, 0);
            
                gptUI->checkbox("Show BVH", &ptAppData->bShowBVH);
                if(gptUI->checkbox("Wireframe", &tDebugOptions.bWireframe))
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
                if(gptUI->begin_combo("Combo", apcShaderDebugModeText[tDebugOptions.tShaderDebugMode], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < PL_ARRAYSIZE(apcShaderDebugModeText); i++)
                    {
                        bool bPlaceHolder = tDebugOptions.tShaderDebugMode == (int)i;
                        if(gptUI->selectable(apcShaderDebugModeText[i], &bPlaceHolder, 0))
                        {
                            tDebugOptions.tShaderDebugMode = (int)i;
                            gptUI->close_current_popup();
                            bReloadShaders = true;
                        }
                    }
                    gptUI->end_combo();
                }

                gptUI->end_collapsing_header();
            }

            gptUI->end_window();
        }
    }

    if(!bReloadScene && !bLoadScene)
    {
        gptRendererEditor->set_scene_options(ptAppData->ptScene, &tEditorSceneOptions);
        gptRendererEditor->set_view_options(ptAppData->ptView, &tEditorViewOptions);
        gptRendererDebug->set_scene_options(ptAppData->ptScene, &tDebugOptions);
    }

    if(bReloadShaders)
    {
        gptRendererEditor->reload_scene_shaders(ptAppData->ptScene);
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
    gptGltf             = pl_get_api_latest(ptApiRegistry, plGltfI);
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
    gptRendererEditor   = pl_get_api_latest(ptApiRegistry, plRendererEditorI);
    gptAsset            = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptTransform        = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptIk               = pl_get_api_latest(ptApiRegistry, plIkI);
    gptSkeleton         = pl_get_api_latest(ptApiRegistry, plSkeletonI);
    gptTexture          = pl_get_api_latest(ptApiRegistry, plTextureI);
    gptTerrain          = pl_get_api_latest(ptApiRegistry, plTerrainI);
    gptStl              = pl_get_api_latest(ptApiRegistry, plStlI);
}

void
pl__refresh_files(plAppData* ptAppData)
{
    pl_sb_reset(ptAppData->sbtSceneFilesCore);

    pl_sb_add(ptAppData->sbtSceneEnvironments);
    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acName, "None", 5);
    strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acPath, "None", 5);

    // local
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFilesCore, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("plscene", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "/assets/scenes/%s.plscene", acFileNameOnly);
                    pl_sprintf(acFullPath, "/assets/scenes/%s.plscene", acFileNameOnly);
                    // if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFilesCore);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesCore).acName, acFileNameOnly, PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesCore).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    // {
    //     plDirectoryInfo tDirectoryInfo = {0};
    //     gptFile->get_directory_info("../resources/core/environments/", &tDirectoryInfo);
    //     for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
    //     {
    //         if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
    //         {
    //             char acExtensionBuffer[16] = {0};
    //             char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
    //             pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
    //             // if(pl_str_equal("json", acExtensionBuffer))
    //             {
    //                 pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
    //                 char acFullPath[PL_MAX_PATH_LENGTH] = {0};
    //                 pl_sprintf(acFullPath, "../resources/core/environments/%s.hdr", acFileNameOnly);
    //                 pl_sb_add(ptAppData->sbtSceneEnvironments);
    //                 strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acName, acFileNameOnly, PL_MAX_PATH_LENGTH);
    //                 strncpy(pl_sb_back(ptAppData->sbtSceneEnvironments).acPath, acFullPath, PL_MAX_PATH_LENGTH);
    //             }
    //         }
    //     }
    //     gptFile->cleanup_directory_info(&tDirectoryInfo);
    // }
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl__ui_demo.cpp"

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
